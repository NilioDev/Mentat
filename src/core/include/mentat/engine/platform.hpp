// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the game sees of the computer it runs on: the clock, the mouse and
// keys, and a queue of sound to play. The app fills in the input before it
// resumes the game, and plays the sound the game queued after it yields.
// See docs/rules/timing.md for the clock.
#pragma once

#include "mentat/engine/fiber.hpp"
#include "mentat/gfx/palette.hpp"
#include "mentat/gfx/surface.hpp"

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
    Count,
};

struct SoundCommand {
    SoundChannel channel = SoundChannel::Digital;
    bool stop = false; // drop what is queued on the channel first
    int rate = 0;
    std::vector<std::uint8_t> samples; // unsigned 8-bit mono
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
    std::vector<SoundCommand> sound;

    Fiber* fiber = nullptr;

private:
    std::uint8_t lastButtons_ = 0;

public:

    // Waits for the next frame of the app. Throws StopRequested when the app
    // is closing.
    void waitFrame() {
        if (fiber != nullptr) {
            fiber->yield();
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
        sound.push_back(SoundCommand{channel, stopFirst, rate, std::move(samples)});
        // Count it at once, so the game sees it as queued before the app has run.
        if (rate > 0) {
            soundQueued[static_cast<int>(channel)] += static_cast<double>(sound.back().samples.size()) / rate;
        }
    }

    void stopSound(SoundChannel channel) {
        sound.push_back(SoundCommand{channel, true, 0, {}});
        soundQueued[static_cast<int>(channel)] = 0;
    }
};

} // namespace mentat
