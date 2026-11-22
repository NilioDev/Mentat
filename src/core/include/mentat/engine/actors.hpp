// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sprites that move over a still background: each one is kept as a
// rectangle, and when one moves only the rectangle it leaves and the one it
// enters are drawn again, from the kept background up. See
// docs/rules/actors.md.
#pragma once

#include "mentat/gfx/scene.hpp"

#include <cstdint>
#include <vector>

namespace mentat {

class Game;

struct Actor {
    ClipRect rect;            // where it is drawn: right and bottom one past the sprite
    int sprite = 0;           // sprite number in the sheet the actors use
    std::uint16_t motion = 0; // what the owner keeps for its motion
    std::uint16_t extra = 0;  // more room for the owner
};

class Actors {
public:
    explicit Actors(Game& game) : game_(game) {}

    // The picture file the actors' sprites come from.
    void useSheet(int resource) { sheet_ = resource; }
    // Nothing is drawn outside this rectangle.
    void setClip(const ClipRect& clip) { clip_ = clip; }

    std::vector<Actor>& list() { return actors_; }

    // Adds an actor with its centre at (x, y), without drawing it.
    void add(int sprite, int x, int y, std::uint16_t motion);
    // Moves actor `i` by (dx, dy) and draws what changes.
    void move(std::size_t i, int dx, int dy);
    // Changes the sprite of actor `i`, keeping its top-left corner (nothing
    // is drawn until it next moves).
    void setSprite(std::size_t i, int sprite);
    // Takes actor `i` away and draws the background where it was.
    void remove(std::size_t i);
    void clear() { actors_.clear(); }

    // Draws the rectangle again: background, then every actor that
    // touches it in list order, then shows it on the screen.
    void redraw(ClipRect rect);

private:
    Game& game_;
    int sheet_ = -1;
    ClipRect clip_{0, 0, 320, 152};
    std::vector<Actor> actors_;
};

} // namespace mentat
