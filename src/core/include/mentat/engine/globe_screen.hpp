// SPDX-License-Identifier: GPL-3.0-or-later
//
// The globe, opened from the map: the planet turning between the frescoes
// of the two houses, with Paul's place marked, arrows that turn and tilt
// it, and the way back to the map. See docs/rules/globe_screen.md.
#pragma once

#include "mentat/engine/globe.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/gfx/scene.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <optional>
#include <utility>
#include <vector>

namespace mentat {

class Game;
class MapScreen;
class SavedGames;
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
    static constexpr std::uint16_t kSeeResults = 0xB96B;
    static constexpr std::uint16_t kStandardVision = 0xB961;
    static constexpr std::uint16_t kLoad = 0xB29E;
    static constexpr std::uint16_t kSave = 0xB28C;
    static constexpr std::uint16_t kSaveSlot = 0xB35A;
    static constexpr std::uint16_t kLoadSlot = 0xB3B0;
    static constexpr std::uint16_t kCancel = 0xD2E2;

    // Without saved games, loading and saving stay greyed.
    GlobeScreen(Game& game, World& world, Panel& panel, MapScreen& map, SavedGames* saves = nullptr);

    bool active() const { return active_; }

    // The list of saved games to load, in the panel, with the globe shown
    // or not (when the game is over). loadSlot() loads the one a line of
    // it names; false when there is none.
    void showLoadList();
    bool loadSlot(std::uint16_t phrase);
    // The same for saving: the list of saved games to save over, and
    // saving in the one a line names; false when it was not saved (the
    // list stays).
    void showSaveList();
    bool saveSlot(std::uint16_t phrase);
    bool showingResults() const { return results_; }

    // Draws the globe screen on the back buffer, the planet facing map
    // point (longitude, latitude).
    void enter(std::uint16_t longitude, int latitude);
    // Shows it (the map rises away) and starts the planet turning.
    void show();
    void leave();

    // A command, a zone of the panel; false when it is not one of the
    // globe's. Sets `leaving` when the map should open again, and `loaded`
    // when a saved game was loaded. `phrase` is the line's, for the lists of
    // saved games.
    bool command(std::uint16_t action, std::uint16_t phrase = 0);
    // A click in the scene: on the planet, the map opens there.
    void click(int x, int y);
    // The mouse moved: over the planet the list offers the map of the area.
    // True when the list changed.
    bool pointAt(int x, int y);
    // What a gamepad's pointer can jump to on the planet: Paul's place and
    // each place known, where they show on it now.
    std::vector<ClipRect> targets() const;

    // After a command or click that leaves: where the map's middle goes.
    bool leaving = false;
    bool loaded = false;
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

    // Loading and saving.
    enum class List { Globe, Save, Load };
    void openList(List list);
    bool saveIn(int slot, int line);
    void loadFrom(int slot);
    void writeSlot(int slot, std::uint16_t clock);
    void drawList();

    // The results.
    void drawSides(int shift);
    void showResults();
    void standardVision();
    void countResults();
    void writeResults();
    void drawTexts(std::size_t list, bool large);
    void growBars();
    void slideStep();
    void loadTexts();
    void loadFonts();
    std::size_t textAt(std::uint16_t phrase) const;
    std::size_t afterNumber(std::size_t at) const;

    Game& g_;
    World& world_;
    Panel& panel_;
    MapScreen& map_;
    std::optional<Globe> globe_;
    bool active_ = false;
    bool overPlanet_ = false;
    bool results_ = false;
    SavedGames* saves_ = nullptr;
    List list_ = List::Globe;
    int sideShift_ = 0;
    // The bars: where they go, and how far they have grown.
    std::array<int, 6> barTarget_{};
    std::array<int, 6> barNow_{};
    // The results' phrases one after the other, with the numbers written
    // into them; they keep what was written last, as the original's do.
    std::string texts_;
    std::vector<std::size_t> textStart_;
    std::optional<std::pair<Font, Font>> fonts_; // large and small
};

} // namespace mentat
