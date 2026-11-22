// SPDX-License-Identifier: GPL-3.0-or-later
//
// What every part of the game shares: the platform, the numbered
// resources, the display, the music, the timers that run while the game
// waits, and the sprite sheet in use. See docs/rules/waiting.md.
#pragma once

#include "mentat/audio/opl.hpp"
#include "mentat/engine/display.hpp"
#include "mentat/engine/hnm_player.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/engine/resources.hpp"
#include "mentat/data/voc.hpp"
#include "mentat/gfx/scene.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mentat {

class Places;
class Talker;

class Game {
public:
    // `music` may be null: the game then runs without music, as the
    // original does without a sound card.
    Game(Platform& platform, DatArchive& archive, const ProgramFile& program, const DisplayTables& displayTables,
         MusicSystem* music);
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    Platform& platform() { return platform_; }
    Resources& resources() { return resources_; }
    Display& display() { return display_; }
    const SceneTables& sceneTables() const { return sceneTables_; }
    Canvas canvas() { return Canvas(display_.target(), display_.originRows()); }

    Talker& talker() { return *talker_; }
    Places& places() { return *places_; }

    // A random number from the original's generator, masked with `mask`.
    int random(int mask);
    // A number from the original's second generator (all 16 bits).
    int random2();
    // Whether the characters speak in a room (their recordings have an
    // inside and an outside version).
    bool indoors = true;

    // --- Sprite sheets
    // Makes a picture file the one sprites are drawn from. Its palette goes
    // into the display's palette the first time it is chosen after another
    // picture file's palette was used.
    void selectSheet(int resource);
    // Chooses a picture file without touching the palette.
    void selectSheetKeepPalette(int resource);
    const SpriteSheet& sheet();
    int sheetResource() const { return sheet_; }
    void forgetPalette() { paletteSheet_ = -1; }
    // Draws sprite `word` of the sheet in use on the target.
    void drawSprite(std::uint16_t word, int x, int y);
    // Draws a list of (sprite word, x, y) entries, ending with 0xFFFF.
    void drawSpriteList(std::span<const std::uint16_t> list);

    // --- Music
    bool hasMusic() const { return music_ != nullptr; }
    // Plays track `n` (1..10); nothing changes when it plays already.
    void playTrack(int n);
    void stopMusic();
    // Moves the music's volume to `volume` and `balance` (0x78 is the
    // middle) over `ticks` timer ticks, as the player's fade does.
    void fadeMusic(int ticks, std::uint8_t volume, std::uint8_t balance);
    bool musicPlaying() const;
    int musicMeasure() const;
    // 16 steps per measure; what the opening waits for.
    int musicPosition() const;
    int track() const { return track_; }
    // When set, a track plays once and stops; otherwise it repeats.
    bool playTracksOnce = true;

    // --- Timers: run while the game waits for input.
    using TimerFn = std::function<void()>;
    void addTimer(int id, int intervalTicks, TimerFn fn);
    void removeTimer(int id);
    void removeAllTimers() { timers_.clear(); }

    // --- Waiting
    // Checks for a key or a click; when there is none, runs the timers.
    // True when the player pressed something.
    bool pollInput();
    // Waits `ticks` timer ticks. False when the player pressed something.
    bool waitTicks(int ticks);
    // Waits until the music reaches `position`. Returns at once without
    // music. False when the player pressed something.
    bool waitMusic(int position);
    // Waits `ticks` when no music plays; when music plays it does not wait.
    // A voice that is speaking is waited for instead.
    bool pause(int ticks);

    // --- Sound effects
    // Plays effect `n` (resource 0xAE + n) on the digital channel.
    // A part marked to repeat plays until stopSound().
    void playSound(int n);
    void stopSound();
    // Plays the sound file `name` of DUNE.DAT on the digital channel.
    // False when there is no such file.
    bool playVoice(const std::string& name);

    // --- Videos (one at a time, like the original)
    // Opens video slot `index` and draws its first frame on the target;
    // its palette goes into the display's palette without being sent.
    void openVideo(int index);
    // Shows the next frame when it is due, drawing on the target. True when
    // a frame was drawn.
    bool videoStep();
    // Frames shown since the video was opened (1 after opening).
    int videoFrame() const;
    bool videoDone() const;
    void closeVideo();

private:
    struct Timer {
        int id = 0;
        std::uint32_t interval = 0;
        std::uint32_t count = 0;
        TimerFn fn;
    };
    void runTimers();
    void keepLooping();

    Platform& platform_;
    Resources resources_;
    Display display_;
    SceneTables sceneTables_;
    MusicSystem* music_;
    int track_ = 0;
    int sheet_ = -1;
    int paletteSheet_ = -1;
    std::vector<Timer> timers_;
    std::uint32_t timersRan_ = 0;
    std::unique_ptr<HnmPlayer> video_;
    std::uint16_t randomSeed_ = 0;
    std::uint16_t randomSeed2_ = 0;
    VocPart looping_;
    std::unique_ptr<Talker> talker_;
    std::unique_ptr<Places> places_;
};

} // namespace mentat
