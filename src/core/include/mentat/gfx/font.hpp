// SPDX-License-Identifier: GPL-3.0-or-later
//
// The original's bitmap fonts. DNCHAR.BIN holds two: the large one of the
// game's text and the small one of the control panel. See
// docs/formats/font.md.
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace mentat {

class Surface;

class Font {
public:
    static constexpr std::size_t kFileSize = 256 + 256 * 8;
    static constexpr int kCharacters = 128;

    enum class Size {
        Large, // 9 rows, the game's text
        Small, // 7 rows, the control panel
    };

    // Takes one of the two fonts from the bytes of DNCHAR.BIN. Throws
    // FormatError when the file has the wrong size.
    static Font parse(std::span<const std::uint8_t> bytes, Size size = Size::Large);

    // Characters from 0x80 on are drawn as '@', like the original does.
    static std::uint8_t shown(std::uint8_t ch) { return ch < kCharacters ? ch : '@'; }

    int width(std::uint8_t ch) const { return widths_[shown(ch)]; }
    int width(std::string_view text) const;
    int height() const { return height_; }
    // Row y of a character, the leftmost pixel in the highest bit.
    std::uint8_t row(std::uint8_t ch, int y) const {
        return y >= 0 && y < height_ ? rows_[static_cast<std::size_t>(shown(ch)) * static_cast<std::size_t>(height_) + static_cast<std::size_t>(y)] : 0;
    }

    // Draws text with its top-left corner at (x, y); returns the x after it.
    // Only set bits are drawn, unless `background` is not 0: each character
    // then fills its whole cell (its width × the font's height), the clear
    // bits in that colour.
    int draw(Surface& to, int x, int y, std::string_view text, std::uint8_t colour, std::uint8_t background = 0) const;

private:
    std::array<std::uint8_t, kCharacters> widths_{};
    std::vector<std::uint8_t> rows_;
    int height_ = 9;
};

} // namespace mentat
