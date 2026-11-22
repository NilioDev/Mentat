// SPDX-License-Identifier: GPL-3.0-or-later
//
// The state of a game: the clock, where the player is, the places of
// Arrakis and the people in them. The original keeps it at the start of its
// data part, and a new game starts from the values the program holds there.
// See docs/formats/state.md.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace mentat {

class FlatMap;
class ProgramFile;

class World {
public:
    // How much of the data part is the game's state.
    static constexpr std::size_t kSize = 0x1261;

    // A new game, from the player's program.
    explicit World(const ProgramFile& program);
    // A state of zeros, for tests.
    World() : bytes_(kSize, 0) {}

    std::uint8_t u8(std::size_t offset) const; // throws FormatError past the end
    std::uint16_t u16(std::size_t offset) const;
    void setU8(std::size_t offset, std::uint8_t value);
    void setU16(std::size_t offset, std::uint16_t value);

    // The clock: 16 parts to a day.
    int clock() const { return u16(kClock); }
    void setClock(int value) { setU16(kClock, static_cast<std::uint16_t>(value)); }
    // The part of the day, 0 to 15, and the day from 1.
    int hour() const { return clock() & 0x0F; }
    int day() const;

    // Where the player is: the room (high byte the kind of rooms, low byte
    // the room from 1) and the place (high byte its number from 1, low
    // byte 0x80 inside).
    int room() const { return u16(kRoom); }
    int place() const { return u16(kPlace); }
    bool inside() const { return (place() & 0xFF) == 0x80; }
    // Where Paul is on the map: inside a place, the place's point;
    // otherwise the room word is the longitude and the place's low byte
    // the latitude.
    void position(std::uint16_t& longitude, int& latitude) const {
        if (inside()) {
            const std::size_t here = placeRecord(place() >> 8);
            longitude = u16(here + 2);
            latitude = static_cast<std::int16_t>(u16(here + 4));
        } else {
            longitude = static_cast<std::uint16_t>(room());
            latitude = static_cast<std::int8_t>(place() & 0xFF);
        }
    }
    void goTo(int room, int place);

    struct Character {
        int room = 0;
        int place = 0;
        int number = 0;
        std::uint16_t talk = 0; // the original's number for what talking to them does
        std::uint8_t flags = 0;
        // Listed after the others in the list of commands.
        bool listedLast() const { return (flags & 0x40) != 0; }
    };
    static constexpr int kCharacters = 16;
    Character character(int index) const;

    // The characters who stand in the player's room (bit n for character
    // n), and those to leave out again.
    std::uint16_t present() const { return u16(kPresent); }
    std::uint16_t away() const { return u16(kAway); }
    // Works out `present` from where each character is, and returns those
    // in the room in the order the list of commands names them.
    std::vector<Character> findPresent();


    // The places and their troops as the program sets them up when it
    // starts: each place moves onto the nearest point of the map, which is
    // marked as a place's; it learns the region of `regions` (MAP2.HSQ)
    // it stands in and how big that is; and its troops take its point.
    void setUpPlaces(FlatMap& map, std::span<const std::uint8_t> regions);

    // Records: places from 1, troops from 1.
    static constexpr std::size_t kPlaces = 0x100;
    static constexpr std::size_t kPlaceSize = 0x1C;
    static constexpr std::size_t kTroops = 0x8AA;
    static constexpr std::size_t kTroopSize = 0x1B;
    static std::size_t placeRecord(int n) { return kPlaces + kPlaceSize * static_cast<std::size_t>(n - 1); }
    static std::size_t troopRecord(int n) { return kTroops + kTroopSize * static_cast<std::size_t>(n - 1); }

    const std::vector<std::uint8_t>& bytes() const { return bytes_; }
    // Puts back a state of kSize bytes, as from a saved game. Throws
    // FormatError for any other size.
    void restore(std::span<const std::uint8_t> bytes);

private:
    static constexpr std::size_t kClock = 0x02;
    static constexpr std::size_t kPlaceRecord = 0x114E;
    static constexpr std::size_t kRoom = 0x04;
    static constexpr std::size_t kPlace = 0x06;
    static constexpr std::size_t kPlaceKind = 0x08;
    static constexpr std::size_t kAway = 0x10;
    static constexpr std::size_t kPresent = 0x12;

    std::vector<std::uint8_t> bytes_;
};

} // namespace mentat
