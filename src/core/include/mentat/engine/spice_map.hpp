// SPDX-License-Identifier: GPL-3.0-or-later
//
// The small map of the regions over the map of Dune ("SEE SPICE DENSITY"):
// a framed window, a screen pixel to a map point, each region Paul knows
// coloured by its spice or by the work of his troops there. Giving a troop
// somewhere to go uses it too. See docs/rules/spice_map.md.
#pragma once

#include "mentat/engine/small_map.hpp"
#include "mentat/gfx/scene.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mentat {

class Game;
class MapScreen;
class Panel;
class World;

class SpiceMap {
public:
    // The original's number for "SEE SPICE DENSITY".
    static constexpr std::uint16_t kOpen = 0x53F1;

    SpiceMap(Game& game, World& world, Panel& panel, MapScreen& map);

    bool shown() const { return shown_; }
    // The window, frame and all.
    ClipRect frame() const { return ClipRect{left_, top_, left_ + kWidth, top_ + kHeight}; }
    // The map inside it.
    ClipRect view() const;

    // Opens the window where the state keeps it, its middle at the main
    // map's. It is drawn on the screen only: the back buffer keeps what is
    // under it.
    void open();
    // Opens it at (left, top) for troop `troop` to be given somewhere to
    // go, marking the troop and its way instead of Paul.
    void openFor(int troop, int left, int top);
    // Closes it: the screen under it comes back from the back buffer.
    void close();
    // The map has gone from the screen, and the window with it.
    void forget() {
        shown_ = false;
        pointed_ = 0;
    }
    // Draws it again, after a change.
    void redraw();
    // Moves its middle by a step of the arrows.
    void scroll(int longitude, int latitude);

    // Where the mouse is: the bar at the bottom tells about what it points
    // at.
    void pointAt(int x, int y);

    enum class Click {
        Outside, // not on the window
        Close,   // the box at the top left
        Title,   // the rest of the top strip
        Toggle,  // the bar: the other colouring
        Map,     // on the map, `place` the place pointed at or 0
    };
    Click click(int x, int y, std::size_t& place);
    // The place the mouse points at, 0 for none.
    std::size_t pointed() const { return pointed_; }

    // The colouring: false the spice of each region, true the work of
    // Paul's troops in it.
    bool occupations = false;

    static constexpr int kWidth = 0xAA;
    static constexpr int kHeight = 0x6C;

private:
    void openAt(int troop, int left, int top);
    void draw();
    void drawRoute();
    void drawRegions();
    void drawPlaces();
    void drawBar();
    void drawLegend(bool lit);
    void drawScale();
    void drawPlaceInfo(std::size_t record);
    void drawMainView();
    void drawMarker();
    void markDensity(int index);
    std::size_t nearestPlace(int x, int y) const;
    std::vector<std::uint8_t> colours() const;
    void counts(std::size_t record, int& spice, int& army, int& ecology) const;

    Game& g_;
    World& world_;
    Panel& panel_;
    MapScreen& map_;
    SmallMap small_;
    bool shown_ = false;
    int troop_ = 0;
    int left_ = 0;
    int top_ = 0;
    std::size_t pointed_ = 0;
    int density_ = -1; // the spice level marked on the bar
    bool legendLit_ = false;
};

} // namespace mentat
