// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/mixer_panel.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/engine/game.hpp"

namespace mentat {

namespace {

// Places in the program's data.
constexpr std::size_t kWindow = 0x2886;    // left, top, right, bottom
constexpr std::size_t kSliders = 0x288E;   // three volumes, 8 bytes each
constexpr std::size_t kDials = 0x28A6;     // three balances, 8 bytes each
constexpr std::size_t kControlSize = 8;
constexpr std::size_t kVoiceTest = 0x28BF; // a rectangle in the window
constexpr std::size_t kList = 0x28C7;      // the rectangle of the list
constexpr std::size_t kRowValues = 0x28CF; // what each row of the list is
constexpr std::size_t kValueRows = 0x28DC; // the row of each value

constexpr int kSheet = 0x55; // MIXR.HSQ
constexpr int kTrackSprite = 1;
constexpr int kKnobSprite = 2;
constexpr int kFirstDialSprite = 3;
constexpr int kFirstMarkerSprite = 0x1C;
constexpr int kTrackTop = 0x22; // the knob's row at the loudest
constexpr int kTrackRows = 0x40;
constexpr int kKnobWidth = 0x16;
constexpr int kKnobHeight = 5;
constexpr int kDialWidth = 0x0D;
constexpr int kDialHeight = 0x0B;
constexpr int kDialStep = 10;
constexpr int kDialMost = 0xF0;
constexpr int kRowHeight = 7;
constexpr int kLanguages = 7;
constexpr int kNoRow = 7;      // the value of the rows between the lists
constexpr int kFirstShown = 8; // the value of "Subtitles"

ClipRect readRect(const ProgramFile& p, std::size_t at) {
    return ClipRect{p.dataU16(at), p.dataU16(at + 2), p.dataU16(at + 4), p.dataU16(at + 6)};
}

// The original's test: strictly inside on every side.
bool inside(const ClipRect& r, int x, int y) { return x > r.left && x < r.right && y > r.top && y < r.bottom; }

// The row of a volume's knob, from the top of the window.
int knobRow(std::uint8_t value) { return kTrackTop + ((255 - value) >> 2); }

} // namespace

MixerPanel::MixerPanel(Game& game) : g_(game) {
    const ProgramFile& p = g_.resources().program();
    window_ = readRect(p, kWindow);
    for (std::size_t i = 0; i < 3; ++i) {
        sliders_[i].x = p.dataU16(kSliders + kControlSize * i + 2);
        dials_[i].x = p.dataU16(kDials + kControlSize * i + 2);
        dials_[i].y = p.dataU16(kDials + kControlSize * i + 4);
    }
}

ClipRect MixerPanel::rect() const { return window_; }

void MixerPanel::readLevels() {
    const Game::SoundLevels& l = g_.levels;
    sliders_[0].value = l.voices;
    sliders_[1].value = l.music;
    sliders_[2].value = l.quietMusic;
    dials_[0].value = l.voicesBalance;
    dials_[1].value = l.musicBalance;
    dials_[2].value = l.quietBalance;
}

void MixerPanel::open(int shown) {
    open_ = true;
    held_ = Held::None;
    subtitles_ = shown;
    readLevels();
    draw();
}

void MixerPanel::close() {
    open_ = false;
    held_ = Held::None;
    Display& d = g_.display();
    d.copyRect(d.back(), d.screen(), window_);
}

void MixerPanel::draw() {
    Display& d = g_.display();
    d.targetScreen();
    g_.selectSheet(kSheet);
    g_.drawSprite(0, window_.left, window_.top);
    for (int i = 0; i < 3; ++i) {
        drawSlider(i);
    }
    for (int i = 0; i < 3; ++i) {
        drawDial(i);
    }
    drawMarker(g_.language);
    drawMarker(kFirstShown + subtitles_);
    d.targetBack();
}

void MixerPanel::drawSlider(int n) {
    const Control& c = sliders_[static_cast<std::size_t>(n)];
    g_.drawSprite(kTrackSprite, window_.left + c.x, window_.top + kTrackTop);
    g_.drawSprite(kKnobSprite, window_.left + c.x, window_.top + knobRow(c.value));
}

void MixerPanel::drawDial(int n) {
    const Control& c = dials_[static_cast<std::size_t>(n)];
    g_.drawSprite(static_cast<std::uint16_t>(kFirstDialSprite + c.value / kDialStep), window_.left + c.x,
                  window_.top + c.y);
}

void MixerPanel::drawMarker(int value) {
    const ProgramFile& p = g_.resources().program();
    const ClipRect list = readRect(p, kList);
    const int row = p.dataU8(kValueRows + static_cast<std::size_t>(value));
    g_.drawSprite(static_cast<std::uint16_t>(kFirstMarkerSprite + 2 * value), window_.left + list.left,
                  window_.top + list.top + row * kRowHeight);
}

int MixerPanel::sliderAt(int x, int y) const {
    for (int i = 0; i < 3; ++i) {
        const Control& c = sliders_[static_cast<std::size_t>(i)];
        if (static_cast<unsigned>(x - c.x) < kKnobWidth && static_cast<unsigned>(y - knobRow(c.value)) < kKnobHeight) {
            return i;
        }
    }
    return -1;
}

int MixerPanel::dialAt(int x, int y) const {
    for (int i = 0; i < 3; ++i) {
        const Control& c = dials_[static_cast<std::size_t>(i)];
        if (static_cast<unsigned>(x - c.x) < kDialWidth && static_cast<unsigned>(y - c.y) < kDialHeight) {
            return i;
        }
    }
    return -1;
}

MixerPanel::Click MixerPanel::press(int x, int y) {
    if (!inside(window_, x, y)) {
        return Click::Outside;
    }
    const int rx = x - window_.left;
    const int ry = y - window_.top;
    const ProgramFile& p = g_.resources().program();
    if (inside(readRect(p, kVoiceTest), rx, ry)) {
        return Click::VoiceTest;
    }
    const ClipRect list = readRect(p, kList);
    if (inside(list, rx, ry)) {
        const int value = p.dataU8(kRowValues + static_cast<std::size_t>((ry - list.top) / kRowHeight));
        if (value < kLanguages) {
            if (value == g_.language) {
                return Click::Nothing;
            }
            // Another language: lines without subtitles get them again.
            subtitles_ &= ~2;
            g_.language = value;
            draw();
            return Click::Language;
        }
        if (value > kNoRow) {
            subtitles_ = value - kFirstShown;
            draw();
        }
        return Click::Nothing;
    }
    if (sliderAt(rx, ry) >= 0) {
        held_ = Held::Slider;
    } else if (dialAt(rx, ry) >= 0) {
        held_ = Held::Dial;
    } else {
        held_ = Held::None;
    }
    return Click::Nothing;
}

void MixerPanel::drag(int x, int y, int dx, int dy) {
    // The control is found again where the pointer was, so a knob moved
    // stays held and the pointer moved off it lets it go.
    const int rx = x - window_.left;
    const int ry = y - dy - window_.top;
    held_ = Held::None;
    const int slider = sliderAt(rx, ry);
    if (slider >= 0) {
        held_ = Held::Slider;
        Control& c = sliders_[static_cast<std::size_t>(slider)];
        const int row = knobRow(c.value) + dy - kTrackTop;
        if (dy == 0 || row < 0 || row >= kTrackRows) {
            return;
        }
        c.value = static_cast<std::uint8_t>(255 - 4 * row);
        g_.display().targetScreen();
        g_.selectSheet(kSheet);
        drawSlider(slider);
        g_.display().targetBack();
        apply(slider);
        return;
    }
    const int dial = dialAt(rx, ry);
    if (dial < 0) {
        return;
    }
    held_ = Held::Dial;
    Control& c = dials_[static_cast<std::size_t>(dial)];
    // Turned as a knob is: up on its right side or right on its upper side
    // turns it one way.
    const int along = (rx - c.x < 6 ? -dy : dy) + (ry - c.y >= 5 ? -dx : dx);
    const int value = c.value + (along >= 0 ? kDialStep : -kDialStep);
    if (value < 0 || value > kDialMost) {
        return;
    }
    c.value = static_cast<std::uint8_t>(value);
    g_.display().targetScreen();
    g_.selectSheet(kSheet);
    drawDial(dial);
    g_.display().targetBack();
    apply(dial);
}

void MixerPanel::apply(int n) {
    Game::SoundLevels& l = g_.levels;
    l.voices = sliders_[0].value;
    l.music = sliders_[1].value;
    l.quietMusic = sliders_[2].value;
    l.voicesBalance = dials_[0].value;
    l.musicBalance = dials_[1].value;
    l.quietBalance = dials_[2].value;
    if (n == 0) {
        g_.applyVoiceLevels();
    } else if (n == 1) {
        g_.applyMusicLevels();
    }
}

} // namespace mentat
