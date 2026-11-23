// SPDX-License-Identifier: GPL-3.0-or-later
//
// Places becoming Paul's: a troop of his reaching a place where nobody
// else is takes it. See docs/rules/places_taken.md.
#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/world.hpp"

#include <algorithm>

namespace mentat {

namespace {

constexpr std::size_t kClockWord = 0x02;
constexpr std::size_t kCharisma = 0x29;
constexpr std::size_t kRegionChange = 0x115A;
constexpr std::size_t kAllTaken = 0xC2;
constexpr std::size_t kHarkonnenSpice = 0xA8;
// Two words the story keeps; both lose bit 1 once the Harkonnens are down
// to one fort.
constexpr std::size_t kStoryWordA = 0xFF7;
constexpr std::size_t kStoryWordB = 0x1007;

// A troop's byte 3, and byte 0x10.
constexpr std::uint8_t kOwnFremen = 0x20;
constexpr std::uint8_t kNotPauls = 0x80;
constexpr std::uint8_t kHarkonnen = 0x80;
constexpr std::uint8_t kStill = 0x10;
constexpr int kArmy = 4;
// The troop that is shown three sietches in turn.
constexpr int kThreeSietches = 3;

// A place's byte 0x0A.
constexpr std::uint8_t kInside = 0x02;
constexpr std::uint8_t kTaken = 0x08;
constexpr int kFirstFort = 0x28;

constexpr std::uint8_t kSpiceMark = 0x30;  // bits 4 and 5 of a map byte
constexpr std::uint8_t kSpiceFound = 0x10;
constexpr std::uint8_t kLookedOver = 0x20;
// A latitude beyond which nothing is marked.
constexpr int kMarkedLatitudes = 0x5D;

} // namespace

void World::markAround(std::size_t record, int radius, std::uint8_t mark, FlatMap& map) {
    map.disc(u16(record + 2), static_cast<std::int16_t>(u16(record + 4)), radius, kMarkedLatitudes,
             [mark](std::uint8_t& b) {
                 if ((b & kSpiceMark) != kSpiceFound) {
                     b = static_cast<std::uint8_t>((b & ~kSpiceMark) | mark);
                 }
             });
}

// Each troop of a place's chain in turn; the next is the one the troop
// names after `each` is done with it (none once it left the chain).
void World::troopsAt(std::size_t place, const std::function<void(int troop)>& each) {
    for (int troop = u8(place + 9); troop != 0;) {
        each(troop);
        troop = u8(troopRecord(troop) + 1);
    }
}

// The chief's messages wait at 0x1190: a count, then up to ten of a
// message word and a place each.
void World::tell(std::uint16_t message, std::size_t place) {
    constexpr std::size_t kQueue = 0x1190;
    constexpr int kMost = 10;
    if (!(u8(0x0A) & 1)) {
        return;
    }
    int count = u8(kQueue);
    for (int i = 0; i < count; ++i) {
        const std::size_t e = kQueue + 1 + 4 * static_cast<std::size_t>(i);
        if (u16(e) == message && u16(e + 2) == place) {
            return;
        }
    }
    if (count >= kMost) {
        // The oldest goes.
        setU8(kQueue, static_cast<std::uint8_t>(u8(kQueue) - 1));
        if (u8(kQueue) == 0) {
            setU8(kQueue - 1, 0);
        }
        for (std::size_t i = 0; i < 0x24; ++i) {
            setU8(kQueue + 1 + i, u8(kQueue + 5 + i));
        }
        for (std::size_t i = 0; i < 4; ++i) {
            setU8(kQueue + 0x25 + i, 0);
        }
        count = kMost - 1;
    }
    setU8(kQueue, static_cast<std::uint8_t>(u8(kQueue) + 1));
    const std::size_t e = kQueue + 1 + 4 * static_cast<std::size_t>(count);
    setU16(e, message);
    setU16(e + 2, static_cast<std::uint16_t>(place));
}

void World::capture(std::size_t place, FlatMap& map, bool told) {
    if (told && place != u16(kPlaceRecord)) {
        tell(0x0F07, place);
    }

    // Paul's troops there are glad of it and turn to the army.
    auto glad = [&](int n) {
        const std::size_t t = troopRecord(n);
        if (u8(t + 3) >= kNotPauls) {
            return;
        }
        if (n == kThreeSietches) {
            setU8(t + 3, static_cast<std::uint8_t>(u8(t + 3) & ~kOwnFremen));
            cannotWork(n);
            return;
        }
        if (u8(t + 3) & kOwnFremen) {
            setU8(t + 3, 0x22);
            return;
        }
        setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) | 0x400));
        if (u8(place + 8) >= kFirstFort) {
            setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) | 0x20));
        }
        setU8(t + 0x15, static_cast<std::uint8_t>(std::min(u8(t + 0x15) + 4, 0x64)));
        const int skill = u8(t + 0x17);
        const int now = std::min(skill + 3, 0x5F);
        setU8(t + 0x17, static_cast<std::uint8_t>(now));
        if ((now ^ skill) & 0xF0) {
            setU8(t + 0x10, static_cast<std::uint8_t>((u8(t + 0x10) & 0xFC) | 2));
        }
        setOccupation(n, kArmy);
    };

    if (u8(place + 8) < kFirstFort) {
        setU8(place + 0x0A, static_cast<std::uint8_t>(u8(place + 0x0A) & ~kInside));
        troopsAt(place, glad);
    } else {
        // A fort: the land around is looked over, its byte 0x0B takes the
        // day plus 2, Paul's charisma grows by 4 and every troop of his
        // takes heart.
        setU8(place + 0x0B, 5);
        markAround(place, 5, kLookedOver, map);
        setU8(place + 0x0B, static_cast<std::uint8_t>((u16(kClockWord) >> 4) + 2));
        raiseCharisma(4);
        raiseSpirits(1);
        setU8(place + 0x0A, static_cast<std::uint8_t>(u8(place + 0x0A) | kTaken));
        const int name = u8(place) & 0x0F;
        setU16(kRegionChange, static_cast<std::uint16_t>((0x8000u << name | 0x8000u >> (16 - name)) & 0xFFFF));
        troopsAt(place, glad);
        // The Harkonnens there: those with an early turn stay as Fremen
        // not yet Paul's, the others go.
        troopsAt(place, [&](int n) {
            const std::size_t t = troopRecord(n);
            if (u8(t + 3) < kNotPauls || !(u8(t + 0x10) & kHarkonnen)) {
                return;
            }
            unlinkTroop(n);
            setU8(t + 0x10, static_cast<std::uint8_t>(u8(t + 0x10) & ~kHarkonnen));
            if (linkTroop(n, place) >= 8) {
                sendAway(n);
                return;
            }
            setU8(t + 3, 0xA0);
            const int a = dice(0xF7F);
            setU8(t + 0x1A, static_cast<std::uint8_t>((a & 0xFF) + 0x64));
            setU8(t + 0x15, static_cast<std::uint8_t>((a >> 8) + 0x14));
            const int b = dice(0x1F1F);
            setU8(t + 0x16, static_cast<std::uint8_t>((b & 0xFF) + 0x0A));
            setU8(t + 0x17, static_cast<std::uint8_t>((b >> 8) + 0x0A));
            setU8(t + 0x19, 0);
            setU8(t + 0x10, static_cast<std::uint8_t>(u8(t + 0x10) & ~kStill));
            if (markChanged) {
                markChanged(n);
            }
        });
    }

    // Harkonnens left there: one in four times one is kept prisoner, the
    // rest go; looked over again until no more than that are left.
    const int keep = (u16(0) & 3) == 0 ? 1 : 0;
    for (int passes = 0; passes < 64; ++passes) {
        int seen = 0;
        troopsAt(place, [&](int n) {
            const std::size_t t = troopRecord(n);
            if (u8(t + 3) < kNotPauls || !(u8(t + 0x10) & kHarkonnen)) {
                return;
            }
            if (seen < keep) {
                setU8(t + 3, 0xAC);
                setU8(t + 0x10, static_cast<std::uint8_t>(u8(t + 0x10) | kStill));
                setU8(t + 0x1A, 0);
                setU16(t + 0x0C, 0);
                setU8(t + 0x19, 0);
            } else {
                sendAway(n);
            }
            ++seen;
        });
        if (seen <= keep) {
            break;
        }
    }

    // When the Harkonnens are down to one fort or none, the story notes
    // it and the place takes the equipment kept everywhere, less 10.
    if (countHarkonnenForts() <= 1) {
        setU8(kAllTaken, 1);
        setU8(kStoryWordA, static_cast<std::uint8_t>(u8(kStoryWordA) & ~2));
        setU8(kStoryWordB, static_cast<std::uint8_t>(u8(kStoryWordB) & ~2));
        int all = 0;
        for (std::size_t r = kPlaces; u8(r) != 0xFF; r += kPlaceSize) {
            all = (all + u8(r + 0x19)) & 0xFF;
        }
        if (all >= 10) {
            setU8(place + 0x19, static_cast<std::uint8_t>(u8(place + 0x19) + all - 10));
        }
    }
    if (placeChanged) {
        placeChanged(place);
    }
}

// The forts not taken by Paul (byte 0x0A without bit 3): how many, and the
// spice they bring the Harkonnens, kept at word 0xA8.
int World::countHarkonnenForts() {
    int count = 0;
    int spice = 0;
    for (std::size_t r = kPlaces; u8(r) != 0xFF; r += kPlaceSize) {
        if (u8(r + 8) < kFirstFort || (u8(r + 0x0A) & kTaken)) {
            continue;
        }
        ++count;
        spice += u8(r + 0x12) >> 3;
    }
    spice += random3 ? random3(spice >> 4) : 0;
    setU16(kHarkonnenSpice, static_cast<std::uint16_t>(spice));
    return count & 0xFF;
}

void World::sendAway(int troop) {
    const std::size_t t = troopRecord(troop);
    unlinkTroop(troop);
    setU16(t + 4, 0xFBC);
    setU8(t + 3, 0xA0);
    setU8(t + 0x1A, 0);
    if (markChanged) {
        markChanged(troop);
    }
}

// Paul's charisma grows, to 200 at most; each time it passes a multiple
// of 4, every troop of his takes heart.
void World::raiseCharisma(int by) {
    const int before = u8(kCharisma);
    const int after = std::min(before + by, 0xC8);
    setU8(kCharisma, static_cast<std::uint8_t>(after));
    const int steps = (((after & 0xFC) - (before & 0xFC)) & 0xFF) >> 2;
    if (steps != 0) {
        raiseSpirits(steps);
    }
}

// The spirits of every troop of Paul's (byte 3 without bits 5 and 7) go
// up, to 100 at most.
void World::raiseSpirits(int by) {
    for (std::size_t t = kTroops; t < 0xFBB; t += kTroopSize) {
        if (u8(t + 3) & 0xA0) {
            continue;
        }
        setU8(t + 0x15, static_cast<std::uint8_t>(std::min(u8(t + 0x15) + by, 0x64)));
    }
}

} // namespace mentat
