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
    boxShown_ = false;
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
    map().draw(d.target(), r.left, r.top, longitude_, latitude_);
    g_.selectSheet(resource::kMapSprites);
    drawPlaces();
    drawPlayer();
    const int previous = g_.sheetResource();
    g_.selectSheet(kPanelSheet);
    drawSpriteClipped(g_.canvas(), g_.sheet().sprite(kOrnamentHead), kOrnamentX, kOrnamentY, r);
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

void MapScreen::playerPoint(std::uint16_t& longitude, int& latitude) const { world_.position(longitude, latitude); }

void MapScreen::tick() {
    if (boxShown_ && g_.platform().ticks - lastClick_ >= kBoxTicks) {
        hideBox();
    }
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

bool MapScreen::command(std::uint16_t action) {
    if (!active_) {
        return false;
    }
    const ProgramFile& program = g_.resources().program();
    auto step = [&](int n) {
        const std::size_t at = kScrollSteps + 4 * static_cast<std::size_t>(n);
        scroll(s16(program.dataU16(at)), s16(program.dataU16(at + 2)));
    };
    switch (action) {
    case kUp:
        step(0);
        return true;
    case kRight:
        step(1);
        return true;
    case kDown:
        step(2);
        return true;
    case kLeft:
        step(3);
        return true;
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

} // namespace mentat
