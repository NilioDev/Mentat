// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/gfx/font.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/gfx/surface.hpp"

#include <algorithm>

namespace mentat {

namespace {
constexpr std::size_t kLargeRows = 256;
constexpr int kLargeHeight = 9;
constexpr std::size_t kSmallRows = kLargeRows + Font::kCharacters * kLargeHeight;
constexpr int kSmallHeight = 7;
} // namespace

Font Font::parse(std::span<const std::uint8_t> bytes, Size size) {
    if (bytes.size() != kFileSize) {
        throw FormatError("font file has the wrong size");
    }
    Font f;
    const bool large = size == Size::Large;
    f.height_ = large ? kLargeHeight : kSmallHeight;
    const std::size_t widths = large ? 0 : kCharacters;
    const std::size_t rows = large ? kLargeRows : kSmallRows;
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(widths), kCharacters, f.widths_.begin());
    const std::size_t count = static_cast<std::size_t>(kCharacters * f.height_);
    f.rows_.assign(bytes.begin() + static_cast<std::ptrdiff_t>(rows),
                   bytes.begin() + static_cast<std::ptrdiff_t>(rows + count));
    return f;
}

int Font::width(std::string_view text) const {
    int w = 0;
    for (const char c : text) {
        w += width(static_cast<std::uint8_t>(c));
    }
    return w;
}

int Font::draw(Surface& to, int x, int y, std::string_view text, std::uint8_t colour, std::uint8_t background) const {
    for (const char c : text) {
        const auto ch = static_cast<std::uint8_t>(c);
        const int w = width(ch);
        for (int row = 0; row < height_; ++row) {
            const std::uint8_t bits = this->row(ch, row);
            for (int col = 0; col < w; ++col) {
                // Columns past the eighth have no bits of their own.
                const bool set = col < 8 && (bits & (0x80 >> col)) != 0;
                if (set) {
                    to.set(x + col, y + row, colour);
                } else if (background != 0) {
                    to.set(x + col, y + row, background);
                }
            }
        }
        x += w;
    }
    return x;
}

} // namespace mentat
