// SPDX-License-Identifier: GPL-3.0-or-later
//
// HNM files: the original's animated scenes and videos. See docs/formats/hnm.md.
#pragma once

#include "mentat/gfx/palette.hpp"
#include "mentat/gfx/sprite.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace mentat {

class Surface;

// One frame, split into its parts. The spans point into the file's bytes.
struct HnmFrame {
    std::span<const std::uint8_t> sound;   // 'sd' chunk body, empty when none
    std::span<const std::uint8_t> palette; // 'pl' chunk body (palette blocks), empty when none
    std::span<const std::uint8_t> video;   // the picture part, may be empty
};

// A decoded picture part of a frame.
struct HnmPicture {
    enum class Kind {
        None,      // the frame has no picture
        FullFrame, // packed rows of the full width, drawn from the top-left corner
        Sprite,    // a sprite placed at (x, y)
    };
    Kind kind = Kind::None;
    int x = 0;
    int y = 0;
    // For FullFrame: width × height bytes. For Sprite: the sprite header
    // and data, ready for readSprite().
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bytes;
};

class HnmFile {
public:
    // Takes the whole file. Throws FormatError.
    static HnmFile parse(std::vector<std::uint8_t> bytes);

    std::span<const std::uint8_t> headerPalette() const { return headerPalette_; }
    std::size_t frameCount() const { return frames_.size(); }
    HnmFrame frame(std::size_t index) const;

    // Unpacks the picture part of a frame. Throws FormatError.
    static HnmPicture decodePicture(std::span<const std::uint8_t> video);

private:
    std::vector<std::uint8_t> bytes_;
    std::span<const std::uint8_t> headerPalette_;
    struct Range {
        std::size_t begin;
        std::size_t end;
    };
    std::vector<Range> frames_;
};

// Draws a decoded picture onto a screen-sized surface. `spaced` draws each
// pixel of a sprite picture in every second column and row, the way the
// original shows its full-motion videos. A sprite picture is moved down by
// `originRows`, like everything the original draws by position.
void drawHnmPicture(Surface& to, const HnmPicture& picture, bool spaced = false, int originRows = 0);

} // namespace mentat
