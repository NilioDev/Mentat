// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/dialogue.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/engine/world.hpp"

namespace mentat {

namespace {

constexpr std::uint8_t kFormulaEnd = 0xFF;
constexpr std::uint8_t kJoin = 0x80;     // a byte with this bit joins two parts
constexpr std::uint8_t kByteValue = 0x80; // operand: a byte follows
constexpr std::uint8_t kReadByte = 1;     // operand: the state's byte at the next byte
constexpr std::size_t kMostParts = 12;
constexpr std::uint16_t kTrue = 0xFFFF;
constexpr std::uint16_t kNoList = 0xFFFF;
constexpr std::uint16_t kListEnd = 0xFFFF;

std::uint16_t operand(Reader& r, const World& world) {
    const std::uint8_t kind = r.u8();
    if (kind < 0x80) {
        const std::uint8_t at = r.u8();
        return kind == kReadByte ? world.u8(at) : world.u16(at);
    }
    if (kind == kByteValue) {
        return r.u8();
    }
    return r.u16();
}

std::uint16_t apply(int op, std::uint16_t a, std::uint16_t b) {
    const auto sa = static_cast<std::int16_t>(a);
    const auto sb = static_cast<std::int16_t>(b);
    switch (op & 0x1F) {
    case 0x00: return a == b ? kTrue : 0;
    case 0x02: return a < b ? kTrue : 0;
    case 0x04: return a > b ? kTrue : 0;
    case 0x06: return a != b ? kTrue : 0;
    case 0x08: return sa <= sb ? kTrue : 0;
    case 0x0A: return sa >= sb ? kTrue : 0;
    case 0x0C: return static_cast<std::uint16_t>(a + b);
    case 0x0E: return static_cast<std::uint16_t>(a - b);
    case 0x10: return a & b;
    case 0x12: return a | b;
    default: return 0;
    }
}

} // namespace

Conditions Conditions::parse(std::span<const std::uint8_t> bytes) {
    Reader r(bytes);
    const std::uint16_t first = r.u16();
    if (first < 2 || first % 2 != 0 || first > bytes.size()) {
        throw FormatError("conditions: bad offset table");
    }
    Conditions c;
    c.bytes_.assign(bytes.begin(), bytes.end());
    c.count_ = first / 2;
    return c;
}

std::uint16_t Conditions::evaluate(int n, const World& world) const {
    if (n < 1 || static_cast<std::size_t>(n) > count_) {
        throw FormatError("conditions: no such condition");
    }
    Reader r(bytes_);
    r.seek(2 * static_cast<std::size_t>(n - 1));
    r.seek(r.u16());
    // Parts are worked out left to right; a joining byte keeps the value
    // so far and starts a new part, and the parts are then joined in order.
    std::vector<std::pair<std::uint16_t, std::uint8_t>> parts;
    std::uint16_t value = operand(r, world);
    for (;;) {
        const std::uint8_t op = r.u8();
        if (op == kFormulaEnd) {
            break;
        }
        if (op & kJoin) {
            if (parts.size() >= kMostParts) {
                throw FormatError("conditions: too many parts");
            }
            parts.emplace_back(value, op);
            value = operand(r, world);
            continue;
        }
        value = apply(op, value, operand(r, world));
    }
    if (parts.empty()) {
        return value;
    }
    std::uint16_t result = parts[0].first;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const std::uint16_t next = i + 1 < parts.size() ? parts[i + 1].first : value;
        result = apply(parts[i].second, result, next);
    }
    return result;
}

Dialogue Dialogue::parse(std::span<const std::uint8_t> bytes) {
    Reader r(bytes);
    const std::uint16_t first = r.u16();
    if (first < 2 || first % 2 != 0 || first > bytes.size()) {
        throw FormatError("dialogue: bad offset table");
    }
    Dialogue d;
    d.bytes_.assign(bytes.begin(), bytes.end());
    r.seek(0);
    for (std::size_t i = 0; i < first / 2; ++i) {
        const std::uint16_t at = r.u16();
        if (at != kNoList && (at < first || static_cast<std::size_t>(at) + 2 > bytes.size())) {
            throw FormatError("dialogue: list outside the file");
        }
        d.lists_.push_back(at);
    }
    return d;
}

std::optional<std::size_t> Dialogue::list(std::size_t n) const {
    if (n >= lists_.size() || lists_[n] == kNoList) {
        return std::nullopt;
    }
    return lists_[n];
}

bool Dialogue::atEnd(std::size_t at) const {
    Reader r(bytes_);
    r.seek(at);
    return r.u16() == kListEnd;
}

DialogueLine Dialogue::line(std::size_t at) const {
    Reader r(bytes_);
    r.seek(at);
    DialogueLine l;
    l.at = at;
    l.flags = r.u8();
    const std::uint8_t b1 = r.u8();
    l.extra = r.u8();
    const std::uint8_t b3 = r.u8();
    l.condition = b1 | ((l.extra >> 6) << 8);
    l.phrase = ((l.extra << 8) | b3) & 0x3FF;
    return l;
}

void Dialogue::markSaid(std::size_t at) {
    if (at >= bytes_.size()) {
        throw FormatError("dialogue: no such line");
    }
    bytes_[at] |= DialogueLine::kSaid;
}

int Dialogue::firstPhrase(int c) const {
    // The first list that is not empty, from the character's first on.
    for (std::size_t n = static_cast<std::size_t>(c) * kListsPerCharacter; n < lists_.size(); ++n) {
        if (const auto at = list(n); at && !atEnd(*at)) {
            return line(*at).phrase;
        }
    }
    throw FormatError("dialogue: a character without lines");
}

bool Dialogue::inSecondFile(std::size_t at) const {
    const std::size_t split = kFirstOfSecondFile * kListsPerCharacter;
    return split < lists_.size() && at >= lists_[split];
}

std::optional<std::size_t> Dialogue::find(std::size_t at, std::uint8_t skipSaid,
                                          const std::function<bool(int)>& condition) const {
    for (; !atEnd(at); at += 4) {
        const DialogueLine l = line(at);
        if (l.said() && !(l.flags & DialogueLine::kAgain) && (l.flags & skipSaid)) {
            continue;
        }
        if (condition(l.condition)) {
            return at;
        }
    }
    return std::nullopt;
}

} // namespace mentat
