// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/data/string_table.hpp"

#include "mentat/data/reader.hpp"

#include <algorithm>

namespace mentat {

StringTable StringTable::parse(std::span<const std::uint8_t> bytes) {
    Reader r(bytes);
    const std::uint16_t first = r.u16();
    if (first < 2 || first % 2 != 0 || first > bytes.size()) {
        throw FormatError("string table: bad offset table");
    }
    const std::size_t count = first / 2;
    StringTable table;
    table.strings_.reserve(count);
    r.seek(0);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint16_t offset = r.u16();
        if (offset < first || offset >= bytes.size()) {
            throw FormatError("string table: offset out of range");
        }
        const auto begin = bytes.begin() + offset;
        const auto end = std::find(begin, bytes.end(), kEnd);
        if (end == bytes.end()) {
            throw FormatError("string table: string without end");
        }
        table.strings_.emplace_back(begin, end);
    }
    return table;
}

const std::string& StringTable::at(std::size_t index) const {
    if (index >= strings_.size()) {
        throw FormatError("string table: no such string");
    }
    return strings_[index];
}

} // namespace mentat
