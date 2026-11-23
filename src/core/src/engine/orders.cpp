// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/orders.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/gfx/sprite_sheet.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/spice_map.hpp"
#include "mentat/engine/talk.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/font.hpp"
#include "mentat/data/string_table.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
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
constexpr std::size_t kClockWord = 0x02;
// Leaving a troop with word 0x12 bit 11 moves the story from these steps
// on to the fourth after the first.
constexpr int kLeftFirst = 0x60;
constexpr std::uint8_t kSamePlace = 0x10; // the situation when the troop is shown where it is
constexpr std::uint8_t kSendTo = 0x0B;    // and when it is shown another place

// Showing a troop where to go: what the chief says, the commands (only
// "Cancel") and where the small map opens, by where the box is.
constexpr std::uint16_t kWhereTo = 0x54;
constexpr std::size_t kMoveList = 0x212E;
constexpr int kMoveMapX = 0x5C;
constexpr int kMoveMapUnderBox = 0x1E;
constexpr int kMoveMapOverBox = 0x0E;
// The troop that is shown three sietches to go to in turn.
constexpr int kThreeSietches = 3;

// Modifying the equipment: the troop's in a box in the chief's, the
// place's in a box under it.
constexpr std::size_t kDoneList = 0x2012;
constexpr std::size_t kHeldColours = 0x1936 + 8; // border, fill
constexpr std::size_t kFreeColours = 0x1940 + 8; // border, fill: the text's colour and its ground
constexpr std::size_t kEquipmentSprites = 0x192F;
constexpr std::uint16_t kNonePhrase = 0x69;
constexpr std::uint16_t kUnusedPhrase = 0x6F;
constexpr std::uint16_t kHeldColour = 0xF0;
constexpr std::uint8_t kMaybe = 0x80;            // the answer of a line that shows the equipment
constexpr std::uint8_t kEquipped = 0x0C;         // the situation while the chief answers
constexpr std::size_t kHeldBefore = 0x3D;        // state: what the troop held, then what it gained
constexpr std::size_t kLost = 0x3E;              // state: what it gave back
constexpr std::size_t kHarvesterHome = 0x3F;     // state: 0x40 when it took a harvester at Paul's home
constexpr int kEcologyWork = 8;
constexpr std::size_t kHome = 0x1150;         // state: Paul's home

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

Orders::Orders(Game& game, World& world, Panel& panel, MapScreen& map, Talk& talk, SpiceMap& spice)
    : g_(game), world_(world), panel_(panel), map_(map), talk_(talk), spice_(spice) {}

void Orders::open(int troop) {
    troop_ = troop;
    Display& d = g_.display();
    d.targetBack();
    map_.choose(troop);
    map_.enter();
    setMenu();
    d.transition(Display::kDissolveScreen);
    drawBox();
    say(kInfoList);
}

void Orders::close() {
    equipping_ = false;
    freeBox_ = ClipRect{};
    heldShown_ = false;
    if (choosing_) {
        spice_.close();
        choosing_ = false;
    }
    talk_.endBox();
    // The troop is left: some of its flags go, and it keeps the day.
    const std::size_t t = World::troopRecord(troop_);
    const int progress = world_.u8(kProgress);
    if ((world_.u16(t + 0x12) & 0x800) && progress >= kLeftFirst && progress < kLeftFirst + 4) {
        talk_.raiseProgress(kLeftFirst + 4);
    }
    world_.setU16(t + 0x10, static_cast<std::uint16_t>(world_.u16(t + 0x10) & 0x3F0));
    world_.setU16(t + 0x12, static_cast<std::uint16_t>(world_.u16(t + 0x12) & 0xE5FF));
    world_.setU8(t + 0x14, static_cast<std::uint8_t>(world_.u16(kClockWord) >> 4));
    troop_ = 0;
    map_.choose(0);
}

Talk::Box drawChiefBox(Game& game, const ClipRect& where, int face) {
    const ProgramFile& program = game.resources().program();
    const ClipRect frame{where.left + 4, where.top + 3, where.left + 4 + kFaceFrame, where.top + 3 + kFaceFrame};
    Display& d = game.display();
    for (Surface* to : {&d.screen(), &d.back(), &d.saved()}) {
        drawFramed(*to, where, program.dataU8(kBoxColours), program.dataU8(kBoxColours + 1));
        drawFramed(*to, frame, program.dataU8(kFaceColours), program.dataU8(kFaceColours + 1));
    }
    Talk::Box box;
    box.face = ClipRect{frame.left + 1, frame.top + 1, frame.right - 1, frame.bottom - 1};
    const std::size_t focus = kFocusTable + 4 * static_cast<std::size_t>(face - kFirstFremenFace);
    box.focusX = static_cast<std::int16_t>(program.dataU16(focus));
    box.focusY = static_cast<std::int16_t>(program.dataU16(focus + 2));
    box.text = ClipRect{where.left + kTextLeft, where.top + 4, where.right - 6, where.bottom - 3};
    box.fill = program.dataU8(kBoxColours + 1);
    box.colour = kTextColour;
    return box;
}

ClipRect chiefBoxHigh() { return ClipRect{kBoxLeft, kBoxHigh, kBoxRight, kBoxHigh + kBoxHeight}; }

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
    int variant = 0;
    const int face = world_.troopFace(troop_, variant);
    const Talk::Box box = drawChiefBox(g_, box_, face);
    talk_.startBox(kChief, face, variant, box);
}

// The chief says the next line of a list, knowing his troop.
void Orders::say(int list) {
    world_.learnTroop(troop_);
    talk_.boxLine(list);
    heldShown_ = false;
    if (talk_.answer() == kMaybe) {
        heldShown_ = true;
        const int bits = world_.u8(World::troopRecord(troop_) + 0x19);
        for (std::size_t i = 0; i < held_.size(); ++i) {
            held_[i] = (bits >> (7 - i)) & 1;
        }
        drawHeld();
    }
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

// --- Moving the troop

// "MOVE TROOP": the chief asks where to, and the small map opens over the
// map for the place to be pointed at.
void Orders::chooseDestination() {
    if (troop_ == kThreeSietches) {
        return; // shown three sietches in turn: not built yet
    }
    talk_.boxCommand(kWhereTo);
    panel_.setCommands(readCommands(g_.resources().program(), kMoveList));
    Display& d = g_.display();
    d.targetScreen();
    panel_.drawCommands();
    d.targetBack();
    panel_.drawCommands();
    spice_.occupations = false;
    spice_.openFor(troop_, kMoveMapX, box_.top == kBoxHigh ? kMoveMapUnderBox : kMoveMapOverBox);
    choosing_ = true;
}

// The small map goes, and the chief tells about his troop again.
void Orders::stopChoosing() {
    spice_.close();
    choosing_ = false;
    say(kInfoList);
}

// A place was pointed at: the chief answers, knowing where to; on a yes
// the troop sets off and the orders are over.
void Orders::confirmDestination(std::size_t place) {
    stopChoosing();
    const std::size_t t = World::troopRecord(troop_);
    const std::size_t at = world_.u16(t + 4);
    std::uint8_t situation = kSamePlace;
    if (place != at) {
        world_.setU16(t + 4, static_cast<std::uint16_t>(place));
        world_.learnTroop(troop_);
        world_.namePlace(place);
        world_.setU16(t + 4, static_cast<std::uint16_t>(at));
        situation = kSendTo;
    }
    world_.setU8(kSituation, situation);
    talk_.expectYes();
    talk_.boxAnswer(kAnswerList);
    if (talk_.answer() != 0xFF) {
        setMenu();
        return;
    }
    world_.startMove(troop_, place, map_.flatMap());
    close();
    if (sent) {
        sent();
    }
}

void Orders::clickSmallMap(int x, int y) {
    if (!choosing_) {
        return;
    }
    std::size_t place = 0;
    switch (spice_.click(x, y, place)) {
    case SpiceMap::Click::Outside:
    case SpiceMap::Click::Close:
        stopChoosing();
        setMenu();
        return;
    case SpiceMap::Click::Map:
        if (place != 0) {
            confirmDestination(place);
        }
        return;
    default: return;
    }
}

// --- Modifying the equipment

// "MODIFY EQUIPMENT": the chief tells his news until he shows what his
// troop holds; the troop's equipment and the place's spare are shown, and
// a click on a piece moves it from one to the other.
void Orders::openEquipment() {
    equipping_ = true;
    panel_.setCommands(readCommands(g_.resources().program(), kDoneList));
    Display& d = g_.display();
    d.targetScreen();
    panel_.drawCommands();
    d.targetBack();
    panel_.drawCommands();
    for (int i = 0; i < 4 && !heldShown_; ++i) {
        say(kInfoList);
    }
    // The two boxes, from the chief's.
    freeBox_.left = box_.left + 0x30;
    heldBox_.left = freeBox_.left + 0x4D;
    heldBox_.right = box_.right;
    freeBox_.right = box_.right;
    heldBox_.top = box_.top + 0x2B;
    heldBox_.bottom = box_.bottom;
    freeBox_.top = box_.bottom + 1;
    if (freeBox_.top >= 0x70) {
        freeBox_.top -= 0x6D;
    }
    freeBox_.bottom = freeBox_.top + 0x28;

    const std::size_t t = World::troopRecord(troop_);
    const std::uint8_t bits = world_.u8(t + 0x19);
    world_.setU8(kHeldBefore, bits);
    for (std::size_t i = 0; i < held_.size(); ++i) {
        held_[i] = (bits >> (7 - i)) & 1;
    }
    free_ = freeEquipment(world_.u16(t + 4));
    drawEquipment();
}

// A list of equipment: each piece's sprite in a column of as many as
// there are, the columns side by side; "none" when there is nothing.
void Orders::drawEquipmentList(const std::array<int, 7>& counts, int x, int y, int bottom, std::uint16_t colour,
                               Spans& spans) {
    spans = Spans{};
    Surface& to = g_.display().target();
    if (std::all_of(counts.begin(), counts.end(), [](int n) { return n == 0; })) {
        const Font large = Font::parse(g_.resources().load(resource::kFont), Font::Size::Large);
        const std::string& text = panel_.phrases().at(kNonePhrase - 1u);
        large.draw(to, x + 0x0C, y + 5, text, static_cast<std::uint8_t>(colour), static_cast<std::uint8_t>(colour >> 8));
        return;
    }
    const ProgramFile& program = g_.resources().program();
    const int previous = g_.sheetResource();
    g_.selectSheet(resource::kMapSprites);
    const SpriteSheet& sheet = g_.sheet();
    for (std::size_t i = 0; i < counts.size(); ++i) {
        int count = counts[i];
        if (count == 0) {
            continue;
        }
        const auto n = static_cast<std::uint16_t>(program.dataU8(kEquipmentSprites + i));
        const SpriteHeader h = sheet.sprite(n).header;
        const int room = bottom - y;
        int step = h.height + 2;
        if (room / count < step) {
            const int rest = room - step;
            step = rest / count;
            if (step < 2) {
                step = 2;
                count = rest / 2;
            }
        }
        spans[i][0] = x;
        for (int k = 0, at = y; k < count; ++k, at += step) {
            g_.drawSprite(n, x, at);
        }
        x += (h.widthAndFlags & 0x0FFF) + 1;
        spans[i][1] = x;
    }
    g_.selectSheet(previous);
}

// What the troop holds, in the chief's box.
void Orders::drawHeld() {
    Display& d = g_.display();
    d.targetScreen();
    drawEquipmentList(held_, box_.left + 0x80, box_.top + 0x2D, box_.bottom, kHeldColour, heldAt_);
    d.targetBack();
}

void Orders::drawEquipment() {
    const ProgramFile& program = g_.resources().program();
    Display& d = g_.display();
    d.targetScreen();
    Surface& to = d.target();
    drawFramed(to, heldBox_, program.dataU8(kHeldColours), program.dataU8(kHeldColours + 1));
    drawFramed(to, freeBox_, program.dataU8(kFreeColours), program.dataU8(kFreeColours + 1));
    drawEquipmentList(held_, box_.left + 0x80, box_.top + 0x2D, box_.bottom, kHeldColour, heldAt_);
    const std::uint16_t colours = program.dataU16(kFreeColours);
    drawEquipmentList(free_, freeBox_.left + 0x50, freeBox_.top + 4, freeBox_.bottom, colours, freeAt_);
    // The place's words, a line each.
    const Font large = Font::parse(g_.resources().load(resource::kFont), Font::Size::Large);
    const std::string& text = panel_.phrases().at(kUnusedPhrase - 1u);
    int x = freeBox_.left + 8;
    int y = freeBox_.top + 4 + (std::any_of(free_.begin(), free_.end(), [](int n) { return n != 0; }) ? 0 : 5);
    for (const char c : text) {
        if (c == '\r') {
            x = freeBox_.left + 8;
            y += 10;
            continue;
        }
        x = large.draw(to, x, y, std::string_view(&c, 1), static_cast<std::uint8_t>(colours),
                       static_cast<std::uint8_t>(colours >> 8));
    }
    d.targetBack();
}

// A click while equipping: on a piece the troop holds it goes back to the
// place; on one the place keeps, the troop takes it when it holds none of
// it; anywhere else but the chief's box the equipping is done.
void Orders::clickEquipment(int x, int y) {
    auto inside = [x, y](const ClipRect& r) { return x > r.left && x < r.right && y > r.top && y < r.bottom; };
    auto pieceAt = [x](const Spans& spans) {
        for (std::size_t i = 0; i < spans.size(); ++i) {
            if (spans[i][0] != 0 && x >= spans[i][0] && x < spans[i][1]) {
                return static_cast<int>(i);
            }
        }
        return -1;
    };
    if (inside(box_)) {
        if (inside(heldBox_)) {
            const int i = pieceAt(heldAt_);
            if (i >= 0) {
                --held_[static_cast<std::size_t>(i)];
                ++free_[static_cast<std::size_t>(i)];
                drawEquipment();
            }
        }
        return;
    }
    if (inside(freeBox_)) {
        const int i = pieceAt(freeAt_);
        if (i >= 0 && held_[static_cast<std::size_t>(i)] == 0) {
            ++held_[static_cast<std::size_t>(i)];
            --free_[static_cast<std::size_t>(i)];
            drawEquipment();
        }
        return;
    }
    closeEquipment();
}

// "DONE": the place's box goes, the troop keeps what it holds now, and
// the chief answers, knowing what it gained and gave back.
void Orders::closeEquipment() {
    equipping_ = false;
    Display& d = g_.display();
    for (const ClipRect& r : {freeBox_, heldBox_}) {
        d.screen().blit(d.back(), r.left, r.top, r.right - r.left, r.bottom - r.top, r.left, r.top);
    }
    freeBox_ = ClipRect{};
    const std::size_t t = World::troopRecord(troop_);
    std::uint8_t now = 0;
    for (std::size_t i = 0; i < held_.size(); ++i) {
        now = static_cast<std::uint8_t>(now | ((held_[i] & 1) << (7 - i)));
    }
    world_.setU8(t + 0x19, now);
    if (world_.markChanged) {
        world_.markChanged(troop_);
    }
    const std::uint8_t before = world_.u8(kHeldBefore);
    const auto changed = static_cast<std::uint8_t>(before ^ now);
    const auto gained = static_cast<std::uint8_t>(now & changed);
    world_.setU8(kHeldBefore, gained);
    world_.setU8(kLost, static_cast<std::uint8_t>(before & changed));
    world_.setU8(kHarvesterHome, (gained & 0x40) && world_.u16(t + 4) == world_.u16(kHome) ? 0x40 : 0);
    if ((world_.u8(t + 3) & 0x0F) == kEcologyWork) {
        world_.cannotWork(troop_);
    }
    world_.learnTroop(troop_);
    world_.setU8(kSituation, kEquipped);
    heldShown_ = false;
    if (!talk_.boxAnswer(kAnswerList)) {
        say(kInfoList);
        say(kInfoList);
    }
    setMenu();
}

void Orders::click(int x, int y) {
    if (equipping_) {
        clickEquipment(x, y);
    } else {
        clickSmallMap(x, y);
    }
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
    case kCancel:
        if (equipping_) {
            closeEquipment();
            return true;
        }
        if (choosing_) {
            stopChoosing();
        }
        setMenu();
        return true;
    case kMove: chooseDestination(); return true;
    case kNoMoreOrders:
    case kLeaveTroop:
        close();
        if (finished) {
            finished();
        }
        return true;
    case kEquipment: openEquipment(); return true;
    case kNextTroop: return true; // not built yet
    default: return false;
    }
}

} // namespace mentat
