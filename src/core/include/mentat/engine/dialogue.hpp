// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the characters can say and when: the lists of lines of DIALOGUE.HSQ,
// each with a condition from CONDIT.HSQ on the state of the game. See
// docs/formats/dialogue.md and docs/rules/talking.md.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace mentat {

class World;

// The conditions: small formulas over the game's state.
class Conditions {
public:
    static Conditions parse(std::span<const std::uint8_t> bytes); // throws FormatError

    std::size_t size() const { return count_; }
    // Condition `n` (from 1) worked out on `world`; true when not 0.
    // Throws FormatError for a damaged condition.
    std::uint16_t evaluate(int n, const World& world) const;
    bool test(int n, const World& world) const { return evaluate(n, world) != 0; }

private:
    std::vector<std::uint8_t> bytes_;
    std::size_t count_ = 0;
};

// One line a character can say: four bytes in a list.
struct DialogueLine {
    std::size_t at = 0; // where in the dialogue data
    std::uint8_t flags = 0;
    int condition = 0;
    int phrase = 0; // from 1
    std::uint8_t extra = 0; // the third byte, with the bits below

    static constexpr std::uint8_t kSaid = 0x80;
    static constexpr std::uint8_t kAgain = 0x40; // may be said again
    static constexpr std::uint8_t kOnceMore = 0x20;
    static constexpr std::uint8_t kRandomTake = 0x10; // one of a few recordings, picked at random
    static constexpr std::uint8_t kActionMask = 0x0F;
    static constexpr std::uint8_t kTextOnly = 0x10; // in `extra`: no recording
    static constexpr std::uint8_t kNoteMask = 0x0C;  // in `extra`

    bool said() const { return (flags & kSaid) != 0; }
    int action() const { return flags & kActionMask; }
};

// The lists of lines, and which lines have been said. The original keeps
// this in the saved game.
class Dialogue {
public:
    static constexpr int kListsPerCharacter = 8;
    // The lists of the characters from 6 on take their phrases from the
    // second phrase file.
    static constexpr int kFirstOfSecondFile = 6;

    static Dialogue parse(std::span<const std::uint8_t> bytes); // throws FormatError

    std::size_t lists() const { return lists_.size(); }
    // Where list `n` starts, or nothing when the table has no list there.
    std::optional<std::size_t> list(std::size_t n) const;

    DialogueLine line(std::size_t at) const; // throws FormatError
    bool atEnd(std::size_t at) const;
    void markSaid(std::size_t at);
    // The line's phrase is in the second phrase file.
    bool inSecondFile(std::size_t at) const;

    // The first line from `at` on that may be said: not said yet (or
    // allowed again) and its condition true. A line already said is passed
    // over when its flags share a bit with `skipSaid`.
    std::optional<std::size_t> find(std::size_t at, std::uint8_t skipSaid,
                                    const std::function<bool(int)>& condition) const;

    // The phrase of character `c`'s first line: the recordings number each
    // character's phrases from it. Throws FormatError when there is none.
    int firstPhrase(int c) const;

    const std::vector<std::uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
    std::vector<std::uint16_t> lists_;
};

} // namespace mentat
