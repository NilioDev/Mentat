// SPDX-License-Identifier: GPL-3.0-or-later
//
// The state of a game: the clock, where the player is, the places of
// Arrakis and the people in them. The original keeps it at the start of its
// data part, and a new game starts from the values the program holds there.
// See docs/formats/state.md.
#pragma once

#include <cstdint>
#include <functional>
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
    bool flying() const { return u8(kPlaceKind) == kFlying; }
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
    // The record of the place the player is in; 0 while flying.
    std::size_t placeHere() const { return u16(kPlaceRecord); }
    void setPlaceHere(std::size_t record) { setU16(kPlaceRecord, static_cast<std::uint16_t>(record)); }
    // The place the player last came into, where a flight can go back to.
    std::size_t home() const { return u16(kHome); }
    // Takes the characters who go with the player from where the player
    // is to (`room`, `place`).
    void takeFollowers(int room, int place);
    // Flying: where the player is is a point of the map.
    void flyOver(std::uint16_t longitude, int latitude);
    // Landing at place `record`: in its first room, the place found if it
    // was hidden.
    void enterPlace(std::size_t record);
    // A hidden place is found: shown on the maps from now on, and counted
    // when it is a sietch.
    void findPlace(std::size_t record);

    // --- The story (see docs/rules/story_steps.md)
    // The story moves on to `step` when that is past the step reached: the
    // story's lines run (storyEvents), then what the step itself changes.
    // Returns whether it moved on.
    bool raiseStory(int step);
    std::function<void()> storyEvents;
    // A way between rooms opened or closed: the arrows are to be set again.
    bool roomsChanged = false;
    // A scene the story starts (its script's place in the program's code),
    // waiting to be played; 0 when none. Scenes are not built yet.
    std::uint16_t scene = 0;
    // At step 0x14, while flying with nobody along, after a long quiet:
    // the story moves to 0x15 and the first message is left.
    void quietAtStep14();
    // The word for a kind of place (byte 8), counted from "a sietch": 0 a
    // sietch, 1 a palace, 2 a village, 3 a fortress.
    static int kindWord(int kind);

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

    // The troops of the place the player is in, as the people of the room:
    // characters 12 (the Harkonnens' captain), 13 (the smuggler), 14 (the
    // Fremen not yet working for Paul) and 15 (the chiefs of his troops)
    // are put in the room their troops stand in, and the state learns
    // what the place holds (see docs/rules/people.md). Troops by number
    // from 1, 0 for none.
    struct People {
        int harkonnens = 0;
        int fremen = 0;
        std::vector<int> chiefs;
        int prospector = 0; // which of the chiefs, from 1, is the prospectors'
    };
    People meetPeople();
    // What the state keeps of place `record` while the player is in it.
    void learnPlace(std::size_t record);
    // What the state keeps of troop `troop` while Paul talks to its
    // people, and of the place it is at.
    void learnTroop(int troop);
    // The Fremen of troop `troop` work for Paul. True when as many troops
    // have rallied to him as the story waits for. With the map, the land
    // around a place first rallied to is looked over.
    bool enlist(int troop, FlatMap* map = nullptr);
    // The land within `radius` points of place `record`'s point is marked
    // as looked over with `mark` (bits 4 and 5 of each map byte), but
    // where spice was found.
    void markAround(std::size_t record, int radius, std::uint8_t mark, FlatMap& map);

    // The face the people of troop `troop` are shown with (a portrait
    // number), and which of its sets in `variant`: by the troop's name.
    int troopFace(int troop, int& variant) const;
    // Troop `troop`'s occupation becomes `occupation` (its byte 3, from 0
    // spice mining; bits 2 and 3 the speciality), its work starting now.
    void setOccupation(int troop, int occupation);
    // Whether the troop cannot do its occupation where it is; its byte 3
    // keeps the answer.
    bool cannotWork(int troop);
    // The troop's work starts again from now.
    void restartWork(int troop);
    // Its occupation is set again even when it has it, its work starting
    // over.
    void resetOccupation(int troop, int occupation);
    // The words a line can say for place `record`: its name and its kind.
    void namePlace(std::size_t record);

    // --- Troops on the move (see docs/rules/moves.md)
    // Takes troop `troop` out of its place's chain, and puts it at the end
    // of the chain of `place`; returns its turn there.
    void unlinkTroop(int troop);
    int linkTroop(int troop, std::size_t place);
    // The troop sets off for `place`, and makes its first steps.
    void startMove(int troop, std::size_t place, FlatMap& map);
    // Up to `steps` steps towards its destination; true when it arrived.
    bool stepTroop(int troop, int steps, FlatMap& map);
    // Each hour (see docs/rules/work.md): the troops on the move step
    // on, but troop `except`, and the others do their work.
    void hourPassed(int except, FlatMap& map);
    void troopsWork(int except, FlatMap& map);
    // The map mode the original keeps (bit 6 while the map shows spice),
    // and a count of the times the map should be drawn again.
    int mapMode = 0;
    int mapRedraws = 0;
    // A troop whose orders are shown (a record, 0 for none): it does not
    // move on when it is done prospecting.
    std::size_t shownTroop = 0;
    // Set when the place Paul is at became a sietch, and when the
    // Harkonnens won the place he is at.
    bool sietchHere = false;
    bool lostWithPaul = false;
    // Set when the Harkonnens attack the place Paul is at, and when the
    // fight there is over.
    bool attackedHere = false;
    bool attackOver = false;
    // How a fight at a place leans, from 0 (the Harkonnens' way) to 255
    // (Paul's), 0x80 even.
    int battleBalance(std::size_t place) const;
    // Why the game ends (the original's codes, 0 while it goes on).
    int gameOverReason = 0;
    // The chiefs' messages waiting: a count, then (message, place) words.
    static constexpr std::size_t kMessageQueue = 0x1190;
    // Who goes with Paul (bit n for character n), the byte that says what
    // kind of rooms he is in, and its value outside (or flying).
    static constexpr std::size_t kAwayWord = 0x10;
    static constexpr std::size_t kPlaceKindByte = 0x08;
    static constexpr std::uint8_t kOutside = 0xFF;
    bool firstMessage(std::uint16_t& message, std::uint16_t& place) const;
    void dropMessage();
    // Turns the state's word 0 one bit left; true for the bit that came
    // round.
    bool turnSeed();
    // Told when a troop's mark on the map should change: what it does or
    // where it heads changed, or it moved on its way.
    std::function<void(int troop)> markChanged;
    std::function<void(int troop)> markMoved;
    // Told when a place changed in a way the map shows.
    std::function<void(std::size_t record)> placeChanged;
    // The game's first and third generators (see Game::random and
    // Game::random3), for what troops do; without them, 0.
    std::function<int(int mask)> random;
    std::function<int(int most)> random3;
    std::function<int()> random2;

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
    int harvest(std::size_t troop);
    void arrive(int troop, FlatMap& map);
    void settle(int troop);
    void capture(std::size_t place, FlatMap& map, bool told = true);
    void tell(std::uint16_t message, std::size_t place);
    void sendAway(int troop);
    int countHarkonnenForts();
    void raiseCharisma(int by);
    void raiseSpirits(int by);
    void troopsAt(std::size_t place, const std::function<void(int troop)>& each);
    int dice(int mask) { return random ? random(mask) : 0; }
    int keenness(std::size_t troop) const;
    void raiseSkill(std::size_t troop, int which, int by);
    void step(int troop, int except, FlatMap& map);
    bool joinOther(int troop);
    void forget(std::size_t troop);
    void newDay(int troop, std::size_t place, FlatMap& map);
    void becameSietch(std::size_t place);
    void mine(int troop, std::size_t place, FlatMap& map);
    void miningEvents(int troop, std::size_t place);
    void prospect(int troop, std::size_t place, FlatMap& map);
    void train(int troop, std::size_t place, FlatMap& map);
    void plant(int troop, std::size_t place, FlatMap& map);
    void spreadGreen(int troop, std::size_t place, FlatMap& map);
    void waitToPlant(int troop, std::size_t place);
    void buildWindTrap(int troop, std::size_t place);
    int strength(std::size_t troop) const;
    void weighForces(std::size_t place);
    void lowerCharisma(int by);
    void flee(int troop);
    int casualties(std::size_t troop, unsigned blow) const;
    void attack(int troop, std::size_t place);
    void fight(int troop, std::size_t place, FlatMap& map);
    void countFighters(std::size_t place, int& harkonnens, int& pauls);
    void strike(int troop, std::size_t place, FlatMap& map);
    void struck(int troop, std::size_t place, FlatMap& map);
    void sendHarkonnensAway(std::size_t place);
    void losePlace(std::size_t place, FlatMap& map);
    void loadDayTables(const ProgramFile& program);
    void dayPassed(FlatMap& map);
    void ageRecords();
    void callGathering();
    bool gathersAt(std::size_t place);
    void gatheringEnds();
    void takeStock(FlatMap& map);
    void keepResults();
    void smugglersChange();
    void waterAndFading(FlatMap& map);
    void remindOfTheHarkonnens();
    void harkonnensGrow();
    void sendNews(std::uint16_t news);
    void emperorsDemand();
    void newDemand();
    void storyStep(int step);
    void strongestFort();
    void harkonnensAttack(FlatMap& map);
    void paulsPlace();
    void lowerSpirits(std::size_t troop, int by);
    void heldEquipment(std::size_t troop, std::size_t place, int change);

    static constexpr std::size_t kClock = 0x02;
    static constexpr std::size_t kPlaceRecord = 0x114E;
    static constexpr std::size_t kHome = 0x1150;
    static constexpr std::uint8_t kFlying = 0xFF;
    static constexpr std::size_t kRoom = 0x04;
    static constexpr std::size_t kPlace = 0x06;
    static constexpr std::size_t kPlaceKind = 0x08;
    // The number of the room Paul is in, and of the one he came from.
    static constexpr std::size_t kRoomNumber = 0x0B;
    static constexpr std::size_t kLastRoom = 0x0D;
    static constexpr std::size_t kAway = 0x10;
    static constexpr std::size_t kPresent = 0x12;

    std::vector<std::uint8_t> bytes_;
    // What the original keeps outside the state for the hour's work.
    bool newDay_ = false;
    std::size_t harkonnenGoal_ = 0;
    std::uint8_t busy_ = 0;
    int spiceLeft_ = 0;
    // Where the green fades next (the original keeps it in its code).
    std::uint16_t fade_ = 1;
    std::vector<std::uint8_t> keep_;
    std::vector<std::uint8_t> warnings_;
};

} // namespace mentat
