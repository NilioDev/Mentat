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
constexpr std::size_t kPoleEdges = 0x2284;
constexpr int kFirstPoleEdge = 70;
// The small map's rows stop this far from the equator.
constexpr int kSmallLimit = 86;
// Its colours: the map's 16 from colour 16, and the planet's rim.
constexpr std::uint8_t kSmallFirstColour = 0x10;
constexpr std::array<std::uint8_t, 4> kRim{0x1C, 0x19, 0x18, 0x17};
constexpr int kSmallStride = 200;

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
    if (driver.size() < kPoleEdges + 18) {
        throw FormatError("video driver: too short for the map's edges");
    }
    Edges e;
    std::copy_n(driver.begin() + kEdgeFade, 8, e.fade.begin());
    std::copy_n(driver.begin() + kNorthEdges, 56, e.north.begin());
    std::copy_n(driver.begin() + kSouthEdges, 56, e.south.begin());
    std::copy_n(driver.begin() + kPoleEdges, 18, e.poles.begin());
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

namespace {

std::int16_t w16(int v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

} // namespace

// The disc is walked as rows of points, two at a time, mirrored about its
// middle: a run from `left` to `right` points off the middle's point on
// the rows `top` and `bottom` rows off its latitude.
void FlatMap::disc(std::uint16_t longitude, int latitude, int radius, int limit,
                   const std::function<void(std::uint8_t&)>& each) {
    auto run = [&](int row, int left, int count) {
        const int lat = latitude + row;
        if (lat > limit || lat < -limit || std::abs(lat) >= kRows) {
            return;
        }
        const int ring = 2 * width(std::abs(lat));
        const int start = rowStart(lat);
        int p = pointAt(longitude, lat) + left;
        while (p < 0) {
            p += ring;
        }
        for (int k = 0; k < count; ++k) {
            const std::size_t at = static_cast<std::size_t>(start + p);
            if (at < map_.size()) {
                each(map_[at]);
            }
            if (++p >= ring) {
                p -= ring;
            }
        }
    };
    int cx = radius;
    int top = -cx;
    int bottom = cx - 1;
    int ax = cx;
    int bp = 0;
    int right = 0;
    int left = 0;
    auto span = [&] {
        run(top, left, right - left + 1);
        run(bottom, left, right - left + 1);
    };
    // The rows of the top and bottom caps, widening while the error
    // allows, then the middle rows.
    bool middle = false;
    for (int guard = 0; guard < 4096 && !middle; ++guard) {
        ax = w16(ax - bp);
        while (ax >= 0) {
            ++bp;
            ++right;
            --left;
            ax = w16(ax - bp);
        }
        span();
        ++top;
        --bottom;
        ax = w16(ax + cx);
        if (ax < 0) {
            middle = true;
        } else {
            cx = w16(cx - 1);
            if (cx == 0) {
                middle = true;
            } else {
                ++bp;
                ++right;
                --left;
            }
        }
    }
    ++bp;
    cx = w16(cx - 1);
    for (int guard = 0; guard < 4096; ++guard) {
        span();
        ++top;
        --bottom;
        const bool carry = static_cast<std::uint16_t>(ax) + static_cast<std::uint16_t>(cx) > 0xFFFF;
        ax = w16(ax + cx);
        if (!carry) {
            cx = w16(cx - 1);
            if (cx != 0) {
                continue;
            }
        }
        cx = w16(cx - 1);
        if (cx < 0) {
            return;
        }
        ++right;
        --left;
        ax = w16(ax - bp);
        ++bp;
    }
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

int FlatMap::gatherSmall(std::span<const std::uint8_t> source, int width, int rows, std::uint16_t longitude,
                         int latitude, std::vector<std::uint8_t>& work) const {
    const int half = (rows - 1) / 2;
    const int limit = kSmallLimit - half;
    if (std::abs(latitude) >= limit) {
        latitude = latitude < 0 ? -limit : limit;
    }
    const int first = latitude - half;
    // Each row holds the points around the longitude; a ring narrower than
    // the rectangle is centred in it.
    work.assign(static_cast<std::size_t>(kSmallStride * (rows + 1)), 0);
    for (int r = 0; r < rows; ++r) {
        const int lat = first + r;
        const int ring = 2 * this->width(std::abs(lat));
        const int start = rowStart(lat);
        const int middle = static_cast<int>((static_cast<std::uint32_t>(longitude) * static_cast<std::uint32_t>(ring)) >> 16);
        int to = kSmallStride * r;
        int count = width;
        if (ring < width) {
            to += (width - ring) / 2;
            count = ring;
        }
        int at = middle - count / 2;
        if (at < 0) {
            at += ring;
        }
        for (int k = 0; k < count; ++k) {
            const int i = start + at;
            work[static_cast<std::size_t>(to + k)] =
                i >= 0 && static_cast<std::size_t>(i) < source.size() ? source[static_cast<std::size_t>(i)] : 0;
            if (++at == ring) {
                at = 0;
            }
        }
    }
    return latitude;
}

void FlatMap::paintSmall(Surface& to, int left, int top, int width, int rows, int firstLatitude,
                         const std::vector<std::uint8_t>& work, int order, const SmallColour& colour) const {
    for (int i = 0; i < rows; ++i) {
        const int r = order < 0 ? rows - 1 - i : i;
        const int lat = std::abs(firstLatitude + r);
        const std::uint8_t* from = &work[static_cast<std::size_t>(kSmallStride * r)];
        const int y = top + r;
        int x = left;
        int in = 0;
        int middle = width;
        int side = 0;
        if (lat >= kFirstPoleEdge) {
            side = edges_.poles[static_cast<std::size_t>(std::min(lat - kFirstPoleEdge, 17))];
            in = side;
            middle = width - 2 * side;
            if (middle < 0) {
                for (int k = 0; k < width; ++k) {
                    to.set(x++, y, 0);
                }
                continue;
            }
            // The left: black, then the rim (only its inner half when the
            // planet reaches nearly to the side).
            if (side >= 4) {
                for (int k = 0; k < side - 4; ++k) {
                    to.set(x++, y, 0);
                }
                to.set(x++, y, kRim[0]);
                to.set(x++, y, kRim[1]);
            }
            to.set(x++, y, kRim[2]);
            to.set(x++, y, kRim[3]);
        }
        for (int k = 0; k < middle; ++k) {
            to.set(x++, y, colour(r, from + in + k, k == middle - 1));
        }
        if (side > 0) {
            to.set(x++, y, kRim[3]);
            to.set(x++, y, kRim[2]);
            if (side >= 4) {
                to.set(x++, y, kRim[1]);
                to.set(x++, y, kRim[0]);
                for (int k = 0; k < side - 4; ++k) {
                    to.set(x++, y, 0);
                }
            }
        }
    }
}

int FlatMap::drawSmall(Surface& to, int left, int top, int right, int bottom, std::uint16_t longitude,
                       int latitude) const {
    const int width = right - left;
    const int rows = bottom - top;
    std::vector<std::uint8_t> work;
    latitude = gatherSmall(map_, width, rows, longitude, latitude, work);
    // Drawn from the bottom row up, each point in its colour.
    paintSmall(to, left, top, width, rows, latitude - (rows - 1) / 2, work, -1,
               [](int, const std::uint8_t* p, bool) { return static_cast<std::uint8_t>(kSmallFirstColour + (*p & 0x0F)); });
    return latitude;
}

int FlatMap::smallStride() { return kSmallStride; }

} // namespace mentat
