// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/audio/opl.hpp"

#include "mentat/engine/platform.hpp"

extern "C" {
#include "opl3.h"
}

#include <algorithm>
#include <cmath>

namespace mentat {

struct Opl::Chip {
    opl3_chip chip{};
};

Opl::Opl() : chip_(std::make_unique<Chip>()) {
    OPL3_Reset(&chip_->chip, kRate);
}

Opl::~Opl() = default;

void Opl::writeRegister(std::uint8_t reg, std::uint8_t value) {
    OPL3_WriteReg(&chip_->chip, reg, value);
}

void Opl::generate(std::size_t count, double gain, std::vector<std::int16_t>& out) {
    out.reserve(out.size() + count);
    for (std::size_t i = 0; i < count; ++i) {
        std::int16_t frame[2] = {};
        OPL3_Generate(&chip_->chip, frame);
        const double v = (static_cast<double>(frame[0]) + frame[1]) * 0.5 * gain;
        out.push_back(static_cast<std::int16_t>(std::clamp(v, -32768.0, 32767.0)));
    }
}

MusicSystem::MusicSystem(const MusicTables& tables) : driver_(tables) {
    driver_.init(opl_);
}

double MusicSystem::gainOf(std::uint8_t mixer) {
    // Mentat's choice: about 2 dB a level, both sides averaged; 0 is silent.
    const auto gain = [](int level) { return level == 0 ? 0.0 : std::pow(10.0, -2.0 * (15 - level) / 20.0); };
    return (gain(mixer >> 4) + gain(mixer & 0x0F)) * 0.5;
}

void MusicSystem::runTo(std::uint32_t tick, std::vector<std::int16_t>& out) {
    if (!started_) {
        started_ = true;
        tick_ = tick;
        return;
    }
    const double perTick = Opl::kRate / kTicksPerSecond;
    while (static_cast<std::int32_t>(tick - tick_) > 0) {
        ++tick_;
        driver_.tick(opl_);
        fraction_ += perTick;
        const auto count = static_cast<std::size_t>(fraction_);
        fraction_ -= static_cast<double>(count);
        opl_.generate(count, gainOf(driver_.mixer()), out);
    }
}

} // namespace mentat
