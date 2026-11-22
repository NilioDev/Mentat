// SPDX-License-Identifier: GPL-3.0-or-later
//
// Sprites: the pictures in the original's picture files and video frames.
// See docs/formats/sprite.md.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace mentat {

class Surface;

struct SpriteHeader {
    static constexpr std::uint16_t kPacked = 0x8000; // rows are run-length packed
    static constexpr std::uint16_t kFlipX = 0x4000;  // drawn mirrored left to right
    static constexpr std::uint16_t kFlipY = 0x2000;  // drawn upside down
    static constexpr std::uint16_t kWidthMask = 0x01FF;

    std::uint16_t widthAndFlags = 0;
    std::uint8_t height = 0;
    // Below 0xFE: four bits per pixel, this value is added to each pixel
    // (pixel 0 is see-through). 0xFE: one byte per pixel, solid. 0xFF: one
    // byte per pixel, colour 0 is see-through.
    std::uint8_t mode = 0;

    int width() const { return widthAndFlags & kWidthMask; }
    bool packed() const { return (widthAndFlags & kPacked) != 0; }
    bool fourBit() const { return mode < 0xFE; }
    bool solid() const { return mode == 0xFE; }
};

struct DrawFlags {
    bool flipX = false;
    bool flipY = false;
};

// A sprite's header and the bytes after it. The bytes may run on past the
// sprite's own data (the end of a packed sprite is only known by unpacking).
struct SpriteView {
    SpriteHeader header;
    std::span<const std::uint8_t> data;
};

// Reads the 4-byte header at the start of the bytes. Throws FormatError.
SpriteView readSprite(std::span<const std::uint8_t> bytes);

// Unpacks a sprite into its pixels, one byte per pixel, row by row; the
// row length is paddedWidth(). See-through pixels come out as -1 in mask
// form: the returned vector holds pixel values and `opaque` tells which
// were drawn. Throws FormatError on damaged data.
struct SpritePixels {
    int width = 0;  // pixels per stored row (4-bit sprites round up to 4)
    int drawnWidth = 0; // the width in the header; a mirrored sprite starts its rows here
    int height = 0;
    std::vector<std::uint8_t> pixels;
    std::vector<std::uint8_t> opaque;
};
SpritePixels decodeSprite(const SpriteView& sprite);

// Draws a sprite with its top-left corner at (x, y), clipped to the
// surface. The flags add to the flip bits in the sprite's own header.
void drawSprite(Surface& to, const SpriteView& sprite, int x, int y, DrawFlags flags = {});
void drawSprite(Surface& to, const SpritePixels& pixels, int x, int y, bool flipX, bool flipY);

} // namespace mentat
