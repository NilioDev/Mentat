// SPDX-License-Identifier: GPL-3.0-or-later
//
// The emulated FM chip (Nuked OPL3, in third_party/), the AdLib Gold's
// mixer, surround module and stereo processor around it, and the music it
// plays, advanced in timer ticks. The game owns one and runs it as its clock
// moves. See docs/rules/music.md and docs/rules/music-gold.md.
#pragma once

#include "mentat/audio/music_driver.hpp"
#include "mentat/audio/music_system.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace mentat {

class Opl : public OplSink {
public:
    static constexpr int kRate = 49716; // the chip's own sample rate

    Opl();
    ~Opl() override;
    Opl(const Opl&) = delete;
    Opl& operator=(const Opl&) = delete;

    void writeRegister(std::uint16_t reg, std::uint8_t value) override;
    // One stereo frame of the chip.
    void generate(std::int16_t frame[2]);

private:
    struct Chip;
    std::unique_ptr<Chip> chip_;
};

// What the AdLib Gold does to the chip's sound: its FM volumes, the
// surround module, and the stereo processor's volume, tone and stereo mode.
class GoldOutput {
public:
    GoldOutput();
    ~GoldOutput();
    GoldOutput(const GoldOutput&) = delete;
    GoldOutput& operator=(const GoldOutput&) = delete;

    void writeControl(std::uint8_t reg, std::uint8_t value);
    void writeSurround(std::uint8_t reg, std::uint8_t value);
    // Turns one frame of the chip into one frame for the speakers.
    void process(const std::int16_t in[2], double out[2]);

    // The FM volume for a control value (bits 0 to 4).
    static double fmVolume(std::uint8_t value) { return (value & 0x1F) / 31.0; }

private:
    struct Chips;
    std::unique_ptr<Chips> chips_;
    double fmLeft_ = 1;
    double fmRight_ = 1;
};

// The chip, and for the AdLib Gold its output stage, as one OplSink.
class MusicChips : public OplSink {
public:
    explicit MusicChips(MusicCard card);

    void writeRegister(std::uint16_t reg, std::uint8_t value) override { opl_.writeRegister(reg, value); }
    void writeControl(std::uint8_t reg, std::uint8_t value) override;
    void writeSurround(std::uint8_t reg, std::uint8_t value) override;

    // Appends `count` stereo frames (left, right), scaled by `gain` for the
    // Sound Blaster Pro (the AdLib Gold sets its own levels).
    void generate(std::size_t count, double gain, std::vector<std::int16_t>& out);

private:
    Opl opl_;
    std::unique_ptr<GoldOutput> gold_;
};

// The FM music player on the timer: one driver tick per timer tick, and the
// chips' sound for the time in between.
class FmMusic : public MusicSystem {
public:
    explicit FmMusic(const MusicTables& tables);

    MusicDriver& driver() { return driver_; }
    OplSink& opl() { return chips_; }

    MusicCard card() const override { return driver_.card(); }
    void start(std::shared_ptr<const Song> song, int plays) override { driver_.start(chips_, std::move(song), plays); }
    void stop() override { driver_.stop(chips_); }
    void endAfterThisPlay() override { driver_.endAfterThisPlay(); }
    void setVolume(std::uint8_t volume, std::uint8_t balance) override { driver_.setVolume(chips_, volume, balance); }
    void fade(int ticks, std::uint8_t volume, std::uint8_t balance) override { driver_.fade(ticks, volume, balance); }
    const MusicStatus& status() const override { return driver_.status(); }
    void runTo(std::uint32_t tick, std::vector<std::int16_t>& out) override;
    int rate() const override { return Opl::kRate; }

    // The gain of the Sound Blaster Pro mixer's FM levels.
    static double gainOf(std::uint8_t mixer);

private:
    MusicChips chips_;
    MusicDriver driver_;
    std::uint32_t tick_ = 0;
    bool started_ = false;
    double fraction_ = 0;
};

} // namespace mentat
