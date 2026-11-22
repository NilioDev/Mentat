// SPDX-License-Identifier: GPL-3.0-or-later
//
// The places of Arrakis and the rooms inside them: which scene a room
// shows and who stands in it. See docs/rules/places.md.
#pragma once

#include "mentat/gfx/scene.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace mentat {

class Game;
class ProgramFile;

// A place as the program's table keeps it (0x1C bytes each).
struct PlaceRecord {
    std::array<std::uint8_t, 0x1C> bytes{};

    // What kind of place it is, which also picks its rooms: below 0x20 a
    // sietch, 0x20 the palace, below 0x28 a village, below 0x30 the
    // Harkonnen fortress.
    int kind() const { return bytes[8]; }
};

class Places {
public:
    explicit Places(Game& game);

    const std::vector<PlaceRecord>& records() const { return records_; }

    // Goes to room `room` (low byte: the room, from 1; high byte: the kind
    // of rooms) of place `place` (high byte: its number in the table, from
    // 1; low byte 0x80 for "inside").
    void enter(int room, int place);

    // Characters standing in the room: bit n for character n.
    std::uint16_t present = 0;
    // Characters to leave out again (the original keeps a second set).
    std::uint16_t absent = 0;
    // Turns which free spot each character takes.
    int spotTurn = 0;

    // Draws the room on the display's target, with the characters in it.
    void drawView();

    // Where each character was drawn by the last drawView(), for the mouse.
    struct Shown {
        int character = 0;
        ClipRect rect;
    };
    const std::vector<Shown>& shown() const { return shown_; }
    // The character under (x, y), or -1; the one drawn last wins.
    int characterAt(int x, int y) const;

    // The room's scene number byte, as the room table has it.
    int roomByte() const;
    // The room's four ways out (up, right, down, left): another room of the
    // same kind from 1, 0 for none; see docs/rules/places.md.
    std::array<std::uint8_t, 4> exits() const;

    int room() const { return room_; }
    int place() const { return place_; }

    static constexpr int kPersons = 0x26; // the characters' small pictures

private:
    void loadScenes();
    std::size_t roomRecord() const;
    void drawPerson(int character, const PersonSlot& slot);

    Game& game_;
    std::vector<PlaceRecord> records_;
    int room_ = 0;
    int place_ = 0;
    int scenesKind_ = -1;
    std::optional<SceneFile> scenes_;
    std::vector<Shown> shown_;
};

} // namespace mentat
