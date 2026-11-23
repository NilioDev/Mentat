// SPDX-License-Identifier: GPL-3.0-or-later
//
// The map screen ("SEE DUNE MAP"): the flat map of Dune around the
// player, with the places Paul knows, a panel of its own and an arrow pad
// that moves the view. See docs/rules/map.md.
#pragma once

#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/gfx/scene.hpp"
#include "mentat/gfx/surface.hpp"

#include <cstdint>
#include <functional>
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
    // Opens it with its middle at map point (longitude, latitude).
    void enter(std::uint16_t longitude, int latitude);
    // Dissolves it onto the screen and writes the box that names the map
    // over it.
    void show();
    // Closes it; the caller draws the room again.
    void leave();

    // Runs every tick while the map is shown: the box goes 1000 ticks
    // after the last click, and the troops' marks move.
    void tick();
    void clicked();
    // A click on the map at (x, y): the label of the place nearest to it,
    // or the label shown goes.
    void click(int x, int y);
    // The place whose label is shown, 0 for none.
    std::size_t labelled() const { return labelled_; }
    // Where the label is on the screen.
    ClipRect label() const { return label_; }

    // A command of the map's list or an arrow. False when it is not one of
    // the map's.
    bool command(std::uint16_t action);
    // How far arrow `action` moves the view; false when it is not an arrow.
    bool arrowStep(std::uint16_t action, int& longitude, int& latitude) const;
    // Puts the view's middle at (longitude, latitude) and shows it.
    void centreOn(std::uint16_t longitude, int latitude);
    // Takes the box that names the map, or a place's label, off the screen.
    void hideBox();
    void hideLabel();
    // The head of the panel's ornament, which stands over the map's
    // bottom, drawn within `clip`.
    void drawHead(const ClipRect& clip);

    // The map's points as they are now (with the marks the game adds).
    std::vector<std::uint8_t>& mapBytes() { return map().bytes(); }
    // Where the map shows on the screen.
    ClipRect view() const;

    // The map itself, for the other views of it.
    FlatMap& flatMap() { return map(); }
    // The sprite that marks place `record` of the state, counting from
    // `first`: by its kind, and further on for a sietch further than Paul
    // can see.
    std::uint16_t placeSprite(std::size_t record, std::uint16_t first);

    // The places drawn on the map, with where, in the order of the state.
    struct Shown {
        std::size_t record;
        int x;
        int y;
    };
    const std::vector<Shown>& shownPlaces() const { return shown_; }
    // Where place `record` is drawn on the screen, if it is.
    bool placeOnScreen(std::size_t record, int& x, int& y) const;
    // --- The troops' marks (see docs/rules/orders.md)

    // A troop's mark on the map: its picture (frames of three bytes in
    // the program, a sprite and how far it moves, ending at a sprite 0)
    // and where it is now.
    struct Mark {
        int troop = 0;
        std::size_t frames = 0; // the first frame
        std::size_t next = 0;   // the frame shown at the next step
        int sprite = 0;
        ClipRect rect{};
        bool animated = false;
        bool chosen = false; // the ring around the troop given orders
    };
    const std::vector<Mark>& marks() const { return marks_; }
    // The troop given orders, ringed when the map is drawn; 0 for none.
    void choose(int troop) { chosen_ = troop; }
    // What stands over the map, which the marks stay under as they move:
    // windows drawn on both buffers, and covers on the screen only
    // (besides the map's own box and label).
    std::function<std::vector<ClipRect>()> windows;
    std::function<std::vector<ClipRect>()> covers;
    // A troop's mark, after its occupation or its way changed: the mark
    // it has goes, and comes back as it is now if it is in sight.
    void markChanged(int troop);
    // A troop on its way has moved: its mark follows it, comes into
    // sight or goes.
    void markMoved(int troop);
    // A place changed in a way the map shows: once nothing stands over
    // the map, the view is drawn again.
    void placeChanged(std::size_t record);
    // The map is drawn again at the next tick when it shows.
    void redraw();

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
    void scroll(int longitude, int latitude);
    void showLabel(std::size_t record, int x, int y);
    int labelSize(std::size_t record) const;
    bool knownInside(std::size_t record) const;
    int battle(std::size_t record) const;
    void playerPoint(std::uint16_t& longitude, int& latitude) const;
    std::vector<Command> commands() const;
    // Where map point (longitude, latitude) is on the screen.
    void toScreen(std::uint16_t longitude, int latitude, int& x, int& y) const;
    bool inView(int x, int y) const;
    ClipRect boxRect() const;
    std::size_t markFrames(int troop) const;
    std::size_t movingFrames(int troop) const;
    bool markPoint(int troop, int& x, int& y) const;
    bool movingPoint(int troop, int& x, int& y) const;
    ClipRect markRect(int sprite, int x, int y) const;
    void addMark(int troop, std::size_t frames, int x, int y, bool chosen);
    void drawMarks();
    void stepMarks();
    void stepMark(Mark& m);
    void redrawMarks(const ClipRect& region);
    int findMark(int troop) const;

    Game& g_;
    World& world_;
    Panel& panel_;
    std::unique_ptr<FlatMap> map_;
    bool active_ = false;
    bool boxShown_ = false;
    std::vector<Shown> shown_;
    std::size_t labelled_ = 0;
    ClipRect label_{};
    std::uint32_t lastClick_ = 0;
    std::uint16_t longitude_ = 0;
    int latitude_ = 0;
    int chosen_ = 0;
    bool stale_ = false;
    std::vector<Mark> marks_;
    Surface base_{320, 200}; // the map without the marks
    std::uint32_t lastStep_ = 0;
    int steps_ = 0;
};

} // namespace mentat
