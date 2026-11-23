// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the troops do each hour: those on the move step on, the others do
// their work, and a new day changes the places. See docs/rules/work.md.
#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/world.hpp"

#include <algorithm>

namespace mentat {

namespace {

constexpr std::size_t kClockWord = 0x02;
constexpr std::size_t kLastClock = 0x1174;
constexpr std::size_t kStoryEnd = 0xC2;
constexpr std::size_t kSietches = 0x27;
constexpr std::size_t kSpice = 0xA0;
constexpr std::size_t kBoost = 0xFA;
constexpr std::size_t kNameKinds = 0x1141;
constexpr std::size_t kProgress = 0x2A;
constexpr std::size_t kEvents = 0x0A;
constexpr std::size_t kGrowing = 0xEC;
constexpr std::size_t kHarkonnenGoal = 0x101A;
constexpr std::size_t kSecondGoal = 0x11D3;
// Twelve records of 16 bytes whose word 2 may name a place.
constexpr std::size_t kNamed = 0xFD8;
constexpr int kNamedCount = 12;

// A troop's byte 3.
constexpr std::uint8_t kOccupation = 0x0F;
constexpr std::uint8_t kCannot = 0x10;
constexpr std::uint8_t kOwnFremen = 0x20;
constexpr std::uint8_t kMoving = 0x40;
constexpr std::uint8_t kNotPauls = 0x80;
constexpr std::uint8_t kHarkonnen = 0x80; // byte 0x10
constexpr int kThreeSietches = 3;
constexpr std::size_t kTroopsEnd = 0xFBB;
constexpr int kSmall = 0x14; // men below which a troop joins another

// A place's byte 0x0A.
constexpr std::uint8_t kGreen = 0x01;
constexpr std::uint8_t kHeld = 0x02;
constexpr std::uint8_t kWatched = 0x04;
constexpr std::uint8_t kTaken = 0x08;
constexpr std::uint8_t kWindTrap = 0x20;
constexpr std::uint8_t kProspected = 0x40;
constexpr std::uint8_t kHidden = 0x80;
constexpr int kFirstFort = 0x28;

constexpr std::uint8_t kLookedOver = 0x20;

std::uint16_t rol16(std::uint16_t v, int by) {
    by &= 15;
    return static_cast<std::uint16_t>((v << by) | (v >> ((16 - by) & 15)));
}

} // namespace

void World::raiseSkill(std::size_t t, int which, int by) {
    const int before = u8(t + 0x16 + static_cast<std::size_t>(which));
    const int after = std::min((before + by) & 0xFF, 0x5F);
    setU8(t + 0x16 + static_cast<std::size_t>(which), static_cast<std::uint8_t>(after));
    if ((after ^ before) & 0xF0) {
        setU8(t + 0x10, static_cast<std::uint8_t>((u8(t + 0x10) & 0xFC) | (which + 1)));
    }
}

void World::hourPassed(int except, FlatMap& map) {
    const std::uint16_t now = u16(kClockWord);
    const std::uint16_t last = u16(kLastClock);
    setU16(kLastClock, now);
    newDay_ = ((last & 0xF0) - (now & 0xF0)) != 0;
    if (u8(kStoryEnd) < 7) {
        troopsWork(except, map);
    }
}

void World::troopsWork(int except, FlatMap& map) {
    harkonnenGoal_ = 0;
    const std::uint16_t goal = u16(kHarkonnenGoal);
    if ((goal & 0xFF) == 0x80) {
        harkonnenGoal_ = kPlaces + kPlaceSize * static_cast<std::size_t>(((goal >> 8) - 1) & 0xFF);
    }
    busy_ = 0;
    for (std::size_t t = kTroops; t < kTroopsEnd; t += kTroopSize) {
        const int troop = static_cast<int>((t - kTroops) / kTroopSize) + 1;
        if (u16(t + 0x12) & 0x430) {
            if (u8(t + 3) & kMoving) {
                step(troop, except, map);
                continue;
            }
            if (u8(kBoost) == 0) {
                continue;
            }
            setU8(t + 0x12, static_cast<std::uint8_t>(u8(t + 0x12) & 0xCF));
            if (u16(t + 0x12) & 0x400) {
                continue;
            }
        }
        if (u8(t + 0x1A) < kSmall && joinOther(troop)) {
            continue;
        }
        const int job = u8(t + 3);
        if (job & (kNotPauls | kOwnFremen)) {
            continue;
        }
        if (job & kMoving) {
            step(troop, except, map);
            continue;
        }
        const std::size_t place = u16(t + 4);
        switch (job & kOccupation) {
        case 0: mine(troop, place, map); break;
        case 1: prospect(troop, place, map); break;
        case 4: train(troop, place, map); break;
        case 8: plant(troop, place, map); break;
        case 9: buildWindTrap(troop, place); break;
        case 10: waitToPlant(troop, place); break;
        case 5: attack(troop, place); break;
        case 6: fight(troop, place, map); break;
        default: break;
        }
        forget(t);
    }
    setU8(kBoost, busy_);
}

void World::step(int troop, int except, FlatMap& map) {
    const std::size_t t = troopRecord(troop);
    if (u8(t) == except) {
        return;
    }
    if (!stepTroop(troop, (u8(t + 0x19) & 0x40) ? 8 : 4, map) && markMoved) {
        markMoved(troop);
    }
}

// A small troop joins the smallest of Paul's other troops there that has
// room for its men.
bool World::joinOther(int troop) {
    const std::size_t t = troopRecord(troop);
    if ((u8(t + 3) & 0xE3) || (u8(t + 0x10) & kHarkonnen) || troop == kThreeSietches) {
        return false;
    }
    const std::size_t place = u16(t + 4);
    int room = ~u8(t + 0x1A) & 0xFF;
    int best = 0;
    troopsAt(place, [&](int n) {
        const std::size_t o = troopRecord(n);
        if (u8(o + 3) >= kNotPauls || (u8(o + 3) & 0xA0) || n == kThreeSietches || n == troop) {
            return;
        }
        if (u8(o + 0x1A) <= room) {
            best = n;
            room = u8(o + 0x1A);
        }
    });
    if (best == 0) {
        return false;
    }
    const std::size_t b = troopRecord(best);
    setU8(b + 0x1A, static_cast<std::uint8_t>(u8(b + 0x1A) + u8(t + 0x1A)));
    const std::uint8_t held = u8(t + 0x19);
    setU8(t + 0x19, static_cast<std::uint8_t>(held & u8(b + 0x19)));
    setU8(b + 0x19, static_cast<std::uint8_t>(u8(b + 0x19) | held));
    setU16(b + 0x12, static_cast<std::uint16_t>(u16(b + 0x12) | 0x200));
    sendAway(troop);
    return true;
}

// Every 64 hours a troop loses a little of the skills of the specialities
// it has worked at before and not now.
void World::forget(std::size_t t) {
    if (u16(kClockWord) & 0x3F) {
        return;
    }
    const int speciality = (u8(t + 3) & kOccupation) >> 2;
    const std::uint16_t worked = rol16(0xC000, speciality) & u16(t + 0x12);
    for (int bit = 0; bit < 3; ++bit) {
        if (worked & (0x8000 >> bit)) {
            const std::size_t skill = t + 0x18 - static_cast<std::size_t>(bit);
            if (u8(skill) != 0) {
                setU8(skill, static_cast<std::uint8_t>(u8(skill) - 1));
            }
        }
    }
}

// What a new day brings a troop working at a place, for spice, the army
// and ecology.
void World::newDay(int troop, std::size_t place, FlatMap& map) {
    if (!newDay_) {
        return;
    }
    const std::size_t t = troopRecord(troop);
    const int day = u16(kClockWord) >> 4;
    if (u8(place + 0x0A) & kTaken) {
        if (((day - u8(place + 0x0B)) & 0xFF) >= 0xFE) {
            return;
        }
        // Two days after it was taken a fort becomes a sietch.
        setU8(place + 0x0A, static_cast<std::uint8_t>(u8(place + 0x0A) & ~kTaken));
        setU8(place + 8, static_cast<std::uint8_t>(u8(place + 8) & 7));
        setU8(kSietches, static_cast<std::uint8_t>(u8(kSietches) + 1));
        becameSietch(place);
        setU8(place + 0x0B, 5);
    }
    // A sietch's land is looked over a little further each day, up to 12.
    if (u8(place + 8) < 0x20 && u8(place + 0x0B) < 0x0C && !(u8(place + 0x0A) & kGreen)) {
        setU8(place + 0x0B, static_cast<std::uint8_t>(u8(place + 0x0B) + 1));
        markAround(place, u8(place + 0x0B), kLookedOver, map);
    }
    if (((day - u8(t + 0x14)) & 0xFF) > 8) {
        lowerSpirits(t, 1);
    }
    if (u8(place + 9) != u8(t)) {
        return;
    }
    // The first troop there: when the troops low in spirits there are
    // both of those that lack and those that have bit 7 of byte 0x12,
    // they stop working.
    int seen = 0;
    troopsAt(place, [&](int n) {
        const std::size_t o = troopRecord(n);
        if (u8(o + 3) >= kNotPauls || place == u16(kPlaceRecord) || u8(place + 8) >= 0x21 || u8(o + 0x15) >= 0x28 ||
            (u8(o + 3) & 0x2F)) {
            return;
        }
        seen |= (u8(o + 0x12) & 0x80) ? 2 : 1;
    });
    if (seen != 3) {
        return;
    }
    troopsAt(place, [&](int n) {
        const std::size_t o = troopRecord(n);
        if (u8(o + 3) >= kNotPauls || u8(o + 0x15) >= 0x28 || (u8(o + 3) & 0x2F & 0xFB)) {
            return;
        }
        setU8(o + 3, static_cast<std::uint8_t>(u8(o + 3) | kCannot));
        setU8(o + 0x12, static_cast<std::uint8_t>(u8(o + 0x12) | 0x10));
    });
    tell(0x0302, place);
}

// A fort that became a sietch: the records naming it learn its new kind,
// and its Harkonnens go.
void World::becameSietch(std::size_t place) {
    const std::uint16_t name = static_cast<std::uint16_t>(((((place - kPlaces) / kPlaceSize) + 1) << 8) | 0x80);
    const std::uint8_t kind = u8(place + 8);
    auto renamed = [&](std::uint16_t word) {
        return static_cast<std::uint16_t>((kind << 8) | ((word & 0xFF) == 1 ? 1 : 2));
    };
    for (int i = 0; i < kNamedCount; ++i) {
        const std::size_t r = kNamed + 16 * static_cast<std::size_t>(i);
        if (u16(r + 2) == name) {
            setU16(r, renamed(u16(r)));
        }
    }
    if (u16(6) == name) {
        sietchHere = true;
        const std::uint16_t w = renamed(u16(4));
        setU16(4, w);
        setU8(0x0B, static_cast<std::uint8_t>(w & 0xFF));
        setU8(8, static_cast<std::uint8_t>(w >> 8));
    }
    troopsAt(place, [&](int n) {
        const std::size_t o = troopRecord(n);
        if (u16(o + 0x10) & 0x20) {
            setU16(o + 0x10, static_cast<std::uint16_t>(u16(o + 0x10) & ~0x20));
            setU16(o + 0x12, static_cast<std::uint16_t>(u16(o + 0x12) | 0x1000));
        }
    });
    sendHarkonnensAway(place);
}

// Spice mining.
void World::mine(int troop, std::size_t place, FlatMap& map) {
    const std::size_t t = troopRecord(troop);
    newDay(troop, place, map);
    if (u16(t + 0x10) & 0x200) {
        // Stopped: it starts again in the hour of its number.
        if ((u16(kClockWord) & 0x0F) != (u8(t) & 0x0F)) {
            setU16(t + 0x0C, 0);
            setU8(t + 3, static_cast<std::uint8_t>(u8(t + 3) | kCannot));
            return;
        }
        setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) & ~0x200));
        if (cannotWork(troop)) {
            return;
        }
    } else {
        if (cannotWork(troop)) {
            setU16(t + 0x0C, 0);
            setU16(t + 0x0E, 0);
            setU8(t + 3, static_cast<std::uint8_t>(u8(t + 3) | kCannot));
            return;
        }
        miningEvents(troop, place);
    }
    setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) | 0x100));
    const int got = harvest(t);
    const std::uint16_t before = u16(t + 0x0E);
    const std::uint16_t after = static_cast<std::uint16_t>(before + got);
    setU16(t + 0x0E, after);
    if ((before ^ after) & 0xFF80) {
        raiseSkill(t, 0, 1);
    }
    const int tenths = got + spiceLeft_;
    spiceLeft_ = tenths % 10;
    setU16(kSpice, static_cast<std::uint16_t>(u16(kSpice) + tenths / 10));
    const int dug = got + u8(place + 0x13);
    const int per = std::max(1, static_cast<int>(u8(place + 0x11)));
    const int taken = (dug / per) & 0xFF;
    setU8(place + 0x13, static_cast<std::uint8_t>(dug % per));
    if (taken > (u8(place + 0x12) & 0x0F) && (mapMode & 0x40)) {
        ++mapRedraws;
    }
    setU8(place + 0x12, static_cast<std::uint8_t>(std::max(0, u8(place + 0x12) - taken)));
}

// What may befall a harvester at work, in the hour of its troop's number.
void World::miningEvents(int troop, std::size_t place) {
    const std::size_t t = troopRecord(troop);
    if (!(u8(kEvents) & 1) || !(u8(t + 0x19) & 0x80) || (u16(kClockWord) & 0x0F) != (u8(t) & 0x0F)) {
        return;
    }
    auto stop = [&] {
        setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) | 0x200));
        setU8(t + 3, static_cast<std::uint8_t>(u8(t + 3) | kCannot));
    };
    if (u8(kProgress) >= 0x35 && (u16(t + 0x12) & 0x40)) {
        turnSeed();
        turnSeed();
        turnSeed();
        if ((u16(0) & 7) == 0) {
            // The Harkonnens see it: it stops, and the place is watched.
            stop();
            setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) | 0x8000));
            setU8(place + 0x0A, static_cast<std::uint8_t>(u8(place + 0x0A) | kWatched));
            tell(0x0F03, place);
        }
    }
    const int r = random2 ? random2() : 0;
    const int high = (r >> 8) & 0xFF;
    if (u8(kNameKinds + u8(place)) < high) {
        return;
    }
    setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) | 0x4000));
    if (u8(t + 0x19) & 0x40) {
        return;
    }
    switch (high & 3) {
    case 1:
        setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) | 0x2000));
        if (u8(t + 0x1A) > 2) {
            setU8(t + 0x1A, static_cast<std::uint8_t>(u8(t + 0x1A) - 2));
        }
        break;
    case 2: stop(); break;
    case 3:
        // A worm takes the harvester.
        setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) | 0x1000));
        setU8(t + 0x19, static_cast<std::uint8_t>(u8(t + 0x19) & 0x7F));
        setU8(place + 0x14, static_cast<std::uint8_t>(u8(place + 0x14) - 1));
        tell(0x0F06, place);
        break;
    default: break;
    }
}

// Prospecting: the place is known for its spice after a time that
// depends on its size and on the troop.
void World::prospect(int troop, std::size_t place, FlatMap& map) {
    const std::size_t t = troopRecord(troop);
    const std::uint8_t flags = u8(place + 0x0A);
    if (flags & kHeld) {
        return;
    }
    if (!(flags & kProspected)) {
        if (cannotWork(troop)) {
            return;
        }
        unsigned need = u16(t + 0x0C);
        if (need == 0) {
            const unsigned by = (u8(t + 0x15) + u8(t + 0x16)) & 0xFF;
            need = by == 0 ? 0 : ((static_cast<unsigned>(u8(place + 0x11)) << 4) / by) & 0xFF;
            setU16(t + 0x0C, static_cast<std::uint16_t>(need));
        }
        const unsigned spent = static_cast<std::uint16_t>(u16(kClockWord) - u16(t + 0x0A));
        if (need > spent) {
            if (spent != 0) {
                setU16(t + 0x0E, static_cast<std::uint16_t>(spent * 100 / need));
            }
            return;
        }
        raiseSkill(t, 0, 2);
        setU8(place + 0x0A, static_cast<std::uint8_t>(u8(place + 0x0A) | kProspected));
        if (mapMode & 0x40) {
            ++mapRedraws;
        }
    }
    const std::uint8_t job = u8(t + 3);
    cannotWork(troop);
    setU16(t + 0x0E, 100);
    if (t == shownTroop) {
        return;
    }
    if (u16(kSecondGoal) != 0) {
        startMove(troop, place, map);
        return;
    }
    if (!(job & kCannot)) {
        tell(0x0F0E, place);
    }
}

// The army: training, and keeping watch at a place the Harkonnens
// watch.
void World::train(int troop, std::size_t place, FlatMap& map) {
    const std::size_t t = troopRecord(troop);
    newDay(troop, place, map);
    setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) & ~0x200));
    if (u8(place + 0x0A) & kWatched) {
        std::uint16_t w = u16(t + 0x0E);
        if ((w >> 8) != 0xFF) {
            w = static_cast<std::uint16_t>(0xFF00 | ((0x40 - u8(t + 0x17)) & 0xFF));
        }
        const std::uint8_t low = static_cast<std::uint8_t>((w & 0xFF) - 1);
        w = static_cast<std::uint16_t>((w & 0xFF00) | low);
        if (!(low & 0x80)) {
            setU16(t + 0x0E, w);
            return;
        }
        // The watch is over.
        setU8(place + 0x0A, static_cast<std::uint8_t>(u8(place + 0x0A) & ~kWatched));
        std::uint16_t seen = 0;
        troopsAt(place, [&](int n) {
            const std::size_t o = troopRecord(n);
            if (u8(o + 3) < kNotPauls && (u16(o + 0x12) & 0x40)) {
                setU16(o + 0x12, static_cast<std::uint16_t>(u16(o + 0x12) & ~0x40));
                seen = 0x100;
            }
        });
        setU16(t + 0x12, static_cast<std::uint16_t>(u16(t + 0x12) | seen));
        setU16(t + 0x0E, 0);
        return;
    }
    const std::uint16_t left = static_cast<std::uint16_t>(u16(t + 0x0C) - 1);
    setU16(t + 0x0C, left);
    if (!(left & 0x8000)) {
        return;
    }
    // A lesson: the next one is further off the more the troop already
    // knows, the less keen it is and the better its weapons.
    int level = 0xA0;
    if (place == harkonnenGoal_) {
        setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) | 0x800));
    } else {
        int count = 0;
        int sum = 0;
        troopsAt(place, [&](int n) {
            const std::size_t o = troopRecord(n);
            if (u8(o + 3) == 4) {
                ++count;
                sum += u8(o + 0x17);
            }
        });
        level = count == 0 ? sum & 0xFF : (sum / (count & 0xFF)) & 0xFF;
    }
    int gap = std::max(0, level - u8(t + 0x17)) * 2;
    gap += std::max(0x1E, keenness(t));
    const int held = u8(t + 0x19);
    const int base = (held & 0x0C) ? 0xC8 : (held & 0x10) ? 0xFA : (held & 0x20) ? 0x12C : 0x190;
    setU16(t + 0x0C, static_cast<std::uint16_t>(base / gap));
    raiseSkill(t, 1, 1);
}

// Ecology: the first time, the place starts to grow green; then each day
// the green spreads while its water lasts.
void World::plant(int troop, std::size_t place, FlatMap& map) {
    const std::size_t t = troopRecord(troop);
    if ((u8(t + 3) & kCannot) || cannotWork(troop)) {
        return;
    }
    busy_ = 0xFF;
    setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) | 0x100));
    if (!(u8(place + 0x0A) & kGreen)) {
        setU8(place + 0x0A, static_cast<std::uint8_t>(u8(place + 0x0A) | kGreen));
        setU8(place + 0x12, 0);
        setU16(place + 0x0C, u16(place + 2));
        setU16(place + 0x0E, u8(place + 4));
        setU8(place + 0x0B, 4);
        spreadGreen(troop, place, map);
        return;
    }
    newDay(troop, place, map);
    int grow = u8(t + 0x18) >> 2;
    if (grow == 0) {
        grow = 1;
    }
    const int sum = u8(place + 0x0F) + grow;
    setU8(place + 0x0F, static_cast<std::uint8_t>(sum));
    if (sum < 0x100) {
        return;
    }
    raiseSkill(t, 2, 1);
    if (u8(place + 0x1B) < 0x0C) {
        setU8(place + 0x1B, 0);
        setU8(t + 3, static_cast<std::uint8_t>(u8(t + 3) | kCannot));
        if (markChanged) {
            markChanged(troop);
        }
        return;
    }
    setU8(place + 0x1B, static_cast<std::uint8_t>(u8(place + 0x1B) - 0x0C));
    if (u8(place + 0x0B) < 0x0C) {
        setU8(place + 0x0B, static_cast<std::uint8_t>(u8(place + 0x0B) + 1));
    }
    const int north = static_cast<std::int8_t>(u8(place + 0x0E)) - 2;
    if (north >= -0x52) {
        setU8(place + 0x0E, static_cast<std::uint8_t>(north));
    }
    spreadGreen(troop, place, map);
}

// The green around a place's word 0x0C and byte 0x0E, within its byte
// 0x0B: low ground turns green one point in four, the rest is looked
// over; a Harkonnen fort it reaches is taken.
void World::spreadGreen(int troop, std::size_t place, FlatMap& map) {
    (void)troop;
    if (placeChanged && !(u8(place + 0x0A) & kHidden)) {
        placeChanged(place);
    }
    std::uint8_t pattern = 0x44;
    std::vector<std::size_t> forts;
    std::uint8_t* first = map.bytes().data();
    map.disc(u16(place + 0x0C), static_cast<std::int8_t>(u8(place + 0x0E)), u8(place + 0x0B), 0x56,
             [&](std::uint8_t& b) {
                 if ((b & 0x30) == 0x10) {
                     return;
                 }
                 if (b & 0x40) {
                     const auto at = static_cast<std::uint16_t>(&b - first);
                     for (std::size_t r = kPlaces; u16(r) != 0xFFFF; r += kPlaceSize) {
                         if (u16(r + 6) != at) {
                             continue;
                         }
                         setU8(r + 0x12, 0);
                         if (u8(r + 8) >= kFirstFort && !(u8(r + 0x0A) & kTaken) && r >= 0x138) {
                             setU8(r + 0x0A, static_cast<std::uint8_t>(u8(r + 0x0A) & ~kHidden));
                             forts.push_back(r);
                         }
                         break;
                     }
                 }
                 std::uint8_t v = static_cast<std::uint8_t>((b & 0xCF) | 0x20);
                 if ((b & 0x0E) < 8) {
                     const bool carry = (pattern & 0x80) != 0;
                     pattern = static_cast<std::uint8_t>((pattern << 1) | (carry ? 1 : 0));
                     if (carry) {
                         v = static_cast<std::uint8_t>((v & 0xCF) | 0x10);
                     }
                 }
                 b = v;
             });
    for (const std::size_t fort : forts) {
        capture(fort, map, false);
    }
}

// Ecology at the one place where nothing grows yet: it waits for the
// first water.
void World::waitToPlant(int troop, std::size_t place) {
    if (u8(place + 0x1A) == 0) {
        setU8(kGrowing, static_cast<std::uint8_t>(u8(kGrowing) + 1));
        if (u8(kGrowing) != 0) {
            return;
        }
        setU8(place + 0x1A, 0x10);
    }
    setOccupation(troop, 8);
}

// A wind-trap: built from what the troop gathers; then the troop goes
// back to ecology.
void World::buildWindTrap(int troop, std::size_t place) {
    const std::size_t t = troopRecord(troop);
    if (!(u8(place + 0x0A) & kWindTrap)) {
        setU8(t + 0x15, static_cast<std::uint8_t>(std::min(u8(t + 0x15) + 1, 0x64)));
        int a = (2 * keenness(t)) & 0xFF;
        a += u8(t + 0x16);
        if (a > 0xFF) {
            a = 0xFF;
        }
        const int gathered = ((a * u8(t + 0x1A)) >> 4) >> 8;
        const int sum = u8(place + 0x1B) + gathered;
        setU8(place + 0x1B, static_cast<std::uint8_t>(sum));
        if (sum < 0x100) {
            return;
        }
        setU8(place + 0x0A, static_cast<std::uint8_t>(u8(place + 0x0A) | kWindTrap));
        setU8(place + 8, static_cast<std::uint8_t>(u8(place + 8) | 8));
        setU8(place + 0x1B, 5);
    }
    setOccupation(troop, u8(t + 3) & 0xFC);
}

} // namespace mentat
