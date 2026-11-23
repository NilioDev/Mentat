// SPDX-License-Identifier: GPL-3.0-or-later
//
// The places of Arrakis and the rooms inside them: which scene a room
// shows and who stands in it. See docs/rules/places.md.
#pragma once

#include "mentat/gfx/scene.hpp"

#include <array>
#include <functional>
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
    // More chiefs of Paul's troops standing here besides the first; they
    // count as characters 16 on.
    int moreChiefs = 0;
    // When not empty, who stands at each spot (-1 nobody) instead.
    std::vector<int> standingList;
    // The person's picture each character is shown with (sprites 2n and
    // 2n + 1 of the persons sheet), and how far further down; character
    // n's own when not set.
    std::function<int(int character, int& down)> personOf;

    // Draws the room on the display's target, with the characters in it.
    // `background`, when given, is drawn after the clearing, before the
    // scene.
    void drawView(const std::function<void()>& background = {});

    // The sky seen through the windows or over a cockpit: four rows of
    // tiles from the first four pictures of the sky file.
    void drawSky();

    // What a place's first room shows behind its scene: the place's
    // surroundings, by the kind of place (byte 8 of its record) and the
    // first word of its name.
    void drawOutside(int kind, int firstName);
    // The picture of that view (a resource) from `step` 256ths of a
    // latitude south of the place, Paul walking away from it; -1 once the
    // place is out of sight.
    int outsidePicture(int kind, int firstName, int step) const;
    // The desert where Paul walks: picture `picture`, nobody in it.
    void drawDesert(int picture);
    // The ornithopters kept at a place, in front of its first room.
    void drawOrnithopters(int count, bool sietch);
    // The first of them taking off (`side` 1) or landing (`side` -1),
    // `state` steps from standing (0) to gone (32), the others parked.
    void drawTakeOff(int count, bool sietch, int state, int side);

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
    // Reads a byte of the state when it lies there (the rooms of some
    // kinds do); nothing otherwise.
    std::function<std::optional<std::uint8_t>(std::size_t)> stateByte;

    int room() const { return room_; }
    int place() const { return place_; }

    static constexpr int kPersons = 0x26; // the characters' small pictures

private:
    void loadScenes();
    std::uint8_t roomData(std::size_t at) const;
    std::size_t roomRecord() const;
    void drawPerson(int character, const PersonSlot& slot);
    void drawOrnithopter(int x, int y, int state);
    void drawRow(int first, int count, bool sietch);

    Game& game_;
    std::vector<PlaceRecord> records_;
    int room_ = 0;
    int place_ = 0;
    int scenesKind_ = -1;
    std::optional<SceneFile> scenes_;
    std::vector<Shown> shown_;
};

} // namespace mentat
