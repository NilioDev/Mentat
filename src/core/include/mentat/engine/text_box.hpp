// SPDX-License-Identifier: GPL-3.0-or-later
//
// How the original lays text out in a box: words wrapped onto lines,
// lines stretched to the box's width, the block centred in the box's
// height. See docs/rules/text.md.
#pragma once

#include "mentat/gfx/scene.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace mentat {

class Font;
class Surface;

struct TextLine {
    int words = 0;
    int gap = 0;   // pixels between words
    int extra = 0; // the first `extra` gaps are one pixel wider
    std::size_t start = 0;
    std::size_t end = 0; // in the text
};

// Breaks `text` into lines no wider than `width`. Each line is stretched
// to the width unless it is the last, ends with a line break, or some line
// would get a gap of 30 or more (then none is stretched). Empty when a
// word alone is wider than the box.
std::vector<TextLine> layoutText(const Font& font, std::string_view text, int width);

// Lays `text` out in `box` and draws it in `colour`, centred in the box's
// height, with 10 rows a line. Returns false when it does not fit.
bool drawTextBox(Surface& to, const Font& font, std::string_view text, const ClipRect& box, std::uint8_t colour);

} // namespace mentat
