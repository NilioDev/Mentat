// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/data/program_file.hpp"

#include "mentat/data/reader.hpp"

#include <array>
#include <fstream>
#include <iterator>

namespace mentat {

std::uint32_t crc32(std::span<const std::uint8_t> bytes) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            t[i] = c;
        }
        return t;
    }();
    std::uint32_t c = 0xFFFFFFFFu;
    for (const auto b : bytes) {
        c = table[(c ^ b) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

ProgramFile ProgramFile::parse(std::vector<std::uint8_t> bytes) {
    if (bytes.size() != kKnownSize || crc32(bytes) != kKnownCrc) {
        throw FormatError("DNCDPRG.EXE is not the CD version 3.7");
    }
    Reader r(bytes);
    if (r.u16() != 0x5A4D) {
        throw FormatError("DNCDPRG.EXE: not a DOS program");
    }
    const std::uint16_t lastPage = r.u16();
    const std::uint16_t pages = r.u16();
    r.skip(2);
    const std::size_t headerSize = static_cast<std::size_t>(r.u16()) * 16;
    const std::size_t fileSize = lastPage != 0 ? (pages - 1u) * 512u + lastPage : pages * 512u;
    if (fileSize > bytes.size() || headerSize > fileSize || fileSize - headerSize != kDataStart + kDataSize) {
        throw FormatError("DNCDPRG.EXE: unexpected layout");
    }
    ProgramFile program;
    program.image_.assign(bytes.begin() + static_cast<std::ptrdiff_t>(headerSize),
                          bytes.begin() + static_cast<std::ptrdiff_t>(fileSize));
    return program;
}

ProgramFile ProgramFile::open(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw FormatError("cannot open " + path.string());
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parse(std::move(bytes));
}

std::span<const std::uint8_t> ProgramFile::code() const {
    return std::span<const std::uint8_t>(image_).first(kDataStart);
}

std::span<const std::uint8_t> ProgramFile::data() const {
    return std::span<const std::uint8_t>(image_).subspan(kDataStart, kDataSize);
}

std::uint8_t ProgramFile::codeU8(std::size_t offset) const {
    if (offset >= kDataStart) {
        throw FormatError("program: code offset out of range");
    }
    return image_[offset];
}

std::uint16_t ProgramFile::codeU16(std::size_t offset) const {
    return static_cast<std::uint16_t>(codeU8(offset) | (codeU8(offset + 1) << 8));
}

std::uint8_t ProgramFile::dataU8(std::size_t offset) const {
    if (offset >= kDataSize) {
        throw FormatError("program: data offset out of range");
    }
    return image_[kDataStart + offset];
}

std::uint16_t ProgramFile::dataU16(std::size_t offset) const {
    return static_cast<std::uint16_t>(dataU8(offset) | (dataU8(offset + 1) << 8));
}

} // namespace mentat
