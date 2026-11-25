// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/audio/mt32_music.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/engine/platform.hpp"

#include <mt32emu.h>

#include <fstream>
#include <iterator>
#include <vector>

namespace mentat {

namespace {

std::vector<MT32Emu::Bit8u> readRom(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw FormatError("cannot read " + path.filename().string());
    }
    return {std::istreambuf_iterator<char>(in), {}};
}

} // namespace

struct Mt32Music::Synth : MidiSink {
    std::vector<MT32Emu::Bit8u> controlBytes;
    std::vector<MT32Emu::Bit8u> pcmBytes;
    std::unique_ptr<MT32Emu::ArrayFile> controlFile;
    std::unique_ptr<MT32Emu::ArrayFile> pcmFile;
    const MT32Emu::ROMImage* control = nullptr;
    const MT32Emu::ROMImage* pcm = nullptr;
    MT32Emu::Synth synth;

    ~Synth() override {
        synth.close();
        if (control != nullptr) {
            MT32Emu::ROMImage::freeROMImage(control);
        }
        if (pcm != nullptr) {
            MT32Emu::ROMImage::freeROMImage(pcm);
        }
    }

    void send(std::span<const std::uint8_t> message) override {
        MT32Emu::Bit32u packed = 0;
        for (std::size_t i = 0; i < message.size() && i < 3; ++i) {
            packed |= static_cast<MT32Emu::Bit32u>(message[i]) << (8 * i);
        }
        synth.playMsgNow(packed);
    }
};

Mt32Music::Mt32Music(const std::filesystem::path& controlRom, const std::filesystem::path& pcmRom)
    : synth_(std::make_unique<Synth>()) {
    Synth& s = *synth_;
    s.controlBytes = readRom(controlRom);
    s.pcmBytes = readRom(pcmRom);
    s.controlFile = std::make_unique<MT32Emu::ArrayFile>(s.controlBytes.data(), s.controlBytes.size());
    s.pcmFile = std::make_unique<MT32Emu::ArrayFile>(s.pcmBytes.data(), s.pcmBytes.size());
    s.control = MT32Emu::ROMImage::makeROMImage(s.controlFile.get());
    s.pcm = MT32Emu::ROMImage::makeROMImage(s.pcmFile.get());
    const auto* controlInfo = s.control != nullptr ? s.control->getROMInfo() : nullptr;
    const auto* pcmInfo = s.pcm != nullptr ? s.pcm->getROMInfo() : nullptr;
    if (controlInfo == nullptr || controlInfo->type != MT32Emu::ROMInfo::Control) {
        throw FormatError(controlRom.filename().string() + " is not an MT-32 control ROM");
    }
    if (pcmInfo == nullptr || pcmInfo->type != MT32Emu::ROMInfo::PCM) {
        throw FormatError(pcmRom.filename().string() + " is not an MT-32 PCM ROM");
    }
    if (!s.synth.open(*s.control, *s.pcm)) {
        throw FormatError("the MT-32 emulator does not start with these ROMs");
    }
    driver_.init(s);
}

Mt32Music::~Mt32Music() = default;

int Mt32Music::rate() const { return static_cast<int>(synth_->synth.getStereoOutputSampleRate()); }

void Mt32Music::start(std::shared_ptr<const Song> song, int plays) { driver_.start(*synth_, std::move(song), plays); }

void Mt32Music::stop() { driver_.stop(*synth_); }

void Mt32Music::setVolume(std::uint8_t volume, std::uint8_t) { driver_.setVolume(volume); }

void Mt32Music::fade(int ticks, std::uint8_t volume, std::uint8_t) { driver_.fade(ticks, volume); }

void Mt32Music::runTo(std::uint32_t tick, std::vector<std::int16_t>& out) {
    if (!started_) {
        started_ = true;
        tick_ = tick;
        return;
    }
    const double perTick = rate() / kTicksPerSecond;
    std::vector<MT32Emu::Bit16s> frames;
    while (static_cast<std::int32_t>(tick - tick_) > 0) {
        ++tick_;
        driver_.tick(*synth_);
        fraction_ += perTick;
        const auto count = static_cast<std::size_t>(fraction_);
        fraction_ -= static_cast<double>(count);
        frames.resize(count * 2);
        synth_->synth.render(frames.data(), static_cast<MT32Emu::Bit32u>(count));
        out.insert(out.end(), frames.begin(), frames.end());
    }
}

} // namespace mentat
