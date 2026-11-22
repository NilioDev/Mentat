// SPDX-License-Identifier: GPL-3.0-or-later
//
// The animation data of a character's portrait: the last entry of a
// character's picture file, which says where the face sits and which
// sprites make up each moment of its idle and speaking animations. See
// docs/formats/portrait.md.
#pragma once

#include "mentat/gfx/scene.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace mentat {

class SpriteSheet;

// One sprite of a frame: a sprite index of the picture file (from 0) and
// its place from the portrait's top-left corner.
struct PortraitPiece {
    int sprite = 0;
    int x = 0;
    int y = 0;

    bool operator==(const PortraitPiece&) const = default;
};

class PortraitAnimation {
public:
    // Reads the animation data from the last entry of `sheet`. Throws
    // FormatError on damaged data.
    static PortraitAnimation parse(const SpriteSheet& sheet);

    // The rectangle the portrait covers on the screen.
    ClipRect area() const { return area_; }

    // The number of animation sequences (the mouth shapes not counted).
    int sequenceCount() const { return static_cast<int>(sequences_.size()); }

    // Where sequence `n` starts, as a place to read frames from.
    std::size_t sequenceStart(int n) const; // throws FormatError

    // True when the sequence that was being read ends at `at`.
    bool endsAt(std::size_t at) const;

    // Reads the frame at `at` and moves `at` to the next one.
    std::vector<PortraitPiece> readFrame(std::size_t& at) const; // throws FormatError

    // Moves `at` past `count` frames.
    void skipFrames(std::size_t& at, int count) const; // throws FormatError

    // The pieces of mouth shape `n`.
    std::vector<PortraitPiece> mouth(int n) const; // throws FormatError

private:
    std::vector<PortraitPiece> readPart(int part) const;
    std::uint8_t byte(std::size_t at) const;
    std::uint16_t word(std::size_t at) const;

    std::vector<std::uint8_t> bytes_; // the entry's bytes after its 4-byte header
    ClipRect area_;
    std::size_t parts_ = 0;    // table of the parts' places
    std::size_t sequenceTable_ = 0;
    std::vector<std::size_t> sequences_;
    std::size_t mouths_ = 0;
};

} // namespace mentat
