// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/gfx/scene.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/gfx/sprite.hpp"
#include "mentat/gfx/sprite_sheet.hpp"
#include "mentat/gfx/surface.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace mentat {

namespace {

constexpr std::size_t kScaleTable = 0x2774;
constexpr std::size_t kLineClip = 0x1470;
constexpr std::uint16_t kEnd = 0xFFFF;
constexpr int kWidth = Surface::kScreenWidth;
constexpr int kPixels = Surface::kScreenWidth * Surface::kScreenHeight;

// The original addresses the screen as one long row of bytes and starts
// each run of pixels at a row no lower than the last one; pixels that run
// past the right edge go on at the left of the next row, and the address
// wraps at 64 KiB, as a 16-bit offset into the screen's segment does.
// Pixels that land in the bytes after the picture are dropped.
class Pen {
public:
    Pen(const Canvas& to, int x, int y) : to_(to.surface) {
        if (y >= Surface::kScreenHeight || y < 0) {
            y = Surface::kScreenHeight - 1;
        }
        at_ = ((y + to.originRows) * kWidth + x) & 0xFFFF;
    }
    void put(std::uint8_t colour) {
        if (at_ < kPixels) {
            to_.data()[at_] = colour;
        }
    }
    void move(int by) { at_ = (at_ + by) & 0xFFFF; }

private:
    Surface& to_;
    int at_ = 0;
};

std::int16_t s16(int v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

// The rows of a shaded shape: for each row, an x on each side.
class EdgeList {
public:
    explicit EdgeList(int side) : side_(side) {}

    // Walks the edge from (x, y) to (toX, toY), noting an x for each row it
    // goes down. See docs/rules/scenes.md for the exact steps.
    void trace(int x, int y, int toX, int toY, std::vector<std::uint16_t>& rows) {
        const int dy = s16(toY - y);
        const int dx = s16(toX - x);
        if (dy == 0) {
            // A flat edge notes one x: the left end when it goes left
            // without passing x = 0, otherwise its start.
            const unsigned sum = static_cast<std::uint16_t>(x) + static_cast<std::uint16_t>(dx);
            note(rows, sum > 0xFFFF ? static_cast<int>(sum & 0xFFFF) : x);
            return;
        }
        const int stepY = dy < 0 ? -1 : 1;
        const int lenY = std::abs(dy);
        if (dx == 0) {
            for (int i = 0; i <= lenY; ++i) {
                note(rows, x);
            }
            return;
        }
        const int stepX = dx < 0 ? -1 : 1;
        const int lenX = std::abs(dx);
        int straightX = stepX;
        int straightY = stepY;
        int major = 0;
        int minor = 0;
        if (lenX > lenY) {
            straightY = 0;
            major = lenX;
            minor = lenY;
        } else {
            straightX = 0;
            major = lenY;
            minor = lenX;
        }
        unsigned err = static_cast<unsigned>(major) >> 1;
        for (int i = 0; i < major; ++i) {
            err = (err + static_cast<unsigned>(minor)) & 0xFFFF;
            int sx = straightX;
            int sy = straightY;
            if (err >= static_cast<unsigned>(major)) {
                err -= static_cast<unsigned>(major);
                sx = stepX;
                sy = stepY;
            }
            if (sy == 1) {
                note(rows, x);
            }
            x = s16(x + sx);
        }
    }

private:
    void note(std::vector<std::uint16_t>& rows, int x) {
        const std::size_t at = 2 * count_ + static_cast<std::size_t>(side_);
        if (at >= rows.size()) {
            throw FormatError("scene: shape too tall");
        }
        rows[at] = static_cast<std::uint16_t>(x);
        ++count_;
    }

    int side_;
    std::size_t count_ = 0;
};

struct Point {
    int x = 0;
    int y = 0;
};

constexpr std::uint16_t kLastOfFirstSide = 0x4000;
constexpr std::uint16_t kLastOfSecondSide = 0x8000;

void drawShape(const Canvas& to, Reader& r, std::uint16_t command, std::vector<std::uint16_t>& rows) {
    const std::uint8_t base = static_cast<std::uint8_t>(command & 0xFF);
    const std::uint8_t flags = static_cast<std::uint8_t>(command >> 8);
    const int gradX = static_cast<std::int8_t>(r.u8()) * 16;
    const int gradY = static_cast<std::int8_t>(r.u8()) * 16;
    Point start;
    start.x = r.u16();
    start.y = r.u16();

    EdgeList first(0);
    EdgeList second(1);
    Point at = start;
    std::uint16_t word = 0;
    do {
        word = r.u16();
        const Point next{word & 0x3FFF, r.u16()};
        first.trace(at.x, at.y, next.x, next.y, rows);
        at = next;
    } while ((word & kLastOfFirstSide) == 0);
    const Point end = at;

    at = start;
    if ((word & kLastOfSecondSide) == 0) {
        do {
            word = r.u16();
            const Point next{word & 0x3FFF, r.u16()};
            second.trace(at.x, at.y, next.x, next.y, rows);
            at = next;
        } while ((word & kLastOfSecondSide) == 0);
    }
    second.trace(at.x, at.y, end.x, end.y, rows);

    const int height = s16(end.y - start.y);
    if (height <= 0) {
        return;
    }
    if (static_cast<std::size_t>(height) * 2 > rows.size()) {
        throw FormatError("scene: shape too tall");
    }
    const unsigned noisy = flags & 0x3Eu;
    unsigned noise = noisy != 0 ? 1u : 0u;
    const unsigned taps = (noisy << 8) | 2u;
    const bool leftward = (flags & 1) != 0;
    unsigned rowShade = static_cast<unsigned>(base) << 8;
    for (int row = 0; row < height; ++row) {
        const std::uint16_t a = rows[2 * static_cast<std::size_t>(row)];
        const std::uint16_t b = rows[2 * static_cast<std::size_t>(row) + 1];
        const int lo = std::min(a, b);
        const int hi = std::max(a, b);
        const int count = hi - lo + 1;
        Pen pen(to, leftward ? hi : lo, start.y + row);
        unsigned shade = rowShade;
        for (int i = 0; i < count; ++i) {
            const bool carry = (noise & 1) != 0;
            noise >>= 1;
            if (carry) {
                noise ^= taps;
            }
            pen.put(static_cast<std::uint8_t>((shade >> 8) + (noise & 3) - 1));
            pen.move(leftward ? -1 : 1);
            shade = (shade + static_cast<unsigned>(gradX)) & 0xFFFF;
        }
        rowShade = (rowShade + static_cast<unsigned>(gradY)) & 0xFFFF;
    }
}

// Draws a sprite at its own size the way the original's screen driver
// does: rows start at y × 320 + x in the screen's bytes, so a sprite that
// runs off the right edge comes back at the left one row lower.
void drawSpriteLinear(const Canvas& to, const SpriteView& view, int x, int y) {
    const SpritePixels px = decodeSprite(view);
    const bool flipX = (view.header.widthAndFlags & SpriteHeader::kFlipX) != 0;
    const bool flipY = (view.header.widthAndFlags & SpriteHeader::kFlipY) != 0;
    const int startX = flipX ? x + px.drawnWidth - 1 : x;
    const int startY = flipY ? y + px.height - 1 : y;
    Pen pen(to, startX, startY);
    for (int row = 0; row < px.height; ++row) {
        Pen rowPen = pen;
        for (int col = 0; col < px.width; ++col) {
            const std::size_t i = static_cast<std::size_t>(row) * static_cast<std::size_t>(px.width) +
                                  static_cast<std::size_t>(col);
            if (px.opaque[i]) {
                rowPen.put(px.pixels[i]);
            }
            rowPen.move(flipX ? -1 : 1);
        }
        pen.move(flipY ? -kWidth : kWidth);
    }
}

bool inside(const ClipRect& c, int x, int y) { return x >= c.left && x < c.right && y >= c.top && y < c.bottom; }

} // namespace

SceneTables SceneTables::fromProgram(const ProgramFile& program) {
    SceneTables t;
    for (std::size_t i = 0; i < t.scaleDivisors.size(); ++i) {
        t.scaleDivisors[i] = program.dataU16(kScaleTable + 2 * i);
        if (t.scaleDivisors[i] < 0x100) {
            throw FormatError("program: scene scale enlarges");
        }
    }
    t.lineClip.left = s16(program.dataU16(kLineClip));
    t.lineClip.top = s16(program.dataU16(kLineClip + 2));
    t.lineClip.right = s16(program.dataU16(kLineClip + 4));
    t.lineClip.bottom = s16(program.dataU16(kLineClip + 6));
    return t;
}

SceneFile SceneFile::parse(std::vector<std::uint8_t> bytes) {
    SceneFile f;
    f.bytes_ = std::move(bytes);
    Reader r(f.bytes_);
    const std::size_t first = r.u16();
    if (first == 0 || first % 2 != 0 || first > f.bytes_.size()) {
        throw FormatError("scene file: bad scene list");
    }
    r.seek(0);
    for (std::size_t i = 0; i < first / 2; ++i) {
        const std::size_t offset = r.u16();
        if (offset < first || offset >= f.bytes_.size()) {
            throw FormatError("scene file: scene " + std::to_string(i) + " outside the file");
        }
        f.offsets_.push_back(offset);
    }
    return f;
}

std::span<const std::uint8_t> SceneFile::scene(std::size_t index) const {
    if (index >= offsets_.size()) {
        throw FormatError("scene file: no scene " + std::to_string(index));
    }
    return std::span<const std::uint8_t>(bytes_).subspan(offsets_[index]);
}

RoomNumber RoomNumber::decode(int room) {
    const int n = (room - 1) & 0xFF;
    RoomNumber out;
    out.scene = n & 0x0F;
    out.pictureFile = (n >> 4) != 0 ? 0x13 + (n >> 4) : 0;
    return out;
}

void drawScaledSprite(const Canvas& to, const SpriteSheet& sheet, std::size_t index, std::uint8_t mode, bool flipX,
                      bool flipY, std::uint16_t divisor, int x, int y) {
    SpriteView view = sheet.sprite(index);
    const int width = view.header.width();
    // The original treats every shrunk sprite as four bits per pixel.
    view.header.mode = 0;
    const SpritePixels nibbles = decodeSprite(view);

    // Sizes are worked out in 8.8 fixed point with the bytes of the width
    // swapped, as the original does; widths above 255 come out wrong there
    // too, but no scene has one.
    const unsigned swapped = ((static_cast<unsigned>(width) & 0xFF) << 8) | (static_cast<unsigned>(width) >> 8);
    const int drawnW = static_cast<int>((swapped / divisor) & 0x3FF);
    const int drawnH = static_cast<int>(((static_cast<unsigned>(view.header.height) << 8) / divisor) & 0xFF);
    if (flipY) {
        y += drawnH - 1;
    }
    if (flipX) {
        x += drawnW - 1;
    }
    const Pen first(to, x, y);
    unsigned srcY = 0;
    for (int row = 0; row < drawnH; ++row) {
        const int sy = static_cast<int>((srcY >> 8) & 0xFF);
        Pen pen = first;
        pen.move((flipY ? -kWidth : kWidth) * row);
        unsigned srcX = 0;
        for (int col = 0; col < drawnW; ++col) {
            const int sx = static_cast<int>((srcX >> 8) & 0xFF);
            if (sy < nibbles.height && sx < nibbles.width) {
                const std::uint8_t v = nibbles.pixels[static_cast<std::size_t>(sy) * static_cast<std::size_t>(nibbles.width) +
                                                      static_cast<std::size_t>(sx)];
                if (v != 0) {
                    pen.put(static_cast<std::uint8_t>(v + mode));
                }
            }
            pen.move(flipX ? -1 : 1);
            srcX = (srcX + divisor) & 0xFFFF;
        }
        srcY = (srcY + divisor) & 0xFFFF;
    }
}

void drawSceneLine(const Canvas& canvas, int x0, int y0, int x1, int y1, std::uint8_t colour, const ClipRect& clip) {
    Surface& to = canvas.surface;
    const int origin = canvas.originRows;
    const int dy = s16(y1 - y0);
    const int dx = s16(x1 - x0);
    if (dy == 0) {
        if (y0 < clip.top || y0 >= clip.bottom) {
            return;
        }
        const int from = std::min(x0, x0 + dx);
        for (int x = from; x <= from + std::abs(dx); ++x) {
            if (x >= clip.left && x < clip.right) {
                to.set(x, y0 + origin, colour);
            }
        }
        return;
    }
    if (dx == 0) {
        if (x0 < clip.left || x0 >= clip.right) {
            return;
        }
        int count = std::abs(dy);
        int from = dy < 0 ? y0 - count : y0;
        if (from >= Surface::kScreenHeight) {
            return;
        }
        if (from < 0) {
            count += from;
            from = 0;
        }
        for (int y = from; y <= from + count; ++y) {
            if (y >= clip.top && y < clip.bottom) {
                to.set(x0, y + origin, colour);
            }
        }
        return;
    }
    // The first point is left out; every step after it is drawn.
    const int stepX = dx < 0 ? -1 : 1;
    const int stepY = dy < 0 ? -1 : 1;
    const int lenX = std::abs(dx);
    const int lenY = std::abs(dy);
    const bool xMajor = lenX > lenY;
    const int major = xMajor ? lenX : lenY;
    const int minor = xMajor ? lenY : lenX;
    int err = major >> 1;
    int x = x0;
    int y = y0;
    for (int i = 0; i < major; ++i) {
        err += minor;
        if (err >= major) {
            err -= major;
            x += stepX;
            y += stepY;
        } else if (xMajor) {
            x += stepX;
        } else {
            y += stepY;
        }
        if (inside(clip, x, y)) {
            to.set(x, y + origin, colour);
        }
    }
}

int drawScene(Surface& to, std::span<const std::uint8_t> scene, const SpriteSheet& sheet, const SceneTables& tables,
              const PersonSlotFn& onPerson) {
    return drawScene(Canvas(to), scene, sheet, tables, onPerson);
}

void drawSpriteWord(const Canvas& to, const SpriteSheet& sheet, std::uint16_t word, int x, int y, const SceneTables& tables,
                    std::uint8_t modeOverride) {
    const std::size_t index = word & 0x01FF;
    const bool flipX = (word & SpriteHeader::kFlipX) != 0;
    const bool flipY = (word & SpriteHeader::kFlipY) != 0;
    const int scale = (word >> 10) & 7;
    if (scale != 0) {
        const std::uint8_t mode = modeOverride != 0 ? modeOverride : sheet.sprite(index).header.mode;
        drawScaledSprite(to, sheet, index, mode, flipX, flipY, tables.scaleDivisors[static_cast<std::size_t>(scale)], x, y);
        return;
    }
    SpriteView view = sheet.sprite(index);
    view.header.widthAndFlags |= static_cast<std::uint16_t>(word & (SpriteHeader::kFlipX | SpriteHeader::kFlipY));
    if (modeOverride != 0) {
        view.header.mode = modeOverride;
    }
    drawSpriteLinear(to, view, x, y);
}

int drawScene(const Canvas& to, std::span<const std::uint8_t> scene, const SpriteSheet& sheet, const SceneTables& tables,
              const PersonSlotFn& onPerson) {
    if (to.surface.width() != Surface::kScreenWidth || to.surface.height() != Surface::kScreenHeight) {
        throw FormatError("scene: drawn on a surface that is not the screen's size");
    }
    Reader r(scene);
    const int persons = r.u8();
    int slot = 0;
    std::vector<std::uint16_t> rows(2 * 1024, 0);
    for (;;) {
        const std::uint16_t command = r.u16();
        if (command == kEnd) {
            return persons;
        }
        if ((command & 0x8000) == 0) {
            const int x = r.u8() | ((command & 0x0200) != 0 ? 0x100 : 0);
            const int y = r.u8();
            const std::uint8_t modeOverride = r.u8();
            if ((command & 0x01FF) == 1) {
                if (onPerson) {
                    onPerson(slot, x, y);
                }
                ++slot;
                continue;
            }
            drawSpriteWord(to, sheet, static_cast<std::uint16_t>((command & 0xFDFF) - 1), x, y, tables, modeOverride);
            continue;
        }
        if ((command >> 8) == 0xC0) {
            const int x0 = s16(r.u16());
            const int y0 = s16(r.u16());
            const int x1 = s16(r.u16());
            const int y1 = s16(r.u16());
            drawSceneLine(to, x0, y0, x1, y1, static_cast<std::uint8_t>(command & 0xFF), tables.lineClip);
            continue;
        }
        drawShape(to, r, command, rows);
    }
}

} // namespace mentat
