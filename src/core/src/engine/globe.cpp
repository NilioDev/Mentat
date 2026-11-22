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

void Globe::startPicture() {
    lowerHalf_ = false;
    at_ = 1;
    y_ = kCentreY;
}

std::uint8_t Globe::colourAt(int index) const {
    const std::uint8_t m = map_[static_cast<std::size_t>(mapMiddle_ + index)];
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
