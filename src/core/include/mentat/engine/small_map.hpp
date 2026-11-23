// SPDX-License-Identifier: GPL-3.0-or-later
//
// The small map of the ornithopter: a screen pixel to a map point around a
// middle, with the places shown on it. The cockpit and the flight's corner
// both use it. See docs/rules/cockpit.md.
#pragma once

#include "mentat/gfx/scene.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mentat {

class Game;
class MapScreen;
class World;

class SmallMap {
public:
    SmallMap(Game& game, World& world, MapScreen& map) : g_(game), world_(world), map_(map) {}

    ClipRect view{};
    // The map point in the middle of the view.
    std::uint16_t longitude = 0;
    int latitude = 0;

    // Draws the map on the target; the middle's latitude may move so that
    // no row goes past latitude 86.
    void drawMap();
    // Draws the places not hidden on the target, clipped to the view, with
    // the panel's picture file in use, and keeps where they are.
    void drawPlaces();

    // Where map point (longitude, latitude) is on the screen; false when it
    // is outside the view.
    bool toScreen(std::uint16_t lon, int lat, int& x, int& y) const;
    // The map point shown at (x, y) on the screen, inside the view.
    void toMap(int x, int y, std::uint16_t& lon, int& lat) const;

    struct Shown {
        std::size_t record = 0;
        int x = 0;
        int y = 0;
    };
    const std::vector<Shown>& shown() const { return shown_; }
    void forget() { shown_.clear(); }

private:
    Game& g_;
    World& world_;
    MapScreen& map_;
    std::vector<Shown> shown_;
};

} // namespace mentat
