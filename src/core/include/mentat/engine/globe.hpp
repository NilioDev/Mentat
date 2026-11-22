// SPDX-License-Identifier: GPL-3.0-or-later
//
// The turning planet: the map of Dune (MAP.HSQ) drawn on a sphere through
// the original's tables (TABLAT.BIN, GLOBDATA.HSQ), one screen line at a
// time. See docs/rules/globe.md.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace mentat {

class Surface;

class Globe {
public:
    static constexpr int kTurn = 398; // rotation steps in a whole turn
    static constexpr int kMaxTilt = 98;
    static constexpr int kCentreX = 160;
    static constexpr int kCentreY = 79; // the lowest row of the upper half

    // The three files, unpacked. Throws FormatError when they are too short.
    Globe(std::span<const std::uint8_t> latitudes, std::vector<std::uint8_t> projection,
          std::vector<std::uint8_t> map);

    // A tilt the program keeps that is closer to level than 32 is used as
    // 32 (from 0 up) or -32 (below 0).
    static int viewTilt(int raw) {
        if (raw >= 0 && raw < 32) {
            return 32;
        }
        if (raw < 0 && raw >= -32) {
            return -32;
        }
        return raw;
    }

    // Turns the planet to `rotation` (0..397) and tilts it by `tilt`
    // (-98..98). The next line drawn starts a new picture.
    void setView(int rotation, int tilt);
    void turn(int steps);
    int rotation() const { return rotation_; }

    // Draws the next screen line of the picture. True when that was the
    // last one; the next call starts the picture over.
    bool drawLine(Surface& to);
    void drawAll(Surface& to);

private:
    struct Row {
        int base = 0;  // from the middle of the map, northwards
        int width = 0; // half the samples around this latitude
        int phase = 0; // where the rotation puts the row's start
    };

    void startPicture();
    std::uint8_t colourAt(int index) const;

    std::vector<Row> rows_;
    std::vector<std::uint8_t> projection_;
    std::vector<std::uint8_t> map_;
    int mapMiddle_ = 0;
    int rotation_ = 0;
    int tilt_ = 0;
    // Where the drawing is.
    bool lowerHalf_ = false;
    std::size_t at_ = 0;
    int y_ = kCentreY;
};

} // namespace mentat
