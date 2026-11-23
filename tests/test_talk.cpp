// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/data/phrase_file.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/dialogue.hpp"
#include "mentat/engine/fiber.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/intro.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/places.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/engine/talk.hpp"
#include "mentat/engine/text_box.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/font.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

using namespace mentat;

namespace {

void putU16(std::vector<std::uint8_t>& v, std::size_t at, std::uint16_t value) {
    v[at] = static_cast<std::uint8_t>(value);
    v[at + 1] = static_cast<std::uint8_t>(value >> 8);
}

struct FakeHost : TalkHost {
    int closed = 0;
    int ended = 0;
    void closeOrnament() override { ++closed; }
    void keepOrnamentBackground() override {}
    void openOrnament() override {}
    void drawOrnament() override {}
    void showPointer(bool) override {}
    void menuChanged() override {}
    void backToRoom() override { ++ended; }
};

} // namespace

MTT_TEST("phrases: pieces of the word list go in with a space after them") {
    // Two phrases and the word list ("bigcat"): pieces of 3 at 0 and at 3.
    const std::vector<std::uint8_t> f = {
        0x06, 0x00, 0x0F, 0x00, 0x10, 0x00,                   // offsets: 6, 15, word list 16
        'A', ' ', 0xE3, 0x00, 0x00, 0xE3, 0x03, 0x00, 0xFF,   // "A " big cat
        0xFF,                                                  // an empty phrase
        'b', 'i', 'g', 'c', 'a', 't', 0xFF,
    };
    const PhraseFile p = PhraseFile::parse(f);
    MTT_CHECK(p.size() == 2);
    MTT_CHECK(p.text(0) == "A big cat");
    MTT_CHECK(p.text(1).empty());
    MTT_CHECK(p.raw(0).size() == 8);
    MTT_CHECK_THROWS(p.text(2));
}

MTT_TEST("conditions: formulas over the state, joined left to right") {
    std::vector<std::uint8_t> f(64, 0xFF);
    putU16(f, 0, 6);
    putU16(f, 2, 13);
    putU16(f, 4, 30);
    // 1: byte [0x2A] == 5
    const std::vector<std::uint8_t> c1 = {0x01, 0x2A, 0x00, 0x80, 0x05, 0xFF};
    // 2: (word [0x02] + 2 > 3) or (byte [0x2A] < 1)
    const std::vector<std::uint8_t> c2 = {0x00, 0x02, 0x0C, 0x80, 0x02, 0x04, 0x80, 0x03,
                                          0x92, 0x01, 0x2A, 0x02, 0x80, 0x01, 0xFF};
    // 3: signed: word [0x04] >= 0x8000 is false for -1 >= 0 ... -1 <= 0x0001
    const std::vector<std::uint8_t> c3 = {0x00, 0x04, 0x08, 0x81, 0x01, 0x00, 0xFF};
    std::copy(c1.begin(), c1.end(), f.begin() + 6);
    std::copy(c2.begin(), c2.end(), f.begin() + 13);
    std::copy(c3.begin(), c3.end(), f.begin() + 30);
    const Conditions c = Conditions::parse(f);
    World w;
    MTT_CHECK(!c.test(1, w));
    w.setU8(0x2A, 5);
    MTT_CHECK(c.test(1, w));
    MTT_CHECK(!c.test(2, w));
    w.setU16(0x02, 2);
    MTT_CHECK(c.test(2, w));
    w.setU16(0x02, 0);
    w.setU8(0x2A, 0);
    MTT_CHECK(c.test(2, w));
    w.setU16(0x04, 0xFFFF);
    MTT_CHECK(c.test(3, w)); // -1 <= 1
    w.setU16(0x04, 2);
    MTT_CHECK(!c.test(3, w));
    MTT_CHECK_THROWS(c.evaluate(4, w));
}

MTT_TEST("dialogue: lines said are passed over unless they may come again") {
    std::vector<std::uint8_t> f = {
        0x04, 0x00, 0xFF, 0xFF,             // list 0 at 4; list 1 missing
        0x20, 0x01, 0x00, 0x01,             // said once only, condition 1, phrase 1
        0x40, 0x02, 0x00, 0x02,             // may come again, condition 2, phrase 2
        0x0B, 0x01, 0x10, 0x03,             // action 11, no recording, phrase 3
        0xFF, 0xFF,
    };
    Dialogue d = Dialogue::parse(f);
    MTT_CHECK(d.lists() == 2 && d.list(0) == std::optional<std::size_t>(4) && !d.list(1));
    const auto always = [](int) { return true; };
    MTT_CHECK(d.find(4, DialogueLine::kSaid, always) == std::optional<std::size_t>(4));
    d.markSaid(4);
    d.markSaid(8);
    MTT_CHECK(d.find(4, DialogueLine::kSaid, always) == std::optional<std::size_t>(8));
    MTT_CHECK(d.find(4, DialogueLine::kOnceMore, always) == std::optional<std::size_t>(8));
    const auto onlyOne = [](int n) { return n == 1; };
    MTT_CHECK(d.find(4, DialogueLine::kSaid, onlyOne) == std::optional<std::size_t>(12));
    const DialogueLine l = d.line(12);
    MTT_CHECK(l.action() == 11 && l.phrase == 3 && l.condition == 1 && (l.extra & DialogueLine::kTextOnly));
    MTT_CHECK(d.firstPhrase(0) == 1);
    MTT_CHECK(!d.find(16, DialogueLine::kSaid, always));
}

MTT_TEST("text: control bytes switch fonts and take no room") {
    std::vector<std::uint8_t> file(Font::kFileSize, 0);
    file['a'] = 4;                    // large
    file[128 + 'a'] = 2;              // small
    const Font large = Font::parse(file);
    const Font small = Font::parse(file, Font::Size::Small);
    const std::string text = "aa \x06" "aa \x08" "aa";
    const auto lines = layoutText(large, text, 100, &small);
    MTT_CHECK(lines.size() == 1 && lines[0].words == 3);
    const auto narrow = layoutText(large, text, 20, &small); // 8 + 6 + 4, then 8
    MTT_CHECK(narrow.size() == 2 && narrow[0].words == 2);
    // A byte from 0x80 on ends the text.
    MTT_CHECK(layoutText(large, std::string("aa \xCF" "aa"), 100).size() == 1);
    MTT_CHECK(layoutText(large, std::string("aa \xCF" "aa"), 100)[0].words == 1);
}

MTT_TEST("talk: Leto's first lines in a new game, then the talk ends") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    World w(*program);
    Panel panel(g, kEnglish);
    FakeHost host;
    Talk talk(g, w, panel, host, kEnglish);
    talk.subtitles = Talk::Subtitles::Below;
    talk.runEvents();
    talk.runEvents();

    // Leto's first phrase has nothing to fill in; his seventh has two pages.
    const PhraseFile leto = PhraseFile::parse(archive->load("PHRASE41.HSQ"));
    MTT_CHECK(talk.phraseText(1 | 0x800, false) == std::vector<std::string>{leto.text(0)});
    const auto twoPages = talk.phraseText(7 | 0x800, false);
    MTT_CHECK(twoPages.size() == 2 && twoPages[0] + "\xFE" + twoPages[1] == leto.text(6));

    g.places().enter(w.room(), w.place());
    g.display().targetBack();
    g.places().present = w.present();
    w.findPresent();
    g.places().drawView();
    talk.start(0, 186, 53);
    MTT_CHECK(talk.active() && talk.phrase() == 1 && talk.subtitleShown());
    MTT_CHECK(host.closed == 1);
    MTT_CHECK((w.u16(0x0E) & 1) != 0);
    MTT_CHECK(panel.commands().size() == 4 && panel.commands()[0].action == Talk::kTalkToMe);
    MTT_CHECK(talk.command(Talk::kTalkToMe));
    MTT_CHECK(talk.phrase() == 5);
    MTT_CHECK(talk.command(Talk::kTalkToMe));
    MTT_CHECK(talk.phrase() == 6);
    MTT_CHECK(talk.command(Talk::kWhat) && talk.phrase() == 6);
    MTT_CHECK(talk.command(Talk::kTalkToMe));
    MTT_CHECK(!talk.active() && host.ended == 1);
    // The lines are marked said; the first one, once only, is not said again.
    talk.start(0, 186, 53);
    MTT_CHECK(talk.phrase() == 5);
    MTT_CHECK(talk.command(Talk::kComeWithMe) && talk.phrase() == 0x23); // he says no
    MTT_CHECK((w.u16(0x10) & 1) == 0);
    MTT_CHECK(talk.command(Talk::kStopTalking) && !talk.active());
    MTT_CHECK((w.u8(0xFD8 + 0x0F) & 0x20) != 0);
    MTT_CHECK(Talk::characterOf(0x92F2) == 0 && Talk::characterOf(0x92F7) == 1 && Talk::characterOf(0x9300) == -1);
}

MTT_TEST("talk: Leto stops Paul going for a door before they have talked") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    World w(*program);
    Panel panel(g, kEnglish);
    FakeHost host;
    Talk talk(g, w, panel, host, kEnglish);
    talk.subtitles = Talk::Subtitles::Below;
    talk.runEvents();
    talk.runEvents();
    g.places().enter(w.room(), w.place());
    g.display().targetBack();
    g.places().present = w.present();
    w.findPresent();
    g.places().drawView();

    // Coming into a room he has nothing to say; going for another door
    // neither.
    w.setU8(0x23, 5);
    MTT_CHECK(!talk.speakUp(0, 186, 53) && !talk.active());
    w.setU8(0x23, 1);
    w.setU8(0x0C, 3);
    talk.expectYes();
    MTT_CHECK(!talk.speakUp(0, 186, 53) && !talk.active() && talk.answer() == 0xFF);
    // Going for room 4 he stops Paul, and the talk starts there.
    w.setU8(0x0C, 4);
    MTT_CHECK(talk.speakUp(0, 186, 53));
    MTT_CHECK(talk.active() && talk.phrase() == 0x1F && talk.answer() == 0);
    MTT_CHECK((w.u16(0x0E) & 1) != 0);
    MTT_CHECK(panel.commands().size() == 4 && panel.commands()[0].action == Talk::kTalkToMe);
    // Then his first list, as when Paul talks to him.
    MTT_CHECK(talk.command(Talk::kTalkToMe) && talk.phrase() == 1);
    // Met now, he lets Paul go.
    MTT_CHECK(talk.command(Talk::kStopTalking) && !talk.active());
    talk.expectYes();
    MTT_CHECK(!talk.speakUp(0, 186, 53) && talk.answer() == 0xFF);
}

MTT_TEST("world: those who follow Paul go with him from room to room") {
    auto program = test::openProgram();
    if (!program) {
        MTT_SKIP("no game files in gamedata/");
    }
    World w(*program);
    w.goTo(0x2001, 0x0180);
    const std::size_t gurney = 0xFD8 + 16 * 4;
    w.setU16(gurney, 0x2001);
    w.setU16(gurney + 2, 0x0180);
    w.setU8(gurney + 0x0F, static_cast<std::uint8_t>(w.u8(gurney + 0x0F) | 0x40));
    w.goTo(0x2003, 0x0180);
    MTT_CHECK(w.u16(gurney) == 0x2003 && w.u8(0x0B) == 3 && w.u8(0x0D) == 1);
    w.setU8(gurney + 0x0F, static_cast<std::uint8_t>(w.u8(gurney + 0x0F) & ~0x40));
    w.goTo(0x2002, 0x0180);
    MTT_CHECK(w.u16(gurney) == 0x2003 && w.u8(0x0B) == 2 && w.u8(0x0D) == 3);
}

MTT_TEST("talk: a companion's remark on the way, and what it answers to") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    World w(*program);
    Panel panel(g, kEnglish);
    FakeHost host;
    Talk talk(g, w, panel, host, kEnglish);
    talk.subtitles = Talk::Subtitles::Below;
    auto remark = [&](int c) {
        bool said = false;
        Fiber fiber([&] {
            g.display().targetBack();
            said = talk.remark(c);
        });
        p.fiber = &fiber;
        while (!fiber.finished()) {
            fiber.resume();
            ++p.ticks;
        }
        p.fiber = nullptr;
        return said;
    };
    // Gurney sees a sietch on the right.
    w.setU8(0x23, 3);
    w.setU8(0xE1, 1);
    w.setU16(0x11F3, 0x48);
    MTT_CHECK(remark(4));
    MTT_CHECK(talk.active() && talk.remarking() && talk.character() == 4 && talk.phrase() == 0x1C5);
    MTT_CHECK(talk.subtitleShown());
    const auto text = talk.phraseText(0x1C5 | 0x800, true);
    MTT_CHECK(!text.empty() && text[0].find("a sietch on the right") != std::string::npos);
    // " WHAT ? " is the talk's; going on is not.
    MTT_CHECK(talk.command(Talk::kWhat) && !talk.command(0xD2E2) && talk.active());
    talk.endRemark();
    MTT_CHECK(!talk.active() && host.ended == 0 && (w.u8(0xFD8 + 16 * 4 + 0x0F) & 0x20) != 0);
    // Said before, it may be said again; on the left it is the next line.
    MTT_CHECK(remark(4) && talk.phrase() == 0x1C5);
    talk.endRemark();
    w.setU8(0xE1, 0);
    MTT_CHECK(remark(4) && talk.phrase() == 0x1C6);
    talk.endRemark();
    // The Harkonnens' zone.
    w.setU8(0x23, 4);
    MTT_CHECK(remark(4) && talk.phrase() == 0x1C7);
    talk.endRemark();
}

MTT_TEST("people: the Fremen of a sietch stand in its second room and agree to work for Paul") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    World w(*program);
    Panel panel(g, kEnglish);
    MapScreen map(g, w, panel);
    map.setUp();
    // As far as the story has come when Gurney first flies with Paul.
    w.setU8(0x2A, 4);
    const std::size_t sietch = World::placeRecord(12);
    w.enterPlace(sietch);
    // Its first room: nobody of the troop.
    auto people = w.meetPeople();
    MTT_CHECK(people.fremen == 0 && people.chiefs.empty());
    w.goTo((w.room() & 0xFF00) | 2, w.place());
    people = w.meetPeople();
    const int troop = w.u8(sietch + 9);
    MTT_CHECK(troop != 0 && people.fremen == troop && people.chiefs.empty());
    MTT_CHECK(w.u16(0x11CE) == sietch && w.u8(0x4D) == w.u8(sietch + 8) && w.u8(0x90) == 1);
    w.findPresent();
    MTT_CHECK((w.present() & (1u << 14)) != 0);

    FakeHost host;
    Talk talk(g, w, panel, host, kEnglish);
    talk.subtitles = Talk::Subtitles::Below;
    int enlisted = 0;
    talk.enlisted = [&] {
        ++enlisted;
        MTT_CHECK(!w.enlist(troop));
        people = w.meetPeople();
        w.findPresent();
    };
    auto run = [&](const std::function<void()>& f) {
        Fiber fiber([&] {
            g.display().targetBack();
            f();
        });
        p.fiber = &fiber;
        while (!fiber.finished()) {
            fiber.resume();
            ++p.ticks;
        }
        p.fiber = nullptr;
    };
    w.learnTroop(troop);
    MTT_CHECK(w.u16(0x2C) == sietch && w.u8(0x30) == w.u8(World::troopRecord(troop) + 3));
    run([&] { talk.start(14, -1, -1); });
    MTT_CHECK(panel.commands().size() == 4 && panel.commands()[1].action == Talk::kWorkForMe &&
              panel.commands()[1].phrase == 0x96);
    run([&] { talk.command(Talk::kWorkForMe); });
    MTT_CHECK(enlisted == 1);
    const std::size_t t = World::troopRecord(troop);
    MTT_CHECK(w.u8(t + 3) == 2 && w.u8(0x28) == 1 && w.u8(sietch + 0x0B) == 2);
    MTT_CHECK(people.fremen == 0 && people.chiefs.size() == 1 && people.chiefs[0] == troop);
    MTT_CHECK((w.present() & (1u << 15)) != 0 && (w.present() & (1u << 14)) == 0);
    MTT_CHECK(panel.commands()[1].action == Talk::kGiveOrders && panel.commands()[1].phrase == 0x93);
}
