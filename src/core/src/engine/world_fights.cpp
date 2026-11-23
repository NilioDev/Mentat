// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fights: Paul's troops getting ready to attack a place, and fighting the
// Harkonnens there hour by hour until one side is gone. See
// docs/rules/fights.md.
#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/world.hpp"

#include <algorithm>

namespace mentat {

namespace {

constexpr std::size_t kClockWord = 0x02;
constexpr std::size_t kCharisma = 0x29;
constexpr std::size_t kSietches = 0x27;
constexpr std::size_t kTheirs = 0x94;
constexpr std::size_t kLean = 0x9C;
constexpr std::size_t kOursHere = 0x60;
constexpr std::size_t kFought = 0x11BC;
constexpr std::size_t kStoryEnd = 0xC2;
constexpr std::size_t kNamed = 0xFD8;
// The Harkonnens' palace.
constexpr std::size_t kPalace = 0x11C;

constexpr std::uint8_t kOwnFremen = 0x20; // byte 3
constexpr std::uint8_t kNotPauls = 0x80;
constexpr std::uint8_t kHarkonnen = 0x80; // byte 0x10
constexpr std::uint8_t kStill = 0x10;
constexpr std::uint8_t kReady = 0x40;
constexpr int kFighting = 6;
constexpr int kFirstFort = 0x28;
constexpr std::size_t kTroopsEnd = 0xFBB;

} // namespace

// Paul's charisma falls, to 1 at least; each time it falls past a multiple
// of 4, the spirits of every troop of his go down.
void World::lowerCharisma(int by) {
    const int before = u8(kCharisma);
    int after = before - by;
    if (after <= 0) {
        after = 1;
    }
    setU8(kCharisma, static_cast<std::uint8_t>(after));
    const int steps = (((before & 0xFC) - (after & 0xFC)) & 0xFF) >> 2;
    if (steps == 0) {
        return;
    }
    for (std::size_t t = kTroops; t < kTroopsEnd; t += kTroopSize) {
        if (!(u8(t + 3) & 0xA0)) {
            lowerSpirits(t, steps);
        }
    }
}

// A troop of Paul's flees: it keeps nothing, and his charisma falls by 4.
void World::flee(int troop) {
    const std::size_t t = troopRecord(troop);
    if ((u8(t + 3) & kOwnFremen) || (u8(t + 0x10) & kHarkonnen)) {
        return;
    }
    setU8(t + 3, static_cast<std::uint8_t>(u8(t + 3) | kOwnFremen));
    setU8(t + 0x19, 0);
    setU16(t + 0x0A, u16(kClockWord));
    lowerCharisma(4);
}

// The men a troop loses to a blow of `blow`: less the better it knows
// the army, never more than it has.
int World::casualties(std::size_t t, unsigned blow) const {
    const unsigned guard = static_cast<unsigned>((0xFF - 2 * u8(t + 0x17)) & 0xFF);
    const unsigned long hit = static_cast<unsigned long>(blow) * guard;
    const int lost = (hit >> 16) != 0 ? 0xFF : static_cast<int>((hit >> 8) & 0xFF);
    return std::min(lost, static_cast<int>(u8(t + 0x1A)));
}

// Attacking (occupation 5): the troop gets ready, rousing the Harkonnens
// there; a troop not first at the place may flee while it waits.
void World::attack(int troop, std::size_t place) {
    const std::size_t t = troopRecord(troop);
    const int waited = static_cast<std::int16_t>(static_cast<std::uint16_t>(u16(kClockWord) - u16(t + 0x0A)));
    if (!(u8(t + 0x10) & kReady)) {
        const int readyAt = static_cast<std::int8_t>(static_cast<std::uint8_t>(0x50 - u8(t + 0x17))) >> 1;
        if ((readyAt >> 1) < waited) {
            bool ready = false;
            if (u16(t + 0x0C) == 0) {
                int roused = 0;
                troopsAt(place, [&](int n) {
                    const std::size_t o = troopRecord(n);
                    if (u8(o + 0x10) & kStill) {
                        setU8(o + 0x10, static_cast<std::uint8_t>(u8(o + 0x10) & ~kStill));
                        ++roused;
                        if (markChanged) {
                            markChanged(n);
                        }
                    }
                });
                if (roused == 0) {
                    ready = true;
                } else {
                    setU16(t + 0x0C, static_cast<std::uint16_t>(roused));
                }
            }
            if (!ready && readyAt < waited) {
                weighForces(place);
                setU16(t + 0x0E, static_cast<std::uint16_t>(u16(kTheirs) / u16(t + 0x0C)));
                ready = true;
            }
            if (ready) {
                setU8(t + 0x10, static_cast<std::uint8_t>(u8(t + 0x10) | kReady));
            }
        }
    }
    if (u8(t) != u8(place + 9) && static_cast<unsigned>(u8(t + 0x17) >> 1) < static_cast<unsigned>(waited & 0xFFFF)) {
        const int r = (random2 ? random2() : 0) & 0x3F;
        if (r >= u8(t + 0x17)) {
            flee(troop);
        }
    }
}

// Fighting (occupation 6): a blow each hour, to the Harkonnens or to the
// troop, by how the fight leans.
void World::fight(int troop, std::size_t place, FlatMap& map) {
    setU8(kFought, static_cast<std::uint8_t>(u8(kFought) | 1));
    if (place == kPalace) {
        // The Harkonnens' palace falls.
        setU8(kStoryEnd, static_cast<std::uint8_t>(u8(kStoryEnd) + 1));
        troopsAt(place, [&](int n) { setU8(troopRecord(n) + 3, 4); });
        sendHarkonnensAway(place);
        for (std::uint8_t& b : map.bytes()) {
            if ((b & 0x30) == 0x30) {
                b = static_cast<std::uint8_t>(b & ~0x10);
            }
        }
        tell(0x0F0A, kPalace);
        return;
    }
    weighForces(place);
    if (u16(kTheirs) == 0) {
        capture(place, map);
        return;
    }
    const int r = (random2 ? random2() : 0) & 0xFF;
    if (r >= u8(kLean)) {
        struck(troop, place, map);
        return;
    }
    strike(troop, place, map);
}

// How many Harkonnen troops (not fled) and how many of Paul's fighting
// troops are at the place.
void World::countFighters(std::size_t place, int& harkonnens, int& pauls) {
    harkonnens = 0;
    pauls = 0;
    troopsAt(place, [&](int n) {
        const std::size_t o = troopRecord(n);
        if (u8(o + 3) & kOwnFremen) {
            return;
        }
        if (u8(o + 0x10) & kHarkonnen) {
            ++harkonnens;
        } else if (u8(o + 3) == kFighting) {
            ++pauls;
        }
    });
}

// The troop strikes: each Harkonnen troop there loses men; when none is
// left the place is taken.
void World::strike(int troop, std::size_t place, FlatMap& map) {
    const std::size_t t = troopRecord(troop);
    int harkonnens = 0;
    int pauls = 0;
    countFighters(place, harkonnens, pauls);
    unsigned worth = static_cast<unsigned>(strength(t));
    if (harkonnens != 0) {
        worth /= static_cast<unsigned>(harkonnens);
    }
    unsigned blow = (worth + 1) & 0xFF;
    if (blow == 0) {
        blow = 0xFF;
    }
    int standing = 0;
    unsigned killed = 0;
    troopsAt(place, [&](int n) {
        const std::size_t o = troopRecord(n);
        if (!(u8(o + 0x10) & kHarkonnen)) {
            return;
        }
        ++standing;
        const int lost = casualties(o, blow);
        killed += static_cast<unsigned>(lost);
        const int men = u8(o + 0x1A) - lost;
        setU8(o + 0x1A, static_cast<std::uint8_t>(men));
        if (men > 0) {
            return;
        }
        setU8(o + 3, static_cast<std::uint8_t>(u8(o + 3) | kOwnFremen));
        setU8(o + 0x10, static_cast<std::uint8_t>(u8(o + 0x10) | kStill));
        --standing;
        if (dice(3) == 0) {
            setU8(o + 0x19, static_cast<std::uint8_t>(u8(o + 0x19) & ~4));
            heldEquipment(o, place, -1);
        }
        if (markChanged) {
            markChanged(n);
        }
    });
    setU16(t + 0x0C, static_cast<std::uint16_t>(u16(t + 0x0C) + killed));
    if (standing == 0) {
        capture(place, map);
    }
}

// The troop is struck: when it has no men left it flees with a few, and
// when none of Paul's troops fight there any more the place is lost.
void World::struck(int troop, std::size_t place, FlatMap& map) {
    const std::size_t t = troopRecord(troop);
    unsigned blow = u16(kTheirs);
    if (u8(kOursHere) != 0) {
        blow /= u8(kOursHere);
    }
    const int lost = casualties(t, blow);
    setU16(t + 0x0E, static_cast<std::uint16_t>(u16(t + 0x0E) + lost));
    const int men = u8(t + 0x1A) - lost;
    setU8(t + 0x1A, static_cast<std::uint8_t>(men));
    if (men > 0) {
        return;
    }
    setU8(t + 0x1A, static_cast<std::uint8_t>(dice(0x7F) + 0x1E));
    flee(troop);
    int harkonnens = 0;
    int pauls = 0;
    countFighters(place, harkonnens, pauls);
    if (pauls == 0) {
        losePlace(place, map);
    }
}

// The Harkonnens there that are not Paul's go, again until none is left.
void World::sendHarkonnensAway(std::size_t place) {
    for (int gone = 1; gone != 0;) {
        gone = 0;
        troopsAt(place, [&](int n) {
            const std::size_t o = troopRecord(n);
            if (u8(o + 3) >= kNotPauls && (u8(o + 0x10) & kHarkonnen)) {
                sendAway(n);
                ++gone;
            }
        });
    }
}

// The Harkonnens win the place: a sietch becomes their fort, Paul's
// troops there flee, the Harkonnens' troops stay still, and the land
// around is theirs.
void World::losePlace(std::size_t place, FlatMap& map) {
    setU8(place + 0x0A, static_cast<std::uint8_t>(u8(place + 0x0A) & ~0x02));
    if (place == u16(kPlaceRecord)) {
        lostWithPaul = true;
        return;
    }
    const int kind = u8(place + 8);
    if (kind < kFirstFort) {
        setU8(place + 8, static_cast<std::uint8_t>(kFirstFort + (kind & 7)));
        setU8(kSietches, static_cast<std::uint8_t>(u8(kSietches) - 1));
        const std::uint16_t name = static_cast<std::uint16_t>(((((place - kPlaces) / kPlaceSize) + 1) << 8) | 0x80);
        for (int i = 0; i < 9; ++i) {
            const std::size_t r = kNamed + 16 * static_cast<std::size_t>(i);
            if (u16(r + 2) == name) {
                setU16(r, static_cast<std::uint16_t>((u8(place + 8) << 8) | 3));
            }
        }
    }
    troopsAt(place, [&](int n) {
        const std::size_t o = troopRecord(n);
        if (u8(o + 0x10) & kHarkonnen) {
            setU8(o + 0x10, static_cast<std::uint8_t>(u8(o + 0x10) | kStill));
        } else {
            setU8(o + 3, static_cast<std::uint8_t>(u8(o + 3) | kOwnFremen));
        }
    });
    markAround(place, 5, 0x30, map);
    setU8(place + 0x0A, static_cast<std::uint8_t>(u8(place + 0x0A) & 0xF6));
    if (placeChanged) {
        placeChanged(place);
    }
}

// The men of each side against what they lost: the Harkonnens' men
// against those they lost, and Paul's fighting men against theirs; the
// difference, as a share of the larger, halved about 0x80.
int World::battleBalance(std::size_t place) const {
    int ours = 0;
    int theirs = 0;
    int ourLost = 0;
    int theirLost = 0;
    for (int troop = u8(place + 9); troop != 0;) {
        const std::size_t at = troopRecord(troop);
        const int men = u8(at + 0x1A);
        if (u8(at + 0x10) & kHarkonnen) {
            theirs += men;
        } else if (u8(at + 3) == kFighting) {
            ours += men;
            theirLost += u16(at + 0x0C);
            ourLost += u16(at + 0x0E);
        }
        troop = u8(at + 1);
    }
    const int ourShare = ours + ourLost == 0 ? 0 : ours * 256 / (ours + ourLost);
    const int theirShare = theirs + theirLost == 0 ? 0 : theirs * 256 / (theirs + theirLost);
    const int most = std::max(ourShare, theirShare);
    int v = ourShare - theirShare;
    if (most != 0) {
        const std::int32_t num = static_cast<std::int32_t>(static_cast<std::int16_t>(v)) * 256 + (v < 0 ? 255 : 0);
        v = static_cast<std::int16_t>(num / most);
    }
    return static_cast<std::uint8_t>((static_cast<std::int16_t>(v) >> 1) + 0x80);
}

} // namespace mentat
