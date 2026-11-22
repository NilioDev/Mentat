// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/engine/display.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/places.hpp"
#include "mentat/engine/talker.hpp"
#include "mentat/gfx/portrait.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

using namespace mentat;

MTT_TEST("talker: voice names follow the character, the phrase and inside or outside") {
    MTT_CHECK(Talker::voiceName(7, 0x190, true) == "PH\\PH190I.VOC");
    MTT_CHECK(Talker::voiceName(0, 0x01A, false) == "PA\\PA01AO.VOC");
}

MTT_TEST("display: zooming enlarges the part around a place") {
    Platform p;
    Display d(p, DisplayTables{});
    MTT_CHECK(Display::zoomWidth(1) == 280 && Display::zoomHeight(1) == 133);
    MTT_CHECK(Display::zoomWidth(4) == 160 && Display::zoomHeight(4) == 76);
    MTT_CHECK(Display::zoomWidth(7) == 40 && Display::zoomHeight(7) == 19);
    for (int y = 0; y < 152; ++y) {
        for (int x = 0; x < 320; ++x) {
            d.back().set(x, y, static_cast<std::uint8_t>((x + 3 * y) & 0xFF));
        }
    }
    d.zoom(4, 40, 10);
    bool same = true;
    for (int r = 0; r < 152; ++r) {
        for (int c = 0; c < 320; ++c) {
            same = same && d.screen().at(c, r) == d.back().at(40 + c / 2, 10 + r / 2);
        }
    }
    MTT_CHECK(same);
}

MTT_TEST("game: the second random generator") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game data");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    MTT_CHECK(g.random2() == 0);
    MTT_CHECK(g.random2() == 0xCB);
    MTT_CHECK(g.random2() == 0x45EC);
    MTT_CHECK(g.random2() == 0x403A);
}

MTT_TEST("game: a repeating sound keeps playing until stopped") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game data");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    constexpr int kBattle = 3;
    g.playSound(kBattle);
    double& queued = p.soundQueued[static_cast<int>(SoundChannel::Digital)];
    queued = 0;
    g.pollInput();
    MTT_CHECK(queued > 0);
    g.stopSound();
    MTT_CHECK(queued == 0);
    g.pollInput();
    MTT_CHECK(queued == 0);
}

MTT_TEST("real data: every portrait's animation reads") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game data");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    for (int character = 0; character < 14; ++character) {
        const SpriteSheet& sheet = g.resources().sheet(0x02 + character);
        const PortraitAnimation a = PortraitAnimation::parse(sheet);
        const ClipRect area = a.area();
        MTT_CHECK(area.left >= 0 && area.right <= 320 && area.top >= 0 && area.bottom <= 200);
        MTT_CHECK(area.left < area.right && area.top < area.bottom);
        MTT_CHECK(a.sequenceCount() > 0);
        for (int n = 0; n < a.sequenceCount(); ++n) {
            std::size_t at = a.sequenceStart(n);
            int frames = 0;
            while (!a.endsAt(at) && frames < 1000) {
                for (const PortraitPiece& piece : a.readFrame(at)) {
                    MTT_CHECK(piece.sprite >= 0 && static_cast<std::size_t>(piece.sprite) < sheet.size());
                }
                ++frames;
            }
            MTT_CHECK(frames > 0 && frames < 1000);
        }
    }
}

MTT_TEST("real data: the places table reads") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game data");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    MTT_CHECK(g.places().records().size() > 50);
}
