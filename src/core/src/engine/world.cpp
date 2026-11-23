// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/world.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/flat_map.hpp"

#include <array>
#include <cstdlib>

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
    loadDayTables(program);
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

void World::restore(std::span<const std::uint8_t> bytes) {
    if (bytes.size() != kSize) {
        throw FormatError("state: wrong size");
    }
    bytes_.assign(bytes.begin(), bytes.end());
}

int World::day() const {
    // A day begins three parts before the clock's count of 16 comes round.
    return ((clock() + 3) >> 4) % kDaysInYear + 1;
}

void World::goTo(int room, int place) {
    // Those who follow Paul come along; the state keeps the room left.
    takeFollowers(room, place);
    setU8(kLastRoom, u8(kRoomNumber));
    setU8(kRoomNumber, static_cast<std::uint8_t>(room));
    setU16(kRoom, static_cast<std::uint16_t>(room));
    setU16(kPlace, static_cast<std::uint16_t>(place));
    setU8(kPlaceKind, static_cast<std::uint8_t>(room >> 8));
    // Where the place's record is (counted from place 0).
    setU16(kPlaceRecord, static_cast<std::uint16_t>(kPlaces + kPlaceSize * static_cast<std::size_t>(place >> 8) - kPlaceSize));
}

void World::takeFollowers(int room, int place) {
    constexpr std::uint8_t kFollows = 0x40;
    for (int i = 0; i < kCharacters; ++i) {
        const std::size_t at = kCharacterTable + kCharacterSize * static_cast<std::size_t>(i);
        if (u16(at) == u16(kRoom) && u16(at + 2) == u16(kPlace) && (u8(at + 0x0F) & kFollows)) {
            setU16(at, static_cast<std::uint16_t>(room));
            setU16(at + 2, static_cast<std::uint16_t>(place));
        }
    }
}

void World::flyOver(std::uint16_t longitude, int latitude) {
    takeFollowers(longitude, static_cast<std::uint16_t>(latitude));
    setU16(kRoom, longitude);
    setU16(kPlace, static_cast<std::uint16_t>(latitude));
    setU8(kPlaceKind, kFlying);
    setPlaceHere(0);
}

int World::kindWord(int kind) {
    if (kind < 0x20) {
        return 0; // a sietch
    }
    if (kind < 0x21 || kind >= 0x30) {
        return 1; // a palace
    }
    return kind < 0x28 ? 2 : 3; // a village, a fortress
}

void World::findPlace(std::size_t record) {
    constexpr std::size_t kSietchesFound = 0x27;
    constexpr std::uint8_t kHidden = 0x80;
    constexpr int kFirstNotSietch = 0x20;
    if (!(u8(record + 0x0A) & kHidden)) {
        return;
    }
    setU8(record + 0x0A, static_cast<std::uint8_t>(u8(record + 0x0A) & ~kHidden));
    setU8(record + 0x0B, 0);
    if (u8(record + 8) < kFirstNotSietch) {
        setU8(kSietchesFound, static_cast<std::uint8_t>(u8(kSietchesFound) + 1));
        // Finding one particular sietch moves the story on.
        constexpr std::uint16_t kStorySietch = 0x0603;
        constexpr int kStepFound = 0x10;
        if (u16(record) == kStorySietch) {
            raiseStory(kStepFound);
        }
    }
}

void World::enterPlace(std::size_t record) {
    constexpr std::size_t kPlaceNumber = 0x09;
    constexpr std::uint8_t kVisited = 0x10;
    constexpr int kFirstNotSietch = 0x20;
    const int n = static_cast<int>((record - kPlaces) / kPlaceSize) + 1;
    const int kind = u8(record + 8);
    findPlace(record);
    if (kind >= kFirstNotSietch) {
        setU8(record + 0x0A, static_cast<std::uint8_t>(u8(record + 0x0A) | kVisited));
    }
    const int room = (kind << 8) | 1;
    const int place = (n << 8) | 0x80;
    goTo(room, place);
    desertSteps_ = 0;
    setU16(kHome, static_cast<std::uint16_t>(record));
    setU8(kPlaceNumber, static_cast<std::uint8_t>(n));
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

void World::setUpPlaces(FlatMap& map, std::span<const std::uint8_t> regions) {
    // How many points of the map each region has, counted from 7.
    std::array<int, 256> size{};
    size.fill(7);
    for (const std::uint8_t r : regions) {
        ++size[r];
    }
    std::vector<std::uint8_t>& points = map.bytes();
    for (std::size_t place = kPlaces; u8(place) != 0xFF; place += kPlaceSize) {
        const auto longitude = u16(place + 2);
        const int latitude = static_cast<std::int16_t>(u16(place + 4));
        const auto ring = static_cast<std::uint32_t>(2 * map.width(std::abs(latitude)));
        const auto index = static_cast<std::uint32_t>(map.pointAt(longitude, latitude));
        const auto at = static_cast<std::size_t>(map.rowStart(latitude)) + index;
        if (at >= points.size() || at >= regions.size()) {
            throw FormatError("world: a place falls outside the map");
        }
        setU16(place + 2, static_cast<std::uint16_t>((index << 16) / ring));
        setU16(place + 6, static_cast<std::uint16_t>(at));
        points[at] = static_cast<std::uint8_t>(points[at] | 0x40);
        const std::uint8_t region = regions[at];
        setU8(place + 0x10, region);
        setU8(place + 0x11, static_cast<std::uint8_t>(size[region] >> 4));
    }
    // The troops of each place: where they are, and the kind of place in
    // their own flags, with bit 7 for kinds 4 and 5 and those above 9.
    for (std::size_t place = kPlaces; u8(place) != 0xFF; place += kPlaceSize) {
        for (int troop = u8(place + 9); troop != 0;) {
            const std::size_t t = troopRecord(troop);
            setU16(t + 4, static_cast<std::uint16_t>(place));
            setU16(t + 6, u16(place + 2));
            setU16(t + 8, u16(place + 4));
            const int kind = u8(place) & 0x0F;
            std::uint8_t flags = static_cast<std::uint8_t>((u8(t + 0x12) & 0x70) | kind);
            if ((kind > 3) != (kind > 5) || kind > 9) {
                flags = static_cast<std::uint8_t>(flags | 0x80);
            }
            setU8(t + 0x12, flags);
            troop = u8(t + 1);
        }
    }
}

} // namespace mentat
