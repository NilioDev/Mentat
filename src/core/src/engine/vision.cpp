// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/vision.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace mentat {

namespace {

constexpr int kClouds = 0x23;      // VIS.HSQ
constexpr int kPlanet = 0x21;      // PALPLAN.HSQ
constexpr int kPanelSheet = 0;
constexpr std::uint16_t kWindowFrame = 7;
constexpr int kFrameX = 0xA8;
constexpr int kFrameY = 0x17;
constexpr ClipRect kWindow{0xB0, 0x20, 0x120, 0x58};
constexpr std::uint16_t kPlaceMark = 0x36;
constexpr std::size_t kPaulsPlace = 0x114E;

// The ripple: how far each row of a run is pushed, from its first row.
constexpr std::array<std::int8_t, 116> kRipple{
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  5,
    5,  5,  5,  5,  5,  5,  5,  5,  4,  4,  4,  4,  4,  4,  4,  3,  3,  3,  3,  3,  3,  2,  2,  2,  2,  2,  1,  1,  1,
    1,  0,  0,  0,  -1, -1, -1, -2, -2, -2, -2, -3, -3, -3, -3, -3, -4, -4, -4, -4, -4, -4, -5, -5, -5, -5, -5, -5, -5,
    -5, -5, -4, -4, -4, -4, -4, -4, -3, -3, -3, -3, -3, -3, -2, -2, -2, -2, -2, -2, -2, -1, -1, -1, -1, -1, -1, -1, -1};
constexpr ClipRect kRippleArea{8, 0, 0x138, 0x94};
constexpr int kTurningFirst = 0x80; // the colours that turn
constexpr int kTurningCount = 0x40;

} // namespace

Vision::Vision(Game& game, World& world, MapScreen& map) : g_(game), world_(world), small_(game, world, map) {}

void Vision::draw(std::size_t place) {
    Display& d = g_.display();
    d.targetBack();
    g_.selectSheet(kClouds);
    g_.drawSprite(0, 0, 0);
    ripple_ = 0;
    lastRun_ = 5;
    if (place == 0 || place == world_.u16(kPaulsPlace)) {
        return;
    }
    g_.selectSheet(kPlanet);
    g_.drawSprite(kWindowFrame, kFrameX, kFrameY);
    small_.view = kWindow;
    small_.longitude = world_.u16(place + 2);
    small_.latitude = static_cast<std::int16_t>(world_.u16(place + 4));
    small_.drawMap();
    g_.selectSheet(kPanelSheet);
    small_.drawPlaces();
    int x = 0;
    int y = 0;
    if (small_.toScreen(world_.u16(place + 2), static_cast<std::int16_t>(world_.u16(place + 4)), x, y)) {
        g_.drawSprite(kPlaceMark, x, y - g_.sheet().sprite(kPlaceMark).header.height);
    }
}

void Vision::shimmer() {
    Display& d = g_.display();
    // The colours turn by one.
    Palette& p = d.palette();
    const Colour first = p[kTurningFirst];
    for (int i = kTurningFirst; i < kTurningFirst + kTurningCount - 1; ++i) {
        p[static_cast<std::size_t>(i)] = p[static_cast<std::size_t>(i + 1)];
        d.markChanged(i);
    }
    p[kTurningFirst + kTurningCount - 1] = first;
    d.markChanged(kTurningFirst + kTurningCount - 1);
    d.flush();

    // The first row of the last run comes back as the back buffer has it;
    // then the rows of a run show the back buffer's pushed sideways.
    const int rows = kRippleArea.bottom - kRippleArea.top;
    int row = kRippleArea.top + ripple_;
    ++ripple_;
    int left = rows - (row - kRippleArea.top);
    if (left <= 0) {
        ripple_ = 0;
        row = kRippleArea.top;
        left = rows;
    }
    const std::uint8_t* back = d.back().data();
    std::uint8_t* screen = d.screen().data();
    const auto size = static_cast<std::ptrdiff_t>(d.screen().size());
    const int width = kRippleArea.right - kRippleArea.left;
    auto copy = [&](std::ptrdiff_t from, std::ptrdiff_t to, int count) {
        for (int k = 0; k < count; ++k) {
            if (from + k >= 0 && from + k < size && to + k >= 0 && to + k < size) {
                screen[to + k] = back[from + k];
            }
        }
    };
    {
        const std::ptrdiff_t at = lastRun_ - 5;
        copy(at, at, width + 10);
    }
    std::ptrdiff_t at = static_cast<std::ptrdiff_t>(std::min(row, 199)) * 320 + kRippleArea.left;
    lastRun_ = at;
    for (const std::int8_t push : kRipple) {
        copy(at, at + push, width);
        at += 320;
        if (--left == 0) {
            at = static_cast<std::ptrdiff_t>(kRippleArea.top) * 320 + kRippleArea.left;
        }
    }
}

void Vision::stop() {
    ripple_ = 0;
    lastRun_ = 5;
}

} // namespace mentat
