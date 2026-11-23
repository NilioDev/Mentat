// SPDX-License-Identifier: GPL-3.0-or-later
//
// A vision: a chief's message comes to Paul as a face over shimmering
// clouds, with a window on the planet showing the place it is about. See
// docs/rules/messages.md.
#pragma once

#include "mentat/engine/small_map.hpp"

#include <cstddef>
#include <cstdint>

namespace mentat {

class Game;
class MapScreen;
class World;

class Vision {
public:
    Vision(Game& game, World& world, MapScreen& map);

    // Draws the vision on the back buffer: the clouds and, for a place
    // other than the one Paul is at, the window on the planet around it.
    void draw(std::size_t place);
    // Only the window on the planet around `place`, on the back buffer.
    void drawWindow(std::size_t place);
    // A turn of the clouds' colours and a ripple through the picture on
    // the screen, every 6 ticks while the vision lasts.
    void shimmer();
    void stop();

private:
    Game& g_;
    World& world_;
    SmallMap small_;
    int ripple_ = 0; // the row the ripple starts at, from the top of its area
    std::ptrdiff_t lastRun_ = 5; // where the last run began on the screen
};

} // namespace mentat
