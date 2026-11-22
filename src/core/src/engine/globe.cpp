// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/globe.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/gfx/surface.hpp"

#include <algorithm>
#include <cstdlib>

namespace mentat {

namespace {

constexpr int kRows = 99;               // latitudes from the equator to the pole
constexpr std::size_t kRecord = 8;      // bytes per latitude in TABLAT.BIN
constexpr std::size_t kColumns = 0xCDA; // where the column tables start in GLOBDATA
constexpr int kColumnCount = 64;
constexpr std::size_t kColumnSize = 200;
constexpr int kLongitudes = 100; // second half of a column: the longitudes
constexpr int kMapMiddle = 0x62FC;
constexpr std::uint8_t kLineEnd = 0x80;

} // namespace

Globe::Globe(std::span<const std::uint8_t> latitudes, std::vector<std::uint8_t> projection,
             std::vector<std::uint8_t> map)
    : projection_(std::move(projection)), map_(std::move(map)), mapMiddle_(kMapMiddle) {
    if (latitudes.size() < kRows * kRecord) {
        throw FormatError("globe: latitude table too short");
    }
    if (projection_.size() < kColumns + kColumnCount * kColumnSize) {
        throw FormatError("globe: projection tables too short");
    }
    for (int r = 0; r < kRows; ++r) {
        const std::size_t at = static_cast<std::size_t>(r) * kRecord;
        Row row;
        row.base = (latitudes[at] << 8) | latitudes[at + 1];
        row.width = (latitudes[at + 2] << 8) | latitudes[at + 3];
        if (row.width == 0 || mapMiddle_ - row.base < 0 ||
            static_cast<std::size_t>(mapMiddle_ + row.base + 2 * row.width) > map_.size()) {
            throw FormatError("globe: a latitude falls outside the map");
        }
        rows_.push_back(row);
    }
    // The lower half starts after the first line, whose length byte 0 gives.
    const int skip = 0x100 - projection_[0];
    if (projection_[0] < 0x80 || 1 + static_cast<std::size_t>(skip) >= kColumns) {
        throw FormatError("globe: bad first line");
    }
    setView(0, 0);
}

void Globe::setView(int rotation, int tilt) {
    tilt_ = std::clamp(tilt, -kMaxTilt, kMaxTilt);
    rotation_ = 0;
    turn(rotation);
    startPicture();
}

void Globe::turn(int steps) {
    rotation_ = ((rotation_ + steps) % kTurn + kTurn) % kTurn;
    // The rotation as a fraction of a turn, in 16 bits, rounded.
    const auto fraction = static_cast<std::uint32_t>(((static_cast<std::uint32_t>(rotation_) << 16) + 0x8000) / kTurn);
    for (Row& row : rows_) {
        row.phase = static_cast<int>((2u * static_cast<std::uint32_t>(row.width) * fraction) >> 16);
    }
}

void Globe::tilt(int steps) { tilt_ = std::clamp(tilt_ + steps, -kMaxTilt, kMaxTilt); }

std::uint16_t Globe::heightEntry(int u) const {
    const std::uint16_t side = u > 0 ? 0xFF00 : 0;
    int j = std::abs(u);
    if (j >= kRows) {
        j = -(2 * kRows - 1 - j); // past the pole
    }
    return static_cast<std::uint16_t>(side | static_cast<std::uint8_t>(j));
}

bool Globe::screenPoint(std::uint16_t longitude, int latitude, int& x, int& y) const {
    const bool north = latitude < 0;
    const int a = std::abs(latitude);
    if (a >= kRows) {
        return false;
    }
    const Row& row = rows_[static_cast<std::size_t>(a)];
    const int w = row.width;
    // The nearest point of the row, and how far round from the middle of
    // the face it is, and on which side.
    const auto product = static_cast<std::uint32_t>(2 * w) * longitude;
    const int point = static_cast<int>((product + 0x8000) >> 16);
    int d = point - row.phase;
    bool left = false;
    if (d < 0) {
        d = -d;
        left = true;
    }
    if (d >= w) {
        d = 2 * w - d;
        left = !left;
    }
    bool behind = false;
    if (d >= w / 2) {
        behind = true;
        d = 2 * (w / 2) - d;
    }

    // The first column, from the middle outwards, that reaches that far
    // round at the height where it crosses the row.
    const int wanted = 2 * a;
    int column = 0;
    int height = 0;
    int lastHeight = -1;
    for (int from = 0;; ++column) {
        if (column >= kColumnCount) {
            return false;
        }
        const std::size_t at = kColumns + static_cast<std::size_t>(column) * kColumnSize;
        int h = from;
        while (h < kLongitudes && projection_[at + static_cast<std::size_t>(h)] != wanted) {
            ++h;
        }
        if (h == kLongitudes) {
            if (lastHeight < 0) {
                return false;
            }
            height = lastHeight;
            break;
        }
        if (d <= projection_[at + kLongitudes + static_cast<std::size_t>(h)]) {
            height = h;
            break;
        }
        lastHeight = h;
        from = h;
    }

    // Which value of the screen lines shows that height.
    const std::uint16_t entry = static_cast<std::uint16_t>((north ? 0xFF00 : 0) | static_cast<std::uint8_t>(behind ? -height : height));
    int found = -1;
    for (int i = 34; i < 34 + 0x80; ++i) {
        if (heightEntry(i - kRows + 1 - tilt_) == entry) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        return false;
    }
    const bool lower = found < kRows - 1;
    const int value = std::abs(found - (kRows - 1));

    // The screen line whose byte in that column comes nearest above it.
    std::size_t pos = 0;
    int best = 0xFF;
    int bestLine = -1;
    for (int line = 0; line < 54; ++line) {
        const int b = projection_[pos];
        const int length = (0x100 - ((b + 1) & 0xFF)) & 0xFF;
        if (((b + 1) & 0xFF) == 0 || length <= column) {
            break;
        }
        const int diff = (projection_[pos + 1 + static_cast<std::size_t>(column)] - value) & 0xFF;
        pos += 1 + static_cast<std::size_t>(length);
        if (diff < best) {
            best = diff;
            bestLine = line;
            if (diff == 0) {
                break;
            }
        }
    }
    if (best > 2) {
        return false;
    }
    x = kCentreX + (left ? -column : column);
    y = kCentreY + (lower ? bestLine : -bestLine);
    return true;
}

bool Globe::mapPoint(int x, int y, std::uint16_t& longitude, int& latitude) const {
    if (x < 96 || x >= 224 || y < 25 || y >= 134) {
        return false;
    }
    const int fromMiddle = y - kCentreY;
    // The screen line |fromMiddle| of the list.
    std::size_t pos = 0;
    int length = 0;
    for (int n = std::abs(fromMiddle) + 1;;) {
        pos += static_cast<std::size_t>(length);
        const int b = projection_[pos++];
        length = (0x100 - ((b + 1) & 0xFF)) & 0xFF;
        if (--n == 0 || length == 0) {
            break;
        }
    }
    const int across = x - kCentreX;
    const int column = std::abs(across);
    if (column >= length) {
        return false;
    }
    const int v = projection_[pos + static_cast<std::size_t>(column)];
    const std::uint16_t entry = heightEntry((fromMiddle >= 0 ? -v : v) - tilt_);
    const int signedHeight = static_cast<std::int8_t>(entry & 0xFF);
    const std::size_t at = kColumns + static_cast<std::size_t>(column) * kColumnSize + static_cast<std::size_t>(std::abs(signedHeight));
    const int rowTimesTwo = projection_[at];
    int around = projection_[at + kLongitudes];
    const std::size_t r = static_cast<std::size_t>(rowTimesTwo >> 1);
    if (r >= rows_.size()) {
        return false;
    }
    const Row& row = rows_[r];
    if (signedHeight < 0) {
        around = row.width - around;
    }
    if (across < 0) {
        around = -around;
    }
    const int ring = 2 * row.width;
    int point = row.phase + around;
    if (point < 0) {
        point += ring;
    }
    if (point >= ring) {
        point -= ring;
    }
    longitude = static_cast<std::uint16_t>((static_cast<std::uint32_t>(point) << 16) / static_cast<std::uint32_t>(ring));
    latitude = ((entry & 0x8000) != 0 ? -rowTimesTwo : rowTimesTwo) >> 1;
    return true;
}

void Globe::startPicture() {
    lowerHalf_ = false;
    at_ = 1;
    y_ = kCentreY;
}

std::uint8_t Globe::colourAt(int index) const {
    const std::uint8_t m = map_[static_cast<std::size_t>(mapMiddle_ + index)];
    if (holders_) {
        // Bits 4 and 5: 01 and 10 move the colour up 16, 11 up 32.
        const int holder = m & 0x30;
        int c = (m & 0x0F) + 0x10;
        if (holder >= 0x10) {
            c += 0x10 + (((holder - 0x10) >> 1) & 0x10);
        }
        return static_cast<std::uint8_t>(c);
    }
    int c = m & 0x0F;
    if ((m & 0x30) == 0x10 && c < 8) {
        c += 0x0C;
    }
    return static_cast<std::uint8_t>(c + 0x10);
}

bool Globe::drawLine(Surface& to) {
    if (projection_[at_] >= kLineEnd) {
        if (lowerHalf_) {
            startPicture();
            return true;
        }
        lowerHalf_ = true;
        at_ = 1 + static_cast<std::size_t>(0x100 - projection_[0]);
        y_ = kCentreY + 1;
    }
    for (int x = 0; projection_[at_] < kLineEnd; ++x, ++at_) {
        if (x >= kColumnCount || at_ + 1 >= kColumns) {
            throw FormatError("globe: a line is too long");
        }
        const int v = lowerHalf_ ? -projection_[at_] : projection_[at_];
        // Where this point of the sphere is, once tilted.
        const int u = v - tilt_;
        const bool south = u > 0;
        int j = std::abs(u);
        bool overPole = false;
        if (j >= kRows) {
            j = 2 * kRows - 1 - j;
            overPole = true;
        }
        const std::size_t column = kColumns + static_cast<std::size_t>(x) * kColumnSize;
        // The latitude is stored doubled.
        const int r = projection_[column + static_cast<std::size_t>(j)] >> 1;
        if (r >= kRows) {
            throw FormatError("globe: bad latitude in the projection");
        }
        const Row& row = rows_[static_cast<std::size_t>(r)];
        int longitude = projection_[column + kLongitudes + static_cast<std::size_t>(j)];
        if (overPole) {
            longitude = row.width - longitude;
        }
        const int base = south ? -row.base : row.base;
        const int around = 2 * row.width;
        int left = row.phase - longitude;
        if (left < 0) {
            left += around;
        }
        int right = row.phase + longitude;
        if (right >= around) {
            right -= around;
        }
        if (left < 0 || left >= around || right < 0 || right >= around) {
            throw FormatError("globe: longitude out of range");
        }
        to.set(kCentreX - 1 - x, y_, colourAt(base + left));
        to.set(kCentreX + x, y_, colourAt(base + right));
    }
    ++at_;
    y_ += lowerHalf_ ? 1 : -1;
    return false;
}

void Globe::drawAll(Surface& to) {
    startPicture();
    while (!drawLine(to)) {
    }
}

} // namespace mentat
