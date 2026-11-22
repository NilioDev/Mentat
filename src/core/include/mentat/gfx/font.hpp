// SPDX-License-Identifier: GPL-3.0-or-later
//
// The original's bitmap fonts (DNCHAR.BIN, DNCHAR2.BIN). See docs/formats/font.md.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace mentat {

class Surface;

class Font {
public:
    static constexpr std::size_t kFileSize = 256 + 256 * 8;
    static constexpr int kHeight = 8;

    // Throws FormatError when the file has the wrong size.
    static Font parse(std::span<const std::uint8_t> bytes);

    int width(std::uint8_t ch) const { return widths_[ch]; }
    int width(std::string_view text) const;
    std::uint8_t row(std::uint8_t ch, int y) const { return rows_[static_cast<std::size_t>(ch) * 8 + static_cast<std::size_t>(y & 7)]; }

    // Draws text with its top-left corner at (x, y); returns the x after it.
    int draw(Surface& to, int x, int y, std::string_view text, std::uint8_t colour) const;

private:
    std::array<std::uint8_t, 256> widths_{};
    std::array<std::uint8_t, 256 * 8> rows_{};
};

} // namespace mentat
