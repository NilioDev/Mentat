// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/engine/display.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/intro.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/places.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/engine/world.hpp"

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
    Panel panel(g, kEnglish);
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
