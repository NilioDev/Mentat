// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the game sees of the computer it runs on: the clock, the mouse and
// keys, and a queue of sound to play. The app fills in the input before it
// resumes the game, and plays the sound the game queued after it yields.
// See docs/rules/timing.md for the clock.
#pragma once

#include "mentat/audio/opl.hpp"
#include "mentat/engine/fiber.hpp"
#include "mentat/gfx/palette.hpp"
#include "mentat/gfx/surface.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

namespace mentat {

// The original sets the PC timer to 1193182 / 5957 Hz.
inline constexpr double kTicksPerSecond = 1193182.0 / 5957.0;

struct KeyPress {
    std::uint8_t scancode = 0; // PC keyboard scancode
    char text = 0;             // the character typed, 0 when none
};

enum class SoundChannel : std::uint8_t {
    Digital, // voices, video sound and effects: one at a time, like the original
    Music,   // the FM chip, 16-bit stereo frames at Opl::kRate
    Count,
};

struct SoundCommand {
    SoundChannel channel = SoundChannel::Digital;
    bool stop = false; // drop what is queued on the channel first
    int rate = 0;
    std::vector<std::uint8_t> samples; // unsigned 8-bit mono (Digital)
    std::vector<std::int16_t> wide;    // signed 16-bit stereo, left first (Music)
};

// A mouse pointer as the original keeps it: where its point is, and per
// row of 16 pixels a mask of the screen pixels kept (bit 15 the leftmost)
// and the pointer's own bits. A pixel that is not kept is white where the
// pointer's bit is set and black where it is not.
struct MouseCursor {
    int hotX = 0;
    int hotY = 0;
    std::array<std::uint16_t, 16> keep{};
    std::array<std::uint16_t, 16> bits{};

    static constexpr std::uint8_t kBlack = 0x00;
    static constexpr std::uint8_t kWhite = 0x0F;

    // Draws the pointer with its point at (x, y), clipped to the surface.
    void draw(Surface& to, int x, int y) const {
        for (int row = 0; row < 16; ++row) {
            for (int col = 0; col < 16; ++col) {
                const std::uint16_t bit = static_cast<std::uint16_t>(0x8000u >> col);
                if (keep[static_cast<std::size_t>(row)] & bit) {
                    continue;
                }
                const bool white = (bits[static_cast<std::size_t>(row)] & bit) != 0;
                to.set(x - hotX + col, y - hotY + row, white ? kWhite : kBlack);
            }
        }
    }
};

struct Platform {
    // Set by the app.
    std::uint32_t ticks = 0;
    int mouseX = 0;
    int mouseY = 0;
    std::uint8_t mouseButtons = 0; // bit 0 left, bit 1 right
    std::deque<KeyPress> keys;
    double soundQueued[static_cast<int>(SoundChannel::Count)] = {}; // seconds still to play

    // Set by the game, taken by the app: the picture on the screen (the VGA
    // memory and colour registers) and sound to play.
    Surface screen;
    Palette palette;
    // The mouse pointer, drawn over the screen when shown.
    bool cursorShown = false;
    MouseCursor cursor;
    std::vector<SoundCommand> sound;

    Fiber* fiber = nullptr;
    // The music player, run on the timer; null when there is no music.
    MusicSystem* music = nullptr;

private:
    std::uint8_t lastButtons_ = 0;

public:

    // Waits for the next frame of the app. Throws StopRequested when the app
    // is closing.
    void waitFrame() {
        runMusic();
        if (fiber != nullptr) {
            fiber->yield();
        }
    }

    // Plays the music up to the current tick.
    void runMusic() {
        if (music == nullptr) {
            return;
        }
        SoundCommand command;
        command.channel = SoundChannel::Music;
        command.rate = Opl::kRate;
        music->runTo(ticks, command.wide);
        if (!command.wide.empty()) {
            sound.push_back(std::move(command));
        }
    }

    // True once when a key was pressed or a mouse button went down since the
    // last call; what the original checks to skip a video or a pause.
    bool takeSkip() {
        const bool pressed = !keys.empty() || (mouseButtons & ~lastButtons_) != 0;
        keys.clear();
        lastButtons_ = mouseButtons;
        return pressed;
    }

    void queueSound(SoundChannel channel, int rate, std::vector<std::uint8_t> samples, bool stopFirst = false) {
        sound.push_back(SoundCommand{channel, stopFirst, rate, std::move(samples), {}});
        // Count it at once, so the game sees it as queued before the app has run.
        if (rate > 0) {
            soundQueued[static_cast<int>(channel)] += static_cast<double>(sound.back().samples.size()) / rate;
        }
    }

    void stopSound(SoundChannel channel) {
        sound.push_back(SoundCommand{channel, true, 0, {}, {}});
        soundQueued[static_cast<int>(channel)] = 0;
    }
};

} // namespace mentat
