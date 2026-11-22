// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/data/phrase_file.hpp"

#include "mentat/data/reader.hpp"

namespace mentat {

namespace {

constexpr std::uint8_t kEnd = 0xFF;
constexpr std::uint8_t kPageBreak = 0xFE;
constexpr std::uint8_t kFirstPiece = 0xE0; // E0 to FD name a piece of the word list
constexpr std::uint16_t kPieceOffset = 0x3FFF;

} // namespace

PhraseFile PhraseFile::parse(std::span<const std::uint8_t> bytes) {
    Reader r(bytes);
    const std::uint16_t first = r.u16();
    if (first < 4 || first % 2 != 0 || first > bytes.size()) {
        throw FormatError("phrases: bad offset table");
    }
    PhraseFile f;
    f.bytes_.assign(bytes.begin(), bytes.end());
    r.seek(0);
    const std::size_t count = first / 2;
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint16_t offset = r.u16();
        if (offset < first || offset >= bytes.size()) {
            throw FormatError("phrases: offset out of range");
        }
        f.offsets_.push_back(offset);
    }
    // The last entry is the word list.
    f.words_ = f.offsets_.back();
    f.offsets_.pop_back();
    return f;
}

std::string PhraseFile::text(std::size_t index) const {
    if (index >= offsets_.size()) {
        throw FormatError("phrases: no such phrase");
    }
    Reader r(bytes_);
    r.seek(offsets_[index]);
    std::string out;
    for (;;) {
        const std::uint8_t b = r.u8();
        if (b == kEnd) {
            break;
        }
        if (b >= kFirstPiece && b != kPageBreak) {
            // A piece of the word list: the length is the low four bits and
            // two bits of the next word; the rest of that word is where.
            const std::uint16_t w = r.u16();
            const std::size_t length = (b & 0x0F) | ((w >> 10) & 0x30);
            const std::size_t at = words_ + (w & kPieceOffset);
            if (at + length > bytes_.size()) {
                throw FormatError("phrases: piece outside the file");
            }
            out.append(bytes_.begin() + static_cast<std::ptrdiff_t>(at),
                       bytes_.begin() + static_cast<std::ptrdiff_t>(at + length));
            // A space follows unless the phrase ends there.
            if (r.remaining() > 0 && bytes_[r.pos()] != kEnd) {
                out.push_back(' ');
            }
        } else {
            out.push_back(static_cast<char>(b));
        }
        if (out.size() >= kMaxText) {
            break;
        }
    }
    return out;
}

std::string PhraseFile::raw(std::size_t index) const {
    if (index >= offsets_.size()) {
        throw FormatError("phrases: no such phrase");
    }
    std::string out;
    for (std::size_t at = offsets_[index]; at < bytes_.size() && bytes_[at] != kEnd; ++at) {
        out.push_back(static_cast<char>(bytes_[at]));
    }
    return out;
}

} // namespace mentat
