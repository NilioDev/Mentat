// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/intro.hpp"

#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/attack_view.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/places.hpp"
#include "mentat/engine/talker.hpp"
#include "mentat/engine/text_box.hpp"
#include "mentat/data/string_table.hpp"
#include "mentat/gfx/font.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <algorithm>
#include <optional>
#include <cstdio>
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

constexpr std::size_t kPaulBackground = 0x1526;
constexpr std::size_t kFeydBackground = 0x154E;
constexpr std::size_t kFeydGuards = 0x155C;
constexpr std::size_t kBaronBackground = 0x153A;

// How long a portrait's first idle animation lasts, in groups of four frames.
constexpr int kLongMood = 0x1E;
// Paul's portrait file comes 0x2D after the first portrait.
constexpr int kPaulPortrait = 0x2D;
// The phrase every character says in the opening.
constexpr int kOpeningPhrase = 0x190;
constexpr int kSceneRows = 152;
constexpr std::size_t kZoomCentres = 0x27B6;
constexpr std::size_t kZoomSteps = 0x2792;
constexpr int kZoomStepTicks = 6;

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
    // Part two: the flight over the desert, the palace, the sietch and the
    // Harkonnens.
    kDesertOpen = 0x06CE,
    kDesert2Open = 0x06D3,
    kDesert3Open = 0x06D8,
    kPlantOpen = 0x06EA,
    kSkyFadeAndVideo = 0x06FC,
    kVideoOnTimer = 0x0704,
    kCorridor = 0x0972,
    kThroneRoomWithJessica = 0x098A,
    kLeto = 0x0995,
    kJessica = 0x0771,
    kSietchRoom = 0x077C,
    kChani = 0x0788,
    kZoomAndSpeak = 0x078D,
    kSpeak = 0x0798,
    kKynes = 0x07A3,
    kStilgar = 0x07C6,
    kPaul = 0x07EE,
    kPalaceOutside = 0x09A5,
    kSunsetFortress = 0x0868,
    kSunsetRun = 0x087B,
    kFeyd = 0x0886,
    kBaron = 0x09AD,
    kBaronLeaves = 0x08B6,
    kVersesOpen = 0x0711,
    kVersesPlay = 0x071D,
    kAttack = 0x0ACD,
    kSietchWithSound = 0x07E0,
    kPicture02 = 0x0737,
    kPicture04 = 0x073C,
    kPicture05 = 0x0740,
    kPicture06 = 0x0747,
    kPicture07 = 0x074B,
    kPicture08 = 0x0752,
    kPicture09 = 0x0756,
    kPicture10 = 0x075A,
    kPicture11 = 0x075E,
    kPicture13 = 0x0762,
    kPicture14 = 0x0766,
    kPicture15 = 0x076A,
};

// --- After the opening

constexpr int kCreditsTrack = 3;
constexpr int kStoryPages = 8;
// Page n's words are entry kStoryText + n of the language's command text.
constexpr std::size_t kStoryText = 0x116;
// The narrator's recordings are named like a character's, as character 0x19.
constexpr int kNarrator = 0x19;
constexpr int kPageTicks = 0xFA0;
// The music is quieter while a page shows and comes back after.
constexpr int kQuietTicks = 0x64;
constexpr std::uint8_t kQuietVolume = 0x8C;
constexpr std::uint8_t kQuietBalance = 0x8C;
constexpr int kLoudTicks = 0x190;
constexpr std::uint8_t kLoudVolume = 0xE6;
constexpr std::uint8_t kLoudBalance = 0x78;
constexpr ClipRect kStoryTextBox{0, 0x99, 0x140, 0x99 + 0x2F};
constexpr std::uint8_t kStoryTextColour = 6;
// The game's clock when the story is told, and the sky it picks.
constexpr int kStoryClock = 2;
constexpr std::size_t kSkyOfTheHour = 0x2280;
// The palace at dawn: a pause, the sky's fade to sprite 0x0C, a hold.
constexpr int kDawnPause = 0xC8;
constexpr int kDawnSky = 0x0C;
constexpr int kDawnTicks = 0x4B0;
constexpr int kDawnFadeSteps = 0x40;
constexpr int kDawnFadeTicks = 0x10;

// Which COMMAND file holds each language's text.
int commandFile(int language) {
    switch (language) {
    case kFrench: return 1;
    case kGerman: return 2;
    case kItalian: return 4;
    case kSpanish: return 5;
    default: return 0;
    }
}

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
    Intro(Game& game, int language) : g_(game), d_(game.display()), attack_(game), language_(language) {}

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
            // Until it is shown, nothing of the step reaches the screen,
            // not even the frames of a face that is already moving.
            d_.beginPrepare();
            act(step.prepare);
            const bool waited = g_.waitMusic(step.musicAfter);
            d_.endPrepare();
            if (!waited) {
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

    void credits() {
        resetStep();
        d_.setOriginRows(0);
        d_.targetBack();
        d_.clear(d_.back());
        g_.openVideo(video::kCredits);
        d_.transition(Display::kFadeThroughBlack);
        g_.playTrack(kCreditsTrack);
        d_.targetBack();
        for (;;) {
            if (g_.pollInput()) {
                break;
            }
            if (g_.videoStep()) {
                d_.showBack();
                if (g_.videoDone()) {
                    break;
                }
            } else {
                g_.platform().waitFrame();
            }
        }
        g_.closeVideo();
    }

    StoryEnd story() {
        font_.emplace(Font::parse(g_.resources().load(resource::kFont)));
        text_.emplace(StringTable::parse(g_.resources().load(resource::kFirstCommands + commandFile(language_))));
        d_.setOriginRows(0);
        g_.playTrack(kCreditsTrack);
        for (int page = 1; page <= kStoryPages; ++page) {
            resetStep();
            d_.beginPrepare();
            d_.clear(d_.target());
            drawPage(page);
            drawTextBox(d_.target(), *font_, text_->at(kStoryText + static_cast<std::size_t>(page)), kStoryTextBox,
                        kStoryTextColour);
            d_.endPrepare();
            d_.transition(Display::kFadeThroughBlack);
            d_.targetBack();
            g_.fadeMusic(kQuietTicks, kQuietVolume, kQuietBalance);
            narrate(page);
            const bool waited = !g_.pollInput() && g_.waitTicks(kPageTicks);
            g_.stopSound();
            g_.fadeMusic(kLoudTicks, kLoudVolume, kLoudBalance);
            if (!waited) {
                resetStep();
                return StoryEnd::Skipped;
            }
        }
        resetStep();
        dawn();
        return StoryEnd::Finished;
    }

private:
    void drawPage(int page) {
        switch (page) {
        case 1: space(0); break;
        case 2:
        case 6: space(0x20); break;
        case 3: desert(); break;
        case 4:
        case 7: act(kPaul); break;
        case 5: act(kBaron); break;
        default: showPicture(pictureFor(kPicture15)); break;
        }
    }

    void narrate(int page) {
        if (!g_.playVoice(Talker::voiceName(kNarrator, page, true))) {
            g_.playVoice(Talker::voiceName(kNarrator, page, false));
        }
    }

    // Sprite `n` of the sheet in use, drawn with its middle at (x, y).
    void drawCentred(int n, int x, int y) {
        const SpriteView v = g_.sheet().sprite(static_cast<std::size_t>(n));
        const int w = v.header.widthAndFlags & SpriteHeader::kWidthMask;
        drawAt(n, x - w / 2, y - v.header.height / 2);
    }

    void drawAt(int n, int x, int y) {
        drawSpriteClipped(g_.canvas(), g_.sheet().sprite(static_cast<std::size_t>(n)), x, y,
                          ClipRect{0, 0, Surface::kScreenWidth, Surface::kScreenHeight});
    }

    // Space: three strips of stars, the sun and two planets, all moved to
    // the left by `c` × `c` / 2 (the planets further), so that a larger `c`
    // shows a part further right.
    void space(int c) {
        g_.selectSheet(resource::kStars);
        const auto shift = static_cast<std::int16_t>(-((c * c) >> 1));
        drawAt(0, shift, 0);
        drawAt(1, shift + 0x130, 0);
        drawAt(2, shift + 0x260, 0);
        drawCentred(0x24, static_cast<std::int16_t>(4 * shift + 0x45), 0x4E);
        const int x = c <= 0x14 ? 4 * c + 0xF2 : static_cast<std::int16_t>(2 * shift + 0x3E2);
        drawCentred((c >> 2) + 0x25, x, 0x67);
        drawCentred(c + 3, ((static_cast<std::uint16_t>(-shift) & 0xFFFF) >> 4) + 0x80, 0x4F);
    }

    // The desert: the sky of the hour in rows of tiles, its colours set at
    // once, and the dunes in front.
    void desert() {
        const int hour = kStoryClock;
        const ProgramFile& program = g_.resources().program();
        const int sky = program.dataU8(kSkyOfTheHour + static_cast<std::size_t>(hour & 0x0F)) + ((hour >> 2) & 0x1C);
        g_.selectSheet(resource::kSky + 1);
        loadSkyPalette(sky);
        for (int i = 73; i < 73 + 151; ++i) {
            d_.palette()[static_cast<std::size_t>(i)] = d_.spare()[static_cast<std::size_t>(i)];
            d_.markChanged(i);
        }
        g_.selectSheet(resource::kSky);
        for (int row = 0; row < 4; ++row) {
            for (int x = 0; x < Surface::kScreenWidth; x += 0x28) {
                g_.drawSprite(static_cast<std::uint16_t>(row), x, row * 0x14);
            }
        }
        g_.selectSheet(resource::kDesert);
        g_.drawSprite(0, 0, 0x3C);
    }

    // The palace at dawn: it dissolves in, then its sky's colours move
    // towards the morning's (as seen from a room: colours 73 to 223), and it
    // dissolves to black.
    void dawn() {
        d_.beginPrepare();
        d_.clear(d_.target());
        showPicture(resource::kDawnPalace);
        d_.endPrepare();
        d_.transition(Display::kDissolve);
        d_.targetBack();
        g_.waitTicks(kDawnPause);
        loadSkyPalette(kDawnSky);
        skyFadeLeft_ = kDawnFadeSteps;
        g_.addTimer(kPaletteFadeTimer, kDawnFadeTicks, [this] { skyFadeTick(); });
        g_.waitTicks(kDawnTicks);
        g_.removeTimer(kPaletteFadeTimer);
        d_.clear(d_.back());
        d_.transition(Display::kDissolve);
    }

private:
    IntroEnd finish(IntroEnd end) {
        d_.endPrepare();
        d_.clear(d_.screen());
        d_.setOriginRows(0);
        g_.closeVideo();
        resetStep();
        return end;
    }

    void resetStep() {
        g_.talker().stop();
        attack_.stop();
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
        case kDesertOpen:
        case kDesert2Open:
        case kDesert3Open:
        case kPlantOpen:
        case kSkyFadeAndVideo:
        case kVideoOnTimer:
        case kCorridor:
        case kThroneRoomWithJessica:
        case kLeto:
        case kJessica:
        case kSietchRoom:
        case kChani:
        case kZoomAndSpeak:
        case kSpeak:
        case kKynes:
        case kStilgar:
        case kPaul:
        case kPalaceOutside:
        case kSunsetFortress:
        case kSunsetRun:
        case kFeyd:
        case kBaron:
        case kBaronLeaves:
        case kVersesOpen:
        case kVersesPlay:
        case kAttack:
        case kSietchWithSound:
        case kPicture02:
        case kPicture04:
        case kPicture05:
        case kPicture06:
        case kPicture07:
        case kPicture08:
        case kPicture09:
        case kPicture10:
        case kPicture11:
        case kPicture13:
        case kPicture14:
        case kPicture15:
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
        case kDesertOpen:
            openRaisedVideo(video::kDesert1);
            return true;
        case kDesert2Open:
            openRaisedVideo(video::kDesert2);
            return true;
        case kDesert3Open:
            g_.talker().stop();
            skyFadeLeft_ = 0x30;
            loadSkyPalette(0x12);
            openRaisedVideo(video::kDesert3);
            return true;
        case kPlantOpen:
            openRaisedVideo(video::kPlant);
            return true;
        case kSkyFadeAndVideo:
            skyFadeLeft_ = 0x3F;
            g_.addTimer(kPaletteFadeTimer, 16, [this] { skyFadeTick(); });
            startVideoTimer();
            return true;
        case kVideoOnTimer:
            startVideoTimer();
            return true;
        case kCorridor:
            d_.clear(d_.target());
            showRoom(0x2002, 0x0180, 0);
            return true;
        case kThroneRoomWithJessica:
            showRoom(0x2004, 0x0180, 0x0002);
            return true;
        case kLeto:
            showRoom(0x200A, 0x0180, 0);
            showCharacter(0, 0, kLongMood);
            return true;
        case kJessica:
            showRoom(0x2004, 0x0180, 0);
            showCharacter(1, 0, kLongMood);
            return true;
        case kSietchRoom:
            d_.clear(d_.target());
            showRoom(0x0803, 0x1080, g_.places().present);
            return true;
        case kChani:
            showCharacter(7, 0, kLongMood);
            return true;
        case kZoomAndSpeak:
            if (&d_.back() != &d_.screen()) {
                d_.flush();
            }
            if (!g_.talker().speaking()) {
                zoomOnFace(g_.talker().character());
            }
            return speak();
        case kSpeak:
            return speak();
        case kKynes:
            g_.selectSheet(resource::kSunrise);
            drawSpriteListAt(kSunriseSprites);
            g_.drawSprite(1, 0x54, 0x0B);
            d_.saveBackground();
            showCharacter(6, 0, kLongMood);
            return true;
        case kStilgar:
            showRoom(0x0802, 0x1080, 0x0100);
            g_.places().present = 0;
            showCharacter(5, 0, kLongMood);
            return true;
        case kPaul:
            g_.selectSheet(resource::kBackgrounds);
            drawSpriteListAt(kPaulBackground);
            d_.saveBackground();
            showCharacter(kPaulPortrait, 0, 1, 1);
            return true;
        case kPalaceOutside:
            d_.clear(d_.target());
            showPicture(resource::kPalacePicture);
            return true;
        case kSunsetFortress:
            sunrisePicture(0x0B);
            g_.drawSprite(7, 0x13, 0x4A);
            return true;
        case kSunsetRun:
            g_.addTimer(kSunriseTimer, 16, [this] { sunriseTick(); });
            return true;
        case kFeyd:
            g_.selectSheet(resource::kBackgrounds);
            fillScene(0xDE);
            drawSpriteListAt(kFeydBackground);
            drawClippedListAt(kFeydGuards);
            d_.saveBackground();
            showCharacter(10, 0x3A, kLongMood);
            return true;
        case kBaron:
            g_.selectSheet(resource::kBackgrounds);
            drawSpriteListAt(kBaronBackground);
            d_.saveBackground();
            showCharacter(9, 0x52, kLongMood);
            return true;
        case kBaronLeaves:
            return baronLeaves();
        case kVersesOpen:
            openRaisedVideo(video::kVerses);
            return true;
        case kVersesPlay:
            return playVerses();
        case kAttack:
            attack_.start();
            return true;
        case kSietchWithSound:
            // The original loads sound 4 here for later; ours are read
            // when played.
            showRoom(0x0804, 0x1080, g_.places().present);
            return true;
        case kPicture02:
        case kPicture04:
        case kPicture06:
        case kPicture08:
        case kPicture09:
        case kPicture10:
        case kPicture11:
        case kPicture13:
        case kPicture14:
            showPicture(pictureFor(action));
            return true;
        case kPicture05:
        case kPicture07:
        case kPicture15:
            d_.clear(d_.target());
            showPicture(pictureFor(action));
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

    // --- Part two

    // The flight videos sit at the opening's origin whatever the target.
    void openRaisedVideo(int slot) { g_.openVideo(slot); }

    // From now on a new frame of the video is shown on every tick.
    void startVideoTimer() {
        g_.addTimer(kVideoTimer, 0, [this] { videoTick(); });
        videoTick();
    }

    void videoTick() {
        d_.targetBack();
        if (g_.videoStep()) {
            d_.copyRect(d_.back(), d_.screen(), ClipRect{0, 0, 320, kSceneRows});
        }
    }

    // Sprite `n` of the sky picture file seen from a room holds the
    // colours 73 to 223; they go into the spare palette, for the sky fade.
    void loadSkyPalette(int n) {
        const int sheet = resource::kSky + 1;
        g_.selectSheetKeepPalette(sheet);
        const SpriteView v = g_.sheet().sprite(static_cast<std::size_t>(n));
        Reader r(v.data);
        r.skip(2);
        const auto rgb = r.bytes(151 * 3);
        for (std::size_t i = 0; i < 151; ++i) {
            d_.spare()[73 + i] = Colour{rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]};
        }
    }

    void skyFadeTick() {
        d_.fadeStep(73, 151, skyFadeLeft_);
        if (--skyFadeLeft_ <= 0) {
            skyFadeLeft_ = 0;
            g_.removeTimer(kPaletteFadeTimer);
        }
    }

    // Goes to a room and draws it, with `present` standing in it, as the
    // new background.
    void showRoom(int room, int place, std::uint16_t present) {
        g_.places().present = present;
        g_.places().enter(room, place);
        g_.indoors = true;
        g_.talker().stop();
        g_.places().drawView();
        d_.saveBackground();
    }

    void showCharacter(int character, int shiftRight, int mood, int variant = 0) {
        g_.talker().show(character, shiftRight, variant, mood);
    }

    bool speak() {
        g_.talker().speak(kOpeningPhrase);
        return true;
    }

    // A close-up on the face of the character shown: the program lists the
    // zoom levels to show in turn (0xFF: hold for 300 ticks; 0: done), each
    // for at least 6 ticks, around a point it keeps per character.
    void zoomOnFace(int character) {
        const ProgramFile& program = g_.resources().program();
        const std::size_t centre = kZoomCentres + 4 * static_cast<std::size_t>(character);
        const int cx = program.dataU16(centre);
        const int cy = program.dataU16(centre + 2);
        if (cx != 0 || cy != 0) {
            Platform& p = g_.platform();
            for (std::size_t at = kZoomSteps;; ++at) {
                const int level = program.dataU8(at);
                if (level == 0) {
                    break;
                }
                if (level >= 0x80) {
                    waitTicksOnly(300);
                    continue;
                }
                const std::uint32_t start = p.ticks;
                d_.zoom(level, std::max(cx - Display::zoomWidth(level) / 2, 0), std::max(cy - Display::zoomHeight(level) / 2, 0));
                waitTicksOnly(static_cast<int>(kZoomStepTicks - (p.ticks - start)));
                if (at > kZoomSteps + 16) {
                    throw FormatError("program: zoom list does not end");
                }
            }
        }
        d_.copyRect(d_.back(), d_.screen(), ClipRect{0, 0, 320, kSceneRows});
    }

    // Waits without looking at the input or running the timers.
    void waitTicksOnly(int ticks) {
        Platform& p = g_.platform();
        const std::uint32_t start = p.ticks;
        while (static_cast<int>(p.ticks - start) < ticks) {
            p.waitFrame();
        }
    }

    std::vector<std::uint16_t> readList(std::size_t at) {
        std::vector<std::uint16_t> list;
        const ProgramFile& program = g_.resources().program();
        for (;; at += 2) {
            list.push_back(program.dataU16(at));
            if ((list.size() - 1) % 3 == 0 && list.back() == 0xFFFF) {
                return list;
            }
            if (list.size() > 300) {
                throw FormatError("program: sprite list does not end");
            }
        }
    }

    void drawSpriteListAt(std::size_t at) { g_.drawSpriteList(readList(at)); }

    // A list of plain sprite numbers, each clipped to the scene.
    void drawClippedListAt(std::size_t at) {
        const auto list = readList(at);
        const Canvas canvas = g_.canvas();
        for (std::size_t i = 0; i + 2 < list.size(); i += 3) {
            drawSpriteClipped(canvas, g_.sheet().sprite(list[i]), static_cast<std::int16_t>(list[i + 1]),
                              static_cast<std::int16_t>(list[i + 2]), g_.sceneTables().lineClip);
        }
    }

    void fillScene(std::uint8_t colour) {
        const ClipRect c = g_.sceneTables().lineClip;
        Surface& t = d_.target();
        for (int y = c.top; y < c.bottom; ++y) {
            for (int x = c.left; x < c.right; ++x) {
                t.set(x, y + d_.originRows(), colour);
            }
        }
    }

    static int pictureFor(std::uint16_t action) { return resource::kFirstIntroPicture + pictureNumber(action); }

    static int pictureNumber(std::uint16_t action) {
        switch (action) {
        case kPicture02: return 0;
        case kPicture04: return 1;
        case kPicture05: return 2;
        case kPicture06: return 3;
        case kPicture07: return 4;
        case kPicture08: return 5;
        case kPicture09: return 6;
        case kPicture10: return 7;
        case kPicture11: return 8;
        case kPicture13: return 9;
        case kPicture14: return 10;
        default: return 11;
        }
    }

    // A full picture: sprite 0 of its file at the top left.
    void showPicture(int res) {
        g_.selectSheet(res);
        g_.drawSprite(0, 0, 0);
    }

    // The video plays to its end with sound effect 8, which stops at its
    // eleventh frame; each frame is copied to the screen.
    bool playVerses() {
        g_.playSound(8);
        d_.targetBack();
        for (;;) {
            if (g_.videoStep()) {
                d_.copyRect(d_.back(), d_.screen(), ClipRect{0, 0, 320, kSceneRows});
                if (g_.videoFrame() == 11) {
                    g_.stopSound();
                }
                if (g_.videoDone()) {
                    return true;
                }
            }
            if (g_.pollInput()) {
                return false;
            }
            g_.platform().waitFrame();
        }
    }

    // The Baron's two halves slide in from the left, drawn straight on the
    // screen over the kept background.
    bool baronLeaves() {
        g_.selectSheet(resource::kBackgrounds);
        d_.saveBackground();
        const ClipRect scene = g_.sceneTables().lineClip;
        for (int x = -0x60; x <= 0; x += 0x20) {
            d_.copyRect(d_.saved(), d_.back(), scene);
            const Canvas canvas = g_.canvas();
            drawSpriteClipped(canvas, g_.sheet().sprite(5), x, 0x0D, scene);
            drawSpriteClipped(canvas, g_.sheet().sprite(6), x, 0x0D, scene);
            d_.copyRect(d_.back(), d_.screen(), scene);
        }
        return true;
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
    AttackView attack_;
    int language_;
    int subtitle_ = 0;
    int sunriseIndex_ = 0;
    int fadeLeft_ = 0;
    int skyFadeLeft_ = 0;
    std::optional<Font> font_;
    std::optional<StringTable> text_;
};

} // namespace

IntroEnd runIntro(Game& game, int language) { return Intro(game, language).run(); }

void runCredits(Game& game) { Intro(game, kEnglish).credits(); }

StoryEnd runStory(Game& game, int language) { return Intro(game, language).story(); }

} // namespace mentat
