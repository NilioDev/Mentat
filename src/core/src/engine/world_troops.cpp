// SPDX-License-Identifier: GPL-3.0-or-later
//
// What Paul's troops do: their occupation and whether they can do it where
// they are. See docs/rules/orders.md.
#include "mentat/engine/world.hpp"

namespace mentat {

namespace {

constexpr std::size_t kClockWord = 0x02;

// A troop's byte 3: the occupation in the low four bits (the speciality
// in bits 2 and 3), and a bit set while it cannot do it where it is.
constexpr std::uint8_t kOccupation = 0x0F;
constexpr std::uint8_t kCannot = 0x10;
constexpr std::uint8_t kWaiting = 2;
constexpr int kMining = 0;
constexpr int kProspecting = 1;
constexpr int kEcology = 8;
constexpr int kEcologyHere = 0x0A;
// The one place where ecology becomes another occupation while nothing
// grows there yet.
constexpr std::size_t kEcologyPlace = 0x7C8;

} // namespace

int World::troopFace(int troop, int& variant) const {
    // One of three Fremen, and one of their sets.
    constexpr int kFirstFremenFace = 0x0E;
    const int name = u8(troopRecord(troop));
    int sets = name / 3;
    const int limit = name % 3 == 0 ? 0x0F : 0x11;
    while (sets >= limit) {
        sets -= limit;
    }
    variant = sets + 1;
    return kFirstFremenFace + name % 3;
}

bool World::cannotWork(int troop) {
    const std::size_t t = troopRecord(troop);
    const std::size_t place = u16(t + 4);
    const int occupation = u8(t + 3) & kOccupation;
    bool cannot = false;
    switch (occupation) {
    case kMining:
        cannot = (u16(t + 0x10) & 0x200) || (u16(t + 0x12) & 0x30) || u8(place + 0x12) < 1 ||
                 ((u8(place + 0x0A) ^ 0x40) & 0x41) != 0;
        break;
    case kProspecting: cannot = (u8(place + 0x0A) & 0x41) != 0; break;
    case kEcology:
        cannot = (u16(t + 0x12) & 0x30) || u8(place + 0x1B) < 1 || !(u8(place + 0x0A) & 0x20) || !(u8(t + 0x19) & 2);
        break;
    default: return false;
    }
    // The troop's byte 3 keeps the answer; a troop that can start again
    // starts its work's times again.
    const bool was = (u8(t + 3) & kCannot) != 0;
    if (was != cannot) {
        setU8(t + 3, static_cast<std::uint8_t>(u8(t + 3) ^ kCannot));
        if (markChanged) {
            markChanged(troop);
        }
        if (was) {
            restartWork(troop);
        }
    }
    return cannot;
}

void World::restartWork(int troop) {
    const std::size_t t = troopRecord(troop);
    setU16(t + 0x0A, u16(kClockWord));
    setU16(t + 0x0C, 0);
    setU16(t + 0x0E, 0);
}

void World::setOccupation(int troop, int occupation) {
    if ((u8(troopRecord(troop) + 3) & kOccupation) == occupation) {
        return;
    }
    resetOccupation(troop, occupation);
}

void World::resetOccupation(int troop, int occupation) {
    const std::size_t t = troopRecord(troop);
    const std::size_t place = u16(t + 4);
    if (occupation == kEcology && place == kEcologyPlace && u8(place + 0x1A) == 0) {
        occupation = kEcologyHere;
    }
    setU8(t + 3, static_cast<std::uint8_t>(occupation));
    setU8(t + 0x12, static_cast<std::uint8_t>(u8(t + 0x12) & ~0x30));
    setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) & ~0x100));
    if (!cannotWork(troop)) {
        setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) | 0x100));
    }
    if (markChanged) {
        markChanged(troop);
    }
    restartWork(troop);
    if (u8(t + 3) != kWaiting) {
        // The troop has worked at this speciality.
        const int speciality = (u8(t + 3) & kOccupation) >> 2;
        setU16(t + 0x12, static_cast<std::uint16_t>(u16(t + 0x12) | ((0x2000u << speciality) & 0xFFFF)));
    }
}

void World::namePlace(std::size_t record) {
    constexpr int kKindWords = 0x0C;
    setU16(0x11ED, u8(record));
    setU16(0x11EF, static_cast<std::uint16_t>(u8(record + 1) + kKindWords));
}

} // namespace mentat
