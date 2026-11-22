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
        font_ = Font::parse(g_.resources().load(resource::kFont), Font::Size::Small);
    }
    Display& d = g_.display();
    for (const char c : text) {
        x = font_->draw(d.screen(), x, kTextY, std::string_view(&c, 1), colour, kTextBackground);
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
            k = kind < 0x21 ? 1 : kind < 0x28 ? 2 : kind < 0x30 ? 3 : 2;
        }
        x = drawText(x, phrase(kKindPhrase + k), kNameColour);
        x = drawText(x, phrase(world_.u8(record)), kNameColour);
        const int second = world_.u8(record + 1);
        x = drawText(x, second >= kHyphenFrom ? "-" : " ", kNameColour);
        x = drawText(x, phrase(kSecondWordPhrase + second), kNameColour);
    }
    clearLineFrom(x);
}

std::size_t Cockpit::destinationAt(int x, int y) {
    if (!active_) {
        return 0;
    }
    pointAt(x, y);
    // The desert is a way to fly rather than a place: not built yet.
    if (target_.kind != Target::Kind::Place) {
        return 0;
    }
    if (!world_.flying() && target_.place == world_.placeHere()) {
        return 0;
    }
    return target_.place;
}

void Cockpit::confirm(std::size_t record) {
    // The voice for a place goes by its name's two words.
    const int name = ((world_.u8(record) - 1) << 4 | world_.u8(record + 1)) & 0xFF;
    say(kAskVoice + name);
    const Target place = target_;
    for (int i = 0; i < kConfirmBlinks; ++i) {
        g_.waitTicks(kConfirmShown);
        target_ = Target{};
        writeTarget();
        g_.waitTicks(kConfirmHidden);
        target_ = place;
        writeTarget();
    }
    g_.waitSound();
}

void Cockpit::say(int phrase) {
    if (!g_.playVoice(Talker::voiceName(kVoiceCharacter, phrase, g_.indoors))) {
        g_.playVoice(Talker::voiceName(kVoiceCharacter, phrase, !g_.indoors));
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
