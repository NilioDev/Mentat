// SPDX-License-Identifier: GPL-3.0-or-later
//
// mentat-run: runs the opening (and, once a key skips it, the credits
// and the story) without a window on a simulated clock and
// writes the screen as PNG files at chosen moments, to compare with the
// original.
//
//   mentat-run GAME-FOLDER out-folder [--every SECONDS] [--until SECONDS] [--press SECONDS]...
//
// Output stays on the developer's machine; never commit it.
#include "mentat/audio/music.hpp"
#include "mentat/audio/opl.hpp"
#include "mentat/data/program_file.hpp"
#include "mentat/engine/fiber.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/intro.hpp"
#include "mentat/engine/play.hpp"
#include "mentat/game_files.hpp"
#include "mentat/gfx/png.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace mentat;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: mentat-run GAME-FOLDER out-folder [--every S] [--until S] [--press S]...\n"
                     "       [--mouse S,X,Y]... [--click S,X,Y]...\n");
        return 2;
    }
    double every = 2;
    double until = 600;
    std::vector<double> presses;
    // Mouse moves and clicks: at a time, to a place; a click holds the
    // button for a tenth of a second.
    struct MouseStep {
        double at;
        int x;
        int y;
        bool click;
    };
    std::vector<MouseStep> mouse;
    for (int i = 3; i + 1 < argc; i += 2) {
        const std::string opt = argv[i];
        if (opt == "--mouse" || opt == "--click") {
            double at = 0;
            int x = 0;
            int y = 0;
            if (std::sscanf(argv[i + 1], "%lf,%d,%d", &at, &x, &y) != 3) {
                std::fprintf(stderr, "mentat-run: %s takes S,X,Y\n", opt.c_str());
                return 2;
            }
            mouse.push_back(MouseStep{at, x, y, opt == "--click"});
            continue;
        }
        const double v = std::stod(argv[i + 1]);
        if (opt == "--every") {
            every = v;
        } else if (opt == "--until") {
            until = v;
        } else if (opt == "--press") {
            presses.push_back(v);
        }
    }
    try {
        const auto files = findGameFiles({argv[1]});
        if (!files || files->program.empty()) {
            std::fprintf(stderr, "mentat-run: no DUNE.DAT and DNCDPRG.EXE in %s\n", argv[1]);
            return 1;
        }
        DatArchive dat = DatArchive::open(files->dat);
        const ProgramFile program = ProgramFile::open(files->program);
        const DisplayTables displayTables = DisplayTables::fromDriver(dat.load("DNVGA.HSQ"));
        MusicSystem music(MusicTables::load(dat, MusicCard::AdLibGold));
        Platform platform;
        platform.music = &music;
        Game game(platform, dat, program, displayTables, &music);
        IntroEnd end = IntroEnd::Finished;
        Fiber fiber([&] {
            while ((end = runIntro(game, kEnglish)) == IntroEnd::Finished) {
            }
            runCredits(game);
            runStory(game, kEnglish);
            runGame(game, kEnglish);
        });
        platform.fiber = &fiber;
        const std::filesystem::path out = argv[2];
        std::filesystem::create_directories(out);
        double t = 0;
        double nextShot = 0;
        const double frame = 1.0 / 70;
        std::size_t pressed = 0;
        while (!fiber.finished() && t < until) {
            platform.ticks = static_cast<std::uint32_t>(t * kTicksPerSecond);
            for (double& q : platform.soundQueued) {
                q = q > frame ? q - frame : 0;
            }
            platform.mouseButtons = 0;
            for (const MouseStep& m : mouse) {
                if (t >= m.at) {
                    platform.mouseX = m.x;
                    platform.mouseY = m.y;
                }
                if (m.click && t >= m.at && t < m.at + 0.1) {
                    platform.mouseButtons = 1;
                }
            }
            if (pressed < presses.size() && t >= presses[pressed]) {
                platform.keys.push_back(KeyPress{0x39, ' '});
                ++pressed;
            }
            platform.sound.clear();
            fiber.resume();
            if (t >= nextShot) {
                char name[32];
                std::snprintf(name, sizeof name, "m%05.1f.png", t);
                Surface shown = platform.screen;
                if (platform.cursorShown) {
                    platform.cursor.draw(shown, platform.mouseX, platform.mouseY);
                }
                writePng(out / name, shown, platform.palette);
                nextShot += every;
            }
            t += frame;
        }
        std::printf("%s after %.1f s (%u ticks)\n", end == IntroEnd::Skipped ? "skipped" : "ended", t,
                    platform.ticks);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "mentat-run: %s\n", e.what());
        return 1;
    }
}
