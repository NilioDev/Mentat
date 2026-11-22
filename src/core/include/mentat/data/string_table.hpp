// SPDX-License-Identifier: GPL-3.0-or-later
//
// String tables: the original's spoken lines, menu words and place names.
// See docs/formats/text.md.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace mentat {

class StringTable {
public:
    // Byte that ends a string.
    static constexpr std::uint8_t kEnd = 0xFF;
    // Byte inside a string that starts a new page of text.
    static constexpr std::uint8_t kPageBreak = 0xFE;

    // Takes the unpacked file. Throws FormatError.
    static StringTable parse(std::span<const std::uint8_t> bytes);

    std::size_t size() const { return strings_.size(); }
    // The raw bytes of string `index`, without the end byte. Throws
    // FormatError when there is no such string.
    const std::string& at(std::size_t index) const;
    // The same, to be written into, as the original writes numbers into its
    // phrases.
    std::string& edit(std::size_t index);

private:
    std::vector<std::string> strings_;
};

} // namespace mentat
