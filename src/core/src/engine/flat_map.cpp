// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/flat_map.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/gfx/surface.hpp"

#include <algorithm>
#include <cstdlib>

namespace mentat {

namespace {

constexpr std::size_t kRecord = 8; // bytes per latitude in TABLAT.BIN
constexpr int kMapMiddle = 0x62FC;

// In the unpacked driver.
constexpr std::size_t kEdgeFade = 0x7EF;
constexpr std::size_t kNorthEdges = 0x7F7;
constexpr std::size_t kSouthEdges = 0x82F;
constexpr int kFirstNorthEdge = 86;
constexpr int kFirstSouthEdge = 87;

// Points to a row: 44 each side of the middle and one more, or the whole
// ring and one more when it has fewer than 88.
constexpr int kSidePoints = 44;
// The shown part of a work line starts this far into it.
constexpr int kShownFrom = 20;
// The front of the work area, for reads before its first line.
constexpr int kPad = 1024;

int shiftRight2(int v) {
    const auto s = static_cast<std::int8_t>(static_cast<std::uint8_t>(v));
    return s >> 2;
}

} // namespace

FlatMap::Edges FlatMap::Edges::fromDriver(std::span<const std::uint8_t> driver) {
    if (driver.size() < kSouthEdges + 56) {
        throw FormatError("video driver: too short for the map's edges");
    }
    Edges e;
    std::copy_n(driver.begin() + kEdgeFade, 8, e.fade.begin());
    std::copy_n(driver.begin() + kNorthEdges, 56, e.north.begin());
    std::copy_n(driver.begin() + kSouthEdges, 56, e.south.begin());
    return e;
}

FlatMap::FlatMap(std::span<const std::uint8_t> latitudes, std::vector<std::uint8_t> map, const Edges& edges)
    : map_(std::move(map)), edges_(edges) {
    if (latitudes.size() < kRows * kRecord) {
        throw FormatError("map: latitude table too short");
    }
    for (int r = 0; r < kRows; ++r) {
        const std::size_t at = static_cast<std::size_t>(r) * kRecord;
        Row row;
        row.base = (latitudes[at] << 8) | latitudes[at + 1];
        row.width = (latitudes[at + 2] << 8) | latitudes[at + 3];
        if (row.width == 0 || kMapMiddle - row.base < 1 ||
            static_cast<std::size_t>(kMapMiddle + row.base + 2 * row.width) >= map_.size()) {
            throw FormatError("map: a latitude falls outside the map");
        }
        row.pointSize = (0x10000 + row.width) / (2 * row.width);
        rows_.push_back(row);
    }
}

int FlatMap::rowStart(int latitude) const {
    const int base = rows_[static_cast<std::size_t>(std::abs(latitude))].base;
    return kMapMiddle + (latitude > 0 ? base : -base);
}

int FlatMap::pointAt(std::uint16_t longitude, int latitude) const {
    const auto ring = static_cast<std::uint32_t>(2 * width(std::abs(latitude)));
    return static_cast<int>((ring * longitude + 0x8000) >> 16);
}

std::uint8_t FlatMap::value(int at) const {
    // Only the colour (the low four bits) counts, a sixteenth of the way
    // into its step.
    const std::uint8_t m = at >= 0 && static_cast<std::size_t>(at) < map_.size() ? map_[static_cast<std::size_t>(at)] : 0;
    return static_cast<std::uint8_t>(((m & 0x0F) << 4) | 8);
}

void FlatMap::spreadRow(Work& work, int line, std::uint16_t longitude, int latitude) const {
    const int width = this->width(std::abs(latitude));
    const int ring = 2 * width;
    const int start = rowStart(latitude);
    const std::uint32_t product = static_cast<std::uint32_t>(longitude) * static_cast<std::uint32_t>(ring);
    const int middle = static_cast<int>(product >> 16);
    const int quarter = static_cast<int>((product & 0xFFFF) >> 14);
    int to = kPad + kFirstLine + kLine * line - quarter;
    int first = 0;
    int points = 0;
    if (ring < 2 * kSidePoints) {
        to += (2 * kSidePoints - ring) * 2;
        first = middle - width;
        points = ring + 1;
    } else {
        first = middle - kSidePoints;
        points = 2 * kSidePoints + 1;
    }
    if (first < 0) {
        first += ring;
    }
    // The point before the first comes straight before it in the map, even
    // at the start of the row.
    int previous = value(start + first - 1);
    int at = first;
    for (int k = 0; k < points; ++k) {
        const int current = value(start + at);
        const int step = shiftRight2(current - previous);
        for (int j = 0; j < 4; ++j) {
            work[static_cast<std::size_t>(to + 4 * k + j)] = static_cast<std::uint8_t>(previous + j * step);
        }
        previous = current;
        if (++at == ring) {
            at = 0;
        }
    }
}

void FlatMap::blend(Work& work, int narrowLine, int wideLine, std::array<int, 3> between, int narrowWidth,
                    int wideWidth) {
    const std::uint32_t ratio = (static_cast<std::uint32_t>(wideWidth - narrowWidth) << 16) / static_cast<std::uint32_t>(narrowWidth);
    const std::array<int, 3> step{static_cast<int>((((ratio >> 2) + 0x80) >> 8) & 0xFF),
                                  static_cast<int>((((ratio >> 1) + 0x80) >> 8) & 0xFF),
                                  static_cast<int>((((ratio >> 1) + (ratio >> 2) + 0x80) >> 8) & 0xFF)};
    for (const int direction : {1, -1}) {
        // Rightwards from the middle, then leftwards from just before it.
        const int back = direction < 0 ? 1 : 0;
        int narrow = kPad + kFirstLine + kLine * narrowLine + kLineMiddle - back;
        int wide = kPad + kFirstLine + kLine * wideLine + kLineMiddle - back;
        std::array<int, 3> at{};
        std::array<int, 3> count = step;
        for (std::size_t k = 0; k < 3; ++k) {
            at[k] = kPad + kFirstLine + kLine * between[k] + kLineMiddle - back;
        }
        std::uint32_t skip = ratio;
        for (int i = 0; i < kLineMiddle; ++i) {
            int v = work[static_cast<std::size_t>(narrow)];
            const int d = shiftRight2(work[static_cast<std::size_t>(wide)] - v);
            narrow += direction;
            wide += direction;
            for (std::size_t k = 0; k < 3; ++k) {
                v = (v + d) & 0xFF;
                work[static_cast<std::size_t>(at[k])] = static_cast<std::uint8_t>(v);
                at[k] += direction;
                count[k] += step[k];
                if (count[k] >= 0x100) {
                    // The line between catches up with the wider one.
                    count[k] -= 0x100;
                    work[static_cast<std::size_t>(at[k])] = static_cast<std::uint8_t>(v);
                    at[k] += direction;
                }
            }
            skip += ratio;
            if (skip >= 0x10000) {
                skip -= 0x10000;
                wide += direction;
            }
        }
    }
}

void FlatMap::roundEdge(Work& work, const std::uint8_t* table) const {
    for (int row = 0; row < 4; ++row) {
        const int side = table[2 * row];
        const int middle = table[2 * row + 1];
        auto* line = &work[static_cast<std::size_t>(kPad + kFirstLine + kLine * row + kShownFrom)];
        int x = 0;
        if (side <= 4) {
            for (int i = 4 - side; i < 4; ++i) {
                line[x++] = edges_.fade[static_cast<std::size_t>(i)];
            }
        } else {
            for (int i = 0; i < side - 4; ++i) {
                line[x++] = 0;
            }
            for (int i = 0; i < 4; ++i) {
                line[x++] = edges_.fade[static_cast<std::size_t>(i)];
            }
        }
        x += 2 * middle;
        if (side <= 4) {
            for (int i = 0; i < side; ++i) {
                line[x++] = edges_.fade[static_cast<std::size_t>(4 + i)];
            }
        } else {
            for (int i = 0; i < 4; ++i) {
                line[x++] = edges_.fade[static_cast<std::size_t>(4 + i)];
            }
            for (int i = 0; i < side - 4; ++i) {
                line[x++] = 0;
            }
        }
    }
}

void FlatMap::draw(Surface& to, int x, int y, std::uint16_t longitude, int latitude) const {
    latitude = std::clamp(latitude, -kMaxLatitude, kMaxLatitude);
    Work work(static_cast<std::size_t>(2 * kPad + kFirstLine + 5 * kLine), 0);
    const int top = latitude - kHalfRows;
    spreadRow(work, 0, longitude, top);
    int outRow = y;
    for (int i = 1; i <= 2 * kHalfRows; ++i) {
        const int above = top + i - 1;
        const int here = top + i;
        spreadRow(work, 4, longitude, here);
        if (here <= 0) {
            blend(work, 0, 4, {1, 2, 3}, width(-above), width(-here));
            if (-here >= kFirstNorthEdge) {
                roundEdge(work, &edges_.north[static_cast<std::size_t>(8 * (-here - kFirstNorthEdge))]);
            }
        } else {
            blend(work, 4, 0, {3, 2, 1}, width(here), width(std::abs(above)));
            if (here >= kFirstSouthEdge) {
                roundEdge(work, &edges_.south[static_cast<std::size_t>(8 * (here - kFirstSouthEdge))]);
            }
        }
        for (int line = 0; line < 4; ++line, ++outRow) {
            const auto* from = &work[static_cast<std::size_t>(kPad + kFirstLine + kLine * line + kShownFrom)];
            for (int c = 0; c < kWidth; ++c) {
                to.set(x + c, outRow, static_cast<std::uint8_t>(16 + ((from[c] >> 4) & 0x0F)));
            }
        }
        // The new line becomes the first, with what follows it.
        const auto first = static_cast<std::ptrdiff_t>(kPad + kFirstLine);
        std::copy_n(work.begin() + first + 4 * kLine, 700, work.begin() + first);
    }
}

} // namespace mentat
