// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/audio/music.hpp"
#include "mentat/audio/midi_driver.hpp"
#include "mentat/audio/music_driver.hpp"
#include "mentat/audio/opl.hpp"
#ifdef MENTAT_WITH_MT32
#include "mentat/audio/mt32_music.hpp"
#endif
#include "mentat/engine/platform.hpp"
#include "mentat/data/reader.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <span>
#include <utility>
#include <vector>

using namespace mentat;

namespace {

struct Log : OplSink {
    std::vector<std::pair<std::uint16_t, std::uint8_t>> writes;
    std::vector<std::pair<std::uint8_t, std::uint8_t>> controls;
    std::vector<std::pair<std::uint8_t, std::uint8_t>> surround;
    void writeRegister(std::uint16_t reg, std::uint8_t value) override { writes.emplace_back(reg, value); }
    void writeControl(std::uint8_t reg, std::uint8_t value) override { controls.emplace_back(reg, value); }
    void writeSurround(std::uint8_t reg, std::uint8_t value) override { surround.emplace_back(reg, value); }
    int count(std::uint16_t reg) const {
        int n = 0;
        for (const auto& w : writes) {
            n += w.first == reg ? 1 : 0;
        }
        return n;
    }
    std::uint8_t last(std::uint16_t reg) const {
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
        t.channel[i] = static_cast<std::uint8_t>(i);
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

MusicTables goldTables() {
    MusicTables t = testTables();
    t.card = MusicCard::AdLibGold;
    for (std::size_t i = 0; i < Song::kVoices; ++i) {
        const auto bank = static_cast<std::uint8_t>(i < 9 ? 0 : 0x80);
        const auto c = static_cast<std::uint8_t>(i % 9);
        const auto op = static_cast<std::uint8_t>((c / 3) * 8 + c % 3);
        t.channel[i] = static_cast<std::uint8_t>(bank | c);
        t.pairedChannel[i] = static_cast<std::uint8_t>(bank | (c + 3));
        t.firstOperator[i] = static_cast<std::uint8_t>(bank | op);
        t.secondOperator[i] = static_cast<std::uint8_t>(bank | (op + 3));
    }
    return t;
}

// Two voices for the AdLib Gold: voice 0 a four-operator sound
// (instruments 0 and 1), voice 1 a two-operator sound on the left only
// (instrument 2). A settings block of 32 bytes follows the header.
std::vector<std::uint8_t> goldSong() {
    std::vector<std::uint8_t> v(52, 0);
    v[48] = 1;
    v[50] = 0;
    v[51] = 1; // tempo 0x100
    v.push_back(0xD6); // stereo processor: pseudo stereo
    for (int r = 0; r < 31; ++r) {
        v.push_back(static_cast<std::uint8_t>(r));
    }
    const std::vector<std::uint8_t> zero{0x00, 0xC0, 0x00, 0x00, 0x90, 60, 100, 0x10, 0xF0, 0};
    const std::vector<std::uint8_t> one{0x00, 0xC0, 0x02, 0x00, 0x90, 64, 100, 0x10, 0xF0, 0};
    v[2] = static_cast<std::uint8_t>(v.size() - 2);
    v.insert(v.end(), zero.begin(), zero.end());
    v[4] = static_cast<std::uint8_t>(v.size() - 2);
    v.insert(v.end(), one.begin(), one.end());
    v[0] = static_cast<std::uint8_t>(v.size());
    for (int i = 0; i < 3; ++i) {
        std::vector<std::uint8_t> instrument(40, 0);
        instrument[0] = i == 0 ? 4 : 0;
        instrument[17] = i == 2 ? 1 : 3; // the sides
        instrument[2 + 8] = static_cast<std::uint8_t>(10 + i);
        v.insert(v.end(), instrument.begin(), instrument.end());
    }
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
    MTT_CHECK(FmMusic::gainOf(0xFF) == 1.0);
    MTT_CHECK(FmMusic::gainOf(0) == 0.0);
}

MTT_TEST("music: the AdLib Gold player gives voices channels and sides") {
    Log log;
    MusicDriver driver(goldTables());
    driver.init(log);
    MTT_CHECK(log.last(0x105) == 1);
    MTT_CHECK(log.count(0x104) == 1 && log.last(0x104) == 0);
    log.writes.clear();
    auto song = std::make_shared<const Song>(Song::parse(goldSong()));
    MTT_CHECK(song.get()->voiceStart(1) != 0);
    driver.start(log, song, 0);
    // The piece's settings go to the stereo processor and the surround.
    MTT_CHECK(log.controls.back().first == 0x0A || log.controls.back().first == 0x09);
    bool switched = false;
    for (const auto& c : log.controls) {
        switched = switched || (c.first == 0x08 && c.second == 0xD6);
    }
    MTT_CHECK(switched);
    MTT_CHECK(log.surround.size() == 31 && log.surround[30].first == 30 && log.surround[30].second == 30);
    // Voice 0: four operators on channels 0 and 3 of the first set.
    MTT_CHECK((log.last(0x104) & 1) == 1);
    MTT_CHECK(log.last(0x40) == 10);
    MTT_CHECK(log.last(0x48) == 11); // operator 3 from the next instrument
    MTT_CHECK((log.last(0xC0) & 0x30) == 0x30);
    MTT_CHECK((log.last(0xB0) & 0x20) != 0);
    // Voice 1: a two-operator sound takes channel 8, left only.
    MTT_CHECK(log.last(0xC8) == 0x11); // left, and the connection bit from byte 15 = 0
    MTT_CHECK(log.last(0x52) == 12);
    MTT_CHECK((log.last(0xB8) & 0x20) != 0);
}

MTT_TEST("music: the AdLib Gold's FM volume") {
    MTT_CHECK(GoldOutput::fmVolume(0xFF) == 1.0);
    MTT_CHECK(GoldOutput::fmVolume(0x81) == 1.0 / 31);
    GoldOutput gold;
    const std::int16_t silence[2] = {0, 0};
    double out[2] = {1, 1};
    for (int i = 0; i < 100; ++i) {
        gold.process(silence, out);
    }
    MTT_CHECK(out[0] == 0.0 && out[1] == 0.0);
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
    const auto gold = MusicTables::fromGoldPlayer(archive->load("DNADG.HSQ"));
    MTT_CHECK(gold.frequency[0] == 0x157 && gold.channel[9] == 0x80);
    bool fourOps = false;
    for (const char* name : {"SEKENCE.AGD", "WATER.AGD", "WORMSUIT.AGD", "WORMINTR.AGD", "WARSONG.AGD", "MORNING.AGD",
                             "SIETCHM.AGD", "BAGDAD.AGD", "ARRAKIS.AGD", "CRYOMUS.AGD"}) {
        Log log;
        MusicDriver driver(gold);
        driver.init(log);
        auto song = std::make_shared<const Song>(Song::parse(archive->load(name)));
        driver.start(log, song, 0);
        for (int i = 0; i < 20000; ++i) {
            driver.tick(log);
        }
        MTT_CHECK(driver.status().playing());
        int keys = 0;
        for (const auto& w : log.writes) {
            keys += (w.first & 0xFF) >= 0xB0 && (w.first & 0xFF) <= 0xB8 && (w.second & 0x20) != 0 ? 1 : 0;
            fourOps = fourOps || (w.first == 0x104 && w.second != 0);
        }
        MTT_CHECK(keys > 10);
    }
    MTT_CHECK(fourOps); // the worm pieces use four-operator sounds
}

namespace {

struct MidiLog : MidiSink {
    std::vector<std::vector<std::uint8_t>> messages;
    void send(std::span<const std::uint8_t> message) override { messages.emplace_back(message.begin(), message.end()); }
};

// A piece of our own in the .M32 layout: one list on channel 1 that sets
// the volume, picks a sound and plays a note for 10 steps.
std::shared_ptr<const Song> midiSong() {
    std::vector<std::uint8_t> b(52, 0);
    const std::uint8_t list[] = {0x00, 0xB0, 0x07, 0x64, 0x00, 0xC0, 0x05, 0x00, 0x90,
                                 0x3C, 0x64, 0x0A, 0x80, 0x3C, 0x00, 0x00, 0xF0, 0x00};
    b.insert(b.end(), std::begin(list), std::end(list));
    b[2] = 50;                                                 // voice 0 starts at 2 + 50
    b[44] = 0xFF;                                              // no repeated part
    b[50] = 0x00;                                              // tempo 0x100: a step a tick
    b[51] = 0x01;
    const auto size = static_cast<std::uint16_t>(b.size());
    b[0] = static_cast<std::uint8_t>(size & 0xFF); // no instruments
    b[1] = static_cast<std::uint8_t>(size >> 8);
    return std::make_shared<const Song>(Song::parse(std::move(b)));
}

} // namespace

MTT_TEST("mt32 music: the MIDI player sends the piece's messages, its volume scaled by the game's") {
    MidiLog log;
    MidiMusicDriver driver;
    driver.init(log);
    MTT_CHECK(log.messages.size() == 20); // notes off and bend for ten channels
    MTT_CHECK(log.messages[0] == std::vector<std::uint8_t>({0xB0, 0x7B, 0x00}));
    MTT_CHECK(log.messages[19] == std::vector<std::uint8_t>({0xE9, 0x00, 0x40}));
    log.messages.clear();
    driver.start(log, midiSong(), 1);
    MTT_CHECK(driver.status().playing());
    // 20 messages of silence, then the volume (0x64 scaled by 0xE6), the
    // sound and the note.
    MTT_CHECK(log.messages.size() == 23);
    MTT_CHECK(log.messages[20] == std::vector<std::uint8_t>({0xB0, 0x07, 0x59}));
    MTT_CHECK(log.messages[21] == std::vector<std::uint8_t>({0xC0, 0x05}));
    MTT_CHECK(log.messages[22] == std::vector<std::uint8_t>({0x90, 0x3C, 0x64}));
    log.messages.clear();
    // The first tick only counts down; a step a tick after that.
    for (int t = 0; t < 10; ++t) {
        driver.tick(log);
    }
    MTT_CHECK(log.messages.empty());
    driver.tick(log); // the note's 10 steps are up; then the end: played once
    MTT_CHECK(!log.messages.empty() && log.messages[0] == std::vector<std::uint8_t>({0x80, 0x3C, 0x00}));
    MTT_CHECK(!driver.status().playing());

    // A fade moves the volume 3 a step and sends it on all ten channels.
    driver.start(log, midiSong(), 0);
    log.messages.clear();
    driver.fade(0x10, 0xE0);
    driver.tick(log);
    MTT_CHECK(driver.volume() == 0xE3);
    MTT_CHECK(log.messages.size() == 10 && log.messages[0] == std::vector<std::uint8_t>({0xB0, 0x07, 0x58}));
    MTT_CHECK(log.messages[9] == std::vector<std::uint8_t>({0xB9, 0x07, 0x44})); // 0x4D scaled
}

#ifdef MENTAT_WITH_MT32
MTT_TEST("mt32 music: the CD's opening music sounds on the emulator") {
    auto archive = test::openGameData();
    const auto roms = findMt32Roms({MENTAT_GAMEDATA_DIR});
    if (!archive || !roms) {
        MTT_SKIP("no game data or MT-32 ROMs");
        return;
    }
    Mt32Music music(roms->control, roms->pcm);
    MTT_CHECK(music.rate() == 32000);
    music.setVolume(0xE6, 0x78);
    music.start(std::make_shared<const Song>(Song::parse(archive->load("CRYOMUS.M32"))), 1);
    std::vector<std::int16_t> out;
    music.runTo(0, out);
    music.runTo(static_cast<std::uint32_t>(kTicksPerSecond * 3), out);
    int loudest = 0;
    for (const auto s : out) {
        loudest = std::max(loudest, std::abs(static_cast<int>(s)));
    }
    MTT_CHECK(out.size() > 2 * 32000 * 2);
    MTT_CHECK(loudest > 1000);
    MTT_CHECK(music.status().playing());
}
#endif
