// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.hpp"

#include "mentat/engine/fiber.hpp"
#include "mentat/engine/platform.hpp"

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
