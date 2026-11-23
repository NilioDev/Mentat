// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/cockpit.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/string_table.hpp"
#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/places.hpp"
#include "mentat/engine/talker.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <algorithm>
#include <cstdlib>

namespace mentat {

namespace {

constexpr int kPanelSheet = 0;
constexpr int kCockpitSheet = 0x24; // ORNYPAN.HSQ

// In the program's data: the map's rectangle, the cockpit's pictures drawn
// before the map and those drawn over it, as (sprite, x, y) up to 0xFFFF.
constexpr std::size_t kMapRect = 0x149C;
constexpr std::size_t kUnderMap = 0x14B4;
constexpr std::size_t kOverMap = 0x14C0;
// The commands: two bytes, then (phrase, action) pairs up to a phrase of
// 0. The arrows are the map's.
constexpr std::size_t kCommands = 0x212E;
constexpr std::size_t kArrowZones = 0x1CCA;

// The warning of the Harkonnens' zone: its phrase, where it starts and
// its colours.
constexpr std::uint16_t kWarningPhrase = 0xBF;
constexpr int kWarningX = 0x66;
constexpr int kWarningY = 0x4E;
constexpr std::uint16_t kWarningColours = 0x200C;
constexpr int kWarningLineStep = 7;

// Paul's mark: blinks every 300 ticks, its bottom at his place and 13
// pixels left of it.
constexpr std::uint16_t kPlayerMark = 0x4C;
constexpr int kPlayerMarkLeft = 13;
constexpr int kBlinkTimer = 0x44AB;
constexpr int kBlinkTicks = 300;

// The line above the map: the title is typed a letter every 24 ticks;
// what the mouse points at is written over it, and the rest of the line
// cleared up to x 237.
constexpr int kTextX = 0x55;
constexpr int kTextY = 0x26;
// Calling a worm: the line is higher, on a bar of its own.
constexpr int kWormTextY = 0x22;
constexpr std::size_t kWormBar = 0x14A4;
constexpr std::uint8_t kWormBarColour = 0xF5;
constexpr int kTextEnd = 0xED;
constexpr std::uint8_t kTextBackground = 0x20;
constexpr std::uint8_t kTitleColour = 0x61;
constexpr std::uint8_t kNameColour = 0xFE;
constexpr std::uint16_t kTitlePhrase = 0x57;
constexpr int kTypeTimer = 0x46B5;
constexpr int kTypeTicks = 0x18;

// The names: the kind of place, then its two words, joined by a hyphen
// when the second is a word of its own (from 3 on).
constexpr std::uint16_t kKindPhrase = 0x44;
constexpr std::uint16_t kSecondWordPhrase = 0x0C;
constexpr int kHyphenFrom = 3;
constexpr std::uint16_t kDesertPhrase = 0xA4;
constexpr std::uint16_t kDirectionPhrase = 0xDA;

// The mouse points at a place within this distance (across plus down).
constexpr int kNearPlace = 9;

// The cockpit's own voice asks where to.
constexpr int kVoiceCharacter = 0x19;
constexpr int kAskVoice = 0x2BC;
constexpr int kDirectionVoice = 0x2AC;
constexpr int kDesertVoice = 0x2BB;
// A destination chosen: its name blinks 9 times, shown 20 ticks and gone
// 10.
constexpr int kConfirmBlinks = 9;
constexpr int kConfirmShown = 0x14;
constexpr int kConfirmHidden = 0x0A;

std::int16_t s16(std::uint16_t v) { return static_cast<std::int16_t>(v); }

} // namespace

Cockpit::Cockpit(Game& game, World& world, Panel& panel, MapScreen& map)
    : g_(game), world_(world), panel_(panel), small_(game, world, map) {}

ClipRect Cockpit::view() const {
    const ProgramFile& program = g_.resources().program();
    auto at = [&](std::size_t o) { return static_cast<int>(s16(program.dataU16(kMapRect + o))); };
    return ClipRect{at(0), at(2), at(4), at(6)};
}

void Cockpit::enter() {
    active_ = true;
    leaving = false;
    target_ = Target{};
    const ProgramFile& program = g_.resources().program();
    g_.selectSheet(kCockpitSheet);

    std::vector<Command> commands;
    for (std::size_t at = kCommands + 2; program.dataU16(at) != 0; at += 4) {
        commands.push_back(Command{program.dataU16(at), program.dataU16(at + 2)});
    }
    Display& d = g_.display();
    d.targetBack();
    panel_.setCommands(std::move(commands));
    panel_.drawCommands();
    panel_.setArrowZones(kArrowZones);

    small_.view = view();
    world_.position(small_.longitude, small_.latitude);
}

void Cockpit::drawScene() {
    const ProgramFile& program = g_.resources().program();
    g_.display().targetBack();
    if (worm) {
        // Over the view on the back buffer: the bar, then the map.
        Display& d = g_.display();
        const int left = s16(program.dataU16(kWormBar));
        const int top = s16(program.dataU16(kWormBar + 2));
        d.target().fillRect(left, top + d.originRows(), s16(program.dataU16(kWormBar + 4)) - left,
                            s16(program.dataU16(kWormBar + 6)) - top, kWormBarColour);
        small_.drawMap();
        // Four rings around the map, from the inside colours 0xFC, 0xFA,
        // 0xF8 and 0xF6.
        ClipRect ring = small_.view;
        const int o = d.originRows();
        for (const int c : {0xFC, 0xFA, 0xF8, 0xF6}) {
            const auto colour = static_cast<std::uint8_t>(c);
            --ring.left;
            --ring.top;
            d.target().fillRect(ring.left, ring.top + o, ring.right - ring.left + 1, 1, colour);
            d.target().fillRect(ring.left, ring.bottom + o, ring.right - ring.left + 1, 1, colour);
            d.target().fillRect(ring.left, ring.top + o, 1, ring.bottom - ring.top + 1, colour);
            d.target().fillRect(ring.right, ring.top + o, 1, ring.bottom - ring.top + 1, colour);
            ++ring.right;
            ++ring.bottom;
        }
        g_.selectSheet(kPanelSheet);
        small_.drawPlaces();
        panel_.drawOrnament();
        return;
    }
    g_.places().drawSky();
    auto drawList = [&](std::size_t at) {
        for (; program.dataU16(at) != 0xFFFF; at += 6) {
            g_.drawSprite(program.dataU16(at), s16(program.dataU16(at + 2)), s16(program.dataU16(at + 4)));
        }
    };
    g_.selectSheet(kCockpitSheet);
    drawList(kUnderMap);
    small_.drawMap();
    g_.selectSheet(kPanelSheet);
    small_.drawPlaces();
    g_.selectSheet(kCockpitSheet);
    drawList(kOverMap);
    // The panel's ornament stays as it is, over the cockpit.
    panel_.drawOrnament();
}

void Cockpit::drawWarning() {
    const ProgramFile& program = g_.resources().program();
    Display& d = g_.display();
    d.targetBack();
    g_.places().drawSky();
    auto drawList = [&](std::size_t at) {
        for (; program.dataU16(at) != 0xFFFF; at += 6) {
            g_.drawSprite(program.dataU16(at), s16(program.dataU16(at + 2)), s16(program.dataU16(at + 4)));
        }
    };
    g_.selectSheet(kCockpitSheet);
    drawList(kUnderMap);
    drawList(kOverMap);
    panel_.drawOrnament();
    if (!font_) {
        font_ = Font::parse(g_.resources().load(g_.fontFile()), Font::Size::Small);
    }
    const std::string& text = panel_.phrases().at(kWarningPhrase - 1);
    int x = kWarningX;
    int y = kWarningY + d.originRows();
    for (const char c : text) {
        if (c == '\r') {
            x = kWarningX;
            y += kWarningLineStep;
            continue;
        }
        x = font_->draw(d.target(), x, y, std::string_view(&c, 1), static_cast<std::uint8_t>(kWarningColours),
                             static_cast<std::uint8_t>(kWarningColours >> 8));
    }
}

void Cockpit::show() {
    Display& d = g_.display();
    d.showBack();
    d.flush();
    placeMark();
    startTyping();
    say(kAskVoice);
}

void Cockpit::leave() {
    active_ = false;
    small_.forget();
    markShown_ = false;
    g_.removeTimer(kBlinkTimer);
    stopTyping();
}

// Paul's mark, when his place is on the map: drawn on the screen only,
// and on and off every 300 ticks.
void Cockpit::placeMark() {
    std::uint16_t lon = 0;
    int lat = 0;
    world_.position(lon, lat);
    int x = 0;
    int y = 0;
    markShown_ = small_.toScreen(lon, lat, x, y);
    if (!markShown_) {
        return;
    }
    const int previous = g_.sheetResource();
    g_.selectSheet(kPanelSheet);
    const SpriteView v = g_.sheet().sprite(kPlayerMark);
    mark_ = ClipRect{x - kPlayerMarkLeft, y - v.header.height, x - kPlayerMarkLeft + v.header.width(), y};
    g_.selectSheet(previous);
    blinks_ = 0;
    blink();
    g_.addTimer(kBlinkTimer, kBlinkTicks, [this] { blink(); });
}

void Cockpit::blink() {
    ++blinks_;
    Display& d = g_.display();
    const ClipRect r = view();
    const ClipRect under{std::max(mark_.left, r.left), std::max(mark_.top, r.top), std::min(mark_.right, r.right),
                         std::min(mark_.bottom, r.bottom)};
    d.copyRect(d.back(), d.screen(), under);
    if (blinks_ & 1) {
        const int previous = g_.sheetResource();
        g_.selectSheet(kPanelSheet);
        d.targetScreen();
        drawSpriteClipped(g_.canvas(), g_.sheet().sprite(kPlayerMark), mark_.left, mark_.top, r);
        d.targetBack();
        g_.selectSheet(previous);
    }
}

// --- The line above the map

int Cockpit::drawText(int x, const std::string& text, std::uint8_t colour) {
    if (!font_) {
        font_ = Font::parse(g_.resources().load(g_.fontFile()), Font::Size::Small);
    }
    Display& d = g_.display();
    for (const char c : text) {
        x = font_->draw(d.screen(), x, worm ? kWormTextY : kTextY, std::string_view(&c, 1), colour,
                        worm ? kWormBarColour : kTextBackground);
    }
    return x;
}

void Cockpit::clearLineFrom(int x) {
    while (x <= kTextEnd) {
        x = drawText(x, " ", kNameColour);
    }
}

void Cockpit::startTyping() {
    if (typing_) {
        return;
    }
    typing_ = true;
    typed_ = 0;
    typeX_ = kTextX;
    g_.addTimer(kTypeTimer, kTypeTicks, [this] { typeNext(); });
}

void Cockpit::stopTyping() {
    if (typing_) {
        typing_ = false;
        g_.removeTimer(kTypeTimer);
    }
}

// A letter of the title, and the next one at once after a space.
void Cockpit::typeNext() {
    const std::string& title = panel_.phrases().at(kTitlePhrase - 1u);
    while (typed_ < title.size()) {
        const char c = title[typed_++];
        typeX_ = drawText(typeX_, std::string(1, c), kTitleColour);
        if (c != ' ') {
            break;
        }
    }
}

Cockpit::Target Cockpit::targetAt(int x, int y) const {
    Target t;
    const ClipRect r = view();
    if (x < r.left || x >= r.right || y < r.top || y >= r.bottom) {
        return t;
    }
    // The nearest place, the first of those as near.
    int best = 0xFFFF;
    std::size_t nearest = 0;
    for (const SmallMap::Shown& s : small_.shown()) {
        const int distance = std::abs(s.x - x) + std::abs(s.y - y);
        if (distance < best) {
            best = distance;
            nearest = s.record;
        }
    }
    if (nearest != 0 && best < kNearPlace) {
        t.kind = Target::Kind::Place;
        t.place = nearest;
        return t;
    }
    t.kind = Target::Kind::Desert;
    if (!markShown_) {
        return t;
    }
    // The direction from Paul's place, when it is near one of the eight.
    const int dx = x - (mark_.left + kPlayerMarkLeft - 2);
    const int dy = y - mark_.bottom;
    int angle = 0;
    if (std::abs(dx) >= std::abs(dy)) {
        if (dx != 0) {
            angle = 0x20 * dy / dx + (dx >= 0 ? 0x40 : 0xC0);
        }
    } else {
        angle = 0x20 * dx / dy;
        if (dy >= 0) {
            angle -= 0x80;
        }
        angle = -angle;
    }
    angle = (angle + 3) & 0xFF;
    if ((angle & 0x1F) < 6) {
        t.direction = angle >> 5;
    }
    return t;
}

void Cockpit::pointAt(int x, int y) {
    if (!active_) {
        return;
    }
    const Target t = targetAt(x, y);
    if (t == target_) {
        return;
    }
    target_ = t;
    writeTarget();
}

void Cockpit::writeTarget() {
    if (target_.kind == Target::Kind::Nothing) {
        startTyping();
        clearLineFrom(kTextX);
        return;
    }
    stopTyping();
    const StringTable& phrases = panel_.phrases();
    auto phrase = [&](int n) { return phrases.at(static_cast<std::size_t>(n) - 1u); };
    int x = kTextX;
    if (target_.kind == Target::Kind::Desert) {
        x = drawText(x, phrase(kDesertPhrase), kNameColour);
        if (target_.direction >= 0) {
            x = drawText(x, " ", kNameColour);
            x = drawText(x, phrase(kDirectionPhrase + target_.direction), kNameColour);
        }
    } else {
        const std::size_t record = target_.place;
        const int kind = world_.u8(record + 8);
        int k = 0;
        if (kind >= 0x20) {
            k = kind < 0x21 ? 1 : kind < 0x28 ? 2 : kind < 0x30 ? 3 : 1;
        }
        x = drawText(x, phrase(kKindPhrase + k), kNameColour);
        x = drawText(x, phrase(world_.u8(record)), kNameColour);
        const int second = world_.u8(record + 1);
        x = drawText(x, second >= kHyphenFrom ? "-" : " ", kNameColour);
        x = drawText(x, phrase(kSecondWordPhrase + second), kNameColour);
    }
    clearLineFrom(x);
}

Cockpit::Destination Cockpit::destinationAt(int x, int y) {
    Destination to;
    if (!active_) {
        return to;
    }
    pointAt(x, y);
    if (target_.kind == Target::Kind::Nothing) {
        return to;
    }
    if (target_.kind == Target::Kind::Place) {
        if (!world_.flying() && target_.place == world_.placeHere()) {
            return to;
        }
        to.record = target_.place;
    }
    to.chosen = true;
    small_.toMap(x, y, to.longitude, to.latitude);
    return to;
}

void Cockpit::confirm() {
    // The voice for a place goes by its name's two words; for the desert
    // by the direction, or the one before the directions without one.
    int voice = kDesertVoice;
    if (target_.kind == Target::Kind::Place) {
        const std::size_t record = target_.place;
        voice = kAskVoice + (((world_.u8(record) - 1) << 4 | world_.u8(record + 1)) & 0xFF);
    } else if (target_.direction >= 0) {
        voice = kDirectionVoice + target_.direction;
    }
    say(voice);
    const Target chosen = target_;
    for (int i = 0; i < kConfirmBlinks; ++i) {
        g_.waitTicks(kConfirmShown);
        target_ = Target{};
        writeTarget();
        g_.waitTicks(kConfirmHidden);
        target_ = chosen;
        writeTarget();
    }
    g_.waitSound();
}

void Cockpit::say(int phrase) {
    if (g_.playVoice(Talker::voiceName(kVoiceCharacter, phrase, g_.indoors)) ||
        g_.playVoice(Talker::voiceName(kVoiceCharacter, phrase, !g_.indoors))) {
        g_.quietenForVoice();
    }
}

bool Cockpit::command(std::uint16_t action) {
    if (!active_ || action != kCancel) {
        return false;
    }
    leaving = true;
    return true;
}

} // namespace mentat
