// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/display.hpp"

#include "mentat/data/reader.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <span>
#include <utility>
#include <vector>

namespace mentat {

namespace {

constexpr int kSceneRows = 152;
constexpr std::uint32_t kStepTicks = 3;

// Component `i` (0..767) of a palette, as the original keeps them: red,
// green and blue of colour 0, then of colour 1, and so on.
std::uint8_t& component(Palette& p, int i) {
    Colour& c = p[static_cast<std::size_t>(i / 3)];
    switch (i % 3) {
    case 0:
        return c.r;
    case 1:
        return c.g;
    default:
        return c.b;
    }
}

} // namespace

DisplayTables DisplayTables::fromDriver(std::span<const std::uint8_t> driver) {
    constexpr std::size_t kDissolveOrder = 0x2ED7; // in the file; the driver runs from offset 0x100
    Reader r(driver);
    r.seek(kDissolveOrder);
    DisplayTables t;
    for (int& place : t.dissolveOrder) {
        place = r.u16();
        if (place % Surface::kScreenWidth > 3 || place / Surface::kScreenWidth > 3) {
            throw FormatError("video driver: unexpected dissolve order");
        }
    }
    if (r.u16() != 0xFFFF) {
        throw FormatError("video driver: unexpected dissolve order");
    }
    t.mapEdges = FlatMap::Edges::fromDriver(driver);
    return t;
}

Display::Display(Platform& platform, const DisplayTables& tables)
    : platform_(platform), tables_(tables), back_(&backStore_), screen_(&platform.screen), target_(&backStore_) {}

void Display::showBack() {
    if (back_ != screen_) {
        *screen_ = *back_;
    }
}

void Display::copyRect(const Surface& from, Surface& to, const ClipRect& rect) {
    if (&from == &to) {
        return;
    }
    for (int y = std::max(rect.top, 0); y < rect.bottom; ++y) {
        const int row = y + originRows_;
        if (row < 0 || row >= to.height() || row >= from.height()) {
            continue;
        }
        for (int x = std::max(rect.left, 0); x < rect.right && x < to.width(); ++x) {
            to.set(x, row, from.at(x, row));
        }
    }
}

void Display::clearRect(Surface& s, const ClipRect& rect) {
    for (int y = std::max(rect.top, 0); y < rect.bottom; ++y) {
        const int row = y + originRows_;
        if (row < 0 || row >= s.height()) {
            continue;
        }
        for (int x = std::max(rect.left, 0); x < rect.right && x < s.width(); ++x) {
            s.set(x, row, 0);
        }
    }
}

namespace {

// For each enlargement: how a group of source pixels is repeated, the same
// way across a row and down a column. A group of `in` pixels becomes the
// listed source pixels, `groups` times; `last` more source pixels may
// follow, each repeated `lastTimes` times.
struct ZoomPattern {
    int width;
    int height;
    std::vector<int> group; // source offsets within a group, one per pixel drawn
    int in;
    int lastTimes; // 0: no extra pixel at the end
    std::vector<int> rowGroup;
    int rowIn;
    int rowLastTimes;
};

const ZoomPattern& zoomPattern(int level) {
    static const std::array<ZoomPattern, 8> patterns = {{
        {320, 152, {0}, 1, 0, {0}, 1, 0},
        {280, 133, {0, 0, 1, 2, 3, 4, 5, 6}, 7, 0, {0, 1, 2, 3, 4, 5, 6, 6}, 7, 0},
        {240, 114, {0, 0, 1, 2}, 3, 0, {0, 1, 2, 2}, 3, 0},
        {213, 100, {0, 0, 1}, 2, 2, {0, 1, 1}, 2, 0},
        {160, 76, {0, 0}, 1, 0, {0, 0}, 1, 0},
        {107, 51, {0, 0, 0}, 1, 2, {0, 0, 0}, 1, 2},
        {80, 38, {0, 0, 0, 0}, 1, 0, {0, 0, 0, 0}, 1, 0},
        {40, 19, {0, 0, 0, 0, 0, 0, 0, 0}, 1, 0, {0, 0, 0, 0, 0, 0, 0, 0}, 1, 0},
    }};
    return patterns[static_cast<std::size_t>(std::clamp(level, 0, 7))];
}

std::vector<int> zoomMap(const std::vector<int>& group, int in, int lastTimes, int size, int limit) {
    std::vector<int> map;
    const int groups = (size - (lastTimes != 0 ? 1 : 0)) / in;
    for (int g = 0; g < groups; ++g) {
        for (const int k : group) {
            map.push_back(g * in + k);
        }
    }
    for (int i = 0; i < lastTimes; ++i) {
        map.push_back(groups * in);
    }
    if (static_cast<int>(map.size()) > limit) {
        map.resize(static_cast<std::size_t>(limit));
    }
    return map;
}

} // namespace

int Display::zoomWidth(int level) { return zoomPattern(level).width; }
int Display::zoomHeight(int level) { return zoomPattern(level).height; }

void Display::zoom(int level, int x, int y) { zoom(level, x, y, *back_, *screen_); }

void Display::zoom(int level, int x, int y, const Surface& from, Surface& to) {
    const ZoomPattern& p = zoomPattern(level);
    const auto cols = zoomMap(p.group, p.in, p.lastTimes, p.width, Surface::kScreenWidth);
    const auto rows = zoomMap(p.rowGroup, p.rowIn, p.rowLastTimes, p.height, kSceneRows);
    for (std::size_t r = 0; r < rows.size(); ++r) {
        const int fromRow = y + rows[r] + originRows_;
        const int row = static_cast<int>(r) + originRows_;
        for (std::size_t c = 0; c < cols.size(); ++c) {
            const int fx = x + cols[c];
            // The original reads straight through memory, so a part that
            // runs off the right edge goes on at the next row.
            const int linear = fromRow * Surface::kScreenWidth + fx;
            if (linear >= 0 && static_cast<std::size_t>(linear) < from.size()) {
                to.set(static_cast<int>(c), row, from.data()[linear]);
            }
        }
    }
}

void Display::markChanged(const std::bitset<256>& colours) {
    dirty_ |= colours;
    if (colours.any()) {
        anyDirty_ = true;
    }
}

void Display::applyPaletteBlocks(std::span<const std::uint8_t> blocks) {
    std::bitset<256> touched;
    shadow_.applyBlocks(blocks, &touched);
    markChanged(touched);
}

void Display::flush() {
    if (!anyDirty_) {
        return;
    }
    anyDirty_ = false;
    for (std::size_t i = 0; i < 256; ++i) {
        if (dirty_.test(i)) {
            platform_.palette[i] = shadow_[i];
        }
    }
    dirty_.reset();
}

void Display::sendColours(int first, int count) {
    for (int i = first; i < first + count && i < 256; ++i) {
        platform_.palette[static_cast<std::size_t>(i)] = shadow_[static_cast<std::size_t>(i)];
    }
}

void Display::fadeStep(int first, int count, int divisor) {
    if (divisor == 0) {
        divisor = 1;
    }
    for (int i = first * 3; i < (first + count) * 3 && i < 768; ++i) {
        std::uint8_t& v = component(shadow_, i);
        const int distance = static_cast<std::int8_t>(static_cast<std::uint8_t>(component(spare_, i) - v));
        v = static_cast<std::uint8_t>(v + distance / divisor);
    }
    sendColours(first, count);
}

// Steps are timed from when the last one was due, not from when the app
// got round to it, so they keep the original's pace.
std::uint32_t Display::waitUntil(std::uint32_t due) {
    while (static_cast<std::int32_t>(due - platform_.ticks) > 0) {
        platform_.waitFrame();
    }
    return due;
}

std::uint32_t Display::waitTicks(std::uint32_t since, std::uint32_t ticks) {
    while (platform_.ticks - since < ticks) {
        platform_.waitFrame();
    }
    return platform_.ticks;
}

// Takes `step` off every component, `chunk` colours at a time, `rounds`
// times. Each chunk is sent to the screen and followed by a short wait.
// Colour 255 is never reached when the chunks are 85 colours long.
void Display::fadeDown(int chunk, int step, int rounds) {
    std::uint32_t due = platform_.ticks;
    for (int round = 0; round < rounds; ++round) {
        for (int first = 0; first < 0xFF; first += chunk) {
            for (int i = first * 3; i < (first + chunk) * 3 && i < 768; ++i) {
                std::uint8_t& v = component(shadow_, i);
                v = v > step ? static_cast<std::uint8_t>(v - step) : 0;
            }
            sendColours(first, chunk);
            due = waitUntil(due + kStepTicks);
        }
    }
}

// Brings colours 0..254 up towards the spare palette over `rounds` rounds,
// `chunk` colours at a time. A component starts to rise only when the
// rounds left are just enough to cover its distance in steps of `step`
// (the odd remainder goes first), so all of them arrive together.
void Display::fadeUp(int chunk, int step, int rounds) {
    std::uint32_t due = platform_.ticks;
    for (int left = rounds; left > 0; --left) {
        for (int first = 0; first * 3 < 0x2FD; first += chunk) {
            for (int i = first * 3; i < (first + chunk) * 3 && i < 768; ++i) {
                std::uint8_t& v = component(shadow_, i);
                const std::uint8_t distance = static_cast<std::uint8_t>(component(spare_, i) - v);
                if (distance == 0) {
                    continue;
                }
                int add = distance % step;
                int steps = distance / step + 1;
                if (add == 0) {
                    add = step;
                    --steps;
                }
                if (steps >= left) {
                    v = static_cast<std::uint8_t>(v + add);
                }
            }
            sendColours(first, chunk);
            due = waitUntil(due + kStepTicks);
        }
    }
}

// Blacks out the scene area through a 4 × 4 pattern, one pattern position
// at a time, then shows the back buffer through the same pattern.
void Display::dissolve(int rows) {
    const int origin = originRows_ * Surface::kScreenWidth;
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) {
            flush();
        }
        std::uint32_t due = platform_.ticks;
        for (const int offset : tables_.dissolveOrder) {
            for (int block = 0; block < rows / 4; ++block) {
                for (int column = 0; column < 80; ++column) {
                    const std::size_t at = static_cast<std::size_t>(origin + offset + block * 0x500 + column * 4);
                    if (at < screen_->size()) {
                        screen_->data()[at] = pass == 0 ? 0 : back_->data()[at];
                    }
                }
            }
            due = waitUntil(due + kStepTicks);
        }
    }
}

// The old picture breaks up into blocks of the sizes `out` (2, 4 or 8
// pixels wide), each block taking the colour of one of its pixels, the
// pixel changing in a fixed order; then the new picture comes out of
// blocks of the sizes `in`. Each size lasts 0x24 ticks.
void Display::mosaic(std::span<const int> out, std::span<const int> in) {
    struct Size {
        int side;
        std::vector<std::array<int, 2>> pixels;
    };
    static const std::array<Size, 3> sizes{{
        {2, {{0, 0}, {1, 1}, {1, 0}, {0, 1}}},
        {4, {{1, 1}, {0, 3}, {3, 2}, {2, 0}, {0, 1}, {2, 3}, {0, 0}, {1, 2},
             {3, 0}, {1, 3}, {2, 1}, {3, 3}, {2, 2}, {1, 0}, {3, 1}, {0, 2}}},
        {8, {{3, 3}, {0, 5}, {5, 4}, {4, 2}, {0, 3}, {4, 5}, {0, 2}, {3, 4},
             {5, 2}, {3, 5}, {4, 3}, {5, 5}, {4, 4}, {3, 2}, {5, 3}, {2, 4}}},
    }};
    constexpr std::uint32_t kSizeTicks = 0x24;
    const int w = Surface::kScreenWidth;
    const std::size_t origin = static_cast<std::size_t>(originRows_ * w);
    const std::size_t area = static_cast<std::size_t>(kSceneRows * w);
    const std::vector<std::uint8_t> old(screen_->data() + origin, screen_->data() + origin + area);
    std::uint8_t* to = screen_->data() + origin;
    auto blocks = [&](const std::uint8_t* from, const Size& size) {
        const std::uint32_t start = platform_.ticks;
        std::uint32_t due = start;
        for (std::size_t k = 0;; k = (k + 1) % size.pixels.size()) {
            const int n = size.side;
            for (int by = 0; by + n <= kSceneRows; by += n) {
                for (int bx = 0; bx < w; bx += n) {
                    const std::size_t at = static_cast<std::size_t>((by + size.pixels[k][1]) * w + bx + size.pixels[k][0]);
                    const std::uint8_t c = from[std::min(at, area - 1)];
                    for (int y = 0; y < n; ++y) {
                        std::fill_n(to + (by + y) * w + bx, n, c);
                    }
                }
            }
            due = waitUntil(due + 1);
            if (platform_.ticks - start >= kSizeTicks) {
                break;
            }
        }
    };
    auto bySide = [&](int side) -> const Size& { return sizes[side == 2 ? 0 : side == 4 ? 1 : 2]; };
    for (const int side : out) {
        blocks(old.data(), bySide(side));
    }
    flush();
    for (const int side : in) {
        blocks(back_->data() + origin, bySide(side));
    }
}

// The old picture moves up 8 rows a step with a band of colour 7 under it,
// over the new one shown in place.
void Display::rise() {
    constexpr int kBand = 8;
    constexpr std::uint8_t kBandColour = 7;
    constexpr std::uint32_t kRiseTicks = 6;
    const int w = Surface::kScreenWidth;
    const std::size_t origin = static_cast<std::size_t>(originRows_ * w);
    std::vector<std::uint8_t> old(screen_->data() + origin,
                                  screen_->data() + origin + static_cast<std::size_t>(kSceneRows * w));
    std::uint8_t* to = screen_->data() + origin;
    const std::uint8_t* from = back_->data() + origin;
    std::uint32_t due = platform_.ticks;
    for (int s = kBand; s <= kSceneRows; s += kBand) {
        const int kept = kSceneRows - s;
        std::copy_n(old.begin() + s * w, kept * w, to);
        std::fill_n(to + kept * w, kBand * w, kBandColour);
        if (kept + kBand < kSceneRows) {
            const auto at = static_cast<std::ptrdiff_t>((kept + kBand) * w);
            std::copy(from + at, from + kSceneRows * w, to + at);
        }
        due = waitUntil(due + kRiseTicks);
    }
}

// The old picture is squashed to the middle line, then the new one grows
// out of it: each step draws every half from the middle out, copying runs
// of rows and leaving some out (see docs/rules/display.md).
void Display::squash() {
    // Per step from 0x11 down to 1: the rows left out after each run, and
    // the run.
    static constexpr std::array<std::array<int, 2>, 0x12> kSteps{{{0, 0},
                                                                  {1, 0x11},
                                                                  {1, 7},
                                                                  {1, 4},
                                                                  {1, 2},
                                                                  {2, 3},
                                                                  {5, 4},
                                                                  {2, 1},
                                                                  {5, 1},
                                                                  {0, 0},
                                                                  {5, 1},
                                                                  {2, 1},
                                                                  {5, 4},
                                                                  {2, 3},
                                                                  {1, 2},
                                                                  {1, 4},
                                                                  {1, 7},
                                                                  {1, 0x11}}};
    constexpr int kMiddleStep = 9;
    constexpr std::uint32_t kSquashTicks = 6;
    const int w = Surface::kScreenWidth;
    const int half = kSceneRows / 2;
    const std::size_t origin = static_cast<std::size_t>(originRows_ * w);
    const std::vector<std::uint8_t> old(screen_->data() + origin,
                                        screen_->data() + origin + static_cast<std::size_t>(kSceneRows * w));
    std::uint8_t* to = screen_->data() + origin;
    const std::uint8_t* from = old.data();
    std::uint32_t due = platform_.ticks;
    for (int step = 0x11; step > 0; --step) {
        std::fill_n(to, kSceneRows * w, std::uint8_t{0});
        if (step == kMiddleStep) {
            flush();
            from = back_->data() + origin;
        } else {
            const int skip = kSteps.at(static_cast<std::size_t>(step))[0];
            const int run = kSteps.at(static_cast<std::size_t>(step))[1];
            int left = half;
            int source = 0;
            int drawn = 0;
            for (;;) {
                bool done = false;
                for (int r = 0; r < run; ++r) {
                    if (--left < 0) {
                        done = true;
                        break;
                    }
                    std::copy_n(from + (half - 1 - source) * w, w, to + (half - 1 - drawn) * w);
                    std::copy_n(from + (half + source) * w, w, to + (half + drawn) * w);
                    ++source;
                    ++drawn;
                }
                if (done || left <= skip) {
                    break;
                }
                left -= skip;
                source += skip;
            }
        }
        due = waitUntil(due + kSquashTicks);
    }
}

// Blocks of 8 × 4 pixels of the new picture, in a square spiral out from
// (156, 74): one side of the spiral a tick.
// The back picture comes pixel by pixel in an order that looks random:
// a 16-bit number from 1, shifted right with 0x4400 mixed in for each bit
// shifted out, names the pixel, and the one 0x7FFE further on goes with
// it; 150 steps a tick, until the number is 1 again. Pixel 0 comes last.
void Display::sparkle() {
    constexpr int kStepsPerTick = 0x96;
    const std::size_t origin = static_cast<std::size_t>(originRows_) * Surface::kScreenWidth;
    const std::size_t area = static_cast<std::size_t>(kSceneRows) * Surface::kScreenWidth;
    const std::size_t size = screen_->size();
    auto copy = [&](std::size_t at) {
        if (origin + at < size) {
            screen_->data()[origin + at] = back_->data()[origin + at];
        }
    };
    std::uint32_t due = platform_.ticks;
    unsigned n = 1;
    int steps = kStepsPerTick;
    for (;;) {
        copy(n);
        if (n + 0x7FFE < area) {
            copy(n + 0x7FFE);
        }
        const bool out = (n & 1) != 0;
        n >>= 1;
        if (out) {
            n ^= 0x4400;
        }
        if (n == 1) {
            break;
        }
        if (--steps == 0) {
            steps = kStepsPerTick;
            due = waitUntil(due + 1);
        }
    }
    copy(0);
}

void Display::transitionIn(int type, const ClipRect& rect) {
    const int w = Surface::kScreenWidth;
    const int rows = screen_->height();
    const int left = std::max(rect.left, 0);
    const int width = std::min(rect.right, w) - left;
    if (width <= 0) {
        return;
    }
    auto copyRow = [&](const Surface& from, int fromRow, int toRow) {
        if (fromRow < 0 || fromRow >= rows || toRow < 0 || toRow >= rows) {
            return;
        }
        std::copy_n(from.data() + static_cast<std::size_t>(fromRow * w + left), width,
                    screen_->data() + static_cast<std::size_t>(toRow * w + left));
    };
    const int top = rect.top + originRows_;
    std::uint32_t due = platform_.ticks;
    int steps = 0;
    // The original goes as fast as it can; Mentat does a few steps a tick.
    auto pace = [&](int perTick) {
        if (++steps % perTick == 0) {
            due = waitUntil(due + 1);
        }
    };
    switch (type) {
    case kUnroll: {
        // The bottom rows of the picture first, at the top: two rows more
        // each step, taken from two rows higher.
        constexpr int kPerTick = 16;
        int count = 2;
        for (int from = rect.bottom - 2 + originRows_; from >= 0; from -= 2, count += 2) {
            for (int r = 0; r < count; ++r) {
                copyRow(*back_, from + r, top + r);
            }
            pace(kPerTick);
        }
        break;
    }
    case kRollUp: {
        // The rows above move up by 6; the 6 under them come from the back
        // picture.
        constexpr int kShift = 6;
        constexpr int kPerTick = 4;
        int count = rect.bottom - rect.top - kShift;
        for (;;) {
            for (int r = 0; r < count; ++r) {
                copyRow(*screen_, top + r + kShift, top + r);
            }
            for (int r = count; r < count + kShift; ++r) {
                copyRow(*back_, top + r, top + r);
            }
            pace(kPerTick);
            count -= kShift;
            if (count >= 0) {
                continue;
            }
            if (count == -kShift) {
                break;
            }
            count = 0;
        }
        break;
    }
    default:
        throw FormatError("display: unknown transition in a rectangle " + std::to_string(type));
    }
    copyRect(*back_, *screen_, rect);
}

// The back picture comes down from the top, pushing the old one down: a
// row every 6 ticks, two when behind.
void Display::slideDown() {
    constexpr int kRowTicks = 6;
    const int w = Surface::kScreenWidth;
    const std::size_t origin = static_cast<std::size_t>(originRows_) * static_cast<std::size_t>(w);
    const std::size_t area = std::min<std::size_t>(static_cast<std::size_t>(kSceneRows * w), screen_->size() - origin);
    const std::vector<std::uint8_t> old(screen_->data() + origin, screen_->data() + origin + area);
    const std::uint32_t start = platform_.ticks;
    int counter = 0;
    std::size_t shown = 0; // bytes of the back picture at the top
    for (;;) {
        std::copy_n(back_->data() + origin + area - shown, shown, screen_->data() + origin);
        std::copy_n(old.data(), area - shown, screen_->data() + origin + shown);
        ++counter;
        const auto due = static_cast<std::uint32_t>(counter * kRowTicks);
        while (platform_.ticks - start < due) {
            platform_.waitFrame();
        }
        std::size_t step = static_cast<std::size_t>(w);
        if (platform_.ticks - start != due) {
            step *= 2;
            ++counter;
        }
        if (shown == area) {
            break;
        }
        shown = std::min(shown + step, area);
    }
}

void Display::spiral() {
    const int w = Surface::kScreenWidth;
    const int size = static_cast<int>(screen_->size());
    int at = (originRows_ + 74) * w + 156;
    std::uint32_t due = platform_.ticks;
    auto run = [&](int blocks, int step) {
        for (int b = 0; b < blocks; ++b) {
            for (int row = 0; row < 4; ++row) {
                for (int x = 0; x < 8; ++x) {
                    const int p = at + row * w + x;
                    if (p >= 0 && p < size) {
                        screen_->data()[p] = back_->data()[p];
                    }
                }
            }
            at += step;
        }
        due = waitUntil(due + 1);
    };
    const int right = 8;
    const int down = 4 * w;
    run(1, right);
    run(2, down);
    for (int n = 2; n < 0x26; n += 2) {
        at -= down + right;
        run(n, -right);
        at += right - down;
        run(n, -down);
        at += down + right;
        if (n + 1 >= 0x26) {
            break;
        }
        run(n + 1, right);
        at += down - right;
        run(n + 1, down);
    }
}

void Display::transition(int type, int direction) {
    switch (type & 0xFE) {
    case kRise:
        if (direction >= 0) {
            throw FormatError("display: the downward transition 0 is not built");
        }
        rise();
        break;
    case kSpiral:
        spiral();
        break;
    case kSquash:
        squash();
        break;
    case kSparkle:
        sparkle();
        break;
    case kSlideDown:
        slideDown();
        break;
    case kMosaic: {
        static constexpr std::array<int, 3> kOut{2, 4, 8};
        static constexpr std::array<int, 3> kIn{8, 4, 2};
        mosaic(kOut, kIn);
        break;
    }
    case kMosaicShort: {
        static constexpr std::array<int, 2> kOut{2, 4};
        static constexpr std::array<int, 2> kIn{4, 2};
        mosaic(kOut, kIn);
        break;
    }
    case kMosaicFine: {
        static constexpr std::array<int, 3> kOut{2, 2, 2};
        static constexpr std::array<int, 1> kIn{2};
        mosaic(kOut, kIn);
        break;
    }
    case kFadeThroughBlack:
        // Colours 1..254 of the new palette wait in the spare one while
        // the old picture fades out.
        for (std::size_t i = 1; i < 255; ++i) {
            std::swap(shadow_[i], spare_[i]);
        }
        fadeDown(0x55, 3, 0x16);
        showBack();
        fadeUp(0x55, 3, 0x16);
        break;
    case kCut: {
        flush();
        const std::size_t origin = static_cast<std::size_t>(originRows_) * Surface::kScreenWidth;
        const std::size_t count = std::min<std::size_t>(kSceneRows * Surface::kScreenWidth, screen_->size() - origin);
        std::copy_n(back_->data() + origin, count, screen_->data() + origin);
        break;
    }
    case kFadeInFromBlack:
        spare_ = shadow_;
        dirty_.reset();
        for (std::size_t i = 0; i < 255; ++i) {
            shadow_[i] = Colour{};
        }
        sendColours(0, 255);
        showBack();
        fadeUp(0x20, 1, 0x40);
        break;
    case kFadeToBlack:
        spare_ = shadow_;
        fadeDown(0x40, 2, 0x20);
        showBack();
        shadow_ = spare_;
        flush();
        break;
    case kDissolve:
        dissolve(kSceneRows);
        break;
    case kDissolveScreen:
        dissolve(Surface::kScreenHeight);
        break;
    default:
        throw FormatError("display: unknown transition " + std::to_string(type));
    }
    showBack();
    flush();
}

} // namespace mentat
