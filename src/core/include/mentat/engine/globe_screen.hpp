// SPDX-License-Identifier: GPL-3.0-or-later
//
// The globe, opened from the map: the planet turning between the frescoes
// of the two houses, with Paul's place marked, arrows that turn and tilt
// it, and the way back to the map. See docs/rules/globe_screen.md.
#pragma once

#include "mentat/engine/globe.hpp"
#include "mentat/engine/panel.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace mentat {

class Game;
class MapScreen;
class World;

class GlobeScreen {
public:
    // The original's numbers for what its zones and commands do.
    static constexpr std::uint16_t kOpen = 0xB8C6; // the globe in the map's panel
    static constexpr std::uint16_t kExit = 0xBC81; // "EXIT GLOBE" and the button beside the list
    static constexpr std::uint16_t kUp = 0xB9B9;
    static constexpr std::uint16_t kRight = 0xB9CC;
    static constexpr std::uint16_t kDown = 0xB9C0;
    static constexpr std::uint16_t kLeft = 0xB9D3;
    static constexpr std::uint16_t kTurnToPlayer = 0xBA9E; // the middle of the arrows

    GlobeScreen(Game& game, World& world, Panel& panel, MapScreen& map);

    bool active() const { return active_; }

    // Draws the globe screen on the back buffer, the planet facing map
    // point (longitude, latitude).
    void enter(std::uint16_t longitude, int latitude);
    // Shows it (the map rises away) and starts the planet turning.
    void show();
    void leave();

    // A command, a zone of the panel; false when it is not one of the
    // globe's. Sets `leaving` when the map should open again.
    bool command(std::uint16_t action);
    // A click in the scene: on the planet, the map opens there.
    void click(int x, int y);
    // The mouse moved: over the planet the list offers the map of the area.
    // True when the list changed.
    bool pointAt(int x, int y);

    // After a command or click that leaves: where the map's middle goes.
    bool leaving = false;
    std::uint16_t mapLongitude = 0;
    int mapLatitude = 0;

private:
    Globe& globe();
    void drawScreen();
    void drawPlanet();
    void showPlanet();
    void turnToPlayer();
    void zoomFrom(int x, int y);
    void mosaic(int left, int top, int right, int bottom, int offset);
    void setCommands(bool overPlanet);
    void playerPoint(std::uint16_t& longitude, int& latitude) const;

    Game& g_;
    World& world_;
    Panel& panel_;
    MapScreen& map_;
    std::optional<Globe> globe_;
    bool active_ = false;
    bool overPlanet_ = false;
};

} // namespace mentat
