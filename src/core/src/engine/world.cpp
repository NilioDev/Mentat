// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/world.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/reader.hpp"

namespace mentat {

namespace {
constexpr std::size_t kCharacterTable = 0xFD8;
constexpr std::size_t kCharacterSize = 0x10;
constexpr int kDaysInYear = 365;
} // namespace

World::World(const ProgramFile& program) : bytes_(kSize) {
    for (std::size_t i = 0; i < kSize; ++i) {
        bytes_[i] = program.dataU8(i);
    }
}

std::uint8_t World::u8(std::size_t offset) const {
    if (offset >= bytes_.size()) {
        throw FormatError("world: read past the state");
    }
    return bytes_[offset];
}

std::uint16_t World::u16(std::size_t offset) const {
    return static_cast<std::uint16_t>(u8(offset) | (u8(offset + 1) << 8));
}

void World::setU8(std::size_t offset, std::uint8_t value) {
    if (offset >= bytes_.size()) {
        throw FormatError("world: write past the state");
    }
    bytes_[offset] = value;
}

void World::setU16(std::size_t offset, std::uint16_t value) {
    setU8(offset, static_cast<std::uint8_t>(value & 0xFF));
    setU8(offset + 1, static_cast<std::uint8_t>(value >> 8));
}

int World::day() const {
    // A day begins three parts before the clock's count of 16 comes round.
    return ((clock() + 3) >> 4) % kDaysInYear + 1;
}

void World::goTo(int room, int place) {
    setU16(kRoom, static_cast<std::uint16_t>(room));
    setU16(kPlace, static_cast<std::uint16_t>(place));
    setU8(kPlaceKind, static_cast<std::uint8_t>(room >> 8));
}

World::Character World::character(int index) const {
    const std::size_t at = kCharacterTable + kCharacterSize * static_cast<std::size_t>(index);
    Character c;
    c.room = u16(at);
    c.place = u16(at + 2);
    c.talk = u16(at + 4);
    c.number = u8(at + 0x0E);
    c.flags = u8(at + 0x0F);
    return c;
}

std::vector<World::Character> World::findPresent() {
    std::vector<Character> here;
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < kCharacters; ++i) {
            const Character c = character(i);
            if (c.room == room() && c.place == place() && c.listedLast() == (pass == 1)) {
                here.push_back(c);
            }
        }
    }
    std::uint16_t mask = 0;
    for (const Character& c : here) {
        mask = static_cast<std::uint16_t>(mask | (1u << (c.number & 0x0F)));
    }
    setU16(kPresent, mask);
    return here;
}

} // namespace mentat
