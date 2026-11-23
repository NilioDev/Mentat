// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/audio/music.hpp"
#include "mentat/engine/display.hpp"
#include "mentat/engine/fiber.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/intro.hpp"

#include <functional>

using namespace mentat;

namespace {

DisplayTables tables() {
    DisplayTables t;
    t.dissolveOrder = {0x141, 0x3C0, 0x283, 0x002, 0x140, 0x3C2, 0x000, 0x281,
                       0x003, 0x3C1, 0x142, 0x3C3, 0x282, 0x001, 0x143, 0x280};
    return t;
}

// Runs `body` as the game with a pretend app at 70 frames per second.
// Returns the ticks it took. `pressAt` presses a key at that tick.
std::uint32_t runGame(Platform& platform, const std::function<void()>& body, std::uint32_t pressAt = 0xFFFFFFFF) {
    Fiber fiber(body);
    platform.fiber = &fiber;
    double seconds = 0;
    const std::uint32_t start = platform.ticks;
    while (!fiber.finished() && seconds < 1000) {
        fiber.resume();
        platform.sound.clear();
        seconds += 1.0 / 70;
        platform.ticks = start + static_cast<std::uint32_t>(seconds * kTicksPerSecond);
        for (double& q : platform.soundQueued) {
            q = q > 1.0 / 70 ? q - 1.0 / 70 : 0;
        }
        if (platform.ticks >= pressAt) {
            platform.keys.push_back(KeyPress{0x39, ' '});
            pressAt = 0xFFFFFFFF;
        }
    }
    platform.fiber = nullptr;
    return platform.ticks - start;
}

Colour grey(std::uint8_t v) { return Colour{v, v, v}; }

// A display showing an old picture with an old palette, with a new picture
// and palette prepared the way the opening prepares its steps.
void prepareChange(Display& d) {
    for (std::size_t i = 0; i < 256; ++i) {
        d.palette()[i] = grey(10);
    }
    d.markChanged(std::bitset<256>().set());
    d.flush();
    d.screen().fill(1);
    d.keepPalette();
    std::bitset<256> changed;
    for (std::size_t i = 0; i < 256; ++i) {
        d.palette()[i] = grey(40);
        changed.set(i);
    }
    d.markChanged(changed);
    d.back().fill(2);
}

} // namespace

MTT_TEST("display: a cut copies the scene rows and sends the palette") {
    Platform p;
    Display d(p, tables());
    prepareChange(d);
    d.setOriginRows(24);
    runGame(p, [&] { d.transition(Display::kCut); });
    MTT_CHECK(p.screen.at(0, 23) == 2 && p.screen.at(319, 175) == 2); // the final copy takes all of it
    MTT_CHECK(p.palette[5] == grey(40));
}

MTT_TEST("display: fading through black takes about two seconds and ends on the new palette") {
    Platform p;
    Display d(p, tables());
    prepareChange(d);
    const std::uint32_t ticks = runGame(p, [&] { d.transition(Display::kFadeThroughBlack); });
    // 66 steps down and 66 up, 3 ticks each.
    MTT_CHECK(ticks >= 396 && ticks < 405);
    MTT_CHECK(p.screen.at(100, 100) == 2);
    MTT_CHECK(p.palette[1] == grey(40) && p.palette[254] == grey(40));
}

MTT_TEST("display: fading to black leaves the screen dark") {
    Platform p;
    Display d(p, tables());
    prepareChange(d);
    d.flush();
    runGame(p, [&] { d.transition(Display::kFadeToBlack); });
    MTT_CHECK(p.screen.at(5, 5) == 2);
    MTT_CHECK(p.palette[0] == Colour{} && p.palette[200] == Colour{});
    MTT_CHECK(d.palette()[200] == grey(40)); // kept for later
}

MTT_TEST("display: fading in from black reaches the palette") {
    Platform p;
    Display d(p, tables());
    prepareChange(d);
    runGame(p, [&] { d.transition(Display::kFadeInFromBlack); });
    MTT_CHECK(p.palette[0] == grey(40) && p.palette[254] == grey(40));
}

MTT_TEST("display: the dissolve ends on the new picture") {
    Platform p;
    Display d(p, tables());
    prepareChange(d);
    const std::uint32_t ticks = runGame(p, [&] { d.transition(Display::kDissolve); });
    MTT_CHECK(ticks >= 96 && ticks < 105); // 32 steps of 3 ticks
    MTT_CHECK(p.screen.at(0, 0) == 2 && p.screen.at(319, 199) == 2);
    MTT_CHECK_THROWS(d.transition(0x04));
}

MTT_TEST("display: the sparkle brings the picture 150 steps a tick") {
    Platform p;
    Display d(p, tables());
    prepareChange(d);
    const std::uint32_t ticks = runGame(p, [&] { d.transition(Display::kSparkle); });
    // 32767 steps of the number, two pixels each.
    MTT_CHECK(ticks >= 218 && ticks < 225);
    MTT_CHECK(p.screen.at(0, 0) == 2 && p.screen.at(319, 151) == 2);
}

MTT_TEST("display: the slide brings the new picture down from the top a row each 6 ticks") {
    Platform p;
    Display d(p, tables());
    prepareChange(d);
    const std::uint32_t ticks = runGame(p, [&] { d.transition(Display::kSlideDown); });
    // 152 rows, one every 6 ticks, or two when it falls behind.
    MTT_CHECK(ticks >= 456 && ticks <= 930);
    MTT_CHECK(p.screen.at(0, 0) == 2 && p.screen.at(319, 151) == 2);
}

MTT_TEST("display: the transitions in a rectangle end on the back picture there only") {
    Platform p;
    Display d(p, tables());
    d.screen().fill(5);
    d.back().fill(2);
    const ClipRect r{160, 0, 320, 116};
    runGame(p, [&] { d.transitionIn(Display::kUnroll, r); });
    MTT_CHECK(d.screen().at(160, 0) == 2 && d.screen().at(319, 115) == 2);
    MTT_CHECK(d.screen().at(159, 0) == 5 && d.screen().at(160, 116) == 5);
    d.back().fill(7);
    runGame(p, [&] { d.transitionIn(Display::kRollUp, r); });
    MTT_CHECK(d.screen().at(200, 0) == 7 && d.screen().at(200, 115) == 7 && d.screen().at(100, 50) == 5);
    MTT_CHECK_THROWS(d.transitionIn(0x04, r));
}

MTT_TEST("display: the rise moves the old picture up with a band under it") {
    Platform p;
    Display d(p, tables());
    prepareChange(d);
    for (int y = 0; y < 152; ++y) {
        d.screen().fillRect(0, y, 320, 1, static_cast<std::uint8_t>(100 + y));
    }
    d.back().fillRect(0, 0, 320, 200, 2);
    const std::uint32_t ticks = runGame(p, [&] { d.transition(Display::kRise, -1); });
    MTT_CHECK(ticks >= 114 && ticks < 120); // 19 steps of 6 ticks
    MTT_CHECK(p.screen.at(0, 0) == 2 && p.screen.at(319, 151) == 2);
    MTT_CHECK(p.palette[0] == grey(40));
    MTT_CHECK_THROWS(d.transition(Display::kRise));

    // Part way: after the first step the old picture is 8 rows higher.
    Display e(p, tables());
    for (int y = 0; y < 152; ++y) {
        e.screen().fillRect(0, y, 320, 1, static_cast<std::uint8_t>(100 + y));
    }
    e.back().fill(2);
    bool seen = false;
    Fiber fiber([&] { e.transition(Display::kRise, -1); });
    p.fiber = &fiber;
    fiber.resume();
    seen = p.screen.at(5, 0) == 108 && p.screen.at(5, 143) == 251 && p.screen.at(5, 144) == 7 && p.screen.at(5, 151) == 7;
    while (!fiber.finished()) {
        p.ticks += 10;
        fiber.resume();
    }
    p.fiber = nullptr;
    MTT_CHECK(seen);
}

MTT_TEST("display: the spiral reaches the whole scene a side a tick") {
    Platform p;
    Display d(p, tables());
    prepareChange(d);
    const std::uint32_t ticks = runGame(p, [&] { d.transition(Display::kSpiral); });
    MTT_CHECK(ticks >= 74 && ticks < 80);
    MTT_CHECK(p.screen.at(0, 0) == 2 && p.screen.at(319, 199) == 2);

    // The first side is the block right of the middle.
    Display e(p, tables());
    e.screen().fill(1);
    e.back().fill(2);
    Fiber fiber([&] { e.transition(Display::kSpiral); });
    p.fiber = &fiber;
    fiber.resume();
    const bool first = p.screen.at(156, 74) == 2 && p.screen.at(163, 77) == 2 && p.screen.at(164, 74) == 1 &&
                       p.screen.at(155, 74) == 1;
    while (!fiber.finished()) {
        p.ticks += 10;
        fiber.resume();
    }
    p.fiber = nullptr;
    MTT_CHECK(first);
}

MTT_TEST("display: a fade step moves part of the way") {
    Platform p;
    Display d(p, tables());
    d.palette()[10] = grey(10);
    d.spare()[10] = grey(40);
    d.fadeStep(10, 1, 3);
    MTT_CHECK(d.palette()[10] == grey(20) && p.palette[10] == grey(20));
    d.spare()[10] = grey(0);
    d.fadeStep(10, 1, 0); // 0 counts as 1
    MTT_CHECK(d.palette()[10] == grey(0));
}

MTT_TEST("game: timers run while waiting, keeping the remainder") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game data");
    }
    Platform p;
    Game g(p, *archive, *program, tables(), nullptr);
    int runs = 0;
    g.addTimer(1, 10, [&] { ++runs; });
    g.addTimer(2, 5, [&] { g.removeTimer(2); });
    bool finished = false;
    runGame(p, [&] { finished = g.waitTicks(100); });
    MTT_CHECK(finished);
    MTT_CHECK(runs >= 9 && runs <= 10);
    MTT_CHECK(g.waitMusic(0x500)); // no music: no wait
    runGame(p, [&] { finished = g.waitTicks(100); }, p.ticks + 20);
    MTT_CHECK(!finished);
}

MTT_TEST("real data: the whole opening plays, and a key skips it") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game data");
    }
    const DisplayTables dt = DisplayTables::fromDriver(archive->load("DNVGA.HSQ"));
    MTT_CHECK(dt.dissolveOrder[0] == tables().dissolveOrder[0]);
    {
        Platform p;
        MusicSystem music(MusicTables::fromPlayer(archive->load("DNADP.HSQ")));
        p.music = &music;
        Game g(p, *archive, *program, dt, &music);
        IntroEnd end = IntroEnd::Skipped;
        const std::uint32_t ticks = runGame(p, [&] { end = runIntro(g, kEnglish); });
        MTT_CHECK(end == IntroEnd::Finished);
        // The original starts it over after about 452 s.
        MTT_CHECK(ticks > 430 * kTicksPerSecond && ticks < 475 * kTicksPerSecond);
    }
    {
        Platform p;
        Game g(p, *archive, *program, dt, nullptr);
        IntroEnd end = IntroEnd::Finished;
        runGame(p, [&] { end = runIntro(g, kEnglish); }, 1000);
        MTT_CHECK(end == IntroEnd::Skipped);
        MTT_CHECK(p.screen.at(160, 100) == 0);
    }
}
