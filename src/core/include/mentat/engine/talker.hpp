// SPDX-License-Identifier: GPL-3.0-or-later
//
// A character's portrait on the screen: its idle animation, and the voice
// and moving mouth when it speaks. See docs/rules/portraits.md.
#pragma once

#include "mentat/gfx/portrait.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace mentat {

class Game;

class Talker {
public:
    explicit Talker(Game& game) : game_(game) {}

    // Shows character `character` (0 Leto … 13 the smuggler; the sheet is
    // the portrait resource plus the number) at its place, moved
    // `shiftRight` columns when the portrait starts left of that.
    // `variant` picks one of a portrait's alternative sets of animations
    // (0 for the usual one). `mood` sets how long the first idle animation
    // runs, in units of four frames. Draws on the target over the saved
    // background and starts the idle animation.
    void show(int character, int shiftRight, int variant, int mood);

    // Speaks phrase `phrase`: plays the character's recording of it and
    // moves the mouth with it. False when there is no recording. `variant`
    // picks one of the other recordings of the phrase (0 for the usual).
    // `voice` is whose recordings say it: the shown character's when
    // negative.
    bool speak(int phrase, int variant = 0, int voice = -1);
    // Plays character `character`'s recording of `phrase` with no face to
    // move. False when there is none.
    bool playVoice(int character, int phrase, int variant = 0);

    // Shows the face again from scratch: a new idle sequence (`mood` as in
    // show()), all of it drawn.
    void restart(int mood);
    // Draws the face again whole, from the start of its idle sequence.
    void redraw();
    // Stops at once, without playing the animation on.
    void halt();

    // Called after a frame is drawn on the back buffer, before it is copied
    // to the screen, with the rectangle that changed; for what lies over
    // the face, such as subtitles.
    std::function<void(const ClipRect&)> overlay;

    // Stops the voice, the animation and the timers.
    void stop();

    bool speaking() const { return speaking_; }
    bool shown() const { return character_ >= 0; }
    int character() const { return character_; }

    // The recording's name in DUNE.DAT, for an inside (`inside`) or outside
    // room. `variant` adds a letter for the alternative takes (0 for none).
    static std::string voiceName(int character, int phrase, bool inside, int variant = 0);

    // Timer numbers, as the original's.
    static constexpr int kAnimationTimer = 0x99BE;
    static constexpr int kSpeechTimer = 0xA7C2;

private:
    void select();
    void startSequence();
    int pickSequence();
    void tick();
    void playFrame();
    void drawFrame(std::vector<PortraitPiece> frame, bool toScreen);
    ClipRect piecesBounds(const std::vector<PortraitPiece>& a, const std::vector<PortraitPiece>& b) const;
    void drawPieces(const std::vector<PortraitPiece>& pieces, const ClipRect& clip, bool onScreen);
    void speechTick();
    void showMouth(int shape);
    void endSpeech();
    double ticksPerMouthShape();

    Game& game_;
    int character_ = -1;
    int sheet_ = -1;
    int variant_ = 0;
    int mood_ = 0;
    std::optional<PortraitAnimation> anim_;
    ClipRect area_;
    bool active_ = false;
    bool restless_ = false; // the idle frames ran out: only short random movements now
    int framesLeft_ = 0;
    std::size_t next_ = 0;
    std::size_t sequenceStart_ = 0;
    int shiftRight_ = 0;
    bool timerRunning_ = false;
    std::vector<PortraitPiece> shownPieces_;

    // Speech
    bool speaking_ = false;
    std::vector<std::uint8_t> mouths_;
    std::size_t mouthAt_ = 0;
    int lastMouth_ = -1;
    double nextMouthTick_ = 0;
    double mouthTicks_ = 0;
};

} // namespace mentat
