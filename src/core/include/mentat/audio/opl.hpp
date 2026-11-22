// SPDX-License-Identifier: GPL-3.0-or-later
//
// The emulated FM chip (Nuked OPL3, in third_party/) and the music it plays,
// advanced in timer ticks. The game owns one and runs it as its clock moves.
#pragma once

#include "mentat/audio/music_driver.hpp"

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

    void writeRegister(std::uint8_t reg, std::uint8_t value) override;
    // Appends `count` mono samples, scaled by `gain` (0..1).
    void generate(std::size_t count, double gain, std::vector<std::int16_t>& out);

private:
    struct Chip;
    std::unique_ptr<Chip> chip_;
};

// The music player on the timer: one driver tick per timer tick, and the
// chip's sound for the time in between.
class MusicSystem {
public:
    explicit MusicSystem(const MusicTables& tables);

    MusicDriver& driver() { return driver_; }
    OplSink& opl() { return opl_; }

    // Runs every timer tick up to `tick` and appends the sound for them.
    void runTo(std::uint32_t tick, std::vector<std::int16_t>& out);

    // The gain of the mixer's FM levels.
    static double gainOf(std::uint8_t mixer);

private:
    Opl opl_;
    MusicDriver driver_;
    std::uint32_t tick_ = 0;
    bool started_ = false;
    double fraction_ = 0;
};

} // namespace mentat
