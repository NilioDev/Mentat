// SPDX-License-Identifier: GPL-3.0-or-later
//
// The map screen ("SEE DUNE MAP"): the flat map of Dune around the
// player, with the places Paul knows, a panel of its own and an arrow pad
// that moves the view. See docs/rules/map.md.
#pragma once

#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/panel.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace mentat {

class Game;
class World;

class MapScreen {
public:
    // The original's numbers for what its commands and arrows do.
    static constexpr std::uint16_t kToggle = 0x186B; // "SEE DUNE MAP" and "EXIT MAPS"
    static constexpr std::uint16_t kUp = 0x8829;
    static constexpr std::uint16_t kRight = 0x8824;
    static constexpr std::uint16_t kDown = 0x882E;
    static constexpr std::uint16_t kLeft = 0x881F;
    static constexpr std::uint16_t kCentre = 0x5B05; // the middle of the arrows

    MapScreen(Game& game, World& world, Panel& panel);
    ~MapScreen();

    bool active() const { return active_; }

    // What the program does to the places when it starts (see
    // World::setUpPlaces); the map's points of places are marked.
    void setUp();

    // Opens the map on the player's place, drawn on the back buffer.
    void enter();
    // Dissolves it onto the screen and writes the box that names the map
    // over it.
    void show();
    // Closes it; the caller draws the room again.
    void leave();

    // Runs every tick while the map is shown: the box goes 1000 ticks
    // after the last click.
    void tick();
    void clicked();

    // A command of the map's list or an arrow. False when it is not one of
    // the map's.
    bool command(std::uint16_t action);

    // The map's point at the middle of the view.
    std::uint16_t longitude() const { return longitude_; }
    int latitude() const { return latitude_; }

private:
    FlatMap& map();
    void drawView();
    void drawPlaces();
    void drawPlayer();
    void drawFrame();
    void showBox();
    void hideBox();
    void scroll(int longitude, int latitude);
    void playerPoint(std::uint16_t& longitude, int& latitude) const;
    std::vector<Command> commands() const;
    // Where map point (longitude, latitude) is on the screen.
    void toScreen(std::uint16_t longitude, int latitude, int& x, int& y) const;
    bool inView(int x, int y) const;

    Game& g_;
    World& world_;
    Panel& panel_;
    std::unique_ptr<FlatMap> map_;
    bool active_ = false;
    bool boxShown_ = false;
    std::uint32_t lastClick_ = 0;
    std::uint16_t longitude_ = 0;
    int latitude_ = 0;
};

} // namespace mentat
