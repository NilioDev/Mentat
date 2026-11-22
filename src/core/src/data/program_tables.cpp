// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/data/program_tables.hpp"

#include "mentat/data/reader.hpp"

namespace mentat {

namespace {

constexpr std::size_t kVideoTable = 0x33A3;
constexpr std::size_t kResourceTable = 0x31FF;
constexpr std::size_t kMaxName = 16;

std::string cString(const ProgramFile& program, std::size_t offset) {
    std::string s;
    for (std::size_t i = 0; i < kMaxName; ++i) {
        const char c = static_cast<char>(program.dataU8(offset + i));
        if (c == 0) {
            return s;
        }
        s.push_back(c);
    }
    throw FormatError("program: name too long");
}

} // namespace

VideoInfo videoInfo(const ProgramFile& program, int index) {
    if (index < 0 || index >= video::kCount) {
        throw FormatError("program: no such video slot");
    }
    const std::size_t entry = program.dataU16(kVideoTable + 2 * static_cast<std::size_t>(index));
    VideoInfo info;
    info.flags = program.dataU8(entry);
    info.delay = program.dataU8(entry + 1);
    info.name = cString(program, entry + 2);
    info.spaced = index >= video::kFirstSpaced;
    return info;
}

std::string resourceName(const ProgramFile& program, int index) {
    if (index < 0 || index > 0xFF) {
        throw FormatError("program: no such resource");
    }
    const std::size_t entry = program.dataU16(kResourceTable + 2 * static_cast<std::size_t>(index));
    if (entry == 0 || entry + 2 >= ProgramFile::kDataSize) {
        return {};
    }
    // A size word comes before the name.
    std::string name = cString(program, entry + 2);
    if (name.find('.') == std::string::npos) {
        return {}; // "libre": a free number
    }
    return name;
}

} // namespace mentat
