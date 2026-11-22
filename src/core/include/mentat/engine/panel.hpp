// SPDX-License-Identifier: GPL-3.0-or-later
//
// The control panel under the scene: its pictures, the clock, the list of
// commands and the arrows, and the zones of the screen the mouse can point
// at. See docs/rules/panel.md.
#pragma once

#include "mentat/data/string_table.hpp"
#include "mentat/gfx/font.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace mentat {

class Game;
class World;

// A part of the screen: what it shows and what pointing at it does.
struct Zone {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    std::uint16_t flags = 0;
    std::uint16_t sprite = kNoSprite; // of the panel's picture file
    std::uint16_t action = 0;         // the original's number for what a click does

    static constexpr std::uint16_t kNoSprite = 0xFFFF;
    static constexpr std::uint16_t kActive = 0x80; // the mouse can use it
    static constexpr std::uint16_t kFill = 0x40;   // blacked out before drawing
    static constexpr std::uint16_t kHidden = 0x20; // not drawn

    bool contains(int x, int y) const { return x >= left && x < right && y >= top && y < bottom; }
};

// One line of the list of commands: a phrase of the command file and what
// choosing it does.
struct Command {
    std::uint16_t phrase = 0; // from 1; bit 14 greys it out, bit 15 lights it up
    std::uint16_t action = 0;

    static constexpr std::uint16_t kGrey = 0x4000;
    static constexpr std::uint16_t kLit = 0x8000;
    static constexpr std::uint16_t kPhrase = 0x3FFF;
};

class Panel {
public:
    // The zones, as the program numbers them.
    static constexpr int kCommandLines = 7; // five, from zone 7
    static constexpr int kLines = 5;
    static constexpr int kArrows = 12;      // six, from zone 12
    static constexpr int kSlots = 21;       // two, from zone 21

    Panel(Game& game, int language);

    std::vector<Zone>& zones() { return zones_; }
    const std::vector<Zone>& zones() const { return zones_; }

    // Draws `count` zones from `first` on the display's target.
    void drawZones(int first, int count);
    // Clears the target and draws every zone and the ornament.
    void drawAll();
    // Gives zones 0 to 3 the pictures of a layout and draws zones 0 to 2.
    void setLayout(std::size_t table);
    // The sun or moon and the day, in the left part.
    void drawClock(const World& world);
    // The two slots for what the player carries.
    void drawSlots(const World& world);
    // The ornament at the top of the panel, open `ornament` steps (0..10).
    void drawOrnament();
    int ornament = 0;

    // The arrows for the ways out of a room: `exits` holds the room's four
    // exits (up, right, down, left).
    void setArrows(const World& world, const std::array<std::uint8_t, 4>& exits);

    // The list of commands, five lines from `scroll`.
    void setCommands(std::vector<Command> commands);
    const std::vector<Command>& commands() const { return commands_; }
    void drawCommands();

    // The language's command file: menu words, names and short texts.
    const StringTable& phrases();

private:
    void drawCommandLine(int line, std::uint16_t phrase);

    Game& game_;
    int language_;
    std::vector<Zone> zones_;
    std::vector<Command> commands_;
    Font font_;
    std::optional<StringTable> phrases_;
};

} // namespace mentat
