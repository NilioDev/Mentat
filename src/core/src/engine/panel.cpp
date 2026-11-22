// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/panel.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/intro.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <string>

namespace mentat {

namespace {

// Tables in the program's data.
constexpr std::size_t kZoneTable = 0x1AE4;   // a count, then 14 bytes per zone
constexpr std::size_t kZoneSize = 14;
constexpr std::size_t kArrowZones = 0x1C76;  // six zones for the arrows inside
constexpr std::size_t kSlotsTaken = 0x1152;  // two bytes: what each slot holds, 0xFF for nothing
constexpr std::size_t kSunPlaces = 0x1E7E;   // per part of the day, two (x, y) pairs
constexpr std::size_t kSunClip = 0x1EFE;     // the rectangle the sun and moon show in
constexpr std::size_t kArrowPad = 0x2458;    // the rectangle behind the arrows

constexpr int kPanelSheet = 0;
constexpr std::uint16_t kOrnamentFrame = 0x0F;
constexpr int kOrnamentFrameX = 0x7E;
constexpr int kOrnamentFrameY = 0x94;
constexpr std::uint16_t kOrnamentFirst = 0x10;
constexpr int kOrnamentX = 0x96;
constexpr int kOrnamentY = 0x89;
constexpr std::uint16_t kSun = 0x4A;
constexpr std::uint16_t kMoon = 0x4B;
constexpr std::uint16_t kEmptySlot = 0x40;
constexpr std::uint16_t kArrowsInside = 0x21;
constexpr std::uint16_t kArrowsFirstRoom = 0x22;
constexpr std::uint16_t kArrowsOutside = 0x23;
constexpr std::uint16_t kFirstArrow = 0x1D;
constexpr std::uint16_t kArrowPadColour = 0xF0;

// The clock's colours and place.
constexpr std::uint8_t kDayColour = 0xFA;
constexpr std::uint8_t kDayBackground = 0xF1;
constexpr int kDayX = 0x0B;
constexpr int kDayY = 0xBE;

// The command lines' colours and place.
constexpr std::uint8_t kCommandColour = 0xFA;
constexpr std::uint8_t kGreyColour = 0xF5;
constexpr std::uint8_t kLineBackground = 0xF3;
constexpr int kCommandX = 0x5D;
constexpr int kCommandRight = 0xE3;
constexpr int kCommandRows = 7;
constexpr std::uint16_t kOthers = 0xA0; // "Others..."
constexpr int kPalace = 1;

ClipRect rectAt(const ProgramFile& program, std::size_t at) {
    auto s = [&](std::size_t o) { return static_cast<int>(static_cast<std::int16_t>(program.dataU16(at + o))); };
    return ClipRect{s(0), s(2), s(4), s(6)};
}

Zone zoneAt(const ProgramFile& program, std::size_t at) {
    Zone z;
    const ClipRect r = rectAt(program, at);
    z.left = r.left;
    z.top = r.top;
    z.right = r.right;
    z.bottom = r.bottom;
    z.flags = program.dataU16(at + 8);
    z.sprite = program.dataU16(at + 10);
    z.action = program.dataU16(at + 12);
    return z;
}

} // namespace

Panel::Panel(Game& game, int language) : game_(game), language_(language) {
    const ProgramFile& program = game.resources().program();
    const int count = program.dataU16(kZoneTable);
    if (count < 24 || count > 64) {
        throw FormatError("panel: unexpected number of zones");
    }
    for (int i = 0; i < count; ++i) {
        zones_.push_back(zoneAt(program, kZoneTable + 2 + kZoneSize * static_cast<std::size_t>(i)));
    }
    font_ = Font::parse(game.resources().load(resource::kFont), Font::Size::Small);
}

const StringTable& Panel::phrases() {
    if (!phrases_) {
        phrases_ = StringTable::parse(game_.resources().load(resource::kFirstCommands + commandFile(language_)));
    }
    return *phrases_;
}

void Panel::drawZones(int first, int count) {
    const int previous = game_.sheetResource();
    game_.selectSheet(kPanelSheet);
    Display& d = game_.display();
    for (int i = first; i < first + count && i < static_cast<int>(zones_.size()); ++i) {
        const Zone& z = zones_[static_cast<std::size_t>(i)];
        if (z.flags & Zone::kFill) {
            d.target().fillRect(z.left, z.top, z.right - z.left, z.bottom - z.top, 0);
        }
        if ((z.flags & Zone::kHidden) || z.sprite == Zone::kNoSprite) {
            continue;
        }
        game_.drawSprite(z.sprite, z.left, z.top);
    }
    if (previous >= 0) {
        game_.selectSheet(previous);
    }
}

void Panel::drawAll() {
    game_.display().clear(game_.display().target());
    drawZones(0, static_cast<int>(zones_.size()));
    drawOrnament();
}

void Panel::setLayout(std::size_t table) {
    const ProgramFile& program = game_.resources().program();
    for (std::size_t i = 0; i < 4; ++i) {
        zones_[i].flags = program.dataU16(table + 4 * i);
        zones_[i].sprite = program.dataU16(table + 4 * i + 2);
    }
    drawZones(0, 3);
}

void Panel::drawOrnament() {
    const int previous = game_.sheetResource();
    game_.selectSheet(kPanelSheet);
    game_.drawSprite(kOrnamentFrame, kOrnamentFrameX, kOrnamentFrameY);
    game_.drawSprite(static_cast<std::uint16_t>(kOrnamentFirst + ornament), kOrnamentX, kOrnamentY);
    if (previous >= 0) {
        game_.selectSheet(previous);
    }
}

void Panel::drawClock(const World& world) {
    // Zone 1 holds the clock only while it shows the panel's first picture.
    if (zones_[1].sprite != 0) {
        return;
    }
    const ProgramFile& program = game_.resources().program();
    const int previous = game_.sheetResource();
    game_.selectSheet(kPanelSheet);
    const ClipRect clip = rectAt(program, kSunClip);
    const std::size_t places = kSunPlaces + 8 * static_cast<std::size_t>(world.hour());
    const std::uint16_t sprites[2] = {kSun, kMoon};
    for (std::size_t i = 0; i < 2; ++i) {
        const int x = static_cast<std::int16_t>(program.dataU16(places + 4 * i));
        const int y = static_cast<std::int16_t>(program.dataU16(places + 4 * i + 2));
        if (x == 0) {
            continue;
        }
        drawSpriteClipped(game_.canvas(), game_.sheet().sprite(sprites[i]), x, y, clip);
    }
    if (previous >= 0) {
        game_.selectSheet(previous);
    }
    // The day, in three places, the leading noughts as spaces, and a space.
    const int day = world.day();
    int x = kDayX;
    if (day < 100) {
        x -= 2;
    }
    if (day < 10) {
        x -= 2;
    }
    std::string text;
    text += day >= 100 ? static_cast<char>('0' + day / 100) : ' ';
    text += day >= 10 ? static_cast<char>('0' + day / 10 % 10) : ' ';
    text += static_cast<char>('0' + day % 10);
    text += ' ';
    font_.draw(game_.display().target(), x, kDayY + game_.display().originRows(), text, kDayColour, kDayBackground);
}

void Panel::drawSlots(const World& world) {
    zones_[kSlots].sprite = kEmptySlot;
    zones_[kSlots + 1].sprite = kEmptySlot;
    drawZones(kSlots, 2);
    for (int i = 0; i < 2; ++i) {
        zones_[static_cast<std::size_t>(kSlots + i)].sprite =
            static_cast<std::uint16_t>(kEmptySlot + 1 + static_cast<std::int8_t>(world.u8(kSlotsTaken + static_cast<std::size_t>(i))));
    }
    drawZones(kSlots, 2);
}

void Panel::setArrows(const World& world, const std::array<std::uint8_t, 4>& exits) {
    const ProgramFile& program = game_.resources().program();
    for (int i = 0; i < 6; ++i) {
        zones_[static_cast<std::size_t>(kArrows + i)] = zoneAt(program, kArrowZones + kZoneSize * static_cast<std::size_t>(i));
    }
    Zone& pad = zones_[kArrows];
    Zone& middle = zones_[kArrows + 5];
    if (world.inside() && (world.room() >> 8) != 0x21) {
        const bool firstRoom = (world.room() & 0xFF) == 1;
        pad.sprite = firstRoom ? kArrowsFirstRoom : kArrowsInside;
        // The middle shows only in the palace, and not in its first room.
        middle.flags = ((world.place() >> 8) == kPalace && !firstRoom) ? Zone::kActive : Zone::kHidden;
        for (int i = 0; i < 4; ++i) {
            const auto exit = static_cast<std::int8_t>(exits[static_cast<std::size_t>(i)]);
            const bool open = exit != 0 && exit >= -5;
            zones_[static_cast<std::size_t>(kArrows + 1 + i)].flags = open ? Zone::kActive : Zone::kHidden;
        }
    } else {
        pad.sprite = kArrowsOutside;
        for (int i = 0; i < 4; ++i) {
            Zone& z = zones_[static_cast<std::size_t>(kArrows + 1 + i)];
            z.sprite = static_cast<std::uint16_t>(kFirstArrow + i);
            z.flags = Zone::kActive;
        }
    }
    drawArrows();
}

void Panel::setArrowZones(std::size_t table) {
    const ProgramFile& program = game_.resources().program();
    for (int i = 0; i < 6; ++i) {
        zones_[static_cast<std::size_t>(kArrows + i)] = zoneAt(program, table + kZoneSize * static_cast<std::size_t>(i));
    }
    drawArrows();
}

void Panel::drawArrows() {
    const ProgramFile& program = game_.resources().program();
    // Zone 2's pictures 3 to 5 leave the place behind the arrows to be filled.
    if (static_cast<std::uint16_t>(zones_[2].sprite - 3) < 3) {
        const ClipRect r = rectAt(program, kArrowPad);
        game_.display().target().fillRect(r.left, r.top + game_.display().originRows(), r.right - r.left, r.bottom - r.top,
                                          kArrowPadColour);
    }
    drawZones(kArrows, 6);
}

void Panel::setCommands(std::vector<Command> commands) {
    commands_ = std::move(commands);
}

void Panel::drawCommands() {
    const bool more = commands_.size() > static_cast<std::size_t>(kLines);
    for (int line = 0; line < kLines; ++line) {
        std::uint16_t phrase = 0;
        if (line == kLines - 1 && more) {
            phrase = kOthers;
        } else if (static_cast<std::size_t>(line) < commands_.size()) {
            phrase = commands_[static_cast<std::size_t>(line)].phrase;
        }
        drawCommandLine(line, phrase);
    }
}

void Panel::drawCommandLine(int line, std::uint16_t phrase) {
    Zone& z = zones_[static_cast<std::size_t>(kCommandLines + line)];
    z.flags = static_cast<std::uint16_t>(z.flags & ~Zone::kActive);
    Surface& to = game_.display().target();
    const int y = z.top + 1 + game_.display().originRows();
    int x = kCommandX;
    std::uint8_t background = kLineBackground;
    if ((phrase & Command::kPhrase) != 0) {
        std::uint8_t colour = kGreyColour;
        if ((phrase & Command::kGrey) == 0) {
            z.flags = static_cast<std::uint16_t>(z.flags | Zone::kActive);
            colour = kCommandColour;
            if (phrase & Command::kLit) {
                std::swap(colour, background);
            }
        }
        const std::string& text = phrases().at((phrase & Command::kPhrase) - 1u);
        x = font_.draw(to, x, y, " ", colour, background);
        for (const char c : text) {
            if (static_cast<std::uint8_t>(c) == 0x0D) {
                break;
            }
            x = font_.draw(to, x, y, std::string_view(&c, 1), colour, background);
        }
    }
    if (x < kCommandRight) {
        to.fillRect(x, y, kCommandRight - x, kCommandRows, background);
    }
}

} // namespace mentat
