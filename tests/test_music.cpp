// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/audio/music.hpp"
#include "mentat/audio/music_driver.hpp"
#include "mentat/audio/opl.hpp"
#include "mentat/data/reader.hpp"

#include <memory>
#include <utility>
#include <vector>

using namespace mentat;

namespace {

struct Log : OplSink {
    std::vector<std::pair<std::uint8_t, std::uint8_t>> writes;
    void writeRegister(std::uint8_t reg, std::uint8_t value) override { writes.emplace_back(reg, value); }
    int count(std::uint8_t reg) const {
        int n = 0;
        for (const auto& w : writes) {
            n += w.first == reg ? 1 : 0;
        }
        return n;
    }
    std::uint8_t last(std::uint8_t reg) const {
        std::uint8_t v = 0;
        for (const auto& w : writes) {
            if (w.first == reg) {
                v = w.second;
            }
        }
        return v;
    }
};

MusicTables testTables() {
    MusicTables t;
    for (std::size_t i = 0; i < 12; ++i) {
        t.frequency[i] = static_cast<std::uint16_t>(0x157 + 20 * i);
    }
    const std::uint8_t first[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};
    for (std::size_t i = 0; i < 9; ++i) {
        t.firstOperator[i] = first[i];
        t.secondOperator[i] = static_cast<std::uint8_t>(first[i] + 3);
    }
    t.distance.fill(20);
    return t;
}

// One voice: instrument 0, note on key 60 for 2 steps, note off, end.
std::vector<std::uint8_t> tinySong(std::uint16_t tempo) {
    std::vector<std::uint8_t> v(52, 0);
    const std::vector<std::uint8_t> events{0x00, 0xC0, 0x00, 0x00, 0x90, 60, 100, 0x02, 0x80, 60, 0, 0x00, 0xF0, 0};
    v[2] = 50; // voice 0 at offset 50 from byte 2 = byte 52
    v[44] = 0; // no repeated part
    v[48] = 1;
    v[50] = static_cast<std::uint8_t>(tempo);
    v[51] = static_cast<std::uint8_t>(tempo >> 8);
    v.insert(v.end(), events.begin(), events.end());
    const auto instruments = v.size();
    v[0] = static_cast<std::uint8_t>(instruments);
    std::vector<std::uint8_t> instrument(40, 0);
    instrument[2 + 8] = 10; // first operator level
    instrument[15 + 8] = 20;
    v.insert(v.end(), instrument.begin(), instrument.end());
    return v;
}

} // namespace

MTT_TEST("music: song header and bad files") {
    const auto song = Song::parse(tinySong(0x100));
    MTT_CHECK(song.tempo() == 0x100);
    MTT_CHECK(song.voiceStart(0) == 52);
    MTT_CHECK(song.voiceStart(1) == 0);
    MTT_CHECK(song.instrumentCount() == 1);
    MTT_CHECK_THROWS(song.instrument(1));
    auto bad = tinySong(0x100);
    bad[0] = 0xFF;
    MTT_CHECK_THROWS(Song::parse(bad));
    auto outside = tinySong(0x100);
    outside[2] = 0xF0;
    MTT_CHECK_THROWS(Song::parse(outside));
    MTT_CHECK_THROWS(Song::parse({1, 2, 3}));
}

MTT_TEST("music: notes start and stop on the steps") {
    Log log;
    MusicDriver driver(testTables());
    auto song = std::make_shared<const Song>(Song::parse(tinySong(0x100)));
    driver.start(log, song, 1);
    // The first step runs at once: instrument, then the note with its key bit.
    MTT_CHECK(driver.status().playing());
    MTT_CHECK(log.last(0x40) == 10);
    MTT_CHECK(log.last(0x43) == 20);
    MTT_CHECK((log.last(0xB0) & 0x20) != 0);
    // Key 60 - 24 = 36: octave 3, semitone 0.
    MTT_CHECK(log.last(0xA0) == 0x57);
    MTT_CHECK(log.last(0xB0) == (0x20 | (3 << 2) | 1));
    // Tempo 0x100: a step every tick, but the first step's tempo is added
    // to a counter of 0, so the second step waits a tick longer.
    log.writes.clear();
    driver.tick(log); // tick 1: no step
    MTT_CHECK(log.writes.empty());
    driver.tick(log); // tick 2: step 2
    MTT_CHECK(log.writes.empty());
    driver.tick(log); // tick 3: step 3, note off, and with no wait the end,
                      // which ends the only play (taking every key off again)
    MTT_CHECK(log.count(0xB0) == 2);
    MTT_CHECK((log.last(0xB0) & 0x20) == 0);
    MTT_CHECK(!driver.status().playing());
}

MTT_TEST("music: forever starts again, measures count") {
    Log log;
    MusicDriver driver(testTables());
    auto song = std::make_shared<const Song>(Song::parse(tinySong(0)));
    driver.start(log, song, 0);
    for (int i = 0; i < 400; ++i) {
        driver.tick(log);
    }
    MTT_CHECK(driver.status().playing());
    MTT_CHECK(driver.status().measure == 1); // it starts again every 4 steps
    MTT_CHECK(driver.status().position() >= 16);
}

MTT_TEST("music: volume levels and fades") {
    MTT_CHECK(MusicDriver::mixerLevels(0xFF, 0x78) == 0xFF);
    MTT_CHECK(MusicDriver::mixerLevels(0x80, 0x78) == 0x88);
    MTT_CHECK(MusicDriver::mixerLevels(0, 0x78) == 0);
    MTT_CHECK(MusicDriver::mixerLevels(0xFF, 0) == 0xF0);
    Log log;
    MusicDriver driver(testTables());
    auto song = std::make_shared<const Song>(Song::parse(tinySong(0)));
    driver.start(log, song, 0);
    MTT_CHECK(driver.mixer() == 0xEE);
    driver.fade(10, 0, 0x78);
    MTT_CHECK((driver.status().flags & MusicStatus::kFading) != 0);
    for (int i = 0; i < 20 && driver.status().playing(); ++i) {
        driver.tick(log);
    }
    MTT_CHECK(driver.mixer() == 0);
    MTT_CHECK(!driver.status().playing());
    MTT_CHECK(MusicSystem::gainOf(0xFF) == 1.0);
    MTT_CHECK(MusicSystem::gainOf(0) == 0.0);
}

MTT_TEST("real data: every piece of music plays") {
    auto archive = test::openGameData();
    if (!archive) {
        MTT_SKIP("no game data");
    }
    const auto tables = MusicTables::fromPlayer(archive->load("DNADP.HSQ"));
    MTT_CHECK(tables.frequency[0] == 0x157);
    for (const char* name : {"SEKENCE.HSQ", "WATER.HSQ", "WORMSUIT.HSQ", "WORMINTR.HSQ", "WARSONG.HSQ", "MORNING.HSQ",
                             "SIETCHM.HSQ", "BAGDAD.HSQ", "ARRAKIS.HSQ", "CRYOMUS.HSQ"}) {
        Log log;
        MusicDriver driver(tables);
        auto song = std::make_shared<const Song>(Song::parse(archive->load(name)));
        driver.start(log, song, 0);
        for (int i = 0; i < 20000; ++i) {
            driver.tick(log);
        }
        MTT_CHECK(driver.status().playing());
        MTT_CHECK(log.count(0xB0) + log.count(0xB1) > 10);
    }
}
