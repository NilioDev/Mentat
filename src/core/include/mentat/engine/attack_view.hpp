// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Harkonnen attack seen from far away: ornithopters cross the sky,
// shots fall and the horizon flashes. See docs/rules/attack.md.
#pragma once

#include "mentat/engine/actors.hpp"

#include <cstdint>
#include <vector>

namespace mentat {

class Game;

class AttackView {
public:
    explicit AttackView(Game& game) : game_(game), actors_(game) {}

    // Draws the empty landscape as the new background and starts the
    // battle and its sound.
    void start();
    // The same at war, with the horizon of Paul's place: a sprite list
    // (sprite, x, y words, ending with 0xFFFF).
    void start(const std::vector<std::uint16_t>& horizon);
    bool running() const { return running_; }
    // A blast lights the horizon: the flash counter starts again from
    // `count` (0x0B, or 0x11 for a brighter one).
    void blast(int count) { flash_ = count; }
    // While set, only the flashes go on (something else is in front).
    bool paused = false;
    // Stops the battle, its sound and its timer, when it runs.
    void stop();

    static constexpr int kTimer = 0x0B45;
    static constexpr int kSheet = 0x2B; // ATTACK.HSQ

private:
    void tick();
    void spawnFlyer();
    void flash(int step);
    void setColours(int sprite, bool spare);
    bool noiseBit();

    Game& game_;
    Actors actors_;
    int nextFlyer_ = 0;   // ticks of the battle until the next flyer
    int flyerWave_ = 0;   // flyers left of the same kind
    std::uint16_t flyerKind_ = 0;
    int pause_ = 0;       // battle ticks without new shots
    int shots_ = 0;       // shots left before the next pause
    int flash_ = 0;       // counts down to and through a flash
    std::uint16_t noise_ = 0;
    bool running_ = false;
};

} // namespace mentat
