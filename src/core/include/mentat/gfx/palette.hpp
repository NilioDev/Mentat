// SPDX-License-Identifier: GPL-3.0-or-later
//
// The 256-colour VGA palette. Colours are kept as the original stores them:
// 6 bits per channel (0..63). See docs/formats/palette.md.
#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <span>

namespace mentat {

class Reader;

struct Colour {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    bool operator==(const Colour&) const = default;
};

class Palette {
public:
    Colour& operator[](std::size_t i) { return colours_[i & 0xFF]; }
    const Colour& operator[](std::size_t i) const { return colours_[i & 0xFF]; }

    // Reads palette blocks from the reader's position up to and including the
    // end marker, and applies them. Throws FormatError on damaged data.
    // `touched`, when given, gets a bit set for each colour a block wrote.
    void applyBlocks(Reader& reader, std::bitset<256>* touched = nullptr);
    void applyBlocks(std::span<const std::uint8_t> blocks, std::bitset<256>* touched = nullptr);

    // A colour scaled to 8 bits per channel, the way a VGA card shows it.
    static std::uint8_t to8Bit(std::uint8_t sixBit) {
        const unsigned v = sixBit & 0x3Fu;
        return static_cast<std::uint8_t>((v << 2) | (v >> 4));
    }

    bool operator==(const Palette&) const = default;

private:
    std::array<Colour, 256> colours_{};
};

} // namespace mentat
