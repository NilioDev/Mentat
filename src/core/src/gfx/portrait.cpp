// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/gfx/portrait.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <string>

namespace mentat {

namespace {

constexpr std::uint8_t kSequenceEnd = 0xFF;
constexpr std::uint8_t kFrameEnd = 0;
constexpr std::uint8_t kHighPart = 1; // the next byte names a part from 256 up
constexpr int kFirstPart = 2;
constexpr int kMaxPieces = 200; // more than any frame needs; stops damaged data

} // namespace

PortraitAnimation PortraitAnimation::parse(const SpriteSheet& sheet) {
    if (sheet.size() < 2) {
        throw FormatError("portrait: no animation data");
    }
    const SpriteView last = sheet.sprite(sheet.size() - 1);
    PortraitAnimation a;
    a.bytes_.assign(last.data.begin(), last.data.end());
    Reader r(a.bytes_);
    a.area_.left = r.u16();
    a.area_.top = r.u16();
    a.area_.right = r.u16();
    a.area_.bottom = r.u16();
    if (a.area_.left > a.area_.right || a.area_.top > a.area_.bottom || a.area_.right > 320 || a.area_.bottom > 200) {
        throw FormatError("portrait: bad area");
    }
    const std::size_t partsSize = r.u16();
    a.parts_ = r.pos();
    a.sequenceTable_ = 8 + partsSize;
    const std::size_t tableSize = a.word(a.sequenceTable_);
    if (tableSize < 4 || tableSize % 2 != 0) {
        throw FormatError("portrait: bad sequence table");
    }
    const std::size_t count = tableSize / 2;
    for (std::size_t i = 0; i + 1 < count; ++i) {
        a.sequences_.push_back(a.sequenceTable_ + a.word(a.sequenceTable_ + 2 * i));
    }
    // The last entry of the table holds the mouth shapes.
    a.mouths_ = a.sequenceTable_ + a.word(a.sequenceTable_ + 2 * (count - 1));
    for (const std::size_t s : a.sequences_) {
        a.byte(s);
    }
    a.byte(a.mouths_);
    return a;
}

std::uint8_t PortraitAnimation::byte(std::size_t at) const {
    if (at >= bytes_.size()) {
        throw FormatError("portrait: data cut short");
    }
    return bytes_[at];
}

std::uint16_t PortraitAnimation::word(std::size_t at) const {
    return static_cast<std::uint16_t>(byte(at) | (byte(at + 1) << 8));
}

std::size_t PortraitAnimation::sequenceStart(int n) const {
    if (n < 0 || n >= sequenceCount()) {
        throw FormatError("portrait: no sequence " + std::to_string(n));
    }
    return sequences_[static_cast<std::size_t>(n)];
}

bool PortraitAnimation::endsAt(std::size_t at) const { return byte(at) == kSequenceEnd; }

std::vector<PortraitPiece> PortraitAnimation::readPart(int part) const {
    std::size_t at = parts_ + word(parts_ + 2 * static_cast<std::size_t>(part - kFirstPart));
    std::vector<PortraitPiece> pieces;
    for (std::uint8_t sprite = byte(at); sprite != 0; sprite = byte(at)) {
        pieces.push_back(PortraitPiece{sprite - 1, byte(at + 1), byte(at + 2)});
        at += 3;
        if (pieces.size() > kMaxPieces) {
            throw FormatError("portrait: part does not end");
        }
    }
    return pieces;
}

std::vector<PortraitPiece> PortraitAnimation::readFrame(std::size_t& at) const {
    std::vector<PortraitPiece> pieces;
    for (int part = byte(at++); part != kFrameEnd; part = byte(at++)) {
        if (part == kHighPart) {
            part = 0x100 + byte(at++);
        }
        if (part < kFirstPart) {
            throw FormatError("portrait: bad part");
        }
        const auto more = readPart(part);
        pieces.insert(pieces.end(), more.begin(), more.end());
        if (pieces.size() > kMaxPieces) {
            throw FormatError("portrait: frame too long");
        }
    }
    return pieces;
}

void PortraitAnimation::skipFrames(std::size_t& at, int count) const {
    for (int i = 0; i < count; ++i) {
        while (byte(at++) != kFrameEnd) {
        }
    }
}

std::vector<PortraitPiece> PortraitAnimation::mouth(int n) const {
    if (n < 0) {
        throw FormatError("portrait: bad mouth shape");
    }
    std::size_t at = mouths_ + 2 * static_cast<std::size_t>(n);
    return readFrame(at);
}

} // namespace mentat
