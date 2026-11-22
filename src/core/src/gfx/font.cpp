// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/gfx/font.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/gfx/surface.hpp"

#include <algorithm>

namespace mentat {

Font Font::parse(std::span<const std::uint8_t> bytes) {
    if (bytes.size() != kFileSize) {
        throw FormatError("font file has the wrong size");
    }
    Font f;
    std::copy_n(bytes.begin(), 256, f.widths_.begin());
    std::copy_n(bytes.begin() + 256, 256 * 8, f.rows_.begin());
    return f;
}

int Font::width(std::string_view text) const {
    int w = 0;
    for (const char c : text) {
        w += width(static_cast<std::uint8_t>(c));
    }
    return w;
}

int Font::draw(Surface& to, int x, int y, std::string_view text, std::uint8_t colour) const {
    for (const char c : text) {
        const auto ch = static_cast<std::uint8_t>(c);
        for (int row = 0; row < kHeight; ++row) {
            const std::uint8_t bits = this->row(ch, row);
            for (int col = 0; col < 8; ++col) {
                if (bits & (0x80 >> col)) {
                    to.set(x + col, y + row, colour);
                }
            }
        }
        x += width(ch);
    }
    return x;
}

} // namespace mentat
