// SPDX-License-Identifier: GPL-3.0-or-later
//
// A picture file: palette blocks followed by a list of sprites.
// See docs/formats/sprite.md.
#pragma once

#include "mentat/gfx/palette.hpp"
#include "mentat/gfx/sprite.hpp"

#include <cstdint>
#include <vector>

namespace mentat {

class SpriteSheet {
public:
    // Takes the unpacked bytes of a picture file. Throws FormatError.
    static SpriteSheet parse(std::vector<std::uint8_t> bytes);

    // The palette blocks at the start of the file, applied over `palette`.
    // Files that carry no palette leave it unchanged.
    void applyPalette(Palette& palette, std::bitset<256>* touched = nullptr) const;
    bool hasPalette() const { return paletteEnd_ > 2; }

    std::size_t size() const { return offsets_.size(); }
    SpriteView sprite(std::size_t index) const; // throws FormatError when out of range
    const std::vector<std::uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
    std::size_t paletteEnd_ = 0;
    std::vector<std::size_t> offsets_;
};

} // namespace mentat
