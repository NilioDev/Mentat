// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/map_screen.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/string_table.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/font.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace mentat {

namespace {

constexpr int kPanelSheet = 0;

// The view: the map's rectangle, and where its middle is.
constexpr std::size_t kViewRect = 0x1482;
constexpr int kMiddleX = 160;
constexpr int kMiddleY = 76;
// Four frames around it, from the inside out, in these colours less 2
// each time.
constexpr std::uint8_t kFrameColour = 0xFC;

// The panel while the map is shown.
constexpr std::size_t kMapLayout = 0x1C66;
constexpr std::size_t kMapArrows = 0x1CCA;
// The map's commands: two bytes, then (phrase, action) pairs up to a
// phrase of 0.
constexpr std::size_t kMapCommands = 0x20F2;
constexpr std::uint16_t kGiveOrders = 0x93; // while Paul leads no more than one troop
constexpr std::uint16_t kTakeOrnithopter = 0xA7;
constexpr int kSpiceDensityProgress = 5;
constexpr std::size_t kProgress = 0x2A;

// How far the arrows move the view.
constexpr std::size_t kScrollSteps = 0x145E; // up, right, down, left: (longitude, latitude)

// The box that names the map.
constexpr std::size_t kBox = 0x194A; // rectangle, then the border and fill colours
constexpr std::uint16_t kBoxPhrase = 0xE2;
constexpr std::uint8_t kBoxText = 0xF0;
constexpr int kBoxTextX = 10;
constexpr int kBoxTextY = 8;
constexpr int kLineRows = 10;
constexpr std::uint32_t kBoxTicks = 1000;

// The places on the map: sprites of the map's picture file.
constexpr std::uint8_t kPlaceHidden = 0x80; // flag at 0x0A: not on the map
constexpr std::uint16_t kPlaceSprite = 0x7A;
constexpr std::uint16_t kFarSietch = 5; // added for a sietch further than Paul can see
constexpr std::size_t kSeeingDistance = 0x1176;
constexpr std::size_t kRalliedTroops = 0x28;

// Spice fields Paul has found: map points whose bits 4 and 5 are 01.
constexpr std::uint16_t kSpiceMark = 0x78;

// A place's label (see docs/rules/map.md).
constexpr int kLabelReach = 0x14;   // a click this far from a place (across plus down) or more misses it
constexpr int kLabelKinds = 0x31;   // places of a kind below this have labels
constexpr std::size_t kLabel = 0x1668; // rectangle, then the border and fill colours
constexpr std::size_t kLabelHeights = 0x11D0; // state: the height of each size of label
constexpr int kLabelWidth = 0x6A;
constexpr int kLabelGap = 0x0F;     // from the place to the label's left
constexpr int kLabelTurn = 0xD2;    // a label starting this far right goes left of the place
constexpr int kLabelBack = 0x82;
constexpr int kLabelTop = 4;
constexpr int kLabelBottom = 0x94;
constexpr std::uint16_t kKindPhrase = 0x44;
constexpr std::uint16_t kSecondWordPhrase = 0x0C;
constexpr int kHyphenFrom = 3;
constexpr std::uint8_t kKindColour = 0x9A;
constexpr std::uint8_t kNameColour = 0x96;
constexpr std::uint8_t kWaterColour = 0x90;
constexpr std::uint16_t kWaterPhrase = 0x6C;
constexpr std::uint16_t kNoWindTrapPhrase = 0x6D;
constexpr std::uint16_t kEquipmentPhrase = 0x6E;
constexpr std::uint16_t kBattlePhrase = 0x4C;
constexpr std::uint16_t kNonePhrase = 0x69;
constexpr std::uint16_t kWaterDrop = 0x75;
constexpr std::uint16_t kBattleGauge = 0x8E;
constexpr std::size_t kEquipmentSprites = 0x192F; // data: the sprite of each of the 7 kinds of equipment
constexpr std::size_t kStoryFlags = 0x0A;          // state: bit 5, water is shown
constexpr std::uint8_t kPlaceSeen = 0x02;    // flag at 0x0A: what is inside is known
constexpr std::uint8_t kPlaceSafe = 0x08;    // flag at 0x0A, for a fort
constexpr std::uint8_t kPlaceShown = 0x10;   // flag at 0x0A, for a fort: its inside is shown
constexpr std::uint8_t kPlaceWindTrap = 0x20; // flag at 0x0A

// Paul's place: sprite 0x4C of the panel's picture file, its bottom at the
// place and 13 pixels left of it.
constexpr std::uint16_t kPlayerMark = 0x4C;
constexpr int kPlayerMarkLeft = 13;

// The ornament's head, drawn again over the map.
constexpr std::uint16_t kOrnamentHead = 0x10 + 10;
constexpr int kOrnamentX = 0x96;
constexpr int kOrnamentY = 0x89;

std::int16_t s16(std::uint16_t v) { return static_cast<std::int16_t>(v); }

} // namespace

MapScreen::MapScreen(Game& game, World& world, Panel& panel) : g_(game), world_(world), panel_(panel) {}

MapScreen::~MapScreen() = default;

void MapScreen::setUp() { world_.setUpPlaces(map(), g_.resources().load(resource::kRegions)); }

FlatMap& MapScreen::map() {
    if (!map_) {
        map_ = std::make_unique<FlatMap>(g_.resources().load(resource::kLatitudes), g_.resources().load(resource::kMap),
                                         g_.display().tables().mapEdges);
    }
    return *map_;
}

namespace {

ClipRect viewRect(const ProgramFile& program) {
    auto s = [&](std::size_t o) { return static_cast<int>(s16(program.dataU16(kViewRect + o))); };
    return ClipRect{s(0), s(2), s(4), s(6)};
}

} // namespace

void MapScreen::enter() {
    std::uint16_t lon = 0;
    int lat = 0;
    playerPoint(lon, lat);
    enter(lon, lat);
}

void MapScreen::enter(std::uint16_t longitude, int latitude) {
    active_ = true;
    longitude_ = longitude;
    latitude_ = latitude;
    Display& d = g_.display();
    d.targetBack();
    drawFrame();
    panel_.ornament = 10;
    panel_.drawOrnament();
    drawView();
    panel_.setCommands(commands());
    panel_.drawCommands();
    panel_.setLayout(kMapLayout);
    panel_.setArrowZones(kMapArrows);
}

void MapScreen::show() {
    g_.display().transition(Display::kDissolveScreen);
    showBox();
}

void MapScreen::leave() {
    active_ = false;
    stale_ = false;
    chosen_ = 0;
    marks_.clear();
    boxShown_ = false;
    labelled_ = 0;
}

void MapScreen::drawFrame() {
    const ClipRect r = viewRect(g_.resources().program());
    Surface& to = g_.display().target();
    std::uint8_t colour = kFrameColour;
    for (int k = 0; k < 4; ++k, colour = static_cast<std::uint8_t>(colour - 2)) {
        const int left = r.left - 1 - k;
        const int top = r.top - 1 - k;
        const int right = r.right + k;
        const int bottom = r.bottom + k;
        for (int x = left; x <= right; ++x) {
            to.set(x, top, colour);
            to.set(x, bottom, colour);
        }
        for (int y = top; y <= bottom; ++y) {
            to.set(left, y, colour);
            to.set(right, y, colour);
        }
    }
}

void MapScreen::toScreen(std::uint16_t longitude, int latitude, int& x, int& y) const {
    const int ring = 2 * map_->width(std::abs(latitude));
    const int along = s16(static_cast<std::uint16_t>(longitude - longitude_));
    // Four pixels to a point: the offset in points, times four, rounded down.
    x = static_cast<int>((static_cast<long>(along) * ring * 4) >> 16) + kMiddleX;
    y = (latitude - latitude_) * 4 + kMiddleY;
}

ClipRect MapScreen::view() const { return viewRect(g_.resources().program()); }

bool MapScreen::inView(int x, int y) const {
    const ClipRect r = viewRect(g_.resources().program());
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

void MapScreen::drawView() {
    latitude_ = std::clamp(latitude_, -FlatMap::kMaxLatitude, FlatMap::kMaxLatitude);
    const ClipRect r = viewRect(g_.resources().program());
    Display& d = g_.display();
    d.targetBack();
    labelled_ = 0; // the label is on the screen only, and goes with the view
    map().draw(d.target(), r.left, r.top, longitude_, latitude_);
    g_.selectSheet(resource::kMapSprites);
    drawPlaces();
    drawPlayer();
    drawHead(r);
    base_ = d.back();
    drawMarks();
}

void MapScreen::drawHead(const ClipRect& clip) {
    const int previous = g_.sheetResource();
    g_.selectSheet(kPanelSheet);
    drawSpriteClipped(g_.canvas(), g_.sheet().sprite(kOrnamentHead), kOrnamentX, kOrnamentY, clip);
    g_.selectSheet(previous);
}

void MapScreen::drawPlaces() {
    const ClipRect r = viewRect(g_.resources().program());
    const SpriteSheet& sheet = g_.sheet();
    auto drawCentred = [&](std::uint16_t n, int x, int y) {
        const SpriteView v = sheet.sprite(n);
        drawSpriteClipped(g_.canvas(), v, x - v.header.width() / 2, y - v.header.height / 2, r);
    };

    // The spice fields Paul has found, row by row from the top, each
    // scanned right from the middle and then left from it.
    FlatMap& m = map();
    const auto& bytes = m.bytes();
    for (int latitude = latitude_ - FlatMap::kHalfRows;; ++latitude) {
        int x = 0;
        int y = 0;
        toScreen(longitude_, latitude, x, y);
        if (!inView(x, y)) {
            break;
        }
        const int ring = 2 * m.width(std::abs(latitude));
        const int start = m.rowStart(latitude);
        for (const int direction : {1, -1}) {
            int point = m.pointAt(longitude_, latitude);
            for (int px = x;;) {
                const std::size_t at = static_cast<std::size_t>(start + point);
                if (at + 1 < bytes.size() && (bytes[at] & 0x30) == 0x10) {
                    // Placed a little off the point, by where it is in memory.
                    const std::uint16_t n = static_cast<std::uint16_t>(kSpiceMark + ((bytes[at + 1] & 0x30) == 0x10 ? 1 : 0));
                    const int jitterY = static_cast<int>(at & 3);
                    const int jitterX = static_cast<int>(((static_cast<std::size_t>(ring) + at) >> 2) & 3);
                    drawCentred(n, px + jitterX, y + jitterY);
                }
                px += 4 * direction;
                if (direction > 0 ? px >= r.right : px < r.left) {
                    break;
                }
                point += direction;
                if (point >= ring) {
                    point -= ring;
                } else if (point < 0) {
                    point += ring;
                }
            }
        }
    }

    // The places, in the order of the state.
    shown_.clear();
    for (std::size_t record = World::kPlaces; world_.u16(record) != 0xFFFF; record += World::kPlaceSize) {
        if (world_.u8(record + 0x0A) & kPlaceHidden) {
            continue;
        }
        const std::uint16_t lon = world_.u16(record + 2);
        const int lat = s16(world_.u16(record + 4));
        int x = 0;
        int y = 0;
        toScreen(lon, lat, x, y);
        if (!inView(x, y)) {
            continue;
        }
        const std::uint16_t n = placeSprite(record, kPlaceSprite);
        drawCentred(n, x, y);
        shown_.push_back(Shown{record, x, y});
    }
}

std::uint16_t MapScreen::placeSprite(std::size_t record, std::uint16_t first) {
    const int kind = world_.u8(record + 8);
    std::uint16_t n = first;
    for (const int step : {0x20, 0x21, 0x28, 0x30}) {
        if (kind >= step) {
            ++n;
        }
    }
    if (kind < 0x20) {
        // A sietch further than Paul can see: in latitudes, or in the
        // points of a longitude at his latitude.
        FlatMap& m = map();
        std::uint16_t hereLon = 0;
        int hereLat = 0;
        playerPoint(hereLon, hereLat);
        const std::uint16_t lon = world_.u16(record + 2);
        const int lat = s16(world_.u16(record + 4));
        const int across = std::abs(static_cast<int>(s16(static_cast<std::uint16_t>(lon - hereLon)))) /
                           m.pointSize(std::abs(hereLat));
        const int distance = std::max(across, std::abs(hereLat - lat));
        if (distance > world_.u16(kSeeingDistance)) {
            n = static_cast<std::uint16_t>(n + kFarSietch);
        }
    }
    return n;
}

bool MapScreen::placeOnScreen(std::size_t record, int& x, int& y) const {
    for (const Shown& s : shown_) {
        if (s.record == record) {
            x = s.x;
            y = s.y;
            return true;
        }
    }
    return false;
}

void MapScreen::playerPoint(std::uint16_t& longitude, int& latitude) const { world_.position(longitude, latitude); }

void MapScreen::tick() {
    if (boxShown_ && g_.platform().ticks - lastClick_ >= kBoxTicks) {
        hideBox();
    }
    if (stale_ && !(windows && !windows().empty()) && !(covers && !covers().empty())) {
        stale_ = false;
        hideBox();
        drawView();
        g_.display().copyRect(g_.display().back(), g_.display().screen(), view());
    }
    stepMarks();
}

void MapScreen::redraw() {
    if (active()) {
        stale_ = true;
    }
}

void MapScreen::placeChanged(std::size_t record) {
    if (!active_ || (world_.u8(record + 0x0A) & kPlaceHidden)) {
        return;
    }
    stale_ = true;
}

void MapScreen::clicked() { lastClick_ = g_.platform().ticks; }

void MapScreen::drawPlayer() {
    std::uint16_t lon = 0;
    int lat = 0;
    playerPoint(lon, lat);
    int x = 0;
    int y = 0;
    toScreen(lon, lat, x, y);
    if (!inView(x, y)) {
        return;
    }
    const ClipRect r = viewRect(g_.resources().program());
    const int previous = g_.sheetResource();
    g_.selectSheet(kPanelSheet);
    const SpriteView v = g_.sheet().sprite(kPlayerMark);
    drawSpriteClipped(g_.canvas(), v, x - kPlayerMarkLeft, (y - v.header.height) & 0xFF, r);
    g_.selectSheet(previous);
}

std::vector<Command> MapScreen::commands() const {
    const ProgramFile& program = g_.resources().program();
    std::vector<Command> list;
    for (std::size_t at = kMapCommands + 2;; at += 4) {
        Command c{program.dataU16(at), program.dataU16(at + 2)};
        if (c.phrase == 0) {
            break;
        }
        list.push_back(c);
    }
    // Giving orders: with one troop or none, only to the troop at the
    // player's place; otherwise to troops on the move.
    if (list.size() > 1) {
        if (world_.u16(kSeeingDistance) < 2) {
            list[1].phrase = kGiveOrders | Command::kGrey;
            if (world_.inside()) {
                const std::size_t here = World::placeRecord(world_.place() >> 8);
                const int troop = world_.u8(here + 9);
                if (troop != 0 && world_.u8(World::troopRecord(troop) + 3) < 0x80) {
                    list[1].phrase = kGiveOrders;
                }
            }
        } else {
            list[1].phrase = static_cast<std::uint16_t>(list[1].phrase | Command::kGrey); // no troops move yet
        }
    }
    // The spice density, and the fifth command, come with the story.
    if (list.size() > 4 && world_.u8(kProgress) < kSpiceDensityProgress) {
        list[2].phrase = static_cast<std::uint16_t>(list[2].phrase | Command::kGrey);
        list.resize(4);
    }
    // Taking an ornithopter from the map: not yet.
    if (list.size() > 3) {
        list[3].phrase = kTakeOrnithopter | Command::kGrey;
    }
    return list;
}

void MapScreen::showBox() {
    const ProgramFile& program = g_.resources().program();
    Display& d = g_.display();
    d.targetScreen();
    Surface& to = d.target();
    const int left = s16(program.dataU16(kBox));
    const int top = s16(program.dataU16(kBox + 2));
    const int right = s16(program.dataU16(kBox + 4));
    const int bottom = s16(program.dataU16(kBox + 6));
    const std::uint8_t border = program.dataU8(kBox + 8);
    const std::uint8_t fill = program.dataU8(kBox + 9);
    to.fillRect(left, top, right - left, bottom - top, fill);
    for (int x = left; x < right; ++x) {
        to.set(x, top, border);
        to.set(x, bottom - 1, border);
    }
    for (int y = top; y < bottom; ++y) {
        to.set(left, y, border);
        to.set(right - 1, y, border);
    }
    // The number of rallied troops goes over the last three characters of
    // the first run of digits.
    std::string text = panel_.phrases().at(kBoxPhrase - 1u);
    std::size_t at = 0;
    while (at < text.size() && (text[at] < '0' || text[at] > '9')) {
        ++at;
    }
    while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
        ++at;
    }
    if (at >= 3) {
        const int n = std::min<int>(world_.u8(kRalliedTroops), 999);
        const char hundreds = n >= 100 ? static_cast<char>('0' + n / 100) : ' ';
        const char tens = n >= 10 ? static_cast<char>('0' + n / 10 % 10) : ' ';
        text[at - 3] = hundreds;
        text[at - 2] = tens;
        text[at - 1] = static_cast<char>('0' + n % 10);
    }
    const Font font = Font::parse(g_.resources().load(resource::kFont));
    int x = left + kBoxTextX;
    int y = top + kBoxTextY;
    for (const char c : text) {
        if (c == '\r') {
            x = left + kBoxTextX;
            y += kLineRows;
            continue;
        }
        x = font.draw(to, x, y, std::string_view(&c, 1), kBoxText);
    }
    d.targetBack();
    boxShown_ = true;
    lastClick_ = g_.platform().ticks;
}

ClipRect MapScreen::boxRect() const {
    const ProgramFile& program = g_.resources().program();
    return ClipRect{s16(program.dataU16(kBox)), s16(program.dataU16(kBox + 2)), s16(program.dataU16(kBox + 4)),
                    s16(program.dataU16(kBox + 6))};
}

void MapScreen::hideBox() {
    if (!boxShown_) {
        return;
    }
    boxShown_ = false;
    const ProgramFile& program = g_.resources().program();
    const ClipRect r{s16(program.dataU16(kBox)), s16(program.dataU16(kBox + 2)), s16(program.dataU16(kBox + 4)),
                     s16(program.dataU16(kBox + 6))};
    g_.display().copyRect(g_.display().back(), g_.display().screen(), r);
}

void MapScreen::scroll(int longitude, int latitude) {
    hideBox();
    longitude_ = static_cast<std::uint16_t>(longitude_ + longitude);
    latitude_ += latitude;
    drawView();
    const ClipRect r = viewRect(g_.resources().program());
    g_.display().copyRect(g_.display().back(), g_.display().screen(), r);
}

bool MapScreen::arrowStep(std::uint16_t action, int& longitude, int& latitude) const {
    int n = 0;
    switch (action) {
    case kUp:
        n = 0;
        break;
    case kRight:
        n = 1;
        break;
    case kDown:
        n = 2;
        break;
    case kLeft:
        n = 3;
        break;
    default:
        return false;
    }
    const ProgramFile& program = g_.resources().program();
    const std::size_t at = kScrollSteps + 4 * static_cast<std::size_t>(n);
    longitude = s16(program.dataU16(at));
    latitude = s16(program.dataU16(at + 2));
    return true;
}

void MapScreen::centreOn(std::uint16_t longitude, int latitude) {
    longitude_ = longitude;
    latitude_ = latitude;
    drawView();
    g_.display().copyRect(g_.display().back(), g_.display().screen(), view());
}

bool MapScreen::command(std::uint16_t action) {
    if (!active_) {
        return false;
    }
    const ProgramFile& program = g_.resources().program();
    int lon = 0;
    int lat = 0;
    if (arrowStep(action, lon, lat)) {
        scroll(lon, lat);
        return true;
    }
    switch (action) {
    case kCentre:
        // Back to the player's place, the box left as it is.
        playerPoint(longitude_, latitude_);
        drawView();
        g_.display().copyRect(g_.display().back(), g_.display().screen(), viewRect(program));
        return true;
    default:
        return false;
    }
}

// --- A place's label

void MapScreen::click(int x, int y) {
    hideBox();
    // The nearest place drawn, across plus down; the first of equals.
    const Shown* nearest = nullptr;
    int best = 0xFFFF;
    for (const Shown& place : shown_) {
        if (world_.u8(place.record + 8) >= kLabelKinds) {
            continue;
        }
        const int distance = std::abs(place.x - x) + std::abs(place.y - y);
        if (distance < best) {
            best = distance;
            nearest = &place;
        }
    }
    if (nearest == nullptr || best >= kLabelReach || nearest->record == labelled_) {
        hideLabel();
        return;
    }
    hideLabel();
    showLabel(nearest->record, nearest->x, nearest->y);
}

void MapScreen::hideLabel() {
    if (labelled_ == 0) {
        return;
    }
    labelled_ = 0;
    g_.display().copyRect(g_.display().back(), g_.display().screen(), label_);
}

// What is inside a place is known: Paul has seen it, or a troop of his
// waits there.
bool MapScreen::knownInside(std::size_t record) const {
    if (world_.u8(record + 0x0A) & kPlaceSeen) {
        return true;
    }
    const int kind = world_.u8(record + 8);
    if (kind >= 0x28 && (world_.u8(record + 0x0A) & kPlaceSafe)) {
        return false;
    }
    for (int troop = world_.u8(record + 9); troop != 0;) {
        const std::size_t at = World::troopRecord(troop);
        const int job = world_.u8(at + 3);
        if (!(job & 0x20) && !(world_.u8(at + 0x10) & 0x80) && job == 6) {
            return true;
        }
        troop = world_.u8(at + 1);
    }
    return false;
}

namespace {

int kindPhrase(int kind) {
    if (kind < 0x20) {
        return 0; // sietch
    }
    if (kind < 0x21) {
        return 1; // palace
    }
    if (kind < 0x28) {
        return 2; // village
    }
    if (kind < 0x30) {
        return 3; // fort
    }
    return 1;
}

} // namespace

// 0 the tallest label, 2 the name alone.
int MapScreen::labelSize(std::size_t record) const {
    if (knownInside(record)) {
        return 1;
    }
    const int kind = world_.u8(record + 8);
    const std::uint8_t flags = world_.u8(record + 0x0A);
    if (!(kind >= 0x28 && (flags & kPlaceSafe)) || !(flags & kPlaceShown)) {
        return 2;
    }
    switch (kindPhrase(kind)) {
    case 0:
    case 3:
        return 0;
    case 2:
        return 1;
    default:
        return 2;
    }
}

// The gauge of the fight at a place, 0 to 8: the men of Paul's troops
// waiting there against what they hold, and the Harkonnens' men against
// theirs.
int MapScreen::battle(std::size_t record) const { return (world_.battleBalance(record) + 0x0F) >> 5; }

void MapScreen::showLabel(std::size_t record, int px, int py) {
    const ProgramFile& program = g_.resources().program();
    const int size = labelSize(record);
    const int height = world_.u8(kLabelHeights + static_cast<std::size_t>(size));
    int top = std::max(py - height / 2, kLabelTop);
    top = std::min(top, kLabelBottom - height);
    int left = px + kLabelGap;
    if (left >= kLabelTurn) {
        left -= kLabelBack;
    }
    label_ = ClipRect{left, top, left + kLabelWidth, top + height};
    labelled_ = record;

    Display& d = g_.display();
    d.targetScreen();
    Surface& to = d.target();
    const std::uint8_t border = program.dataU8(kLabel + 8);
    const std::uint8_t fill = program.dataU8(kLabel + 9);
    to.fillRect(label_.left, label_.top, kLabelWidth, height, fill);
    for (int x = label_.left; x < label_.right; ++x) {
        to.set(x, label_.top, border);
        to.set(x, label_.bottom - 1, border);
    }
    for (int y = label_.top; y < label_.bottom; ++y) {
        to.set(label_.left, y, border);
        to.set(label_.right - 1, y, border);
    }

    const StringTable& phrases = panel_.phrases();
    auto phrase = [&](int n) -> const std::string& { return phrases.at(static_cast<std::size_t>(n) - 1u); };
    const Font large = Font::parse(g_.resources().load(resource::kFont), Font::Size::Large);
    auto write = [&](const Font& font, int x, int y, const std::string& text, std::uint8_t colour) {
        for (const char c : text) {
            x = font.draw(to, x, y, std::string_view(&c, 1), colour, fill);
        }
        return x;
    };

    // What it is and its name.
    write(large, left + 0x0C, top + 4, phrase(kKindPhrase + kindPhrase(world_.u8(record + 8))), kKindColour);
    int y = top + 13;
    {
        int x = write(large, left + 4, y, phrase(world_.u8(record)), kNameColour);
        const int second = world_.u8(record + 1);
        x = write(large, x, y, second >= kHyphenFrom ? "-" : " ", kNameColour);
        write(large, x, y, phrase(kSecondWordPhrase + second), kNameColour);
    }
    if (size == 2) {
        d.targetBack();
        return;
    }

    const int previous = g_.sheetResource();
    g_.selectSheet(resource::kMapSprites);
    const SpriteSheet& sheet = g_.sheet();
    // A row (or column) of n copies of a sprite, closer together when they
    // do not fit before `end`.
    auto repeat = [&](std::uint16_t n, int count, int atX, int atY, int end, bool across) {
        if (count == 0) {
            return;
        }
        const SpriteHeader h = sheet.sprite(n).header;
        const int room = end - (across ? atX : atY);
        int step = (across ? (h.widthAndFlags & 0x0FFF) : h.height) + 2;
        if (room / count < step) {
            const int rest = room - step;
            step = rest / count;
            if (step < 2) {
                step = 2;
                count = rest / 2;
            }
        }
        for (int i = 0; i < count; ++i) {
            g_.drawSprite(n, atX, atY);
            (across ? atX : atY) += step;
        }
    };

    // The water, for a fort once the story shows it.
    if (size == 0 && (world_.u8(kStoryFlags) & 0x20)) {
        const Font small = Font::parse(g_.resources().load(resource::kFont), Font::Size::Small);
        y += 0x0A;
        int x = write(small, left + 4, y, phrase(kWaterPhrase), kWaterColour);
        y += 7;
        if (world_.u8(record + 0x0A) & kPlaceWindTrap) {
            const int water = world_.u8(record + 0x1B);
            std::string number(3, ' ');
            if (water >= 100) {
                number[0] = static_cast<char>('0' + water / 100);
            }
            if (water >= 10) {
                number[1] = static_cast<char>('0' + water / 10 % 10);
            }
            number[2] = static_cast<char>('0' + water % 10);
            write(small, x, y - 7, number, kWaterColour);
            repeat(kWaterDrop, water, left + 4, y, label_.right, true);
        } else {
            write(small, left + 0x0A, y, phrase(kNoWindTrapPhrase), kWaterColour);
        }
    }

    y += 0x0C;
    if (knownInside(record)) {
        write(large, left + 4, y, phrase(kBattlePhrase), kKindColour);
        g_.drawSprite(static_cast<std::uint16_t>(kBattleGauge + battle(record)), left + 4 + 0x2F, y + 6);
    } else {
        write(large, left + 4, y, phrase(kEquipmentPhrase), kKindColour);
        y += 0x0A;
        bool any = false;
        for (std::size_t i = 0; i < 7; ++i) {
            any = any || world_.u8(record + 0x14 + i) != 0;
        }
        if (!any) {
            write(large, left + 4 + 0x0C, y + 5, phrase(kNonePhrase), kKindColour);
        } else {
            int x = left + 4;
            for (std::size_t i = 0; i < 7; ++i) {
                const int count = world_.u8(record + 0x14 + i);
                if (count == 0) {
                    continue;
                }
                const auto n = static_cast<std::uint16_t>(program.dataU8(kEquipmentSprites + i));
                repeat(n, count, x, y, label_.bottom, false);
                x += (sheet.sprite(n).header.widthAndFlags & 0x0FFF) + 1;
            }
        }
    }
    g_.selectSheet(previous);
    d.targetBack();
}

} // namespace mentat
