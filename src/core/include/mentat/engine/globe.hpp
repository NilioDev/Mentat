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
    // Tilts the planet further by `steps`, no further than the poles.
    void tilt(int steps);
    int rotation() const { return rotation_; }
    int tilt() const { return tilt_; }
    // The results' colours: who holds each point instead of the spice
    // fields found.
    void showHolders(bool on) { holders_ = on; }

    // Where map point (longitude, latitude) shows on the screen, found the
    // program's way through the projection tables. False when the program
    // finds no place for it.
    bool screenPoint(std::uint16_t longitude, int latitude, int& x, int& y) const;
    // The map point at screen point (x, y). False when it is off the planet.
    bool mapPoint(int x, int y, std::uint16_t& longitude, int& latitude) const;

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
    // What the sphere holds at height `u` of the tilted view: whether it is
    // on the side the tilt brings up, and the row of the columns (negative
    // past the pole). As the program keeps it, in one 16-bit word.
    std::uint16_t heightEntry(int u) const;

    std::vector<Row> rows_;
    std::vector<std::uint8_t> projection_;
    std::vector<std::uint8_t> map_;
    int mapMiddle_ = 0;
    int rotation_ = 0;
    int tilt_ = 0;
    bool holders_ = false;
    // Where the drawing is.
    bool lowerHalf_ = false;
    std::size_t at_ = 0;
    int y_ = kCentreY;
};

} // namespace mentat
