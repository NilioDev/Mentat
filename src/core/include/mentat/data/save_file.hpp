// SPDX-License-Identifier: GPL-3.0-or-later
//
// The original's saved games, DUNE37S?.SAV: the clock, then the game's
// state packed with runs of equal bytes. See docs/formats/save.md.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace mentat {

class SaveFile {
public:
    // The parts of the contents, in order.
    static constexpr std::size_t kMapBytes = 0xC5FC;          // map bytes whose holders are kept
    static constexpr std::size_t kMapPart = kMapBytes / 4;    // two bits of each, four to a byte
    static constexpr std::size_t kCodeOffset = 0xAA;          // bytes of the program's code part
    static constexpr std::size_t kCodePart = 0xA2;
    static constexpr std::size_t kDialoguePart = 0x11F8;      // the dialogue lists
    static constexpr std::size_t kStatePart = 0x1261;         // the state
    static constexpr std::size_t kSize = kMapPart + kCodePart + kDialoguePart + kStatePart;
    static constexpr std::uint8_t kMarker = 0xF7;

    // The whole file for contents of kSize bytes. Throws FormatError for any
    // other size.
    static std::vector<std::uint8_t> encode(std::uint16_t clock, std::span<const std::uint8_t> contents);
    // The contents of a file, kSize bytes. Throws FormatError when the file
    // is cut short, holds too much or too little, or a run goes past it.
    static std::vector<std::uint8_t> decode(std::span<const std::uint8_t> file);
    // The clock at the start of the file. Throws FormatError when short.
    static std::uint16_t clock(std::span<const std::uint8_t> file);

    // The holders' bits (0x30) of the first kMapBytes map bytes, packed into
    // kMapPart bytes, and back. The map itself is a little shorter: the
    // original packs what lies after it, and Mentat packs zeros there and
    // leaves them out when unpacking. Throw FormatError when the packed part
    // is too short.
    static void packMap(std::span<const std::uint8_t> map, std::span<std::uint8_t> to);
    static void unpackMap(std::span<const std::uint8_t> from, std::span<std::uint8_t> map);
};

} // namespace mentat
