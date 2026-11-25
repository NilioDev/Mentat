// SPDX-License-Identifier: GPL-3.0-or-later
//
// What every part of the game shares: the platform, the numbered
// resources, the display, the music, the timers that run while the game
// waits, and the sprite sheet in use. See docs/rules/waiting.md.
#pragma once

#include "mentat/audio/music_system.hpp"
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
    // The second face of a pair shown together (see Talker::setCompanion).
    Talker& companion() { return *companion_; }
    Places& places() { return *places_; }

    // A random number from the original's generator, masked with `mask`.
    int random(int mask);
    // A number from the original's second generator (all 16 bits).
    int random2();
    // A number from 0 to `most` from the original's third generator.
    int random3(int most);
    // Starts the generators again from `seed`, as a new game does.
    void seedRandom(std::uint16_t seed) {
        randomSeed_ = seed;
        randomSeed2_ = seed;
        randomSeed3_ = seed;
    }
    // When not negative, the seed a new game uses instead of the time of
    // day (for comparing runs).
    int fixedSeed = -1;
    // Whether the characters speak in a room (their recordings have an
    // inside and an outside version).
    bool indoors = true;
    // The words of the last text laid out in a box; how long a face shown
    // next stays calm goes by it.
    int textWords = 0;
    // How characters' lines reach the player: -1 as the original chooses,
    // else 0 subtitles, 1 voice and text, 2 voice only.
    int subtitles = -1;

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

    // --- The language and the mixer panel's settings (see
    // docs/rules/mixer.md)
    // 0 American, 1 French, 2 German, 3 English, 4 Italian, 5 Spanish,
    // 6 Fremen: the files of text and the font go by it.
    int language = 3;
    // The font of the language: the Fremen one has its own letters.
    int fontFile() const;
    // The volumes (0 to 255) and balances (0x78 the middle) of the voices,
    // of the music, and of the music while a voice speaks.
    struct SoundLevels {
        std::uint8_t voices = 0xFF;
        std::uint8_t voicesBalance = 0x78;
        std::uint8_t music = 0xE6;
        std::uint8_t musicBalance = 0x78;
        std::uint8_t quietMusic = 0xB4;
        std::uint8_t quietBalance = 0x78;
    };
    SoundLevels levels;
    // The voices' volume goes to the digital channel.
    void applyVoiceLevels();
    // The music goes to its volume at once.
    void applyMusicLevels();
    // A voice starts: the music fades to its quiet volume, and back when
    // the voice has ended.
    void quietenForVoice();
    // "MUSIC OFF": no track plays until it is turned on again.
    bool musicOff = false;
    // How the music goes: 0 with the game; bit 0 as a CD plays, one track
    // after another; bit 1 in a shuffled order.
    int musicStyle = 0;

    // --- Music
    bool hasMusic() const { return music_ != nullptr; }
    // Plays track `n` (1..10); nothing changes when it plays already.
    void playTrack(int n);
    void stopMusic();
    // Plays the music on `next` from now on (null: none): a piece that was
    // playing starts again there, at the volume of the moment.
    void switchMusic(MusicSystem* next);
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
    // A part marked to repeat plays its count, or until stopSound() when
    // it has none.
    void playSound(int n);
    // The part repeating now ends after the play queued, and the next part
    // follows.
    void endSoundRepeat();
    void stopSound();
    // Waits until the digital channel has played everything queued. False
    // when the player pressed something.
    bool waitSound();
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
    // Ends the music's quiet once the voice has been heard.
    void checkVoiceEnded();
    bool quietForVoice_ = false;

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
    std::uint16_t randomSeed3_ = 0;
    bool repeating() const { return soundPart_ < soundParts_.size(); }
    std::vector<VocPart> soundParts_;
    std::size_t soundPart_ = 0;
    int playsLeft_ = 0;
    std::unique_ptr<Talker> talker_;
    std::unique_ptr<Talker> companion_;
    std::unique_ptr<Places> places_;
};

} // namespace mentat
