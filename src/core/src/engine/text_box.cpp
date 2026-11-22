// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/text_box.hpp"

#include "mentat/gfx/font.hpp"
#include "mentat/gfx/surface.hpp"

namespace mentat {

namespace {

constexpr int kGap = 6;
constexpr int kWidestGap = 30;
constexpr int kLineHeight = 10;
constexpr char kLineBreak = 0x0D;

constexpr char kSmallFont = 0x06;
constexpr char kLargeFont = 0x08;
constexpr unsigned char kLastControl = 0x08;

bool isSpace(char c) { return c == ' ' || c == kLineBreak; }

bool isControl(char c) { return c != 0 && static_cast<unsigned char>(c) <= kLastControl; }

// The font after control byte `c`.
const Font* switchFont(char c, const Font* current, const Font& large, const Font* small) {
    if (small != nullptr && c == kSmallFont) {
        return small;
    }
    if (c == kLargeFont) {
        return &large;
    }
    return current;
}

std::string_view untilEnd(std::string_view text) {
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (static_cast<unsigned char>(text[i]) >= 0x80) {
            return text.substr(0, i);
        }
    }
    return text;
}

} // namespace

std::vector<TextLine> layoutText(const Font& font, std::string_view text, int width, const Font* small) {
    text = untilEnd(text);
    const Font* current = &font;
    std::vector<TextLine> lines;
    TextLine line;
    int left = width; // room left after the words so far, each with a gap after it
    bool stretch = true;
    const auto finish = [&](bool natural) {
        if (line.words > 0) {
            const int room = left; // room left with no gap after the last word
            if (natural) {
                line.gap = kGap;
                line.extra = 0;
            } else if (line.words > 1) {
                line.gap = kGap + room / (line.words - 1);
                line.extra = room % (line.words - 1);
            } else {
                line.gap = kGap + room;
                line.extra = 0;
            }
            lines.push_back(line);
        }
        line = TextLine{};
        left = width;
    };
    std::size_t at = 0;
    while (at < text.size()) {
        if (text[at] == ' ') {
            ++at;
            continue;
        }
        if (text[at] == kLineBreak) {
            if (line.words > 0) {
                line.end = at;
                left += kGap;
                finish(true);
            }
            ++at;
            continue;
        }
        const std::size_t start = at;
        int w = 0;
        while (at < text.size() && !isSpace(text[at])) {
            if (isControl(text[at])) {
                current = switchFont(text[at], current, font, small);
            } else {
                w += current->width(static_cast<std::uint8_t>(text[at]));
            }
            ++at;
        }
        if (w == 0) {
            continue; // a word that takes no room is not one
        }
        for (;;) {
            if (line.words == 0) {
                line.start = start;
            }
            if (left - (w + kGap) >= 0) {
                left -= w + kGap;
                ++line.words;
                line.end = at;
                break;
            }
            if (left - w >= 0) {
                // It fits without a gap after it: the line ends with it.
                left -= w;
                ++line.words;
                line.end = at;
                finish(false);
                break;
            }
            if (line.words == 0) {
                return {}; // a word wider than the box
            }
            left += kGap;
            finish(false);
        }
    }
    if (line.words > 0) {
        left += kGap;
        finish(true);
    }
    // The last line is never stretched.
    if (!lines.empty()) {
        lines.back().gap = kGap;
        lines.back().extra = 0;
    }
    for (const auto& l : lines) {
        stretch = stretch && l.gap < kWidestGap;
    }
    if (!stretch) {
        for (auto& l : lines) {
            l.gap = kGap;
            l.extra = 0;
        }
    }
    return lines;
}

int drawTextLines(Surface& to, const Font& font, std::string_view text, const std::vector<TextLine>& lines, int x0, int y,
                  std::uint8_t colour, const Font* small) {
    const Font* current = &font;
    for (const auto& line : lines) {
        int x = x0;
        int extra = line.extra;
        std::size_t at = line.start;
        while (at < line.end) {
            if (isSpace(text[at])) {
                while (at < line.end && isSpace(text[at])) {
                    ++at;
                }
                x += line.gap + (extra > 0 ? 1 : 0);
                extra = extra > 0 ? extra - 1 : 0;
                continue;
            }
            const char c = text[at++];
            if (isControl(c)) {
                current = switchFont(c, current, font, small);
                continue;
            }
            x = current->draw(to, x, y, std::string_view(&c, 1), colour);
        }
        y += kLineHeight;
    }
    return y;
}

bool drawTextBox(Surface& to, const Font& font, std::string_view text, const ClipRect& box, std::uint8_t colour) {
    const auto lines = layoutText(font, text, box.right - box.left);
    if (lines.empty()) {
        return false;
    }
    const int height = box.bottom - box.top;
    const int used = static_cast<int>(lines.size()) * kLineHeight;
    drawTextLines(to, font, text, lines, box.left, box.top + (height > used ? (height - used) / 2 : 0), colour);
    return true;
}

} // namespace mentat
