// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/orders.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/talk.hpp"
#include "mentat/engine/world.hpp"

#include <array>
#include <vector>

namespace mentat {

namespace {

// The program's lists of commands: the orders, and the occupations to
// choose from by what the troop does now.
constexpr std::size_t kOrdersList = 0x210A;
constexpr std::size_t kFirstOccupations = 0x215A; // waiting for orders
constexpr std::size_t kSpiceOccupations = 0x216E;
constexpr std::size_t kArmyOccupations = 0x2182;
constexpr std::size_t kAttackOccupations = 0x219A;
constexpr std::size_t kEcologyOccupations = 0x21A6;
constexpr std::uint16_t kChangeOccupation = 0x4F;
constexpr std::uint16_t kSelectOccupation = 0x56;
constexpr std::uint16_t kEcologyPhrase = 0x77;
constexpr std::uint16_t kEspionagePhrase = 0x5F;

// The chief's box: its frame, and the frame of the face in it.
constexpr std::size_t kBoxColours = 0x18E9 + 8; // border, fill
constexpr std::size_t kFaceColours = 0x18F3 + 8;
constexpr int kBoxLeft = 5;
constexpr int kBoxRight = 232;
constexpr int kBoxHigh = 5;    // the box's top when the troop is low on the map
constexpr int kBoxLow = 0x50;  // and when it is high
constexpr int kBoxHeight = 0x43;
constexpr int kLowHalf = 0x4C; // a troop's mark from this row down is low
constexpr int kMarkHalf = 7;   // the mark's top is this far above its middle
constexpr std::size_t kMarkOffsets = 0x1672; // a troop's mark off its place, by its turn there
constexpr int kFaceFrame = 0x3D;
constexpr int kTextLeft = 0x49;
constexpr int kTextColour = 0xF0;
// Where each Fremen face is seen from in the box: the point of the
// portrait at the frame's corner, by face from 0x0E.
constexpr std::size_t kFocusTable = 0x22B9;
constexpr int kFirstFremenFace = 0x0E;
constexpr int kChief = 15;
constexpr int kInfoList = 2;   // the chief's list of news
constexpr int kAnswerList = 4; // his answers to an order

// State.
constexpr std::size_t kStoryFlags = 0x0A;
constexpr std::size_t kProgress = 0x2A;
constexpr std::size_t kSituation = 0x23;
constexpr std::uint8_t kAsked = 0x0A; // the situation while the chief answers an order
constexpr std::size_t kSpies = 0xE2;

void drawFramed(Surface& to, const ClipRect& r, std::uint8_t border, std::uint8_t fill) {
    to.fillRect(r.left, r.top, r.right - r.left, r.bottom - r.top, fill);
    for (int x = r.left; x < r.right; ++x) {
        to.set(x, r.top, border);
        to.set(x, r.bottom - 1, border);
    }
    for (int y = r.top; y < r.bottom; ++y) {
        to.set(r.left, y, border);
        to.set(r.right - 1, y, border);
    }
}

std::vector<Command> readCommands(const ProgramFile& program, std::size_t list) {
    std::vector<Command> commands;
    for (std::size_t at = list + 2;; at += 4) {
        const Command c{program.dataU16(at), program.dataU16(at + 2)};
        if (c.phrase == 0) {
            break;
        }
        commands.push_back(c);
    }
    return commands;
}

} // namespace

// The equipment kept at a place that none of its troops holds: the
// place's seven counts (bytes 0x14 on), less one for each troop with
// that bit of its byte 0x19 (bit 7 the first).
std::array<int, 7> Orders::freeEquipment(std::size_t place) const {
    std::array<int, 7> counts{};
    for (std::size_t i = 0; i < counts.size(); ++i) {
        counts[i] = world_.u8(place + 0x14 + i);
    }
    for (int troop = world_.u8(place + 9); troop != 0;) {
        const std::size_t t = World::troopRecord(troop);
        const int held = world_.u8(t + 0x19);
        for (std::size_t i = 0; i < counts.size(); ++i) {
            if ((held & (0x80 >> i)) && counts[i] > 0) {
                --counts[i];
            }
        }
        troop = world_.u8(t + 1);
    }
    return counts;
}

namespace {

std::uint16_t grey(std::uint16_t phrase, bool greyed) {
    return greyed ? static_cast<std::uint16_t>(phrase | Command::kGrey)
                  : static_cast<std::uint16_t>(phrase & ~Command::kGrey);
}

} // namespace

Orders::Orders(Game& game, World& world, Panel& panel, MapScreen& map, Talk& talk)
    : g_(game), world_(world), panel_(panel), map_(map), talk_(talk) {}

void Orders::open(int troop) {
    troop_ = troop;
    Display& d = g_.display();
    d.targetBack();
    map_.enter();
    setMenu();
    d.transition(Display::kDissolveScreen);
    drawBox();
    say(kInfoList);
}

void Orders::close() {
    talk_.endBox();
    troop_ = 0;
}

// The box goes where it leaves the troop's mark on the map in sight.
void Orders::drawBox() {
    const std::size_t t = World::troopRecord(troop_);
    const std::size_t place = world_.u16(t + 4);
    int x = 0;
    int y = 0;
    int top = kBoxHigh;
    if (map_.placeOnScreen(place, x, y)) {
        // The troop's mark stands off the place by its turn there.
        const ProgramFile& program = g_.resources().program();
        int turn = (world_.u8(t + 2) - 1) & 0x0F;
        if (world_.u8(place + 0x0A) & 2) {
            turn ^= 8;
        }
        const int down = static_cast<std::int8_t>(program.dataU8(kMarkOffsets + 2 * static_cast<std::size_t>(turn) + 1));
        if (y + down - kMarkHalf < kLowHalf) {
            top = kBoxLow;
        }
    }
    box_ = ClipRect{kBoxLeft, top, kBoxRight, top + kBoxHeight};
    const ProgramFile& program = g_.resources().program();
    const ClipRect frame{box_.left + 4, box_.top + 3, box_.left + 4 + kFaceFrame, box_.top + 3 + kFaceFrame};
    Display& d = g_.display();
    for (Surface* to : {&d.screen(), &d.back(), &d.saved()}) {
        drawFramed(*to, box_, program.dataU8(kBoxColours), program.dataU8(kBoxColours + 1));
        drawFramed(*to, frame, program.dataU8(kFaceColours), program.dataU8(kFaceColours + 1));
    }

    Talk::Box box;
    box.face = ClipRect{frame.left + 1, frame.top + 1, frame.right - 1, frame.bottom - 1};
    int variant = 0;
    const int face = world_.troopFace(troop_, variant);
    const std::size_t focus = kFocusTable + 4 * static_cast<std::size_t>(face - kFirstFremenFace);
    box.focusX = static_cast<std::int16_t>(program.dataU16(focus));
    box.focusY = static_cast<std::int16_t>(program.dataU16(focus + 2));
    box.text = ClipRect{box_.left + kTextLeft, box_.top + 4, box_.right - 6, box_.bottom - 3};
    box.fill = program.dataU8(kBoxColours + 1);
    box.colour = kTextColour;
    talk_.startBox(kChief, face, variant, box);
}

// The chief says the next line of a list, knowing his troop.
void Orders::say(int list) {
    world_.learnTroop(troop_);
    talk_.boxLine(list);
}

// The orders, greyed by what the troop can be asked now.
void Orders::setMenu() {
    std::vector<Command> commands = readCommands(g_.resources().program(), kOrdersList);
    if (commands.size() == 5) {
        const std::size_t t = World::troopRecord(troop_);
        const int occupation = world_.u8(t + 3) & 0x0F;
        Command& change = commands[1];
        Command& equipment = commands[2];
        Command& move = commands[3];
        change.phrase = grey(kChangeOccupation, true);
        equipment.phrase = grey(equipment.phrase, true);
        move.phrase = grey(move.phrase, true);
        if (!(world_.u16(t + 0x12) & 0x400)) {
            if (occupation != 1) {
                change.phrase = grey(change.phrase, false);
            }
            if (occupation == 2) {
                change.phrase = kSelectOccupation;
            } else {
                const int progress = world_.u8(kProgress);
                if (progress >= 5) {
                    move.phrase = grey(move.phrase, false);
                }
                const std::size_t place = world_.u16(t + 4);
                if (progress >= 4 && !(world_.u16(t + 0x10) & 0x200) &&
                    ((world_.u8(place + 0x0A) & 8) || world_.u8(place + 8) < 0x28)) {
                    int any = world_.u8(t + 0x19);
                    for (const int n : freeEquipment(place)) {
                        any |= n;
                    }
                    if (any != 0) {
                        equipment.phrase = grey(equipment.phrase, false);
                    }
                }
            }
        }
    }
    panel_.setCommands(std::move(commands));
    Display& d = g_.display();
    d.targetScreen();
    panel_.drawCommands();
    d.targetBack();
    panel_.drawCommands();
}

// The occupations to choose from, by what the troop does now.
void Orders::occupationMenu() {
    const std::size_t t = World::troopRecord(troop_);
    const int occupation = world_.u8(t + 3) & 0x0F;
    std::size_t list = kFirstOccupations;
    bool searchGreyed = false;
    bool hasSearch = false;
    if (occupation != 2) {
        switch (occupation >> 2) {
        case 0: list = kSpiceOccupations; break;
        case 1: list = occupation == 5 ? kAttackOccupations : kArmyOccupations; break;
        default: list = kEcologyOccupations; break;
        }
        hasSearch = list != kAttackOccupations;
        searchGreyed = world_.u8(kProgress) < 0x10;
    }
    std::vector<Command> commands = readCommands(g_.resources().program(), list);
    if (hasSearch && !commands.empty()) {
        commands[0].phrase = grey(commands[0].phrase, searchGreyed);
    }
    for (Command& c : commands) {
        const std::uint16_t phrase = c.phrase & Command::kPhrase;
        if (phrase == kEspionagePhrase) {
            c.phrase = grey(c.phrase, world_.u16(kSpies) < 0x1E);
        } else if (phrase == kEcologyPhrase) {
            c.phrase = grey(c.phrase, !(world_.u8(kStoryFlags) & 0x20));
        }
    }
    panel_.setCommands(std::move(commands));
    Display& d = g_.display();
    d.targetScreen();
    panel_.drawCommands();
    d.targetBack();
    panel_.drawCommands();
}

// The troop is asked to take up an occupation; its chief answers, and on
// a no it keeps the one it had.
void Orders::specialize(int occupation) {
    const std::size_t t = World::troopRecord(troop_);
    if ((world_.u8(t + 3) & 0x0F) == occupation) {
        setMenu();
        return;
    }
    const std::uint8_t before = world_.u8(t + 3);
    const std::uint16_t worked = world_.u16(t + 0x12);
    world_.setOccupation(troop_, occupation);
    talk_.expectYes();
    world_.learnTroop(troop_);
    world_.setU8(kSituation, kAsked);
    talk_.boxAnswer(kAnswerList);
    if (talk_.answer() == 0xFF) {
        if (((world_.u8(t + 3) & 0x0F) >> 2) != 0) {
            world_.setU8(t + 0x19, static_cast<std::uint8_t>(world_.u8(t + 0x19) & 0x7F));
        }
    } else {
        world_.setU16(t + 0x12, worked);
        world_.setOccupation(troop_, before);
    }
    setMenu();
}

bool Orders::command(std::uint16_t action) {
    if (!active()) {
        return false;
    }
    switch (action) {
    case kInformation: say(kInfoList); return true;
    case kOccupation: occupationMenu(); return true;
    case kSpice: specialize(0); return true;
    case kArmy: specialize(4); return true;
    case kEcology: specialize(8); return true;
    case kCancel: setMenu(); return true;
    case kNoMoreOrders:
    case kLeaveTroop:
        close();
        if (finished) {
            finished();
        }
        return true;
    case kEquipment:
    case kMove:
    case kNextTroop: return true; // not built yet
    default: return false;
    }
}

} // namespace mentat
