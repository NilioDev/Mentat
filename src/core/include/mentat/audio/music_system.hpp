// SPDX-License-Identifier: GPL-3.0-or-later
//
// A music player and the sound device it plays on, as the game sees it:
// pieces started and stopped, volumes and fades, and where the piece is.
// The FM player (mentat/audio/opl.hpp) is one; the game runs it as its
// clock moves and can swap it for another while playing.
#pragma once

#include "mentat/audio/music.hpp"
#include "mentat/audio/music_driver.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace mentat {

class MusicSystem {
public:
    virtual ~MusicSystem() = default;

    // Which arrangement of the music it plays.
    virtual MusicCard card() const = 0;
    // Starts a piece. `plays` 0 repeats it forever.
    virtual void start(std::shared_ptr<const Song> song, int plays) = 0;
    // A player with pieces of its own (the Mega CD's) is not handed the CD
    // version's: the game names the piece by its number instead.
    virtual bool playsOwnPieces() const { return false; }
    virtual void startPiece(int /*track*/, int /*plays*/) {}
    virtual void stop() = 0;
    // Lets the piece end after the play that is running.
    virtual void endAfterThisPlay() = 0;
    // The game's volume byte and balance (0x78 is the middle).
    virtual void setVolume(std::uint8_t volume, std::uint8_t balance) = 0;
    virtual void fade(int ticks, std::uint8_t volume, std::uint8_t balance) = 0;
    virtual const MusicStatus& status() const = 0;
    // Runs every timer tick up to `tick` and appends the sound for them, as
    // stereo frames at rate().
    virtual void runTo(std::uint32_t tick, std::vector<std::int16_t>& out) = 0;
    virtual int rate() const = 0;
};

} // namespace mentat
