// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/gfx/sprite_sheet.hpp"

#include "mentat/data/reader.hpp"

#include <span>
#include <string>

namespace mentat {

SpriteSheet SpriteSheet::parse(std::vector<std::uint8_t> bytes) {
    SpriteSheet sheet;
    sheet.bytes_ = std::move(bytes);
    Reader r(sheet.bytes_);
    const std::size_t table = r.u16();
    if (table < 2 || table > sheet.bytes_.size()) {
        throw FormatError("picture file: sprite list outside the file");
    }
    sheet.paletteEnd_ = table;
    r.seek(table);
    const std::size_t first = r.u16();
    if (first % 2 != 0 || first == 0) {
        throw FormatError("picture file: odd sprite list size");
    }
    const std::size_t count = first / 2;
    r.seek(table);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t offset = table + r.u16();
        if (offset + 4 > sheet.bytes_.size()) {
            throw FormatError("picture file: sprite " + std::to_string(i) + " outside the file");
        }
        sheet.offsets_.push_back(offset);
    }
    return sheet;
}

void SpriteSheet::applyPalette(Palette& palette, std::bitset<256>* touched) const {
    if (!hasPalette()) {
        return;
    }
    Reader r(std::span<const std::uint8_t>(bytes_).subspan(2, paletteEnd_ - 2));
    palette.applyBlocks(r, touched);
}

SpriteView SpriteSheet::sprite(std::size_t index) const {
    if (index >= offsets_.size()) {
        throw FormatError("picture file: no sprite " + std::to_string(index));
    }
    return readSprite(std::span<const std::uint8_t>(bytes_).subspan(offsets_[index]));
}

} // namespace mentat
