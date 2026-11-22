// SPDX-License-Identifier: GPL-3.0-or-later
//
// The map of Dune seen flat, as the game's map screen shows it: the map
// (MAP.HSQ) around a point, four screen pixels to a map point, smoothed
// between neighbouring points. See docs/rules/map.md.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace mentat {

class Surface;

class FlatMap {
public:
    static constexpr int kRows = 99;      // latitudes from the equator to a pole
    static constexpr int kMaxLatitude = 75; // the furthest the view's middle goes
    static constexpr int kHalfRows = 18;  // map rows above and below the middle
    static constexpr int kWidth = 312;    // pixels drawn
    static constexpr int kHeight = 144;

    // The rounded edge drawn where the view reaches a pole: from the
    // player's video driver (DNVGA.HSQ, unpacked).
    struct Edges {
        std::array<std::uint8_t, 8> fade{};    // left edge, then right edge
        std::array<std::uint8_t, 56> north{};  // latitudes 86 to 92: 4 rows of (side, middle)
        std::array<std::uint8_t, 56> south{};  // latitudes 87 to 93
        static Edges fromDriver(std::span<const std::uint8_t> driver); // throws FormatError
    };

    // TABLAT.BIN and MAP.HSQ, unpacked. Throws FormatError when they do not
    // fit together.
    FlatMap(std::span<const std::uint8_t> latitudes, std::vector<std::uint8_t> map, const Edges& edges);

    // Half the points around the planet at latitude `row` (0..98).
    int width(int row) const { return rows_[static_cast<std::size_t>(row)].width; }
    // Longitude units (65536 a turn) a map point covers at latitude `row`.
    int pointSize(int row) const { return rows_[static_cast<std::size_t>(row)].pointSize; }
    // Where latitude `latitude` (negative north) starts in the map.
    int rowStart(int latitude) const;
    // The point of the map at `latitude` nearest to `longitude`.
    int pointAt(std::uint16_t longitude, int latitude) const;

    std::vector<std::uint8_t>& bytes() { return map_; }
    const std::vector<std::uint8_t>& bytes() const { return map_; }

    // Draws the map with its middle at (`longitude`, `latitude`) into
    // `to`, its top-left corner at (x, y): kWidth × kHeight pixels. The
    // latitude is first kept within ±kMaxLatitude.
    void draw(Surface& to, int x, int y, std::uint16_t longitude, int latitude) const;

private:
    struct Row {
        int base = 0;
        int width = 0;
        int pointSize = 0;
    };

    // Work lines: five of 400 bytes, the middle of each at byte 176.
    static constexpr int kLine = 400;
    static constexpr int kLineMiddle = 176;
    static constexpr int kFirstLine = 0x8C;
    using Work = std::vector<std::uint8_t>;

    void spreadRow(Work& work, int line, std::uint16_t longitude, int latitude) const;
    static void blend(Work& work, int narrowLine, int wideLine, std::array<int, 3> between, int narrowWidth,
                      int wideWidth);
    void roundEdge(Work& work, const std::uint8_t* table) const;
    std::uint8_t value(int at) const;

    std::vector<Row> rows_;
    std::vector<std::uint8_t> map_;
    Edges edges_;
};

} // namespace mentat
