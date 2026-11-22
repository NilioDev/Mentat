// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/gfx/sprite.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/gfx/surface.hpp"

namespace mentat {

SpriteView readSprite(std::span<const std::uint8_t> bytes) {
    Reader r(bytes);
    SpriteView v;
    v.header.widthAndFlags = r.u16();
    v.header.height = r.u8();
    v.header.mode = r.u8();
    v.data = bytes.subspan(4);
    return v;
}

namespace {

// Bytes stored per row: two pixels per byte rounded up to whole words for
// 4-bit sprites, one per pixel otherwise.
int storedRowBytes(const SpriteHeader& h) {
    return h.fourBit() ? (h.width() + 3) / 4 * 2 : h.width();
}

} // namespace

SpritePixels decodeSprite(const SpriteView& sprite) {
    const SpriteHeader& h = sprite.header;
    const int rowBytes = storedRowBytes(h);
    SpritePixels out;
    out.width = h.fourBit() ? rowBytes * 2 : rowBytes;
    out.drawnWidth = h.width();
    out.height = h.height;
    const std::size_t total = static_cast<std::size_t>(out.width) * static_cast<std::size_t>(out.height);
    out.pixels.assign(total, 0);
    out.opaque.assign(total, 0);

    Reader r(sprite.data);
    std::vector<std::uint8_t> row(static_cast<std::size_t>(rowBytes));
    for (int y = 0; y < out.height; ++y) {
        if (h.packed()) {
            // Runs may not cross the end of a row.
            int filled = 0;
            while (filled < rowBytes) {
                const std::uint8_t n = r.u8();
                if (n & 0x80) {
                    const int count = 257 - n;
                    const std::uint8_t value = r.u8();
                    if (count > rowBytes - filled) {
                        throw FormatError("sprite run crosses the end of a row");
                    }
                    for (int i = 0; i < count; ++i) {
                        row[static_cast<std::size_t>(filled++)] = value;
                    }
                } else {
                    const int count = n + 1;
                    if (count > rowBytes - filled) {
                        throw FormatError("sprite run crosses the end of a row");
                    }
                    const auto bytes = r.bytes(static_cast<std::size_t>(count));
                    for (int i = 0; i < count; ++i) {
                        row[static_cast<std::size_t>(filled++)] = bytes[static_cast<std::size_t>(i)];
                    }
                }
            }
        } else {
            const auto bytes = r.bytes(static_cast<std::size_t>(rowBytes));
            row.assign(bytes.begin(), bytes.end());
        }
        const std::size_t base = static_cast<std::size_t>(y) * static_cast<std::size_t>(out.width);
        if (h.fourBit()) {
            for (int i = 0; i < rowBytes; ++i) {
                const std::uint8_t b = row[static_cast<std::size_t>(i)];
                const std::uint8_t lo = b & 0x0F;
                const std::uint8_t hi = b >> 4;
                const std::size_t p = base + static_cast<std::size_t>(i) * 2;
                if (lo != 0) {
                    out.pixels[p] = static_cast<std::uint8_t>(lo + h.mode);
                    out.opaque[p] = 1;
                }
                if (hi != 0) {
                    out.pixels[p + 1] = static_cast<std::uint8_t>(hi + h.mode);
                    out.opaque[p + 1] = 1;
                }
            }
        } else {
            for (int i = 0; i < rowBytes; ++i) {
                const std::uint8_t b = row[static_cast<std::size_t>(i)];
                out.pixels[base + static_cast<std::size_t>(i)] = b;
                out.opaque[base + static_cast<std::size_t>(i)] = (h.solid() || b != 0) ? 1 : 0;
            }
        }
    }
    return out;
}

void drawSprite(Surface& to, const SpritePixels& px, int x, int y, bool flipX, bool flipY) {
    for (int sy = 0; sy < px.height; ++sy) {
        const int dy = flipY ? y + px.height - 1 - sy : y + sy;
        for (int sx = 0; sx < px.width; ++sx) {
            const std::size_t i = static_cast<std::size_t>(sy) * static_cast<std::size_t>(px.width) +
                                  static_cast<std::size_t>(sx);
            if (!px.opaque[i]) {
                continue;
            }
            const int dx = flipX ? x + px.drawnWidth - 1 - sx : x + sx;
            to.set(dx, dy, px.pixels[i]);
        }
    }
}

void drawSprite(Surface& to, const SpriteView& sprite, int x, int y, DrawFlags flags) {
    const bool flipX = ((sprite.header.widthAndFlags & SpriteHeader::kFlipX) != 0) != flags.flipX;
    const bool flipY = ((sprite.header.widthAndFlags & SpriteHeader::kFlipY) != 0) != flags.flipY;
    drawSprite(to, decodeSprite(sprite), x, y, flipX, flipY);
}

} // namespace mentat
