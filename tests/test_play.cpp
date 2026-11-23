// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/engine/display.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/intro.hpp"
#include "mentat/engine/mixer_panel.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/places.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/engine/world.hpp"

#include <optional>
#include <vector>

using namespace mentat;

MTT_TEST("world: a new game starts in the throne room with Leto") {
    auto program = test::openProgram();
    if (!program) {
        MTT_SKIP("no DNCDPRG.EXE in gamedata/");
    }
    World w(*program);
    MTT_CHECK(w.room() == 0x200A && w.place() == 0x0180 && w.inside());
    MTT_CHECK(w.clock() == 2 && w.hour() == 2 && w.day() == 1);
    const auto here = w.findPresent();
    MTT_CHECK(here.size() == 1 && here[0].number == 0);
    MTT_CHECK(w.present() == 0x0001);
    // A day starts three parts before the clock's count of 16 comes round.
    w.setClock(12);
    MTT_CHECK(w.day() == 1);
    w.setClock(13);
    MTT_CHECK(w.day() == 2 && w.hour() == 13);
    w.goTo(0x2004, 0x0180);
    w.findPresent();
    MTT_CHECK(w.present() == 0x0002); // Jessica
    MTT_CHECK_THROWS(w.u8(World::kSize));
}

MTT_TEST("panel: the zones, the arrows of a room and the command lines") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    Panel panel(g);
    MTT_CHECK(panel.zones().size() == 24);
    MTT_CHECK(panel.zones()[Panel::kCommandLines].left == 0x5C && panel.zones()[Panel::kCommandLines].top == 0x9F);

    World w(*program);
    g.places().enter(w.room(), w.place());
    const auto exits = g.places().exits();
    MTT_CHECK(exits[0] == 9 && exits[1] == 5 && exits[2] == 4 && exits[3] == 0);
    g.display().targetBack();
    panel.setArrows(w, exits);
    const auto& z = panel.zones();
    MTT_CHECK(z[Panel::kArrows].sprite == 0x21);
    MTT_CHECK(z[Panel::kArrows + 1].flags == Zone::kActive && z[Panel::kArrows + 4].flags == Zone::kHidden);
    MTT_CHECK(z[Panel::kArrows + 5].flags == Zone::kActive); // the middle shows in the palace

    // A greyed command is not active; a lit one swaps its colours.
    panel.setCommands({Command{0x98, 0}, Command{0x9E | Command::kGrey, 0}, Command{0x78 | Command::kLit, 0}});
    panel.drawCommands();
    MTT_CHECK((z[Panel::kCommandLines].flags & Zone::kActive) != 0);
    MTT_CHECK((z[Panel::kCommandLines + 1].flags & Zone::kActive) == 0);
    MTT_CHECK((z[Panel::kCommandLines + 3].flags & Zone::kActive) == 0);
    const Surface& s = g.display().back();
    MTT_CHECK(s.at(0x5D, z[Panel::kCommandLines + 2].top + 1) == 0xFA); // lit: the background in the text colour
    MTT_CHECK(s.at(0xE2, z[Panel::kCommandLines].top + 1) == 0xF3);
    MTT_CHECK(s.at(0xE3, z[Panel::kCommandLines].top + 1) == 0);
}

MTT_TEST("story: a step opens the way to the message room and moves people") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables::fromDriver(archive->load("DNVGA.HSQ")), nullptr);
    World w(*program);
    g.places().stateByte = [&w](std::size_t at) -> std::optional<std::uint8_t> {
        if (at < World::kSize) {
            return w.u8(at);
        }
        return std::nullopt;
    };
    int events = 0;
    w.storyEvents = [&events] { ++events; };
    g.places().enter(0x2007, 0x0180);
    MTT_CHECK(static_cast<std::int8_t>(g.places().exits()[3]) < -5); // shut
    MTT_CHECK(!w.raiseStory(0));
    MTT_CHECK(w.raiseStory(0x0C) && events == 1 && w.u8(0x2A) == 0x0C);
    MTT_CHECK(w.roomsChanged && w.scene != 0);
    MTT_CHECK(g.places().exits()[3] == 8);
    g.places().enter(0x2008, 0x0180);
    MTT_CHECK(g.places().exits()[1] == 7);
    // Step 0x10 sends Leto to room 5 and news from the Emperor.
    MTT_CHECK(w.raiseStory(0x10) && (w.u16(0xFD8) & 0xFF) == 5);
    MTT_CHECK(w.u8(0xC8) == 1 && w.u16(0x1179) == 0x010B);
    g.places().stateByte = {};
}

MTT_TEST("news: the message room lists news from the newest, and looking at one marks it seen") {
    auto program = test::openProgram();
    if (!program) {
        MTT_SKIP("no DNCDPRG.EXE in gamedata/");
    }
    World w(*program);
    // Three pieces of news, the middle one already seen.
    w.setU16(0x1179, 0x020B);
    w.setU16(0x117B, 0x0189);
    w.setU16(0x117D, 0x0C0B);
    w.setU8(0xC8, 3);
    w.setU8(0xC9, 2);
    MTT_CHECK((w.newsShown(false) == std::vector<int>{2, 0}));
    MTT_CHECK((w.newsShown(true) == std::vector<int>{1}));
    w.setU8(0xBF, 0);
    MTT_CHECK(w.lookAtNews(0, false) == 0x020B);
    MTT_CHECK(w.u8(0x24) == 2 && w.u8(0xC9) == 1 && (w.u8(0x1179) & 0x80));
    MTT_CHECK(w.u8(0xBF) == 0x20); // the Emperor's second warning
    MTT_CHECK((w.newsShown(true) == std::vector<int>{1, 0}));
    // Looking again at news already seen changes nothing but what it is
    // about.
    MTT_CHECK(w.lookAtNews(1, true) == 0x0189 && w.u8(0xC9) == 1 && w.u8(0x24) == 1);

    // The reminders that news waits go when the list is shown.
    w.setU8(World::kMessageQueue, 3);
    w.setU16(World::kMessageQueue + 1, 0x0201);
    w.setU16(World::kMessageQueue + 3, 0);
    w.setU16(World::kMessageQueue + 5, 0x0105);
    w.setU16(World::kMessageQueue + 7, 0x0180);
    w.setU16(World::kMessageQueue + 9, 0x0001);
    w.setU16(World::kMessageQueue + 11, 0);
    w.dropNewsReminders();
    MTT_CHECK(w.u8(World::kMessageQueue) == 1);
    MTT_CHECK(w.u16(World::kMessageQueue + 1) == 0x0105 && w.u16(World::kMessageQueue + 3) == 0x0180);
    MTT_CHECK(w.u16(World::kMessageQueue + 5) == 0 && w.u16(World::kMessageQueue + 7) == 0);
}

MTT_TEST("mixer panel: volumes, balances, the language and the subtitles") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    // The settings start as the program keeps them; the voices lean a
    // little to one side.
    MTT_CHECK(g.levels.voices == 0xFF && g.levels.music == 0xE6 && g.levels.quietMusic == 0xB4);
    MTT_CHECK(g.levels.voicesBalance == 0x64 && g.levels.musicBalance == 0x78 && g.levels.quietBalance == 0x8C);
    MTT_CHECK(p.digitalLevels == 0xFC);

    MixerPanel m(g);
    m.open(2);
    MTT_CHECK(m.active());
    MTT_CHECK(m.press(10, 180) == MixerPanel::Click::Outside);
    MTT_CHECK(m.press(100, 128) == MixerPanel::Click::VoiceTest);
    // For a gamepad: three knobs, three balances, the test and ten rows.
    const std::vector<ClipRect> targets = m.targets();
    MTT_CHECK(targets.size() == 17);
    for (const ClipRect& r : targets) {
        MTT_CHECK(m.press((r.left + r.right) / 2, (r.top + r.bottom) / 2) != MixerPanel::Click::Outside);
        m.release();
    }
    m.open(2);
    g.language = kEnglish;
    m.open(2);

    // The voices' knob, dragged down 8 rows, takes 4 a row off.
    MTT_CHECK(m.press(57, 37) == MixerPanel::Click::Nothing && m.holding());
    m.drag(57, 45, 0, 8);
    MTT_CHECK(g.levels.voices == 0xFF - 32);
    MTT_CHECK(p.digitalLevels != 0xFC);
    // Not past the top of its track.
    m.drag(57, 25, 0, -20);
    MTT_CHECK(g.levels.voices == 0xFF - 32);
    m.release();
    MTT_CHECK(!m.holding());

    // The music's balance turns 10 at a time: up on its right side one way.
    m.drag(104, 106, 0, -1);
    MTT_CHECK(g.levels.musicBalance == 0x6E);
    m.drag(104, 107, 0, 1);
    MTT_CHECK(g.levels.musicBalance == 0x78);

    // Another language: lines without subtitles get them.
    MTT_CHECK(m.press(184, 57) == MixerPanel::Click::Language);
    MTT_CHECK(g.language == kFrench && m.subtitles() == 0);
    MTT_CHECK(g.fontFile() == 0xBB);
    MTT_CHECK(m.press(184, 85) == MixerPanel::Click::Language && g.fontFile() == 0xC7);
    // Balloons, from the second list.
    MTT_CHECK(m.press(184, 1 + 0x27 + 11 * 7 + 3) == MixerPanel::Click::Nothing && m.subtitles() == 1);
    m.close();
    MTT_CHECK(!m.active());
}
