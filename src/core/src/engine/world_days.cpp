// SPDX-License-Identifier: GPL-3.0-or-later
//
// The rest of each hour, and each new day: the accounts of the spice and
// the land, the places named by the story's records, the water of the
// wind-traps and the green fading, and what certain hours bring. See
// docs/rules/days.md.
#include "mentat/data/program_file.hpp"
#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/world.hpp"

#include <algorithm>

namespace mentat {

namespace {

constexpr std::size_t kClockWord = 0x02;
constexpr std::size_t kLastClock = 0x1174;
constexpr std::size_t kProgress = 0x2A;
constexpr std::size_t kStepSeen = 0xFE;
constexpr std::size_t kDaysAtStep = 0xFF;
constexpr std::size_t kStoryEnd = 0xC2;
constexpr std::size_t kSpice = 0xA0;
constexpr std::size_t kNamed = 0xFD8;
constexpr int kNamedCount = 12;
// The table of how long each kind of place keeps a record's count.
constexpr std::size_t kKeepTable = 0x1D35;
constexpr std::size_t kKeepKinds = 0x31;
// Paul's place at war, and how its fight leans.
constexpr std::size_t kAtWar = 0x2B;
constexpr std::size_t kBalance = 0xFD;
// Why the game ends: Paul caught by the Harkonnens, his place lost.
constexpr int kCaught = 4;
constexpr int kPlaceLost = 6;

std::uint16_t rol16(std::uint16_t v) { return static_cast<std::uint16_t>((v << 1) | (v >> 15)); }

} // namespace

void World::loadDayTables(const ProgramFile& program) {
    keep_.assign(kKeepKinds, 0);
    for (std::size_t i = 0; i < kKeepKinds; ++i) {
        keep_[i] = program.codeU8(kKeepTable + i);
    }
    constexpr std::size_t kWarnings = 0x2161;
    warnings_.assign(16, 0);
    for (std::size_t i = 0; i < warnings_.size(); ++i) {
        warnings_[i] = program.codeU8(kWarnings + i);
    }
}

void World::hourPassed(int except, FlatMap& map) {
    const std::uint16_t now = u16(kClockWord);
    const std::uint16_t last = u16(kLastClock);
    setU16(kLastClock, now);
    newDay_ = ((last & 0xF0) - (now & 0xF0)) != 0;
    if (newDay_) {
        dayPassed(map);
    }
    if (u8(kStoryEnd) < 7) {
        gatheringEnds();
        const std::uint16_t learned = u16(0x11CE);
        const std::uint16_t talked = u16(0x11F7);
        troopsWork(except, map);
        waterAndFading(map);
        switch (now & 0x0F) {
        case 3: emperorsDemand(); break;
        case 4: harkonnensAttack(map); break;
        case 8: remindOfTheHarkonnens(); break;
        case 15: harkonnensGrow(); break;
        default: break;
        }
        learnPlace(learned);
        setU16(0x11F7, talked);
        paulsPlace();
    }
}

// The place Paul is at, while it is at war: the fight there goes on while
// the Harkonnens are known to be inside, or while Paul's troops fight
// them there; when they are left alone with Paul, he is caught.
void World::paulsPlace() {
    if (u8(kAtWar) == 0) {
        return;
    }
    const std::size_t at = u16(0x114E);
    setU8(kBalance, 0);
    setU8(kAtWar, 0);
    bool goesOn = (u8(at + 0x0A) & 0x02) != 0;
    if (!goesOn && !(u8(at + 8) >= 0x28 && (u8(at + 0x0A) & 0x08))) {
        int harkonnens = 0;
        int pauls = 0;
        countFighters(at, harkonnens, pauls);
        if (pauls != 0) {
            goesOn = true;
        } else if (harkonnens != 0) {
            gameOverReason = kCaught;
        }
    }
    if (goesOn) {
        setU8(kAtWar, 1);
        setU8(kBalance, static_cast<std::uint8_t>(battleBalance(at) | 1));
        const int kind = u8(at + 8);
        int music = 0x2F;
        if (kind >= 0x20) {
            music = kind != 0x30 && kind >= 0x28 ? 0x33 : 0x30;
        }
        setU8(0x11DD, static_cast<std::uint8_t>(music));
    }
    if (gameOverReason != 0) {
        gameOverReason = kPlaceLost;
    }
    if (u8(kAtWar) == 0) {
        attackOver = true;
    }
}

// --- A new day

void World::dayPassed(FlatMap& map) {
    const std::uint8_t step = u8(kProgress);
    if (u8(kStepSeen) != step) {
        setU8(kDaysAtStep, 0);
    }
    setU8(kStepSeen, step);
    setU8(kDaysAtStep, static_cast<std::uint8_t>(u8(kDaysAtStep) + 1));
    ageRecords();
    callGathering();
    if (((u8(0xD5) + 1) & 0xFF) >= 2) {
        setU8(0xD5, static_cast<std::uint8_t>(u8(0xD5) + 1));
    }
    // The spice of the day.
    const unsigned extra = u16(0x1172);
    setU16(0x1172, 0);
    const unsigned stock = u16(kSpice);
    const unsigned before = u16(0x1170);
    setU16(0x1170, static_cast<std::uint16_t>(stock));
    const unsigned total = (extra + stock) & 0xFFFF;
    const unsigned made = total >= before ? total - before : 0;
    setU16(0xA6, static_cast<std::uint16_t>(made));
    const unsigned yesterday = u16(0xAE);
    setU16(0xAE, static_cast<std::uint16_t>(made));
    if (yesterday >= made) {
        setU16(0xB2, static_cast<std::uint16_t>(yesterday - made));
        setU16(0xB0, 0);
    } else {
        setU16(0xB2, 0);
        setU16(0xB0, static_cast<std::uint16_t>(made - yesterday));
    }
    countHarkonnenForts();
    takeStock(map);
    keepResults();
    smugglersChange();
}

// The records at 0xFD8 that name a place count days there; a count past
// what the place's kind keeps, or a place whose kind changed, starts again
// from 1.
void World::ageRecords() {
    for (int i = 0; i < kNamedCount; ++i) {
        const std::size_t r = kNamed + 16 * static_cast<std::size_t>(i);
        const std::uint16_t name = u16(r + 2);
        if ((name & 0xFF) != 0x80 || (name >> 8) == 0xFF) {
            continue;
        }
        const std::size_t place = 0xE4 + kPlaceSize * static_cast<std::size_t>(name >> 8);
        const std::uint16_t word = u16(r);
        const int kind = u8(place + 8);
        const bool changed = (word >> 8) != kind;
        const int limit = static_cast<std::size_t>(kind) < keep_.size() ? keep_[static_cast<std::size_t>(kind)] : 0;
        if (changed || (word & 0xFF) > limit) {
            setU16(r, static_cast<std::uint16_t>((kind << 8) | 1));
        }
    }
}

// At step 0x5C of the story, once the day at 0x1156 has come: the sietch
// with the most of Paul's troops not at ecology gathers them.
void World::callGathering() {
    const int day = u16(kClockWord) >> 4;
    if (day < u16(0x1156) || u8(kProgress) != 0x5C || u16(kPlaceRecord) == 0x7C8) {
        return;
    }
    int most = 0;
    std::size_t where = 0;
    for (std::size_t r = kPlaces; u8(r) != 0xFF; r += kPlaceSize) {
        if (u8(r + 8) >= 0x28 || (u8(r + 0x0A) & 0x80) || r == 0x2C0) {
            continue;
        }
        int count = 0;
        troopsAt(r, [&](int n) {
            const std::size_t o = troopRecord(n);
            if (u8(o + 3) < 0x80 && u8(o + 3) < 8) {
                ++count;
            }
        });
        if (count > most) {
            most = count;
            where = r;
        }
    }
    if (most == 0) {
        return;
    }
    setU16(0x11DB, static_cast<std::uint16_t>(where));
    setU8(0xF8, static_cast<std::uint8_t>(u8(0xF8) + 1));
    troopsAt(where, [&](int n) {
        const std::size_t o = troopRecord(n);
        if (u8(o + 3) >= 0x80) {
            return;
        }
        setU16(o + 0x12, static_cast<std::uint16_t>(u16(o + 0x12) | 0x400));
        setU8(o + 3, static_cast<std::uint8_t>(u8(o + 3) | 0x10));
    });
    tell(0x0F08, where);
}

// Whether any of Paul's troops at the place gathers (word 0x12 bit 10).
bool World::gathersAt(std::size_t place) {
    bool any = false;
    for (int n = u8(place + 9); n != 0 && !any;) {
        const std::size_t o = troopRecord(n);
        if (u16(o + 0x12) & 0x400) {
            any = true;
        }
        n = u8(o + 1);
    }
    return any;
}

// Each hour at step 0x5D, while the record at 0x1048 names a place where
// troops gather: after 32 hours they are done.
void World::gatheringEnds() {
    if (u16(0x12) & 0x80) {
        return;
    }
    constexpr std::size_t r = 0x1048;
    if (u8(kProgress) != 0x5D || u8(r + 0x0E) != 7 || (u16(r + 2) & 0xFF) != 0x80) {
        return;
    }
    setU8(r, 2);
    const std::size_t place = 0xE4 + kPlaceSize * static_cast<std::size_t>(u16(r + 2) >> 8);
    if (u8(place + 9) == 0 || !gathersAt(place)) {
        return;
    }
    setU8(0xF9, static_cast<std::uint8_t>(u8(0xF9) + 8));
    if (u8(0xF9) != 0) {
        return;
    }
    troopsAt(place, [&](int n) {
        const std::size_t o = troopRecord(n);
        setU16(o + 0x12, static_cast<std::uint16_t>((u16(o + 0x12) & ~0x400) | 0x800));
    });
    tell(0x0709, place);
    setU8(0xF8, static_cast<std::uint8_t>(u8(0xF8) - 1));
    std::size_t next = 0;
    for (std::size_t p = kPlaces; u8(p) != 0xFF; p += kPlaceSize) {
        if (u8(p + 9) != 0 && gathersAt(p)) {
            next = p;
            break;
        }
    }
    setU16(0x11DB, static_cast<std::uint16_t>(next));
    if (next == 0) {
        // The story moves on to step 0x60, aimed at the Harkonnens' palace.
        setU8(kDaysAtStep, 0);
        setU8(kProgress, 0x60);
        setU16(0x1048, static_cast<std::uint16_t>((u8(0x11C + 8) << 8) | 2));
        setU16(0x104A, static_cast<std::uint16_t>((((0x11C - kPlaces) / kPlaceSize + 1) << 8) | 0x80));
    }
}

// The accounts of the land and the men: how much of the map is Paul's and
// the Harkonnens', the most spice made in a day, and the men on each side.
void World::takeStock(FlatMap& map) {
    unsigned marked = 0;
    unsigned ours = 0;
    for (const std::uint8_t b : map.bytes()) {
        const int m = b & 0x30;
        if (m == 0) {
            continue;
        }
        ++marked;
        if (m != 0x30) {
            ++ours;
        }
    }
    const unsigned theirs = marked - ours;
    const unsigned size = static_cast<unsigned>(map.bytes().size()) - 0x188 + 1;
    auto percent = [&](unsigned count) {
        const unsigned share = ((count << 16) / size) & 0xFFFF;
        const unsigned long scaled = static_cast<unsigned long>(share) * 100;
        unsigned high = static_cast<unsigned>(scaled >> 16);
        if (scaled & 0x8000) {
            ++high;
        }
        return high;
    };
    setU16(0xA4, static_cast<std::uint16_t>(percent(theirs)));
    setU16(0xA2, static_cast<std::uint16_t>(percent(ours) + 1));
    const int stock = u16(kSpice) + u16(0x1172) - u16(0x1170);
    const unsigned made = stock < 0 ? 0 : static_cast<unsigned>(stock) & 0xFFFF;
    if (made >= u16(0xA6)) {
        setU16(0xA6, static_cast<std::uint16_t>(made));
    }
    unsigned paul = 0;
    unsigned harkonnen = 0;
    for (std::size_t t = kTroops; u8(t) != 0; t += kTroopSize) {
        if (u8(t + 3) & 0x20) {
            continue;
        }
        if (u8(t + 0x10) & 0x80) {
            harkonnen += u8(t + 0x1A);
        } else if (!(u8(t + 3) & 0x80)) {
            paul += u8(t + 0x1A);
        }
    }
    setU16(0xAA, static_cast<std::uint16_t>(paul));
    setU16(0xAC, static_cast<std::uint16_t>(harkonnen));
}

// The results kept for the day: six numbers at 0x115E and whether each
// rose (1), fell (2) or stayed (3) at 0x116A.
void World::keepResults() {
    const std::size_t from[] = {0xA4, 0xA2, 0xA8, 0xA6, 0xAC, 0xAA};
    for (std::size_t i = 0; i < 6; ++i) {
        const std::uint16_t now = u16(from[i]);
        const std::uint16_t before = u16(0x115E + 2 * i);
        setU16(0x115E + 2 * i, now);
        if (now != before) {
            setU8(0x116A + i, now < before ? 2 : 1);
        } else if (u16(0x115C) != (u16(kClockWord) & 0xFFF0)) {
            setU8(0x116A + i, 3);
        }
    }
}

// The smugglers' wares not on offer may come back.
void World::smugglersChange() {
    unsigned bits = static_cast<unsigned>(random2 ? random2() : 0);
    for (std::size_t s = 0x10D8; s + 0x11 <= kSize && u8(s) < 0x14; s += 0x11) {
        if (!(u8(s + 2) & 8)) {
            continue;
        }
        for (int k = 4; k >= 0; --k) {
            const std::size_t at = s + static_cast<std::size_t>(k);
            if (u8(at + 4) == 0 && (u8(at + 9) & 0x80)) {
                bits = rol16(rol16(static_cast<std::uint16_t>(bits)));
                setU8(at + 4, static_cast<std::uint8_t>(bits & 3));
            }
        }
    }
}

// --- Each hour

// On a new day the wind-traps gather water by the green around them; and
// the green fades a little each day.
void World::waterAndFading(FlatMap& map) {
    if (!newDay_) {
        return;
    }
    auto& bytes = map.bytes();
    for (std::size_t r = kPlaces; u16(r) != 0xFFFF; r += kPlaceSize) {
        if (!(u8(r + 0x0A) & 0x20) || u8(r + 0x1B) >= 0xFA) {
            continue;
        }
        int green = 0;
        const std::size_t at = u16(r + 6);
        for (std::size_t k = 0; k < 6; ++k) {
            const std::size_t i = at - 1 + k;
            if (i < bytes.size() && (bytes[i] & 0x30) == 0x10) {
                ++green;
            }
        }
        setU8(r + 0x1B, static_cast<std::uint8_t>(std::min(u8(r + 0x1B) + (green >> 1) + 1, 0xFA)));
    }
    int faded = 0;
    for (int n = 0; n < 0x46; ++n) {
        const bool low = (fade_ & 1) != 0;
        fade_ = static_cast<std::uint16_t>(fade_ >> 1);
        if (low) {
            fade_ = static_cast<std::uint16_t>(fade_ ^ 0x402);
        }
        for (std::size_t i = fade_; i < 0xC5F9 && i < bytes.size(); i += 0x7FF) {
            if ((bytes[i] & 0x30) == 0x10) {
                bytes[i] = static_cast<std::uint8_t>((bytes[i] & 0xCF) | 0x20);
                ++faded;
            }
        }
    }
    if (faded != 0 && (mapMode & 0x80)) {
        ++mapRedraws;
    }
}

// The message room lists the news from the newest; a word with bit 7 has
// been seen.
std::vector<int> World::newsShown(bool seen) const {
    constexpr std::size_t kNews = 0x1179;
    std::vector<int> shown;
    for (int i = u8(0xC8) - 1; i >= 0; --i) {
        const bool wasSeen = (u8(kNews + 2 * static_cast<std::size_t>(i)) & 0x80) != 0;
        if (wasSeen == seen) {
            shown.push_back(i);
        }
    }
    return shown;
}

std::uint16_t World::lookAtNews(int at, bool seen) {
    constexpr std::size_t kNews = 0x1179;
    const std::size_t e = kNews + 2 * static_cast<std::size_t>(at);
    const std::uint16_t news = u16(e);
    const int about = news >> 8;
    setU8(0x24, static_cast<std::uint8_t>(about));
    if (!seen) {
        setU8(e, static_cast<std::uint8_t>(u8(e) | 0x80));
        setU8(0xC9, static_cast<std::uint8_t>(u8(0xC9) - 1));
        // The Emperor's second and third warnings.
        if ((news & 0x3F) == 0x0B && static_cast<std::uint8_t>(about - 2) < 2) {
            setU8(0xBF, static_cast<std::uint8_t>(u8(0xBF) | 0x20));
        }
    }
    return news;
}

void World::dropNewsReminders() {
    const int count = u8(kMessageQueue);
    int kept = 0;
    for (int i = 0; i < count; ++i) {
        const std::size_t from = kMessageQueue + 1 + 4 * static_cast<std::size_t>(i);
        if (u8(from) == 1) {
            continue;
        }
        const std::size_t to = kMessageQueue + 1 + 4 * static_cast<std::size_t>(kept);
        const std::uint16_t message = u16(from);
        const std::uint16_t place = u16(from + 2);
        setU16(to, message);
        setU16(to + 2, place);
        ++kept;
    }
    setU8(kMessageQueue, static_cast<std::uint8_t>(kept));
    if (kept != count) {
        const std::size_t after = kMessageQueue + 1 + 4 * static_cast<std::size_t>(kept);
        setU16(after, 0);
        setU16(after + 2, 0);
    }
}

// News for Paul (from the Emperor, the Harkonnens...) waits at 0x1179: a
// count at 0xC8, up to ten words; 0xC9 counts all that came.
void World::sendNews(std::uint16_t news) {
    constexpr std::size_t kNews = 0x1179;
    constexpr int kMost = 10;
    int count = u8(0xC8);
    for (int i = 0; i < count; ++i) {
        if (u16(kNews + 2 * static_cast<std::size_t>(i)) == news) {
            return;
        }
    }
    if (count >= kMost) {
        setU8(0xC8, static_cast<std::uint8_t>(u8(0xC8) - 1));
        for (std::size_t i = 0; i < 18; ++i) {
            setU8(kNews + i, u8(kNews + 2 + i));
        }
        setU16(kNews + 18, 0);
        count = kMost - 1;
    }
    setU16(kNews + 2 * static_cast<std::size_t>(count), news);
    setU8(0xC8, static_cast<std::uint8_t>(u8(0xC8) + 1));
    setU8(0xC9, static_cast<std::uint8_t>(u8(0xC9) + 1));
    if (u8(kProgress) >= 0x38 && u8(0x0B) != 8) {
        tell(0x0201, 0);
    }
}

// The third hour: the Emperor asks for spice, and waits for it.
void World::emperorsDemand() {
    if (!(u8(0xBF) & 0x80)) {
        return;
    }
    const int day = u16(kClockWord) >> 4;
    const int since = (day - u16(0x118D)) & 0xFFFF;
    if (u8(kStoryEnd) != 0) {
        if (since != 0) {
            setU8(0xCF, static_cast<std::uint8_t>(-since));
        }
        return;
    }
    const int mood = u8(0xBE);
    if (u8(0xBF) & 0x10) {
        // A demand waits: each day a warning, and after four the end.
        if (since == 0) {
            return;
        }
        if (since >= 4) {
            gameOverReason = 7;
            return;
        }
        int level = (mood < 1) + (mood < 0x40) + (mood < 0x80) + (mood < 0x90) + (mood < 0xFF);
        if (level >= 3) {
            level = 2;
        }
        const int warning = warnings_.empty() ? 0 : warnings_[static_cast<std::size_t>((since * 4 + level) & 15)];
        if (warning == 0) {
            gameOverReason = 7;
            return;
        }
        sendNews(static_cast<std::uint16_t>((warning << 8) | 0x0B));
        return;
    }
    if (u8(0x11BB) != 0) {
        gameOverReason = 7;
        return;
    }
    if (since != 0) {
        setU8(0xCF, static_cast<std::uint8_t>(-since));
        return;
    }
    newDemand();
}

// The day of a new demand: more each time, by the Emperor's mood.
void World::newDemand() {
    const int mood = u8(0xBE);
    const int times = u8(0xC3);
    setU8(0xC3, static_cast<std::uint8_t>(times + 1));
    unsigned demand = 0xFFFF;
    const unsigned base = static_cast<unsigned>(times) * 0x96 + 0x64;
    if (base <= 0xFFFF) {
        const unsigned long p = static_cast<unsigned long>(dice(0x3F) + 0xE0) * base;
        if ((p >> 24) == 0) {
            demand = static_cast<unsigned>(p >> 8) & 0xFFFF;
            if (!(mood & 0x80)) {
                const unsigned by = 0x100u + (~(static_cast<unsigned>(mood) << 1) & 0xFFu);
                const unsigned long q = static_cast<unsigned long>(by) * demand;
                demand = (q >> 24) != 0 ? 0xFFFF : static_cast<unsigned>(q >> 8) & 0xFFFF;
            }
        }
    }
    setU16(0xBC, static_cast<std::uint16_t>(demand));
    setU8(0xCF, 0);
    setU8(0xBF, static_cast<std::uint8_t>(u8(0xBF) | 0x90));
    sendNews(static_cast<std::uint16_t>((mood & 0x80) ? 0x020B : 0x030B));
}

// The fourth hour, on even days: the Harkonnens may attack the
// northernmost sietch of Paul's that they can reach from a fort of theirs.
void World::harkonnensAttack(FlatMap& map) {
    if (u8(kProgress) < 0x3C) {
        const int since = static_cast<int>(u16(kClockWord)) - static_cast<int>(u16(0x1154));
        if (since < 0x70) {
            return;
        }
    }
    if (u16(kClockWord) & 0x10) {
        return;
    }
    const std::uint8_t fought = u8(0x11BC);
    setU8(0x11BC, 0);
    if (fought != 0 || !turnSeed()) {
        return;
    }
    std::size_t target = 0;
    std::size_t from = 0;
    int north = 0x64;
    for (std::size_t p = kPlaces; u8(p) < 8; p += kPlaceSize) {
        if (u8(p + 8) >= 0x20 || (u8(p + 0x0A) & 0x82)) {
            continue;
        }
        if (north <= static_cast<std::int16_t>(u16(p + 4)) || gathersAt(p)) {
            continue;
        }
        learnPlace(p);
        if (u8(0x60) == u8(0x63)) {
            continue;
        }
        std::size_t fort = u16(0xE4);
        if (u16(0xE2) >= 0x1E) {
            if (u16(0xDC) >= 0x1E) {
                continue;
            }
            fort = u16(0xDE);
        }
        if (fort == 0x11C) {
            continue;
        }
        int harkonnens = 0;
        int pauls = 0;
        countFighters(fort, harkonnens, pauls);
        if (harkonnens != 0 && pauls == 0) {
            north = static_cast<std::int16_t>(u16(p + 4));
            target = p;
            from = fort;
        }
    }
    if (target == 0) {
        return;
    }
    setU8(0xC4, static_cast<std::uint8_t>(u8(0xC4) + 1));
    // Two of the fort's Harkonnen troops go there at once.
    int sent = 0;
    for (int n = u8(from + 9); n != 0 && sent < 2;) {
        const std::size_t o = troopRecord(n);
        const int next = u8(o + 1);
        if (u8(o + 0x10) & 0x80) {
            setU8(o + 3, 0x8D);
            startMove(n, target, map);
            setU8(o + 0x10, static_cast<std::uint8_t>(u8(o + 0x10) & ~0x10));
            arrive(n, map);
            ++sent;
            n = u8(from + 9); // the round starts again from the first
            continue;
        }
        n = next;
    }
    setU8(target + 0x0A, static_cast<std::uint8_t>(u8(target + 0x0A) | 0x02));
    troopsAt(target, [&](int n) {
        const std::size_t o = troopRecord(n);
        if (u8(o + 3) >= 0x80 || (u8(o + 3) & 0x20)) {
            return;
        }
        const int job = u8(o + 3) & 0x0F;
        if (job == 1) {
            setU8(o + 3, static_cast<std::uint8_t>(u8(o + 3) | 0x10));
        } else if (job != 6) {
            setOccupation(n, 6);
        }
    });
    const std::uint16_t name = static_cast<std::uint16_t>(((((target - kPlaces) / kPlaceSize) + 1) << 8) | 0x80);
    const std::uint16_t word = static_cast<std::uint16_t>((u8(target + 8) << 8) | 1);
    for (int i = 0; i < 9; ++i) {
        const std::size_t r = kNamed + 16 * static_cast<std::size_t>(i);
        if (u16(r + 2) == name) {
            setU16(r, word);
        }
    }
    tell(u16(troopRecord(3) + 4) == target ? 0x0F0D : 0x0F0C, target);
    if (u16(6) == name) {
        setU16(4, word);
        setU8(0x0B, 1);
        setU8(kAtWar, 1);
        setU8(kBalance, static_cast<std::uint8_t>(battleBalance(target) | 1));
        attackedHere = true;
    } else if (placeChanged && !(u8(target + 0x0A) & 0x80)) {
        placeChanged(target);
    }
}

// The eighth hour: a reminder of the Harkonnens while the story waits.
void World::remindOfTheHarkonnens() {
    if (!(u8(0xBF) & 0x10) || (u16(0x10) & 8) || u8(0x0B) == 8 || u8(kStoryEnd) != 0) {
        return;
    }
    tell(0x030B, 0);
}

// The last hour of the day: half the time the Harkonnen troops gain a man,
// up to 200.
void World::harkonnensGrow() {
    if (!turnSeed()) {
        return;
    }
    for (std::size_t t = kTroops; t < 0xFA0; t += kTroopSize) {
        if (!(u8(t + 0x10) & 0x80)) {
            continue;
        }
        if (((u8(t + 0x1A) - 1) & 0xFF) < 0xC7) {
            setU8(t + 0x1A, static_cast<std::uint8_t>(u8(t + 0x1A) + 1));
        }
    }
}

} // namespace mentat
