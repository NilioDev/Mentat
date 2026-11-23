// SPDX-License-Identifier: GPL-3.0-or-later
//
// The story's steps: what each one changes in the state when the story
// reaches it. See docs/rules/story_steps.md.
#include "mentat/engine/world.hpp"

#include <array>
#include <initializer_list>

namespace mentat {

namespace {

constexpr std::size_t kProgress = 0x2A;
constexpr std::size_t kProgressDone = 0xFF;
constexpr std::size_t kFlags = 0x0A;
constexpr int kLastStep = 0x6C;

// The characters' records (0x10 bytes each): word 0 the room, word 2 the
// place.
constexpr std::size_t who(int n) { return 0xFD8 + 0x10 * static_cast<std::size_t>(n); }
constexpr std::uint16_t kInPalace = 0x0180;
// A place's number, from 1, as words name it in their high byte.
constexpr unsigned placeNumber(std::size_t record) { return static_cast<unsigned>((record - 0x100) / 0x1C + 1); }

// The palace's rooms are kept in the state from 0x1225, five bytes each:
// the room's picture, then the ways out up, right, down and left. A way
// with bit 7 is shut.
constexpr std::size_t kPalaceRooms = 0x1225;
constexpr std::size_t way(int room, int direction) {
    return kPalaceRooms + 5 * static_cast<std::size_t>(room - 1) + 1 + static_cast<std::size_t>(direction);
}
// The palace's plan: its sprite list, where the overlays of the shut
// parts begin.
constexpr std::size_t kPlanOverlay3 = 0x1211;
constexpr std::size_t kPlanOverlay4 = 0x1217;
constexpr std::size_t kPlanOverlay5 = 0x121D;

// Scenes (scripts in the program's code).
constexpr std::uint16_t kSceneCouncil = 0x1321;
constexpr std::uint16_t kSceneStep48 = 0x1313;
constexpr std::uint16_t kSceneStep58 = 0x12FB;

} // namespace

bool World::raiseStory(int step) {
    if (step <= u8(kProgress)) {
        return false;
    }
    setU8(kProgress, static_cast<std::uint8_t>(step));
    setU8(kProgressDone, 0);
    if (storyEvents) {
        storyEvents();
    }
    // The program keeps one handler for every fourth step.
    const int now = u8(kProgress);
    if (now <= kLastStep && (now & 3) == 0) {
        storyStep(now);
    }
    return true;
}

void World::storyStep(int step) {
    const auto find = [this](std::initializer_list<std::size_t> places) {
        for (const std::size_t p : places) {
            findPlace(p);
        }
    };
    const auto open = [this](int room, int direction) {
        const std::size_t at = way(room, direction);
        setU8(at, static_cast<std::uint8_t>(u8(at) & 0x7F));
        roomsChanged = true;
    };
    const auto setRoom = [this](int n, int room) { setU8(who(n), static_cast<std::uint8_t>(room)); };
    const auto orBits = [this](std::size_t at, std::uint8_t bits) { setU8(at, static_cast<std::uint8_t>(u8(at) | bits)); };

    switch (step) {
    case 0x04:
        setU8(kPalaceRooms + 5, static_cast<std::uint8_t>(u8(kPalaceRooms + 5) - 1)); // room 2's picture
        find({0x218, 0x2DC});
        setRoom(1, 9);
        break;
    case 0x08:
        open(2, 3);
        break;
    case 0x0C:
        open(8, 1);
        open(7, 3);
        setU16(kPlanOverlay5, 0xFFFF);
        scene = kSceneCouncil;
        break;
    case 0x10:
        setRoom(0, 5);
        setRoom(1, 9);
        sendNews(0x010B);
        break;
    case 0x14:
        setRoom(0, 0x0A);
        find({0x34C, 0x368, 0x384});
        break;
    case 0x1C:
        open(7, 1);
        setU16(kPlanOverlay4, 0xFFFF);
        break;
    case 0x20:
        setU8(who(2) + 3, 1);
        break;
    case 0x28:
        find({0x800});
        break;
    case 0x2C:
        setU16(0x1154, static_cast<std::uint16_t>(clock()));
        setU16(who(4), 0x2006);
        setU16(who(2), 0x2008);
        setU16(who(2) + 2, kInPalace);
        setRoom(1, 0x0A);
        setU16(who(1) + 2, kInPalace);
        setU16(0x1201, 0x0109);
        raiseCharisma(0x14);
        orBits(kFlags, 0x10);
        find({0x5EC, 0x5D0, 0x608, 0x640, 0x65C});
        break;
    case 0x30:
        tell(0x0004, 0);
        sendNews(0x1409);
        break;
    case 0x34:
        if (u8(who(1)) != 8) {
            setRoom(1, 0x0A);
            setU16(who(1) + 2, kInPalace);
        }
        sendNews(0x280A);
        break;
    case 0x38:
        setU8(who(0) + 3, 0xFF);
        break;
    case 0x40:
        orBits(who(8) + 0x0F, 2);
        break;
    case 0x44:
        find({0x3D8});
        break;
    case 0x48:
        raiseCharisma(0x0A);
        scene = kSceneStep48;
        setU8(who(7) + 0x0F, static_cast<std::uint8_t>((u8(who(7) + 0x0F) | 0x10) & ~2));
        setU8(0x1178, static_cast<std::uint8_t>(u8(0x28) + 2));
        find({0x3F4, 0x410, 0x3BC, 0x88C});
        break;
    case 0x4C:
        setU8(0x1141, static_cast<std::uint8_t>(u8(0x1141) + 1));
        setRoom(1, 2);
        setU16(who(1) + 2, kInPalace);
        tell(0x0105, 0);
        break;
    case 0x50:
        orBits(kFlags, 0x40);
        raiseCharisma(0x28);
        setRoom(1, 9);
        break;
    case 0x54:
        open(11, 1);
        setU16(kPlanOverlay3, 0xFFFF);
        break;
    case 0x58:
        orBits(kFlags, 0x20);
        scene = kSceneStep58;
        find({0x7E4, 0x790, 0x7AC, 0x854, 0x81C});
        break;
    case 0x5C:
        setU8(who(6), 5);
        setU8(0x11D0, static_cast<std::uint8_t>(u8(0x11D0) + 0x0C));
        setU16(0x1156, static_cast<std::uint16_t>((clock() >> 4) + 3));
        setU8(0x1141, static_cast<std::uint8_t>(u8(0x1141) + 1));
        break;
    case 0x60: {
        // The Harkonnens' palace becomes the goal.
        constexpr std::size_t kPalace = 0x11C;
        setU16(who(7), static_cast<std::uint16_t>((u8(kPalace + 8) << 8) | 2));
        setU16(who(7) + 2, static_cast<std::uint16_t>(((placeNumber(kPalace)) << 8) | 0x80));
        break;
    }
    case 0x64:
        strongestFort();
        break;
    default:
        break;
    }
}

// Of the places from the third on, the Harkonnen fort furthest south
// (latitude above −100) where none of Paul's troops fight: the goal of the
// last battle, and news of it. With none, the palace.
void World::strongestFort() {
    constexpr std::size_t kFirst = 0x138;
    int best = -100;
    std::size_t chosen = 0x100;
    for (std::size_t at = kFirst;; at += kPlaceSize) {
        const int latitude = static_cast<std::int16_t>(u16(at + 4));
        const bool harkonnens = u8(at + 8) >= 0x28 && (u8(at + 0x0A) & 8) == 0;
        if (latitude > best && harkonnens) {
            int theirs = 0;
            int pauls = 0;
            countFighters(at, theirs, pauls);
            if (pauls == 0) {
                best = latitude;
                chosen = at;
            }
        }
        if (u8(at + kPlaceSize) >= 8) {
            break;
        }
    }
    setU16(who(7), static_cast<std::uint16_t>((u8(chosen + 8) << 8) | 3));
    setU16(who(7) + 2, static_cast<std::uint16_t>((placeNumber(chosen) << 8) | 0x80));
    setU16(0xF2, static_cast<std::uint16_t>((u8(chosen) << 8) | u8(chosen + 1)));
    sendNews(0x2B0A);
}

void World::quietAtStep14() {
    setU8(kProgressDone, 0);
    setU8(kProgress, 0x15);
    setU8(who(0) + 3, 1);
    setU16(who(4), 0x200B);
    setU16(who(4) + 2, kInPalace);
    setU8(who(1), 0x0A);
    setU8(0xD5, 0xFF);
    // The Emperor's first demand, from today.
    setU16(0x118D, static_cast<std::uint16_t>(clock() >> 4));
    newDemand();
    setU8(kFlags, static_cast<std::uint8_t>(u8(kFlags) | 1));
    tell(0x0001, 0);
    if (storyEvents) {
        storyEvents();
    }
}

} // namespace mentat
