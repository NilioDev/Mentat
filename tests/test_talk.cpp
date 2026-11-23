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
#include "mentat/engine/orders.hpp"
#include "mentat/engine/spice_map.hpp"
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
    Panel panel(g);
    FakeHost host;
    Talk talk(g, w, panel, host);
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
    Panel panel(g);
    FakeHost host;
    Talk talk(g, w, panel, host);
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
    Panel panel(g);
    FakeHost host;
    Talk talk(g, w, panel, host);
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
    Panel panel(g);
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
    Talk talk(g, w, panel, host);
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

MTT_TEST("orders: a troop's chief answers in a box over the map and takes up an occupation") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    World w(*program);
    Panel panel(g);
    MapScreen map(g, w, panel);
    map.setUp();
    w.setU8(0x2A, 4);
    const std::size_t sietch = World::placeRecord(12);
    w.enterPlace(sietch);
    w.goTo((w.room() & 0xFF00) | 2, w.place());
    const int troop = w.u8(sietch + 9);
    MTT_CHECK(troop != 0);
    w.enlist(troop);
    const std::size_t t = World::troopRecord(troop);
    MTT_CHECK((w.u8(t + 3) & 0x0F) == 2);

    FakeHost host;
    Talk talk(g, w, panel, host);
    SpiceMap spice(g, w, panel, map);
    Orders orders(g, w, panel, map, talk, spice);
    int finished = 0;
    orders.finished = [&] { ++finished; };
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
    run([&] { orders.open(troop); });
    MTT_CHECK(orders.active() && map.active() && talk.active() && talk.inBox());
    // The chief says where his troop is, waiting; the commands are the
    // orders, with only the occupation to choose and no more orders.
    MTT_CHECK(talk.phrase() == 0x118);
    const auto& c = panel.commands();
    MTT_CHECK(c.size() == 5 && c[0].action == Orders::kInformation && c[1].phrase == 0x56 && (c[2].phrase & Command::kGrey) &&
              (c[3].phrase & Command::kGrey) && c[4].action == Orders::kNoMoreOrders);
    // The men and their keenness come next.
    run([&] { orders.command(Orders::kInformation); });
    MTT_CHECK(talk.phrase() == 0x121);
    // The occupations: spice, army, and ecology greyed at first.
    run([&] { orders.command(Orders::kOccupation); });
    MTT_CHECK(panel.commands().size() == 4 && panel.commands()[0].action == Orders::kSpice &&
              (panel.commands()[2].phrase & Command::kGrey));
    run([&] { orders.command(Orders::kSpice); });
    MTT_CHECK(w.u8(0x23) == 0x0A && talk.answer() == 0xFF && (w.u8(t + 3) & 0x0F) == 0);
    MTT_CHECK((w.u16(t + 0x12) & 0x2000) != 0 && panel.commands()[1].phrase == 0x4F);
    run([&] { orders.command(Orders::kNoMoreOrders); });
    MTT_CHECK(!orders.active() && !talk.active() && finished == 1);
}

MTT_TEST("orders: a troop shown a place on the small map sets off and walks there hour by hour") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    World w(*program);
    Panel panel(g);
    MapScreen map(g, w, panel);
    map.setUp();
    w.setU8(0x2A, 4);
    // The troop of sietch 14 (troop 3, at sietch 12, is shown three
    // sietches instead).
    const std::size_t sietch = World::placeRecord(14);
    w.enterPlace(sietch);
    w.goTo((w.room() & 0xFF00) | 2, w.place());
    const int troop = w.u8(sietch + 9);
    MTT_CHECK(troop != 0);
    w.enlist(troop);
    const std::size_t t = World::troopRecord(troop);
    w.markChanged = [&](int n) { map.markChanged(n); };
    w.markMoved = [&](int n) { map.markMoved(n); };
    // Paul knows the places around.
    for (std::size_t r = World::kPlaces; r + World::kPlaceSize <= World::kTroops; r += World::kPlaceSize) {
        w.setU8(r + 0x0A, static_cast<std::uint8_t>(w.u8(r + 0x0A) & 0x7F));
    }

    FakeHost host;
    Talk talk(g, w, panel, host);
    SpiceMap spice(g, w, panel, map);
    Orders orders(g, w, panel, map, talk, spice);
    int sent = 0;
    orders.sent = [&] { ++sent; };
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
    run([&] { orders.open(troop); });
    run([&] { orders.command(Orders::kOccupation); });
    run([&] { orders.command(Orders::kSpice); });
    // Moving the troop comes with the story's fifth step.
    MTT_CHECK((panel.commands()[3].phrase & Command::kGrey) != 0);
    w.setU8(0x2A, 5);
    run([&] { orders.command(Orders::kCancel); });
    MTT_CHECK(panel.commands()[3].action == Orders::kMove && !(panel.commands()[3].phrase & Command::kGrey));
    // The troop's mark is on the map, ringed.
    std::size_t ringed = 0;
    for (const MapScreen::Mark& m : map.marks()) {
        ringed += m.troop == troop ? 1 : 0;
    }
    MTT_CHECK(ringed == 2);

    // The chief asks where to; the small map opens under his box, and
    // "Cancel" alone is offered. A click outside it goes back.
    run([&] { orders.command(Orders::kMove); });
    MTT_CHECK(orders.choosing() && spice.shown() && !spice.occupations);
    MTT_CHECK(spice.frame().left == 0x5C && (spice.frame().top == 0x1E || spice.frame().top == 0x0E));
    MTT_CHECK(panel.commands().size() == 1 && panel.commands()[0].action == Orders::kCancel);
    run([&] { orders.click(2, 2); });
    MTT_CHECK(!orders.choosing() && !spice.shown() && orders.active() && panel.commands().size() == 5);

    // Pointing at another place and clicking it: the chief agrees, the
    // troop leaves its place's chain and sets off.
    run([&] { orders.command(Orders::kMove); });
    std::size_t to = 0;
    int cx = 0;
    int cy = 0;
    const ClipRect v = spice.view();
    run([&] {
        for (int y = v.top; y < v.bottom && to == 0; ++y) {
            for (int x = v.left; x < v.right && to == 0; ++x) {
                spice.pointAt(x, y);
                const std::size_t at = spice.pointed();
                if (at != 0 && at != sietch && w.u8(at + 8) < 0x20) {
                    to = at;
                    cx = x;
                    cy = y;
                }
            }
        }
    });
    MTT_CHECK(to != 0);
    run([&] { orders.click(cx, cy); });
    MTT_CHECK(w.u8(0x23) == 0x0B && talk.answer() == 0xFF);
    MTT_CHECK(sent == 1 && !orders.active() && !spice.shown());
    MTT_CHECK((w.u8(t + 3) & 0x40) != 0 && w.u16(t + 4) == to && w.u8(t + 1) == 0 && w.u8(t + 2) == 0);
    for (int other = w.u8(sietch + 9); other != 0; other = w.u8(World::troopRecord(other) + 1)) {
        MTT_CHECK(other != troop);
    }

    // Hour by hour it walks on until it is there, and joins the place.
    for (int hour = 0; hour < 200 && (w.u8(t + 3) & 0x40); ++hour) {
        w.setU16(0, static_cast<std::uint16_t>(g.random2()));
        w.troopsWork(0, map.flatMap());
    }
    MTT_CHECK(!(w.u8(t + 3) & 0x40));
    MTT_CHECK(w.u16(t + 6) == w.u16(to + 2) && w.u16(t + 8) == w.u16(to + 4) && w.u8(t + 2) >= 1);
    bool there = false;
    for (int other = w.u8(to + 9); other != 0; other = w.u8(World::troopRecord(other) + 1)) {
        there = there || other == troop;
    }
    MTT_CHECK(there);
}

MTT_TEST("orders: an army troop takes up espionage, sets off for the nearest fort, then attacks") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    World w(*program);
    Panel panel(g);
    MapScreen map(g, w, panel);
    map.setUp();
    w.setU8(0x2A, 0x10);
    const std::size_t sietch = World::placeRecord(14);
    w.enterPlace(sietch);
    w.goTo((w.room() & 0xFF00) | 2, w.place());
    const int troop = w.u8(sietch + 9);
    MTT_CHECK(troop != 0);
    w.enlist(troop);
    const std::size_t t = World::troopRecord(troop);
    w.setOccupation(troop, 4);

    FakeHost host;
    Talk talk(g, w, panel, host);
    SpiceMap spice(g, w, panel, map);
    Orders orders(g, w, panel, map, talk, spice);
    int sent = 0;
    orders.sent = [&] { ++sent; };
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
    run([&] { orders.open(troop); });
    run([&] { orders.command(Orders::kOccupation); });
    MTT_CHECK(panel.commands().size() == 5 && panel.commands()[1].action == Orders::kEspionage);
    // The fort nearest the troop's place that Paul has not found.
    w.learnPlace(sietch);
    const std::size_t fort = w.u16(0xE4);
    MTT_CHECK(fort != 0 && fort != sietch && w.u8(fort + 8) >= 0x28);
    run([&] { orders.command(Orders::kEspionage); });
    MTT_CHECK(sent == 1 && !orders.active());
    MTT_CHECK((w.u8(t + 3) & 0x0F) == 5 && (w.u8(t + 3) & 0x40) && w.u16(t + 4) == fort);

    // On its way and out of Paul's reach, it can only be left.
    run([&] { orders.open(troop); });
    MTT_CHECK(panel.commands().size() == 2 && panel.commands()[0].action == Orders::kNextTroop);
    // Within reach, its destination can be changed, or it is left.
    w.setU16(0x1176, 0xFFE2);
    run([&] { orders.command(Orders::kNextTroop); }); // the only one: nothing changes
    MTT_CHECK(orders.troop() == troop);
    run([&] { orders.command(Orders::kLeaveTroop); });
    MTT_CHECK(sent == 2 && !orders.active());
    run([&] { orders.open(troop, false); });
    MTT_CHECK(panel.commands().size() == 3 && panel.commands()[0].action == Orders::kMove &&
              panel.commands()[2].action == Orders::kCancel);
    run([&] { orders.command(Orders::kCancel); });
    MTT_CHECK(sent == 3 && !orders.active());

    // Spying, the only order left is to attack.
    run([&] { orders.open(troop); });
    run([&] { orders.command(Orders::kOccupation); });
    MTT_CHECK(panel.commands().size() == 2 && panel.commands()[0].action == Orders::kAttack);
    run([&] { orders.command(Orders::kAttack); });
    MTT_CHECK(talk.answer() == 0xFF && (w.u8(t + 3) & 0x0F) == 6);
}

MTT_TEST("orders: a troop takes equipment the sietch keeps and gives it back") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    World w(*program);
    Panel panel(g);
    MapScreen map(g, w, panel);
    map.setUp();
    w.setU8(0x2A, 4);
    const std::size_t sietch = World::placeRecord(14);
    w.enterPlace(sietch);
    w.goTo((w.room() & 0xFF00) | 2, w.place());
    const int troop = w.u8(sietch + 9);
    MTT_CHECK(troop != 0);
    w.enlist(troop);
    const std::size_t t = World::troopRecord(troop);
    // The sietch keeps a piece of the second kind.
    for (std::size_t i = 0; i < 7; ++i) {
        w.setU8(sietch + 0x14 + i, i == 1 ? 1 : 0);
    }

    FakeHost host;
    Talk talk(g, w, panel, host);
    SpiceMap spice(g, w, panel, map);
    Orders orders(g, w, panel, map, talk, spice);
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
    run([&] { orders.open(troop); });
    run([&] { orders.command(Orders::kOccupation); });
    run([&] { orders.command(Orders::kSpice); });
    MTT_CHECK(panel.commands()[2].action == Orders::kEquipment && !(panel.commands()[2].phrase & Command::kGrey));
    w.setU8(t + 0x19, 0);

    // "Done" alone is offered; the place's box is under the chief's.
    run([&] { orders.command(Orders::kEquipment); });
    MTT_CHECK(orders.equipping() && panel.commands().size() == 1 && panel.commands()[0].action == Orders::kCancel);
    const ClipRect place = orders.placeBox();
    MTT_CHECK(place.left == orders.box().left + 0x30 && place.bottom - place.top == 0x28);
    // Its first piece is taken, and "done" keeps it.
    run([&] { orders.click(place.left + 0x52, place.top + 6); });
    run([&] { orders.command(Orders::kCancel); });
    MTT_CHECK(!orders.equipping() && w.u8(t + 0x19) == 0x40 && w.u8(0x3D) == 0x40 && w.u8(0x3E) == 0);
    MTT_CHECK(w.u8(0x23) == 0x0C && panel.commands().size() == 5);

    // Given back from the chief's box; a click elsewhere is "done".
    run([&] { orders.command(Orders::kEquipment); });
    run([&] { orders.click(orders.box().left + 0x82, orders.box().top + 0x2F); });
    run([&] { orders.click(300, 120); });
    MTT_CHECK(!orders.equipping() && w.u8(t + 0x19) == 0 && w.u8(0x3D) == 0 && w.u8(0x3E) == 0x40);
}

MTT_TEST("talk: a scene's line comes from the character's list 7, said once") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    World w(*program);
    Panel panel(g);
    FakeHost host;
    Talk talk(g, w, panel, host);
    talk.subtitles = Talk::Subtitles::Below;
    w.setU8(0x2A, 0x0C);
    g.display().targetBack();
    MTT_CHECK(talk.sceneLine(0, -1, -1, -1, 0));
    const int first = talk.phrase();
    MTT_CHECK(talk.active() && talk.subtitleShown());
    MTT_CHECK(talk.command(Talk::kWhat) && talk.phrase() == first);
    // The next time Leto says the following line of the list.
    MTT_CHECK(talk.sceneLine(0, -1, -1, -1, 0) && talk.phrase() != first);
    talk.leave();
    MTT_CHECK(!talk.active() && host.ended == 0);
}

MTT_TEST("talk: Duncan proposes a shipment for the Emperor and shows the amount") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    World w(*program);
    Panel panel(g);
    FakeHost host;
    Talk talk(g, w, panel, host);
    talk.subtitles = Talk::Subtitles::Below;
    w.setU8(0x2A, 0x10);
    w.setU8(0xBF, 0xB0); // a demand waits, its news seen
    w.setU16(0xBC, 400);
    w.setU16(0xA0, 700);
    talk.start(3, -1, -1);
    // His prospecting news, then the stock with its amount beside the
    // face, then the demand.
    MTT_CHECK(talk.phrase() == 0xD9 && talk.label().empty());
    MTT_CHECK(talk.command(Talk::kTalkToMe) && talk.phrase() == 0xE8 && talk.label() == "7000 kgs");
    // In the small font, a line of seven rows filled with colour 0x46,
    // the letters in 0x2C, at (113, 89).
    const Surface& back = g.display().back();
    MTT_CHECK(back.at(113, 89) == 0x46 && back.at(113, 95) == 0x46 && back.at(112, 92) != 0x46);
    int letters = 0;
    for (int x = 113; x < 160; ++x) {
        letters += back.at(x, 91) == 0x2C;
    }
    MTT_CHECK(letters > 5);
    MTT_CHECK(talk.command(Talk::kTalkToMe) && talk.phrase() == 0xEE && talk.label().empty());
    MTT_CHECK(back.at(113, 89) != 0x46);
    // What he asked for, as a question.
    MTT_CHECK(talk.command(Talk::kTalkToMe) && talk.phrase() == 0xF5 && talk.label() == "4000 kgs");
    const auto& menu = panel.commands();
    MTT_CHECK(menu.size() == 4 && menu[0].action == Talk::kArgue && menu[1].action == Talk::kAccept &&
              menu[2].action == Talk::kRefuse && menu[3].action == Talk::kWhat);
    MTT_CHECK(!talk.command(Talk::kTalkToMe));
    // Argued with, he proposes the whole stock; accepted, it is agreed.
    MTT_CHECK(talk.command(Talk::kArgue) && talk.phrase() == 0xF6 && talk.label() == "7000 kgs");
    MTT_CHECK(w.u8(0x1A) == 1 && w.u8(0x9F) == 0); // the next question waits for its answer
    MTT_CHECK(talk.command(Talk::kAccept) && talk.phrase() == 0xFF);
    MTT_CHECK(panel.commands()[0].action == Talk::kTalkToMe);
    MTT_CHECK(w.u16(0xC0) == 700 && w.u16(0x1158) == 0xFFFF);
}

MTT_TEST("talk: the smuggler names his price, haggles and sells") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    World w(*program);
    Panel panel(g);
    FakeHost host;
    Talk talk(g, w, panel, host);
    talk.subtitles = Talk::Subtitles::Below;
    const std::size_t s = 0x10D8;
    w.setU8(0x2A, 0x20);
    w.setU8(s + 1, 2);       // his patience
    w.setU8(s + 4 + 1, 2);   // two of the second kind
    w.setU8(s + 9 + 1, 60);  // at 1200 kilos
    w.setU8(0x1141, 4);
    w.setU16(0x11F1, 0xE8);
    w.setU16(0x10B4, static_cast<std::uint16_t>(s));
    w.setU16(0x114E, 0x200);
    w.setU8(0x1E, 1);
    w.random = [](int mask) { return 1 & mask; };
    talk.start(13, -1, -1);
    // His offer, then its price beside the face, as a question.
    MTT_CHECK(talk.command(Talk::kTalkToMe) && w.u16(0x11F1) == 0xE9 && w.u8(0x9D) == 120);
    MTT_CHECK(w.u8(0x2A) == 0x3C);
    MTT_CHECK(talk.command(Talk::kTalkToMe) && talk.label() == "1200 kgs");
    MTT_CHECK(panel.commands().size() == 4 && panel.commands()[1].action == Talk::kAccept);
    // Argued with, he comes down an eighth and asks again.
    MTT_CHECK(talk.command(Talk::kArgue) && w.u8(0x1A) == 1 && w.u8(0x9D) == 105);
    for (int i = 0; i < 4 && panel.commands()[1].action != Talk::kAccept; ++i) {
        talk.command(Talk::kTalkToMe);
    }
    MTT_CHECK(talk.label() == "1050 kgs");
    // Accepted: on his bill, and in Paul's place.
    MTT_CHECK(talk.command(Talk::kAccept) && w.u8(0x9F) == 1);
    MTT_CHECK(w.u16(s + 0x0E) == 105 && w.u8(s + 5) == 1 && w.u8(0x200 + 0x15) == 1 && w.u8(0x22) == 1);
}
