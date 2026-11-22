// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/intro.hpp"

#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/game.hpp"

#include <string>
#include <vector>

namespace mentat {

namespace {

// The opening script sits in the program's code as steps of six words.
constexpr std::size_t kScript = 0x337;
constexpr std::uint16_t kRestart = 0xFFFF;
// The scene rows sit 24 rows down the screen during the opening.
constexpr int kOpeningOrigin = 24;

// Places in the program's data.
constexpr std::size_t kSubtitleFrames = 0x35A8;
constexpr std::size_t kSunriseSprites = 0x1500;

// Timer numbers (the original's own, so a step can remove another's).
constexpr int kVideoTimer = 0x70C;
constexpr int kSunriseTimer = 0x826;
constexpr int kPaletteFadeTimer = 0x3916;

// The steps' work, named by where the original keeps it.
enum Action : std::uint16_t {
    kNothing = 0x0F66,
    kClear = 0xC0AD,
    kVirginOpen = 0x061C,
    kVirginPlay = 0x0625,
    kCryoOpen = 0x064D,
    kPlayToEnd = 0x0661,
    kCryo2Open = 0x0658,
    kPresentOpen = 0x0678,
    kPresentPlay = 0x0684,
    kIrulanOpen = 0xCEFC,
    kIrulanPlay = 0xCF1B,
    kTitleOpen = 0x069E,
    kTitlePlayPart = 0x06AA,
    kPlayOnScreen = 0x06BD,
    kSunriseOpen = 0x07FD,
    kSunriseRun = 0x085D,
};

struct Step {
    std::uint16_t musicBefore; // music position to wait for before preparing
    std::uint16_t prepare;     // what is drawn in the back buffer
    std::uint16_t musicAfter;  // music position to wait for before showing
    std::int16_t transition;   // how it is shown; negative: not at all
    std::uint16_t run;         // what runs once it shows
    std::uint16_t pause;       // ticks to wait after, when no music plays
};

std::vector<Step> readScript(const ProgramFile& program) {
    std::vector<Step> steps;
    for (std::size_t at = kScript;; at += 12) {
        if (steps.size() > 200) {
            throw FormatError("program: the opening script does not end");
        }
        const std::uint16_t first = program.codeU16(at);
        if (first == kRestart) {
            return steps;
        }
        steps.push_back(Step{first, program.codeU16(at + 2), program.codeU16(at + 4),
                             static_cast<std::int16_t>(program.codeU16(at + 6)), program.codeU16(at + 8),
                             program.codeU16(at + 10)});
    }
}

class Intro {
public:
    Intro(Game& game, int language) : g_(game), d_(game.display()), language_(language) {}

    IntroEnd run() {
        const std::vector<Step> steps = readScript(g_.resources().program());
        d_.keepPalette();
        g_.stopMusic();
        for (const Step& step : steps) {
            d_.setOriginRows(kOpeningOrigin);
            if (!g_.waitMusic(step.musicBefore)) {
                return finish(IntroEnd::Skipped);
            }
            resetStep();
            d_.keepPalette();
            if (!known(step.prepare) || !known(step.run)) {
                // Not built yet: the opening ends here for now.
                return finish(IntroEnd::Finished);
            }
            d_.prepare([&] { act(step.prepare); });
            if (!g_.waitMusic(step.musicAfter)) {
                return finish(IntroEnd::Skipped);
            }
            if (step.transition >= 0) {
                d_.transition(step.transition);
            }
            d_.targetBack();
            if (g_.pollInput() || !act(step.run)) {
                return finish(IntroEnd::Skipped);
            }
            if (step.pause != 0 && !g_.pause(step.pause)) {
                return finish(IntroEnd::Skipped);
            }
        }
        return finish(IntroEnd::Finished);
    }

private:
    IntroEnd finish(IntroEnd end) {
        d_.clear(d_.screen());
        d_.setOriginRows(0);
        g_.closeVideo();
        resetStep();
        return end;
    }

    void resetStep() {
        g_.removeTimer(kVideoTimer);
        g_.removeTimer(kPaletteFadeTimer);
        g_.removeTimer(kSunriseTimer);
    }

    static bool known(std::uint16_t action) {
        switch (action) {
        case kNothing:
        case kClear:
        case kVirginOpen:
        case kVirginPlay:
        case kCryoOpen:
        case kPlayToEnd:
        case kCryo2Open:
        case kPresentOpen:
        case kPresentPlay:
        case kIrulanOpen:
        case kIrulanPlay:
        case kTitleOpen:
        case kTitlePlayPart:
        case kPlayOnScreen:
        case kSunriseOpen:
        case kSunriseRun:
            return true;
        default:
            return false;
        }
    }

    // Returns false when the player skipped.
    bool act(std::uint16_t action) {
        switch (action) {
        case kNothing:
            return true;
        case kClear:
            d_.clear(d_.target());
            return true;
        case kVirginOpen:
            g_.stopMusic();
            g_.playTrack(6);
            g_.openVideo(video::kVirgin);
            return true;
        case kVirginPlay:
            return playToEnd(true);
        case kCryoOpen:
            g_.playTrack(10);
            g_.openVideo(video::kCryo);
            return true;
        case kPlayToEnd:
            return playToEnd(false);
        case kCryo2Open:
            d_.clear(d_.target());
            g_.openVideo(video::kCryo2);
            return true;
        case kPresentOpen:
            d_.setOriginRows(0);
            d_.clear(d_.target());
            g_.openVideo(video::kPresent);
            return true;
        case kPresentPlay:
        case kPlayOnScreen:
            return playOnScreen(-1);
        case kIrulanOpen:
            g_.selectSheet(resource::kFirstSubtitles + language_);
            subtitle_ = 0;
            d_.setOriginRows(0);
            d_.clear(d_.target());
            g_.openVideo(video::kIrulan);
            return true;
        case kIrulanPlay:
            return playIrulan();
        case kTitleOpen:
            d_.targetBack();
            d_.setOriginRows(0);
            g_.openVideo(video::kTitle);
            return true;
        case kTitlePlayPart:
            return playOnScreen(0x56);
        case kSunriseOpen:
            d_.clear(d_.target());
            sunrisePicture(8);
            return true;
        case kSunriseRun:
            g_.addTimer(kSunriseTimer, 9, [this] { sunriseTick(); });
            return true;
        default:
            throw FormatError("opening: unknown step " + std::to_string(action));
        }
    }

    // Plays the open video in the back buffer, copying each new frame to
    // the screen. The logo video also stops the music at its 8th measure.
    bool playToEnd(bool stopMusicAtMeasure8) {
        d_.targetBack();
        for (;;) {
            if (g_.pollInput()) {
                return false;
            }
            if (!g_.videoStep()) {
                g_.platform().waitFrame();
                continue;
            }
            d_.showBack();
            if (stopMusicAtMeasure8 && g_.musicMeasure() >= 8 && g_.track() != 0) {
                g_.stopMusic();
            }
            if (g_.videoDone()) {
                return true;
            }
        }
    }

    // Plays the open video straight on the screen, to its end or until it
    // reaches frame `untilFrame`.
    bool playOnScreen(int untilFrame) {
        d_.setOriginRows(0);
        d_.targetScreen();
        for (;;) {
            const bool drew = g_.videoStep();
            if (g_.pollInput()) {
                return false;
            }
            if (untilFrame >= 0 ? g_.videoFrame() == untilFrame : g_.videoDone()) {
                return true;
            }
            if (!drew) {
                g_.platform().waitFrame();
            }
        }
    }

    // Irulan's introduction, with her words written under the picture at
    // the frames the program lists.
    bool playIrulan() {
        d_.targetScreen();
        bool skipped = false;
        for (;;) {
            const std::uint16_t at = g_.resources().program().dataU16(kSubtitleFrames + 2 * static_cast<std::size_t>(subtitle_));
            if (at <= g_.videoFrame()) {
                showSubtitle(subtitle_++);
            }
            const bool drew = g_.videoStep();
            if (g_.pollInput()) {
                skipped = true;
                break;
            }
            if (g_.videoDone()) {
                break;
            }
            if (!drew) {
                g_.platform().waitFrame();
            }
        }
        g_.closeVideo();
        g_.platform().stopSound(SoundChannel::Digital);
        g_.stopMusic();
        g_.playTrack(6);
        d_.targetBack();
        return !skipped;
    }

    // Even entries show line n/2; odd ones clear the line again.
    void showSubtitle(int entry) {
        constexpr int kLineY = 190;
        if (entry % 2 == 0) {
            g_.drawSprite(static_cast<std::uint16_t>(entry / 2), 0, kLineY);
            return;
        }
        Surface& s = d_.screen();
        for (int y = kLineY; y < Surface::kScreenHeight; ++y) {
            for (int x = 0; x < Surface::kScreenWidth; ++x) {
                s.set(x, y, 0);
            }
        }
    }

    // --- The sunrise over the dunes

    // Sprite `n` of the sunrise picture file holds a palette for colours
    // 128..207; it goes into the spare palette.
    void sunrisePalette(int n) {
        g_.selectSheetKeepPalette(resource::kSunrise);
        sunriseIndex_ = n;
        const SpriteView v = g_.sheet().sprite(static_cast<std::size_t>(n));
        Reader r(v.data);
        r.skip(2);
        const auto rgb = r.bytes(80 * 3);
        for (std::size_t i = 0; i < 80; ++i) {
            d_.spare()[128 + i] = Colour{rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]};
        }
    }

    void sunrisePicture(int n) {
        sunrisePalette(n);
        fadeLeft_ = 0;
        g_.selectSheet(resource::kSunrise);
        std::vector<std::uint16_t> list;
        const ProgramFile& program = g_.resources().program();
        for (std::size_t at = kSunriseSprites;; at += 2) {
            list.push_back(program.dataU16(at));
            if ((list.size() - 1) % 3 == 0 && list.back() == 0xFFFF) {
                break;
            }
            if (list.size() > 300) {
                throw FormatError("program: sunrise sprite list does not end");
            }
        }
        g_.drawSpriteList(list);
    }

    // Every 9 ticks: moves the palette a step towards the next sunrise
    // palette; when it arrives, takes the next one.
    void sunriseTick() {
        if (fadeLeft_ == 0) {
            const int next = sunriseIndex_ + 1;
            if (next == 11) {
                g_.selectSheet(resource::kFirstPortrait + 7); // made ready for the next scene
                return;
            }
            if (next == 14) {
                return;
            }
            fadeLeft_ = next == 13 ? 10 : 30;
            sunrisePalette(next);
        }
        d_.fadeStep(73, 151, fadeLeft_);
        --fadeLeft_;
        if (fadeLeft_ == 0) {
            g_.removeTimer(kPaletteFadeTimer);
        }
    }

    Game& g_;
    Display& d_;
    int language_;
    int subtitle_ = 0;
    int sunriseIndex_ = 0;
    int fadeLeft_ = 0;
};

} // namespace

IntroEnd runIntro(Game& game, int language) { return Intro(game, language).run(); }

} // namespace mentat
