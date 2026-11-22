// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/gfx/palette.hpp"

#include "mentat/data/reader.hpp"

namespace mentat {

void Palette::applyBlocks(Reader& reader, std::bitset<256>* touched) {
    for (;;) {
        const std::uint16_t word = reader.u16();
        if (word == 0xFFFF) {
            return;
        }
        if (word == 0x0100) {
            // A block for colour 0 alone is never applied: colour 0 stays.
            reader.skip(3);
            continue;
        }
        const unsigned first = word & 0xFFu;
        unsigned count = word >> 8;
        if (count == 0) {
            count = 256;
        }
        const auto rgb = reader.bytes(count * 3u);
        for (unsigned i = 0; i < count; ++i) {
            const unsigned slot = first + i;
            if (slot > 255) {
                throw FormatError("palette block runs past colour 255");
            }
            colours_[slot] = Colour{static_cast<std::uint8_t>(rgb[i * 3] & 0x3F),
                                    static_cast<std::uint8_t>(rgb[i * 3 + 1] & 0x3F),
                                    static_cast<std::uint8_t>(rgb[i * 3 + 2] & 0x3F)};
            if (touched != nullptr) {
                touched->set(slot);
            }
        }
    }
}

void Palette::applyBlocks(std::span<const std::uint8_t> blocks, std::bitset<256>* touched) {
    Reader reader(blocks);
    applyBlocks(reader, touched);
}

} // namespace mentat
