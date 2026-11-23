// SPDX-License-Identifier: GPL-3.0-or-later
//
// Troops on the move: the chains of troops at the places, setting off,
// the steps across the map and arriving. See docs/rules/moves.md.
#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/world.hpp"

#include <cstdlib>

namespace mentat {

namespace {

// A troop's byte 3.
constexpr std::uint8_t kOccupation = 0x0F;
constexpr std::uint8_t kCannot = 0x10;
constexpr std::uint8_t kOwnFremen = 0x20;
constexpr std::uint8_t kMoving = 0x40;
constexpr std::uint8_t kNotPauls = 0x80;
constexpr int kProspecting = 1;
constexpr int kAttacking = 5;
constexpr int kFighting = 6;
// Its byte 0x10.
constexpr std::uint8_t kHarkonnen = 0x80;
constexpr std::uint8_t kStill = 0x10;

// A place's byte 0x0A.
constexpr std::uint8_t kHeld = 0x02;
constexpr std::uint8_t kGuarded = 0x08;
constexpr std::uint8_t kHidden = 0x80;
constexpr int kFirstFort = 0x28;

// Turns at a place: 30, the Harkonnens' from the ninth.
constexpr int kTurns = 30;
constexpr int kHarkonnenTurns = 8;
// Where a troop sent away goes.
constexpr std::uint16_t kNowhere = 0xFBC;
constexpr std::size_t kTroopsEnd = 0xFBB;

// Steps: a troop arrives within this many points both ways.
constexpr int kNear = 7;

std::int16_t s16(std::uint16_t v) { return static_cast<std::int16_t>(v); }

} // namespace

bool World::turnSeed() {
    const std::uint16_t w = u16(0);
    const bool carry = (w & 0x8000) != 0;
    setU16(0, static_cast<std::uint16_t>((w << 1) | (carry ? 1 : 0)));
    return carry;
}

void World::unlinkTroop(int troop) {
    const std::size_t t = troopRecord(troop);
    if (!(u8(t + 3) & kMoving)) {
        const std::size_t place = u16(t + 4);
        if (u8(place + 9) == troop) {
            setU8(place + 9, u8(t + 1));
        } else {
            for (int other = u8(place + 9); other != 0;) {
                const std::size_t o = troopRecord(other);
                if (u8(o + 1) == troop) {
                    setU8(o + 1, u8(t + 1));
                    break;
                }
                other = u8(o + 1);
            }
        }
    }
    setU8(t + 1, 0);
}

int World::linkTroop(int troop, std::size_t place) {
    const std::size_t t = troopRecord(troop);
    const bool harkonnen = (u8(t + 0x10) & kHarkonnen) != 0;
    if (u8(place + 9) == 0) {
        setU8(place + 9, static_cast<std::uint8_t>(troop));
        const int turn = harkonnen ? 1 + kHarkonnenTurns : 1;
        setU8(t + 2, static_cast<std::uint8_t>(turn));
        return turn;
    }
    // The turns taken; the troop goes at the end of the chain and takes
    // the first turn free.
    bool taken[kTurns] = {};
    int last = 0;
    for (int other = u8(place + 9); other != 0;) {
        const std::size_t o = troopRecord(other);
        const int turn = u8(o + 2);
        if (turn >= 1 && turn <= kTurns) {
            taken[turn - 1] = true;
        }
        last = other;
        other = u8(o + 1);
    }
    setU8(troopRecord(last) + 1, static_cast<std::uint8_t>(troop));
    const int from = harkonnen ? kHarkonnenTurns : 0;
    int turn = kTurns;
    for (int i = from; i < kTurns; ++i) {
        if (!taken[i]) {
            turn = i + 1;
            break;
        }
    }
    setU8(t + 2, static_cast<std::uint8_t>(turn));
    return turn;
}

void World::lowerSpirits(std::size_t t, int by) {
    int spirit = u8(t + 0x15) - by;
    if (spirit < 0) {
        spirit = 0;
    }
    if (spirit < 5) {
        spirit = 4;
        setU8(t + 3, static_cast<std::uint8_t>(u8(t + 3) | kCannot));
        setU16(t + 0x12, static_cast<std::uint16_t>(u16(t + 0x12) | 0x20));
    }
    setU8(t + 0x15, static_cast<std::uint8_t>(spirit));
}

// The equipment the troop holds (bits 7 to 1 of its byte 0x19) counted
// at the place, or no longer.
void World::heldEquipment(std::size_t t, std::size_t place, int change) {
    const int held = u8(t + 0x19);
    for (std::size_t i = 0; i < 7; ++i) {
        if (!(held & (0x80 >> i))) {
            continue;
        }
        const int count = u8(place + 0x14 + i);
        if (change > 0) {
            setU8(place + 0x14 + i, static_cast<std::uint8_t>(count + 1));
        } else if (count > 0) {
            setU8(place + 0x14 + i, static_cast<std::uint8_t>(count - 1));
        }
    }
}

void World::startMove(int troop, std::size_t place, const FlatMap& map) {
    const std::size_t t = troopRecord(troop);
    if (u8(t + 3) & kMoving) {
        // Already on the way: only the destination changes.
        setU16(t + 4, static_cast<std::uint16_t>(place));
        if ((u8(t + 3) & 3) == 3) {
            setU8(t + 3, static_cast<std::uint8_t>(u8(t + 3) & ~3));
        }
        if (markChanged) {
            markChanged(troop);
        }
        return;
    }
    const std::size_t from = u16(t + 4);
    // Those of Paul's troops there who could not work for being too many
    // try again.
    for (int other = u8(from + 9); other != 0;) {
        const std::size_t o = troopRecord(other);
        const int next = u8(o + 1);
        if (u8(o + 3) < kNotPauls && (u8(o + 0x12) & 0x10)) {
            resetOccupation(other, u8(o + 3) & kOccupation);
        }
        other = next;
    }
    unlinkTroop(troop);
    if (u8(t + 3) == kFighting && u8(place + 8) < kFirstFort) {
        // A troop leaving a fight for a sietch loses heart; for one held
        // (byte 0x0A bit 1) it goes nowhere, out of any place's chain.
        if (u8(place + 0x0A) & kHeld) {
            return;
        }
        lowerSpirits(t, 3);
    }
    setU16(t + 4, static_cast<std::uint16_t>(place));
    setU8(t + 3, static_cast<std::uint8_t>(u8(t + 3) | kMoving));
    setU8(t + 2, 0);
    heldEquipment(t, from, -1);
    if (markChanged) {
        markChanged(troop);
    }
    if (!(u8(t + 0x10) & kStill) && !stepTroop(troop, 7, map) && markMoved) {
        markMoved(troop);
    }
}

bool World::stepTroop(int troop, int steps, const FlatMap& map) {
    const std::size_t t = troopRecord(troop);
    for (int k = 0; k < steps; ++k) {
        const std::size_t place = u16(t + 4);
        const int lat = s16(u16(t + 8));
        const int toLat = s16(u16(place + 4)) - lat;
        const int sign = toLat < 0 ? -1 : 1;
        const int acrossLat = std::abs(toLat);
        const int point = map.pointSize(std::min(std::abs(lat), FlatMap::kRows - 1));
        const int toLon = s16(static_cast<std::uint16_t>(u16(place + 2) - u16(t + 6)));
        const int acrossLon = std::abs(toLon) / point;
        int dx = 0;
        int dy = 0;
        if (acrossLon >= acrossLat) {
            if (acrossLon < kNear) {
                arrive(troop);
                return true;
            }
            dy = acrossLat == 0 ? 0 : sign;
            dx = toLon < 0 ? -point : point;
            if (!turnSeed()) {
                dy = 0;
            }
        } else {
            if (acrossLat < kNear) {
                arrive(troop);
                return true;
            }
            dy = sign;
            if (acrossLon == 0) {
                dx = toLon;
            } else {
                dx = toLon < 0 ? -point : point;
                if (!turnSeed()) {
                    dx = 0;
                }
            }
        }
        setU16(t + 6, static_cast<std::uint16_t>(u16(t + 6) + dx));
        setU16(t + 8, static_cast<std::uint16_t>(u16(t + 8) + dy));
    }
    return false;
}

void World::moveTroops(int except, const FlatMap& map) {
    for (std::size_t t = kTroops; t < kTroopsEnd; t += kTroopSize) {
        const int job = u8(t + 3);
        if (!(job & kMoving) || u8(t) == except) {
            continue;
        }
        if (!(u16(t + 0x12) & 0x430) && (job & (kNotPauls | kOwnFremen))) {
            continue;
        }
        const int troop = static_cast<int>((t - kTroops) / kTroopSize) + 1;
        if (!stepTroop(troop, (u8(t + 0x19) & 0x40) ? 8 : 4, map) && markMoved) {
            markMoved(troop);
        }
    }
}

// The troop reaches its destination.
void World::arrive(int troop) {
    const std::size_t t = troopRecord(troop);
    int occupation = u8(t + 3) & kOccupation;
    const std::size_t place = u16(t + 4);
    setU16(t + 6, u16(place + 2));
    setU16(t + 8, u16(place + 4));
    const std::uint8_t flags = u8(place + 0x0A);
    if (!(flags & kHeld) && u8(place + 8) >= kFirstFort && (flags & kGuarded)) {
        // A guarded fort: no attacking there.
        if (occupation == kAttacking || occupation == kFighting) {
            const int keep = u8(t + 3) & 0xFC;
            if ((u8(t + 3) & kOccupation) != keep) {
                resetOccupation(troop, keep);
            }
        }
        settle(troop);
        return;
    }
    settle(troop);
    if (occupation == kAttacking && (u8(place + 0x0A) & kHidden)) {
        setU8(place + 0x0A, static_cast<std::uint8_t>(u8(place + 0x0A) & ~kHidden));
        return;
    }
    if (occupation != kAttacking && u8(place + 9) == troop) {
        return; // alone there: taking the place is not built yet
    }
    // With others there, Paul's troops get ready to fight.
    for (int other = u8(place + 9); other != 0;) {
        const std::size_t o = troopRecord(other);
        const int job = u8(o + 3);
        if (job < kNotPauls && !(job & kOwnFremen)) {
            const int what = job & kOccupation;
            if (what == kProspecting) {
                setU8(o + 3, static_cast<std::uint8_t>(job | kCannot));
            } else if (what != kFighting) {
                resetOccupation(other, kFighting);
            }
        }
        other = u8(o + 1);
    }
}

// The troop joins the place's chain and takes up its work there.
void World::settle(int troop) {
    const std::size_t t = troopRecord(troop);
    const std::size_t place = u16(t + 4);
    linkTroop(troop, place);
    setU8(t + 3, static_cast<std::uint8_t>(u8(t + 3) & ~kMoving));
    if (!(u8(t + 0x10) & kHarkonnen) && u8(t + 2) > kHarkonnenTurns) {
        // Too many: the first of the Fremen there not yet Paul's who are
        // not Harkonnens is sent away.
        unlinkTroop(troop);
        for (int other = u8(place + 9); other != 0;) {
            const std::size_t o = troopRecord(other);
            const int next = u8(o + 1);
            if (!(u8(o + 0x10) & kHarkonnen) && (u8(o + 3) & kOwnFremen)) {
                unlinkTroop(other);
                setU16(o + 4, kNowhere);
                setU8(o + 3, 0xA0);
                setU8(o + 0x1A, 0);
                break;
            }
            other = next;
        }
        linkTroop(troop, place);
    }
    heldEquipment(t, place, 1);
    if (u8(t + 3) < kNotPauls) {
        resetOccupation(troop, u8(t + 3) & kOccupation);
    }
}

} // namespace mentat
