// SPDX-License-Identifier: GPL-3.0-or-later
//
// The mixer panel: a window over the scene with the volumes of the voices
// and the music, their balances, a voice test, the language and how the
// lines are shown. See docs/rules/mixer.md.
#pragma once

#include "mentat/gfx/scene.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace mentat {

class Game;

class MixerPanel {
public:
    // The original's numbers for the panel's commands.
    static constexpr std::uint16_t kOpen = 0xA3F0;
    static constexpr std::uint16_t kMusicOff = 0xAEAF;
    static constexpr std::uint16_t kGameRelative = 0xAC6E;
    static constexpr std::uint16_t kCdStyle = 0xAC7E;
    static constexpr std::uint16_t kStandardOrder = 0xAC97;
    static constexpr std::uint16_t kShuffle = 0xAC90;
    static constexpr std::uint16_t kCancelCdStyle = 0xD2DF;
    static constexpr std::uint16_t kDone = 0xD2E2;

    // What a click on the window did.
    enum class Click {
        Outside,   // not on the window: it closes
        Nothing,   // on the window, nothing that acts (or a control grabbed)
        VoiceTest, // the voice test is to be heard
        Language,  // the language changed
    };

    explicit MixerPanel(Game& game);

    bool active() const { return open_; }
    // Draws the window, with its controls as they are, on the screen;
    // `shown` is how lines are shown now.
    void open(int shown);
    // The window goes: the scene under it comes back from the back buffer,
    // and the choice of subtitles is kept.
    void close();
    // The window's rectangle on the screen.
    ClipRect rect() const;

    // The mouse button went down at (x, y).
    Click press(int x, int y);
    // The mouse moved by (dx, dy) to (x, y) with the button down.
    void drag(int x, int y, int dx, int dy);
    // The control held, if any, is let go.
    void release() { held_ = Held::None; }
    // Whether a control is held (the pointer changes meanwhile).
    bool holding() const { return held_ != Held::None; }

    // What can be clicked or held on the window, for a gamepad's pointer:
    // the knobs, the voice test and the rows of the lists.
    std::vector<ClipRect> targets() const;

    // How lines are shown (0 subtitles, 1 balloons, 2 none), as chosen
    // on the panel.
    int subtitles() const { return subtitles_; }

private:
    // A volume or a balance: its value, where it is in the window, and
    // for a volume the row of its knob.
    struct Control {
        std::uint8_t value = 0;
        int x = 0;
        int y = 0;
    };
    enum class Held { None, Slider, Dial };

    void draw();
    void drawSlider(int n);
    void drawDial(int n);
    void drawMarker(int value);
    // The control (0 to 2, the volume) whose knob is at (x, y) in the
    // window; -1 for none.
    int sliderAt(int x, int y) const;
    // The balance (0 to 2) under (x, y) in the window; -1 for none.
    int dialAt(int x, int y) const;
    // A control changed: the sound follows.
    void apply(int n);
    void readLevels();

    Game& g_;
    bool open_ = false;
    ClipRect window_;
    std::array<Control, 3> sliders_{};
    std::array<Control, 3> dials_{};
    Held held_ = Held::None;
    int subtitles_ = 0;
};

} // namespace mentat
