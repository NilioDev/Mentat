// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/audio/opl.hpp"

#include "mentat/engine/platform.hpp"

// Third-party C headers, not held to our warnings.
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#endif
extern "C" {
#include "TDA8425_emu.h"
#include "YM7128B_emu.h"
#include "opl3.h"
}
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cmath>

namespace mentat {

// --- The chip

struct Opl::Chip {
    opl3_chip chip{};
};

Opl::Opl() : chip_(std::make_unique<Chip>()) {
    OPL3_Reset(&chip_->chip, kRate);
}

Opl::~Opl() = default;

void Opl::writeRegister(std::uint16_t reg, std::uint8_t value) {
    OPL3_WriteReg(&chip_->chip, reg, value);
}

void Opl::generate(std::int16_t frame[2]) {
    OPL3_Generate(&chip_->chip, frame);
}

// --- The AdLib Gold's output

namespace {

// Mentat's choices for the levels around the chips (see
// docs/rules/music-gold.md): the chip's full scale is 1, the surround
// module's sound is added to the direct sound, and the end result is
// brought back near the Sound Blaster Pro's loudness.
constexpr double kChipScale = 1.0 / 32768.0;
constexpr double kSurroundLevel = 1.0;
constexpr double kOutputLevel = 0.75;

} // namespace

struct GoldOutput::Chips {
    YM7128B_ChipIdeal surround{};
    TDA8425_Chip stereo{};
};

GoldOutput::GoldOutput() : chips_(std::make_unique<Chips>()) {
    YM7128B_ChipIdeal_Ctor(&chips_->surround);
    YM7128B_ChipIdeal_Reset(&chips_->surround);
    YM7128B_ChipIdeal_Setup(&chips_->surround, static_cast<YM7128B_TapIdeal>(Opl::kRate));
    YM7128B_ChipIdeal_Start(&chips_->surround);

    // The pseudo stereo capacitors are not known for the AdLib Gold; these
    // are the emulator's first preset.
    TDA8425_Chip_Ctor(&chips_->stereo);
    TDA8425_Chip_Setup(&chips_->stereo, static_cast<TDA8425_Float>(Opl::kRate), 15e-9, 15e-9,
                       TDA8425_Tfilter_Mode_Disabled);
    TDA8425_Chip_Reset(&chips_->stereo);
    // Until the player sets it: full volume, flat tone, linear stereo.
    TDA8425_Chip_Write(&chips_->stereo, TDA8425_Reg_VL, 0xFC);
    TDA8425_Chip_Write(&chips_->stereo, TDA8425_Reg_VR, 0xFC);
    TDA8425_Chip_Write(&chips_->stereo, TDA8425_Reg_BA, 0xF6);
    TDA8425_Chip_Write(&chips_->stereo, TDA8425_Reg_TR, 0xF6);
    TDA8425_Chip_Write(&chips_->stereo, TDA8425_Reg_SF, 0xCE);
    TDA8425_Chip_Start(&chips_->stereo);
}

GoldOutput::~GoldOutput() {
    YM7128B_ChipIdeal_Stop(&chips_->surround);
    YM7128B_ChipIdeal_Dtor(&chips_->surround);
    TDA8425_Chip_Stop(&chips_->stereo);
    TDA8425_Chip_Dtor(&chips_->stereo);
}

void GoldOutput::writeControl(std::uint8_t reg, std::uint8_t value) {
    switch (reg) {
    case 0x04: // stereo processor: volume left, right, bass, treble
        TDA8425_Chip_Write(&chips_->stereo, TDA8425_Reg_VL, value);
        break;
    case 0x05:
        TDA8425_Chip_Write(&chips_->stereo, TDA8425_Reg_VR, value);
        break;
    case 0x06:
        TDA8425_Chip_Write(&chips_->stereo, TDA8425_Reg_BA, value);
        break;
    case 0x07:
        TDA8425_Chip_Write(&chips_->stereo, TDA8425_Reg_TR, value);
        break;
    case 0x08: {
        // The switch functions. Mode bits 1 and 2 both 0 is not defined
        // for the chip; Mentat takes it as stereo (see the rules).
        std::uint8_t v = value;
        if ((v & 0x06) == 0) {
            v = static_cast<std::uint8_t>(v | 0x06);
        }
        TDA8425_Chip_Write(&chips_->stereo, TDA8425_Reg_SF, v);
        break;
    }
    case 0x09:
        fmLeft_ = fmVolume(value);
        break;
    case 0x0A:
        fmRight_ = fmVolume(value);
        break;
    default:
        break;
    }
}

void GoldOutput::writeSurround(std::uint8_t reg, std::uint8_t value) {
    YM7128B_ChipIdeal_Write(&chips_->surround, reg, value);
}

void GoldOutput::process(const std::int16_t in[2], double out[2]) {
    const double left = in[0] * kChipScale * fmLeft_;
    const double right = in[1] * kChipScale * fmRight_;

    YM7128B_ChipIdeal_Process_Data surround{};
    surround.inputs[YM7128B_InputChannel_Mono] = (left + right) * 0.5;
    YM7128B_ChipIdeal_Process(&chips_->surround, &surround);

    TDA8425_Chip_Process_Data stereo{};
    for (int s = 0; s < TDA8425_Source_Count; ++s) {
        stereo.inputs[s][TDA8425_Stereo_L] = left + surround.outputs[YM7128B_OutputChannel_Left] * kSurroundLevel;
        stereo.inputs[s][TDA8425_Stereo_R] = right + surround.outputs[YM7128B_OutputChannel_Right] * kSurroundLevel;
    }
    TDA8425_Chip_Process(&chips_->stereo, &stereo);
    out[0] = stereo.outputs[TDA8425_Stereo_L] * kOutputLevel;
    out[1] = stereo.outputs[TDA8425_Stereo_R] * kOutputLevel;
}

// --- Both together

MusicChips::MusicChips(MusicCard card) {
    if (card == MusicCard::AdLibGold) {
        gold_ = std::make_unique<GoldOutput>();
    }
}

void MusicChips::writeControl(std::uint8_t reg, std::uint8_t value) {
    if (gold_) {
        gold_->writeControl(reg, value);
    }
}

void MusicChips::writeSurround(std::uint8_t reg, std::uint8_t value) {
    if (gold_) {
        gold_->writeSurround(reg, value);
    }
}

void MusicChips::generate(std::size_t count, double gain, std::vector<std::int16_t>& out) {
    out.reserve(out.size() + count * 2);
    const auto clamp16 = [](double v) { return static_cast<std::int16_t>(std::clamp(v, -32768.0, 32767.0)); };
    for (std::size_t i = 0; i < count; ++i) {
        std::int16_t frame[2] = {};
        opl_.generate(frame);
        if (gold_) {
            double speakers[2] = {};
            gold_->process(frame, speakers);
            out.push_back(clamp16(speakers[0] * 32768.0));
            out.push_back(clamp16(speakers[1] * 32768.0));
        } else {
            // The Sound Blaster Pro player plays every voice on both sides.
            const std::int16_t v = clamp16((static_cast<double>(frame[0]) + frame[1]) * 0.5 * gain);
            out.push_back(v);
            out.push_back(v);
        }
    }
}

FmMusic::FmMusic(const MusicTables& tables) : chips_(tables.card), driver_(tables) {
    driver_.init(chips_);
}

double FmMusic::gainOf(std::uint8_t mixer) {
    // Mentat's choice: about 2 dB a level, both sides averaged; 0 is silent.
    const auto gain = [](int level) { return level == 0 ? 0.0 : std::pow(10.0, -2.0 * (15 - level) / 20.0); };
    return (gain(mixer >> 4) + gain(mixer & 0x0F)) * 0.5;
}

void FmMusic::runTo(std::uint32_t tick, std::vector<std::int16_t>& out) {
    if (!started_) {
        started_ = true;
        tick_ = tick;
        return;
    }
    const double perTick = Opl::kRate / kTicksPerSecond;
    while (static_cast<std::int32_t>(tick - tick_) > 0) {
        ++tick_;
        driver_.tick(chips_);
        fraction_ += perTick;
        const auto count = static_cast<std::size_t>(fraction_);
        fraction_ -= static_cast<double>(count);
        chips_.generate(count, gainOf(driver_.mixer()), out);
    }
}

} // namespace mentat
