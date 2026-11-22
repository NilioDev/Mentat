// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.hpp"

#include "game_data.hpp"
#include "mentat/engine/fiber.hpp"
#include "mentat/engine/globe.hpp"
#include "mentat/gfx/surface.hpp"
#include "mentat/engine/platform.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

using namespace mentat;

MTT_TEST("fiber: runs in lock step and stops cleanly") {
    std::vector<int> log;
    Platform platform;
    {
        Fiber fiber([&] {
            for (int i = 0;; ++i) {
                log.push_back(i);
                platform.waitFrame();
            }
        });
        platform.fiber = &fiber;
        fiber.resume();
        MTT_CHECK(log.size() == 1);
        fiber.resume();
        fiber.resume();
        MTT_CHECK(log.size() == 3);
        MTT_CHECK(!fiber.finished());
    }
    MTT_CHECK(log.size() == 3);
}

MTT_TEST("fiber: passes on an error and finishes") {
    Fiber fiber([] { throw std::runtime_error("x"); });
    MTT_CHECK_THROWS(fiber.resume());
    MTT_CHECK(fiber.finished());
    fiber.resume(); // nothing left to run
}

MTT_TEST("platform: queued sound counts at once") {
    Platform platform;
    platform.queueSound(SoundChannel::Digital, 1000, std::vector<std::uint8_t>(500, 0x80));
    MTT_CHECK(platform.soundQueued[0] == 0.5);
    platform.stopSound(SoundChannel::Digital);
    MTT_CHECK(platform.soundQueued[0] == 0);
    MTT_CHECK(platform.sound.size() == 2);
}

#include "game_data.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/engine/videos.hpp"

namespace {

// Runs a video with a pretend app at 70 frames per second whose sound
// device plays exactly in real time. Returns the seconds it took.
double timeVideo(DatArchive& archive, const ProgramFile& program, int index) {
    Platform platform;
    bool done = false;
    Fiber fiber([&] {
        playVideo(platform, archive, program, index);
        done = true;
    });
    platform.fiber = &fiber;
    double seconds = 0;
    const double frame = 1.0 / 70;
    while (!done && seconds < 600) {
        seconds += frame;
        platform.ticks = static_cast<std::uint32_t>(seconds * kTicksPerSecond);
        auto& queued = platform.soundQueued[0];
        queued = queued > frame ? queued - frame : 0;
        fiber.resume();
        platform.sound.clear();
    }
    return seconds;
}

} // namespace

MTT_TEST("real data: videos take as long as in the original") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game data");
    }
    // VIRGIN: 189 frames, 16 ticks apart, and each frame waits for the next
    // app frame after it is due (at most 1/70 s late).
    const double virgin = timeVideo(*archive, *program, video::kVirgin);
    MTT_CHECK(virgin > 188 * 16 / kTicksPerSecond);
    MTT_CHECK(virgin < 189 * (16 / kTicksPerSecond + 1.0 / 70));
    // IRULAN is paced by its sound: about 104 seconds of speech and music.
    const double irulan = timeVideo(*archive, *program, video::kIrulan);
    MTT_CHECK(irulan > 100 && irulan < 115);
}

MTT_TEST("real data: the planet fills a round area with map colours") {
    auto archive = test::openGameData();
    if (!archive) {
        MTT_SKIP("no game data");
    }
    Globe globe(archive->load("TABLAT.BIN"), archive->load("GLOBDATA.HSQ"), archive->load("MAP.HSQ"));
    MTT_CHECK(Globe::viewTilt(-4) == -32 && Globe::viewTilt(0) == 32 && Globe::viewTilt(-50) == -50);
    globe.setView(39, -32);
    Surface s(320, 200);
    int lines = 1;
    while (!globe.drawLine(s)) {
        ++lines;
    }
    MTT_CHECK(lines == 110); // 55 + 54 lines and the call that ends the picture
    int minX = 320, maxX = -1, minY = 200, maxY = -1;
    bool coloursOk = true;
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 320; ++x) {
            const int c = s.at(x, y);
            if (c != 0) {
                coloursOk = coloursOk && c >= 0x10 && c < 0x2C;
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
        }
    }
    MTT_CHECK(coloursOk);
    MTT_CHECK(minX + maxX == 319); // as wide on both sides of the middle
    MTT_CHECK(minY == Globe::kCentreY - 54 && maxY == Globe::kCentreY + 54);
    globe.turn(Globe::kTurn + 1);
    MTT_CHECK(globe.rotation() == 40);
}
