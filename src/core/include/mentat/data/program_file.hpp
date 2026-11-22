// SPDX-License-Identifier: GPL-3.0-or-later
//
// The original's program file, DNCDPRG.EXE. Mentat never runs it: it only
// reads the numbers and tables the game needs (rules, starting state, text
// positions) from the player's own copy, so that none of them have to be
// copied into Mentat. See docs/formats/program.md.
#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace mentat {

class ProgramFile {
public:
    // The only version Mentat knows: the CD version 3.7.
    static constexpr std::size_t kKnownSize = 78700;
    static constexpr std::uint32_t kKnownCrc = 0xB628E0FFu;
    // Where the data part starts in the loaded program, and how much of it
    // the file holds; the rest starts out as zeros.
    static constexpr std::size_t kDataStart = 0xF4B0;
    static constexpr std::size_t kDataSize = 0x3CBC;

    // Throws FormatError when the file is not the known version.
    static ProgramFile parse(std::vector<std::uint8_t> bytes);
    static ProgramFile open(const std::filesystem::path& path);

    // The loaded program: code from offset 0, data from kDataStart.
    std::span<const std::uint8_t> code() const;
    std::span<const std::uint8_t> data() const;

    // Bounds-checked reads; throw FormatError.
    std::uint8_t codeU8(std::size_t offset) const;
    std::uint16_t codeU16(std::size_t offset) const;
    std::uint8_t dataU8(std::size_t offset) const;
    std::uint16_t dataU16(std::size_t offset) const;

private:
    std::vector<std::uint8_t> image_;
};

std::uint32_t crc32(std::span<const std::uint8_t> bytes);

} // namespace mentat
