// SPDX-License-Identifier: GPL-3.0-or-later
//
// The ornithopter's cockpit, where Paul chooses where to fly: the sky
// above the instruments, a small map around him with the places he knows,
// the title typed out letter by letter, the name of what the mouse points
// at, and "Cancel" back to the room. See docs/rules/cockpit.md.
#pragma once

#include "mentat/engine/panel.hpp"
#include "mentat/engine/small_map.hpp"
#include "mentat/gfx/font.hpp"
#include "mentat/gfx/scene.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mentat {

class Game;
class MapScreen;
class World;

class Cockpit {
public:
    // The original's numbers for what its commands do.
    static constexpr std::uint16_t kOpen = 0x42E9; // "TAKE AN ORNITHOPTER" in a palace's entrance
    static constexpr std::uint16_t kCancel = 0xD2E2;

    Cockpit(Game& game, World& world, Panel& panel, MapScreen& map);

    bool active() const { return active_; }
    // Where the map shows on the screen.
    ClipRect view() const;

    // Sets up the panel for the cockpit, and its colours.
    void enter();
    // Draws the cockpit on the back buffer, the map around Paul; the
    // caller has set the sky's colours in between.
    void drawScene();
    // Shows it at once, with Paul's mark blinking, the title being typed
    // and the cockpit's voice asking where to.
    void show();
    void leave();
    // The warning when the ornithopter enters the Harkonnens' zone with
    // nobody on board: the cockpit without its map, the warning written
    // on the screen, on the back buffer.
    void drawWarning();

    // A command of the cockpit's list; false when it is not one of them.
    // Sets `leaving` when the room should come back.
    bool command(std::uint16_t action);
    // The mouse is at (x, y): the line above the map names what it points
    // at.
    void pointAt(int x, int y);
    // Where a click on the map sends the ornithopter: a place, or the
    // desert's point clicked.
    struct Destination {
        bool chosen = false;
        std::size_t record = 0; // 0 for the desert
        std::uint16_t longitude = 0;
        int latitude = 0;
    };
    // A click at (x, y); nothing chosen off the map or on the place Paul is
    // in.
    Destination destinationAt(int x, int y);
    // The name blinks while the cockpit's voice says it.
    void confirm();

    bool leaving = false;
    // Calling a worm: the map over the view where Paul is, under a bar
    // for its line, instead of the cockpit.
    bool worm = false;

private:
    // What the mouse points at.
    struct Target {
        enum class Kind { Nothing, Place, Desert } kind = Kind::Nothing;
        std::size_t place = 0; // the place's record, for a place
        int direction = -1;    // 0 north, then clockwise in eighths; -1 for none
        bool operator==(const Target&) const = default;
    };

    void placeMark();
    void blink();
    void startTyping();
    void stopTyping();
    void typeNext();
    void writeTarget();
    void say(int phrase);
    int drawText(int x, const std::string& text, std::uint8_t colour);
    void clearLineFrom(int x);
    Target targetAt(int x, int y) const;

    Game& g_;
    World& world_;
    Panel& panel_;
    SmallMap small_;
    bool active_ = false;
    // Paul's mark: where it is drawn, and whether it is on the map.
    bool markShown_ = false;
    ClipRect mark_{};
    int blinks_ = 0;
    // The title, while it is being typed.
    bool typing_ = false;
    std::size_t typed_ = 0;
    int typeX_ = 0;
    Target target_{};
    std::optional<Font> font_;
};

} // namespace mentat
