// SPDX-License-Identifier: GPL-3.0-or-later
//
// The original's picture buffers and palettes, and the ways it moves
// pictures between them: the back buffer where scenes are put together, the
// screen, a clean copy of the background, the palette being prepared and a
// spare one, and the transitions from one picture to the next. See
// docs/rules/display.md.
#pragma once

#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/gfx/palette.hpp"
#include "mentat/gfx/scene.hpp"
#include "mentat/gfx/surface.hpp"

#include <array>
#include <bitset>
#include <cstdint>
#include <span>

namespace mentat {

// Values the transitions take from the player's DNVGA.HSQ (unpacked).
struct DisplayTables {
    // The order in which the dissolve goes through the 16 places of a
    // 4 × 4 block, each as row × 320 + column.
    std::array<int, 16> dissolveOrder{};
    // The rounded edge of the flat map at the poles.
    FlatMap::Edges mapEdges{};

    static DisplayTables fromDriver(std::span<const std::uint8_t> driver); // throws FormatError
};

class Display {
public:
    Display(Platform& platform, const DisplayTables& tables);
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    Platform& platform() { return platform_; }
    const DisplayTables& tables() const { return tables_; }

    // --- Buffers
    Surface& back() { return *back_; }
    Surface& screen() { return *screen_; }
    Surface& saved() { return saved_; }
    Surface& aux() { return aux_; }
    Surface& target() { return *target_; }

    void targetBack() { target_ = back_; }
    void targetScreen() { target_ = screen_; }
    void targetAux() { target_ = &aux_; }

    // From beginPrepare() to endPrepare() the back buffer is the drawing
    // target and also "the screen", so what would show at once (an
    // animation's frame too) is only prepared until it is shown.
    void beginPrepare() {
        targetBack();
        if (hiddenScreen_ == nullptr) {
            hiddenScreen_ = screen_;
            screen_ = back_;
        }
    }
    void endPrepare() {
        if (hiddenScreen_ != nullptr) {
            screen_ = hiddenScreen_;
            hiddenScreen_ = nullptr;
        }
    }

    // Rows that everything drawn by position is moved down by. The opening
    // uses 24 to centre the 152-row scenes on the screen.
    int originRows() const { return originRows_; }
    void setOriginRows(int rows) { originRows_ = rows; }

    void clear(Surface& s) { s.fill(0); }
    // Copies a rectangle (moved down by the origin) from one buffer to
    // another; nothing happens when they are the same buffer.
    void copyRect(const Surface& from, Surface& to, const ClipRect& rect);
    // Blacks out a rectangle (moved down by the origin).
    void clearRect(Surface& s, const ClipRect& rect);
    // Shows a part of the back buffer enlarged on the screen's scene area.
    // `level` 1 to 7 picks the enlargement (from 8/7 up to 8 times); the
    // part starts at (x, y). See docs/rules/display.md.
    void zoom(int level, int x, int y);
    // The same from one buffer to another.
    void zoom(int level, int x, int y, const Surface& from, Surface& to);
    // The size of the part each zoom level enlarges.
    static int zoomWidth(int level);
    static int zoomHeight(int level);

    // Keeps the target as the clean background that portraits and
    // animations restore from.
    void saveBackground() { saved_ = *target_; }
    // Copies the whole back buffer to the screen.
    void showBack();

    // --- Palettes
    // The palette being prepared. Changes reach the screen on flush().
    Palette& palette() { return shadow_; }
    Palette& spare() { return spare_; }
    void markChanged(int colour) { dirty_.set(static_cast<std::size_t>(colour & 0xFF)); anyDirty_ = true; }
    void markChanged(const std::bitset<256>& colours);
    void applyPaletteBlocks(std::span<const std::uint8_t> blocks);
    // Sends the changed colours to the screen.
    void flush();
    // spare = palette, for a later fade.
    void keepPalette() { spare_ = shadow_; }
    // Moves `count` colours from `first` a 1/`divisor` part of the way to
    // the spare palette and sends them to the screen at once.
    void fadeStep(int first, int count, int divisor);

    // --- Transitions (the original's numbers; see docs/rules/display.md)
    static constexpr int kRise = 0x00;   // with direction -1: the old picture goes up
    static constexpr int kSpiral = 0x02;
    static constexpr int kMosaic = 0x06;
    static constexpr int kSparkle = 0x08;
    static constexpr int kSlideDown = 0x22;
    static constexpr int kMosaicShort = 0x26;
    static constexpr int kMosaicFine = 0x28;
    static constexpr int kDissolve = 0x10;
    static constexpr int kCut = 0x30;
    static constexpr int kDissolveScreen = 0x34;
    static constexpr int kFadeInFromBlack = 0x36;
    static constexpr int kFadeToBlack = 0x38;
    static constexpr int kFadeThroughBlack = 0x3A;

    // Shows the back buffer on the screen with a transition, then copies
    // it over whole and sends the palette. Unknown numbers throw FormatError.
    void transition(int type) { transition(type, 0); }
    // `direction` is the program's second number for a transition; only
    // the rise uses it.
    void transition(int type, int direction);

    // The transitions inside a rectangle (the original's second set):
    // 0x10, the back picture unrolls downwards into it; 0x12, the picture
    // in it rolls up and away, the back picture showing from below.
    static constexpr int kUnroll = 0x10;
    static constexpr int kRollUp = 0x12;
    void transitionIn(int type, const ClipRect& rect);

    // Waits until `ticks` timer ticks have passed since `since`, and returns
    // the tick it stopped at.
    std::uint32_t waitTicks(std::uint32_t since, std::uint32_t ticks);

private:
    std::uint32_t waitUntil(std::uint32_t due);
    void sendColours(int first, int count);
    void fadeDown(int chunk, int step, int rounds);
    void fadeUp(int chunk, int step, int rounds);
    void dissolve(int rows);
    void rise();
    void mosaic(std::span<const int> out, std::span<const int> in);
    void spiral();
    void sparkle();
    void slideDown();

    Platform& platform_;
    DisplayTables tables_;
    Surface backStore_;
    Surface* back_;
    Surface* hiddenScreen_ = nullptr;
    Surface* screen_;
    Surface* target_;
    Surface saved_;
    Surface aux_;
    int originRows_ = 0;
    Palette shadow_;
    Palette spare_;
    std::bitset<256> dirty_;
    bool anyDirty_ = false;
};

} // namespace mentat
