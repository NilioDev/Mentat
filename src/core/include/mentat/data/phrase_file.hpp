// SPDX-License-Identifier: GPL-3.0-or-later
//
// The files of what the characters say (PHRASExy.HSQ): strings like the
// other text files, with often-used pieces of text kept once in a word list
// at the end. See docs/formats/text.md.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace mentat {

class PhraseFile {
public:
    // Takes the unpacked file. Throws FormatError.
    static PhraseFile parse(std::span<const std::uint8_t> bytes);

    // The number of phrases (the word list is not one).
    std::size_t size() const { return offsets_.size(); }

    // Phrase `index` (from 0) with the pieces of the word list put in, up
    // to its end byte. Throws FormatError when there is no such phrase or
    // it points outside the file.
    std::string text(std::size_t index) const;

    // Phrase `index` as the file keeps it, pieces not put in. The game
    // uses this for a phrase named inside another.
    std::string raw(std::size_t index) const;

    // The longest text the original makes of a phrase.
    static constexpr std::size_t kMaxText = 0x18F;

private:
    std::vector<std::uint8_t> bytes_;
    std::vector<std::uint16_t> offsets_;
    std::size_t words_ = 0; // where the word list starts
};

} // namespace mentat
