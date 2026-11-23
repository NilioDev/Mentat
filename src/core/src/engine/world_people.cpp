// SPDX-License-Identifier: GPL-3.0-or-later
//
// The people of the place the player is in, and what the state keeps of
// that place. See docs/rules/people.md.
#include "mentat/engine/world.hpp"

#include <algorithm>
#include <cstdlib>

namespace mentat {

namespace {

// State.
constexpr std::size_t kSeed = 0x00;
constexpr std::size_t kClockWord = 0x02;
constexpr std::size_t kRoomWord = 0x04;
constexpr std::size_t kPlaceWord = 0x06;
constexpr std::size_t kProgress = 0x2A;
constexpr std::size_t kFirstRoomFight = 0x2B; // troops stand in the first room
constexpr std::size_t kBoost = 0xFA;
constexpr std::size_t kPlaceRecordWord = 0x114E;
constexpr std::size_t kHomeWord = 0x1150;
constexpr std::size_t kSmugglers = 0x10D8; // records of 0x11 bytes, by the name of their village
constexpr std::size_t kSmugglerSize = 0x11;
constexpr std::size_t kSmugglerNow = 0x10B4;
constexpr std::size_t kNameKinds = 0x1141; // a byte for each second word of a place's name
constexpr std::size_t kLearned = 0x11CE;
constexpr std::size_t kRallied = 0x28;
constexpr std::size_t kCharisma = 0x29;
constexpr std::size_t kRallyGoal = 0x1178;

// Characters 12 to 15 and their records.
constexpr std::size_t kCharacterTable = 0xFD8;
constexpr std::size_t kCharacterSize = 0x10;
constexpr int kCaptain = 12;
constexpr int kSmuggler = 13;
constexpr int kFremen = 14;
constexpr int kChief = 15;
constexpr std::uint16_t kNowhere = 0x7F80;
constexpr int kProspectors = 3; // the troop whose chief is the prospectors'
constexpr int kMaxChiefs = 8;

// Troops.
constexpr std::uint8_t kMoving = 0x20;  // byte 3
constexpr std::uint8_t kNotPauls = 0x80; // byte 3
constexpr std::uint8_t kHarkonnen = 0x80; // byte 0x10
constexpr int kVillage = 0x21;
constexpr int kFort = 0x28;

std::size_t characterRecord(int c) { return kCharacterTable + kCharacterSize * static_cast<std::size_t>(c); }

} // namespace

World::People World::meetPeople() {
    People people;
    for (int c = kCaptain; c <= kChief; ++c) {
        setU16(characterRecord(c) + 2, kNowhere);
    }
    const int roomWord = u16(kRoomWord);
    const int placeWord = u16(kPlaceWord);
    if ((placeWord & 0xFF) != 0x80) {
        return people;
    }
    const std::size_t here = u16(kPlaceRecordWord);
    const int kind = u8(here + 8);
    auto put = [&](int c) {
        setU16(characterRecord(c), static_cast<std::uint16_t>(roomWord));
        setU16(characterRecord(c) + 2, static_cast<std::uint16_t>(placeWord));
    };
    int count = 0;
    for (int troop = u8(here + 9); troop != 0;) {
        const std::size_t t = troopRecord(troop);
        const int next = u8(t + 1);
        const int job = u8(t + 3);
        const bool harkonnen = (u8(t + 0x10) & kHarkonnen) != 0;
        // The room the troop stands in.
        int room = 2;
        bool skip = false;
        if (job & kMoving) {
            room = kind < kFort ? 2 : 3;
        } else if (harkonnen) {
            skip = true;
        } else if (u8(kFirstRoomFight) != 0) {
            room = 1;
        }
        if (!skip && room == (roomWord & 0xFF)) {
            if (harkonnen) {
                const std::size_t r = characterRecord(kCaptain);
                setU8(r + 0x0F, static_cast<std::uint8_t>((u8(r + 0x0F) & ~0x10) | (job & 0x10)));
                setU16(0xEE, 0);
                setU8(0xED, (job & 0x10) ? 0xFF : u8(t + 0x15));
                people.harkonnens = troop;
                put(kCaptain);
            } else if (job < kNotPauls) {
                const int index = count & (kMaxChiefs - 1);
                count = index + 1;
                if (troop == kProspectors) {
                    people.prospector = index + 1;
                }
                if (people.chiefs.size() <= static_cast<std::size_t>(index)) {
                    people.chiefs.resize(static_cast<std::size_t>(index) + 1);
                }
                people.chiefs[static_cast<std::size_t>(index)] = troop;
                put(kChief);
            } else {
                people.fremen = troop;
                put(kFremen);
            }
        }
        troop = next;
    }
    if (kind == kVillage) {
        put(kSmuggler);
        // The smuggler of this village.
        const int name = u8(here);
        std::size_t s = kSmugglers;
        while (s + kSmugglerSize <= kSize && u8(s) != name) {
            s += kSmugglerSize;
        }
        if (s + kSmugglerSize <= kSize) {
            setU16(kSmugglerNow, static_cast<std::uint16_t>(s));
            setU8(0x1C, u8(s + 2));
            setU16(0x20, u16(s + 0x0E));
            setU8(0x1F, 0);
            const int days = u16(kClockWord) >> 4;
            if (u16(s + 0x0E) != 0) {
                setU8(0x1F, static_cast<std::uint8_t>(days - u8(s + 0x10)));
            }
            setU8(0x1D, u8(s + 1));
            std::uint8_t since = static_cast<std::uint8_t>(days - u8(s + 3));
            if (!(u8(s + 2) & 0x08)) {
                since = 1;
                setU8(s + 2, static_cast<std::uint8_t>(u8(s + 2) | 0x08));
            }
            setU8(0x1E, since);
            setU8(0x9D, 0);
            int n = u16(kSeed) & 7;
            const int kinds = u8(kNameKinds);
            while (kinds > 0 && n >= kinds) {
                n -= kinds;
            }
            setU16(0x11F1, static_cast<std::uint16_t>(n + 0xE8));
            setU8(0x9F, 0);
        }
    }
    learnPlace(here);
    return people;
}

namespace {

// How keen a troop is, 0 to 100.
int keenness(const World& w, std::size_t t) {
    const int job = w.u8(t + 3) & 0x0F;
    int keen = w.u8(t + 0x15);
    if (w.u8(kBoost) != 0) {
        keen = (keen + 0x14) & 0xFF;
    }
    bool cap = true;
    if (job == 6) {
        if (w.u16(t + 4) == w.u16(kPlaceRecordWord)) {
            keen = (keen + 0x1E) & 0xFF;
        } else {
            cap = false;
        }
    } else if ((job & 0x0E) == 8) {
        keen = 0x64;
        cap = false;
    }
    if (cap && keen >= 0x64) {
        keen = 0x64;
    }
    const int progress = w.u8(kProgress);
    if (progress >= 0x64 && progress < 0x68) {
        keen = static_cast<std::int8_t>(static_cast<std::uint8_t>(keen - 0x28));
        if (keen < 0x0A) {
            keen = 0x0A;
        }
    }
    return keen & 0xFF;
}

// What a troop is worth in a fight.
int strength(const World& w, std::size_t t) {
    int a = (2 * keenness(w, t)) & 0xFF;
    a += w.u8(t + 0x17);
    if (a > 0xFF) {
        a = 0xFF;
    }
    const int men = w.u8(t + 0x1A);
    unsigned value = static_cast<unsigned>(a * men) >> 4;
    unsigned step = (value << 1) & 0xFFFF;
    const int arms = w.u8(t + 0x19);
    for (int bit = 5; bit >= 2; --bit) {
        if (arms & (1 << bit)) {
            value += step;
            if (value > 0xFFFF) {
                value = 0xFFFF;
                break;
            }
        }
        step = (step << 1) & 0xFFFF;
    }
    int result = static_cast<int>(value >> 8);
    if (result == 0 && men >= 1) {
        result = 1;
    }
    return result;
}

// The direction, 0 to 7, from one point of the map to another.
int direction(int fromLongitude, int fromLatitude, int toLongitude, int toLatitude) {
    int down = static_cast<std::int16_t>(static_cast<std::uint16_t>(toLatitude - fromLatitude));
    int across = static_cast<std::int16_t>(static_cast<std::uint16_t>(toLongitude - fromLongitude));
    if (down < -0x80 || down >= 0x80) {
        down >>= 1;
        across >>= 1;
    }
    const int scaled = static_cast<std::int16_t>(static_cast<std::uint16_t>((down & 0xFF) << 8));
    int angle = 0;
    if (std::abs(across) >= std::abs(scaled)) {
        if (across != 0) {
            angle = (0x20 * scaled / across) & 0xFF;
            angle = (angle + (across >= 0 ? 0x40 : 0xC0)) & 0xFF;
        }
    } else if (scaled != 0) {
        angle = (0x20 * across / scaled) & 0xFF;
        if (scaled >= 0) {
            angle = (angle - 0x80) & 0xFF;
        }
        angle = (-angle) & 0xFF;
    }
    return ((angle + 0x10) & 0xFF) >> 5;
}

} // namespace

void World::learnPlace(std::size_t record) {
    setU16(kLearned, static_cast<std::uint16_t>(record));
    setU8(0x4E, u8(record + 1));
    setU8(0x4F, u8(record));
    setU8(0x50, u8(kNameKinds + u8(record + 1)));
    setU8(0x51, u8(record + 0x0A));
    setU8(0x52, u8(record + 0x12));
    setU8(0x54, u8(record + 0x1B));
    setU8(0x4D, u8(record + 8));
    for (std::size_t i = 0; i < 7; ++i) {
        setU8(0x55 + i, u8(record + 0x14 + i));
    }

    // The fight: what each side's troops here are worth, and how it leans.
    unsigned theirs = 0;
    unsigned ours = 0;
    std::uint16_t ourJobs = 0;
    std::uint16_t ourGoods = 0;
    for (int troop = u8(record + 9); troop != 0;) {
        const std::size_t t = troopRecord(troop);
        if (!(u8(t + 3) & kMoving)) {
            const int s = strength(*this, t);
            if (u8(t + 0x10) & kHarkonnen) {
                theirs = (theirs + static_cast<unsigned>(s)) & 0xFFFF;
            } else {
                ours = (ours + static_cast<unsigned>(s)) & 0xFFFF;
                ourJobs = static_cast<std::uint16_t>(ourJobs | u16(t + 0x10));
                ourGoods = static_cast<std::uint16_t>(ourGoods | u16(t + 0x12));
            }
        }
        troop = u8(t + 1);
    }
    setU16(0x94, static_cast<std::uint16_t>(theirs));
    setU16(0x96, static_cast<std::uint16_t>(ours));
    setU16(0x5C, ourJobs);
    setU16(0x5E, ourGoods);
    {
        const bool ahead = ours >= theirs;
        const unsigned big = ahead ? ours : theirs;
        const unsigned small = ahead ? theirs : ours;
        unsigned lean = 0xFC;
        if (small != 0 && (big >> 8) < small) {
            lean = std::min((big * 256 / small) >> 1, 0xFCu);
        }
        if (!ahead) {
            lean = (0x100u - lean) & 0xFFu;
        }
        setU8(0x9C, static_cast<std::uint8_t>(lean));
    }

    // The troops here and those on their way here, counted by side and job.
    for (std::size_t at = 0x60; at < 0x93; ++at) {
        setU8(at, 0);
    }
    auto count = [&](std::size_t t) {
        const int job = u8(t + 3);
        if (job & kMoving) {
            return;
        }
        const std::size_t base = (job & 0x40) ? 0x7F : 0x61;
        std::size_t side = base;
        if (!(u8(t + 0x10) & kHarkonnen)) {
            --side;
            if (job == 0x80) {
                setU8(0x90, static_cast<std::uint8_t>(u8(0x90) + 1));
                return;
            }
        }
        setU8(side, static_cast<std::uint8_t>(u8(side) + 1));
        int kindOfJob = job & 0x0F;
        if ((job & 3) == 3) {
            kindOfJob &= 0xFC;
        }
        const std::size_t at = base + static_cast<std::size_t>(kindOfJob);
        setU8(at + 1, static_cast<std::uint8_t>(u8(at + 1) + 1));
        if (at < 0x7F) {
            const std::size_t goods = 0x71 + (u16(t + 0x12) & 0x0F);
            setU8(goods, static_cast<std::uint8_t>(u8(goods) + 1));
        }
    };
    for (int troop = u8(record + 9); troop != 0;) {
        const std::size_t t = troopRecord(troop);
        count(t);
        troop = u8(t + 1);
    }
    for (std::size_t t = kTroops; t < 0xFBB; t += kTroopSize) {
        const int job = u8(t + 3);
        if (!(job & 0x40)) {
            continue;
        }
        const std::size_t to = (job & 3) == 3 ? u16(t + 0x0C) : u16(t + 4);
        if (to == record) {
            count(t);
        }
    }
    setU8(0x91, static_cast<std::uint8_t>(u8(0x60) + u8(0x7E)));
    setU8(0x92, static_cast<std::uint8_t>(u8(0x61) + u8(0x7F)));

    // The equipment here that no troop has taken.
    {
        std::uint8_t left[7];
        for (std::size_t i = 0; i < 7; ++i) {
            left[i] = u8(record + 0x14 + i);
        }
        for (int troop = u8(record + 9); troop != 0;) {
            const std::size_t t = troopRecord(troop);
            const int arms = u8(t + 0x19);
            for (int i = 0; i < 7; ++i) {
                if ((arms & (0x80 >> i)) && left[i] > 0) {
                    --left[i];
                }
            }
            troop = u8(t + 1);
        }
        std::uint8_t mask = 0;
        for (int i = 0; i < 7; ++i) {
            if (left[i] != 0) {
                mask = static_cast<std::uint8_t>(mask | (0x80 >> i));
            }
        }
        setU8(0x53, mask);
    }

    // Characters 4, 5 and 7 staying here, away from home.
    setU8(0xF7, 0);
    if (record != u16(kHomeWord)) {
        const int placeWord = ((static_cast<int>((record - kPlaces) / kPlaceSize) + 1) << 8) | 0x80;
        for (const int c : {4, 5, 7}) {
            const std::size_t r = characterRecord(c);
            if (u16(r + 2) == placeWord) {
                setU8(0xF7, static_cast<std::uint8_t>(u8(0xF7) | (1 << (u8(r + 0x0E) & 7))));
            }
        }
    }

    // The nearest places of each sort, and which way they lie.
    const int lon = u16(record + 2);
    const int lat = u16(record + 4);
    struct Nearest {
        std::size_t distance;
        std::size_t place;
    };
    const std::size_t slots[] = {0xCA, 0xD0, 0xD6, 0xDC, 0xE2};
    for (const std::size_t s : slots) {
        setU16(s, 0xFFFF);
    }
    for (std::size_t p = kPlaces; u16(p) != 0xFFFF; p += kPlaceSize) {
        if (p == record) {
            continue;
        }
        int across = std::abs(static_cast<int>(static_cast<std::int16_t>(static_cast<std::uint16_t>(u16(p + 2) - lon))));
        int down = std::abs(static_cast<int>(static_cast<std::int16_t>(static_cast<std::uint16_t>(u16(p + 4) - lat))));
        // The original compares the high byte of one with the low byte of
        // the other.
        int distance = (across >> 8) & 0xFF;
        if (distance < (down & 0xFF)) {
            distance = down & 0xFFFF;
        }
        std::size_t slot = 0;
        const bool hidden = (u8(p + 0x0A) & 0x80) != 0;
        if (u8(p + 8) >= kFort) {
            slot = hidden ? 0xE2 : 0xDC;
        } else if (!hidden) {
            slot = 0xD0;
        } else {
            if (u8(kProgress) < u8(p + 0x0B)) {
                continue;
            }
            slot = 0xD6;
        }
        if (static_cast<unsigned>(distance) < u16(slot)) {
            setU16(slot, static_cast<std::uint16_t>(distance));
            setU16(slot + 2, static_cast<std::uint16_t>(p));
        }
        if (static_cast<unsigned>(distance) < u16(0xCA)) {
            setU16(0xCA, static_cast<std::uint16_t>(distance));
            setU16(0xCC, static_cast<std::uint16_t>(p));
        }
    }
    auto way = [&](std::size_t slot) {
        const std::size_t p = u16(slot);
        int d = 0;
        if (p + 6 <= kSize) {
            d = direction(lon, lat, u16(p + 2), u16(p + 4));
        }
        setU8(slot + 2, static_cast<std::uint8_t>(d));
        return d;
    };
    way(0xDE);
    way(0xE4);
    setU16(0x11FD, static_cast<std::uint16_t>(way(0xD8) + 0xDA));
    way(0xCC);
    way(0xD2);
}

// How much spice a troop that harvests brings in, kept at its word 0x0C;
// bits 2 and 3 of its word 0x10 tell whether that went down or up.
int World::harvest(std::size_t t) {
    const std::size_t place = u16(t + 4);
    int a = (keenness(*this, t) + (u8(t + 0x16) & 0xF0)) & 0xFF;
    unsigned product = static_cast<unsigned>(a * u8(t + 0x1A));
    if (!(u8(t + 0x19) & 0x80)) {
        product >>= 2;
    }
    const unsigned high = (product >> 8) & 0xFF;
    const unsigned field = (u8(place + 0x12) & 0xF0) + 1u;
    const unsigned rate = ((field * high) >> 7) & 0x1FF;
    const unsigned before = u16(t + 0x0C);
    setU16(t + 0x0C, static_cast<std::uint16_t>(rate));
    if (before != rate) {
        const std::uint8_t trend = before < rate ? 8 : 4;
        setU8(t + 0x10, static_cast<std::uint8_t>((u8(t + 0x10) & 0xF3) | trend));
    }
    return static_cast<int>(rate);
}

void World::learnTroop(int troop) {
    const std::size_t t = troopRecord(troop);
    const std::size_t place = u16(t + 4);
    const int job = u8(t + 3);
    setU16(0x2C, static_cast<std::uint16_t>(place));
    setU8(0x2E, u8(t));
    setU8(0x30, static_cast<std::uint8_t>(job));
    setU8(0x2F, static_cast<std::uint8_t>(job & 0x0F));
    setU16(0x11F3, static_cast<std::uint16_t>((job & 0x0F) + 0x18));

    // How long it has done what it does.
    const int since = static_cast<std::uint16_t>(u16(kClockWord) - u16(t + 0x0A));
    setU16(0x42, static_cast<std::uint16_t>(since));
    setU8(0x41, static_cast<std::uint8_t>(since >> 4));
    int howLong = 0x74;
    if (!(job & 0x10)) {
        howLong = since < 3 ? 0x70 : since < 0x10 ? 0x71 : since < 0x20 ? 0x72 : 0x73;
    }
    setU16(0x11F5, static_cast<std::uint16_t>(howLong));

    int rate = 0;
    if (job != 0) {
        setU16(t + 0x10, static_cast<std::uint16_t>(u16(t + 0x10) & 0xFFF3));
    } else {
        int share = 0;
        if (since != 0) {
            const unsigned made = u16(t + 0x0E);
            share = static_cast<int>(made / static_cast<unsigned>(since));
            if (2 * (made % static_cast<unsigned>(since)) > static_cast<unsigned>(since)) {
                ++share;
            }
        }
        setU16(0x4A, static_cast<std::uint16_t>(share));
        rate = harvest(t);
    }
    setU16(0x48, static_cast<std::uint16_t>(rate));
    setU16(0x32, u16(t + 0x10));
    setU16(0x34, u16(t + 0x12));
    setU8(0x31, static_cast<std::uint8_t>(u16(t + 0x12) & 0x0F));
    setU16(0x11FF, static_cast<std::uint16_t>(u16(t + 0x12) & 0x0F));
    setU8(0x36, static_cast<std::uint8_t>(keenness(*this, t)));
    const std::size_t skills[] = {0x16, 0x17, 0x18};
    const std::size_t words[] = {0x11F7, 0x11F9, 0x11FB};
    for (int i = 0; i < 3; ++i) {
        const int v = u8(t + skills[i]);
        setU8(0x38 + static_cast<std::size_t>(i), static_cast<std::uint8_t>(v));
        setU16(words[i], static_cast<std::uint16_t>((v >> 4) + 0xD1));
    }
    setU16(0x44, u16(t + 0x0C));
    setU16(0x46, u16(t + 0x0E));
    setU16(0x11F1, static_cast<std::uint16_t>((u16(t + 0x0E) & 0xFF) + 0xE8));
    setU8(0x37, u8(t + 0x16 + static_cast<std::size_t>((job & 0x0F) >> 2)));
    setU8(0x3B, u8(t + 0x19));
    setU8(0x3C, u8(t + 0x1A));
    setU8(0x40, static_cast<std::uint8_t>((u16(kClockWord) >> 4) - u8(t + 0x14)));
    learnPlace(place);
}

bool World::enlist(int troop) {
    const std::size_t t = troopRecord(troop);
    const int job = u8(t + 3);
    if (!(job & kNotPauls) || (u8(t + 0x10) & kHarkonnen)) {
        return false;
    }
    setU8(kRallied, static_cast<std::uint8_t>(u8(kRallied) + 1));
    const bool goal = u8(kRallied) >= u8(kRallyGoal);
    // Paul's charisma grows by one; each time it passes a multiple of 4,
    // every troop of his grows keener.
    const int before = u8(kCharisma);
    const int after = std::min(before + 1, 0xC8);
    setU8(kCharisma, static_cast<std::uint8_t>(after));
    const int steps = (((after & 0xFC) - (before & 0xFC)) & 0xFF) >> 2;
    if (steps != 0) {
        for (std::size_t r = kTroops; r < 0xFBB; r += kTroopSize) {
            if (u8(r + 3) & 0xA0) {
                continue;
            }
            setU8(r + 0x15, static_cast<std::uint8_t>(std::min(u8(r + 0x15) + steps, 0x64)));
        }
    }
    setU8(t + 3, static_cast<std::uint8_t>((job & kMoving) | 2));
    setU16(t + 0x0A, u16(kClockWord));
    setU16(t + 0x0C, 0);
    setU16(t + 0x0E, 0);
    setU8(t + 0x14, static_cast<std::uint8_t>(u16(kClockWord) >> 4));
    const std::size_t place = u16(t + 4);
    if (u8(place + 0x0B) == 0) {
        setU8(place + 0x0B, 2);
        // The land around the place is looked over for spice; the map's
        // marks for that are not built yet.
    }
    return goal;
}

} // namespace mentat
