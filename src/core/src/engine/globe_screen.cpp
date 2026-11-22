// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/globe_screen.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <algorithm>
#include <cstdlib>

namespace mentat {

namespace {

constexpr int kPanelSheet = 0;

// The picture: a fill, the planet's frame, the planet, and the frescoes of
// the two houses at the sides.
constexpr std::uint8_t kFill = 0xF0;
constexpr int kFrameSprite = 2;
constexpr int kFrameX = 0x5B;
constexpr int kFrameY = 0x14;
constexpr int kLeftSide = 0;
constexpr int kRightSide = 1;
constexpr int kRightSideX = 0xD6;
constexpr ClipRect kPlanetArea{96, 15, 235, 135};

// The panel: the map's layout stays; the arrows and the button beside the
// list come from here.
constexpr std::size_t kGlobeZones = 0x1DC6;
// The commands, and the one shown while the mouse is over the planet:
// two bytes, then (phrase, action) pairs up to a phrase of 0.
constexpr std::size_t kGlobeCommands = 0x204A;
constexpr std::size_t kPlanetCommands = 0x2062;

// The planet turns a step for each picture, and the timer draws a line of
// the next picture every tick.
constexpr int kTimer = 0xB9AE;

// Paul's mark on the planet: a sprite of the panel's picture file, its
// bottom row just above the point.
constexpr std::uint16_t kPlayerMark = 0x36;

// What the arrows do.
constexpr int kTiltStep = 8;
constexpr int kTurnStep = 0x20;
// Turning to face Paul: at most this much a picture.
constexpr int kMaxTurn = 0x20;
constexpr int kMaxTilt = 0x18;

// Zooming into the map: a frame of colour 7 grows from a point, 8 times,
// no further than the planet's area, and its inside is shown in blocks of
// 2 × 2 for 10 ticks each time.
constexpr std::uint8_t kZoomFrame = 7;
constexpr int kZoomSteps = 8;
constexpr ClipRect kZoomLimit{96, 15, 234, 134};
constexpr std::uint32_t kMosaicTicks = 10;
constexpr int kMosaicOffsets[] = {0, 321, 1, 320};

// Where the mouse finds the planet.
constexpr ClipRect kPlanetZone{96, 25, 224, 134};

bool onPlanet(int x, int y) {
    return x >= kPlanetZone.left && x < kPlanetZone.right && y >= kPlanetZone.top && y < kPlanetZone.bottom;
}

// The phrases of the commands not built yet.
constexpr std::uint16_t kSeeResults = 0xB1;

} // namespace

GlobeScreen::GlobeScreen(Game& game, World& world, Panel& panel, MapScreen& map)
    : g_(game), world_(world), panel_(panel), map_(map) {}

Globe& GlobeScreen::globe() { return *globe_; }

void GlobeScreen::playerPoint(std::uint16_t& longitude, int& latitude) const { world_.position(longitude, latitude); }

void GlobeScreen::enter(std::uint16_t longitude, int latitude) {
    active_ = true;
    leaving = false;
    overPlanet_ = false;
    // The planet as the map shows it now, with the spice fields Paul found.
    Resources& r = g_.resources();
    globe_.emplace(r.load(resource::kLatitudes), r.load(resource::kProjection), map_.mapBytes());
    globe_->setView((Globe::kTurn * longitude) >> 16, Globe::viewTilt(latitude));
    drawScreen();
}

void GlobeScreen::drawScreen() {
    Display& d = g_.display();
    d.targetBack();
    d.target().fillRect(0, d.originRows(), Surface::kScreenWidth, 152, kFill);
    g_.selectSheet(resource::kFresk);
    g_.drawSprite(kFrameSprite, kFrameX, kFrameY);
    globe_->drawAll(d.target());
    g_.drawSprite(kLeftSide, 0, 0);
    g_.drawSprite(kRightSide, kRightSideX, 0);
    panel_.drawOrnament();
    setCommands(false);
    panel_.drawCommands();
    panel_.drawZones(0, 3);
    panel_.setArrowZones(kGlobeZones);
}

void GlobeScreen::show() {
    // The new colours are sent before the old picture rises.
    g_.display().flush();
    g_.display().transition(Display::kRise, -1);
    g_.addTimer(kTimer, 1, [this] {
        if (globe_->drawLine(g_.display().back())) {
            showPlanet();
        }
    });
}

void GlobeScreen::leave() {
    active_ = false;
    leaving = false;
    g_.removeTimer(kTimer);
}

// The finished picture goes to the screen with Paul's mark over it, and the
// planet turns a step for the next one.
void GlobeScreen::showPlanet() {
    std::uint16_t lon = 0;
    int lat = 0;
    playerPoint(lon, lat);
    int x = 0;
    int y = 0;
    const bool marked = globe_->screenPoint(lon, lat, x, y);
    Display& d = g_.display();
    d.copyRect(d.back(), d.screen(), kPlanetArea);
    if (marked) {
        d.targetScreen();
        g_.selectSheet(kPanelSheet);
        const int h = g_.sheet().sprite(kPlayerMark).header.height;
        g_.drawSprite(kPlayerMark, x, (y - h) & 0xFF);
        d.targetBack();
    }
    globe_->turn(1);
}

void GlobeScreen::drawPlanet() {
    globe_->drawAll(g_.display().back());
    showPlanet();
}

void GlobeScreen::turnToPlayer() {
    for (int times = 1; times > 0; --times) {
        std::uint16_t lon = 0;
        int lat = 0;
        playerPoint(lon, lat);
        // The shorter way round.
        int turn = ((Globe::kTurn * lon) >> 16) - globe_->rotation();
        if (turn < 0) {
            if (-turn >= Globe::kTurn + turn) {
                turn += Globe::kTurn;
            }
        } else if (Globe::kTurn - turn < turn) {
            turn -= Globe::kTurn;
        }
        int tilt = lat - globe_->tilt();
        // A step that had to be cut short is followed by another.
        times = 1;
        if (turn >= kMaxTurn) {
            turn = kMaxTurn;
            ++times;
        }
        if (turn <= -kMaxTurn) {
            turn = -kMaxTurn;
            ++times;
        }
        if (tilt >= kMaxTilt) {
            tilt = kMaxTilt;
            ++times;
        }
        if (tilt <= -kMaxTilt) {
            tilt = -kMaxTilt;
            ++times;
        }
        globe_->turn(turn);
        globe_->tilt(tilt);
        drawPlanet();
    }
}

void GlobeScreen::mosaic(int left, int top, int right, int bottom, int offset) {
    Display& d = g_.display();
    const int w = Surface::kScreenWidth;
    const int origin = d.originRows() * w;
    const std::uint8_t* from = d.back().data();
    std::uint8_t* to = d.screen().data();
    const int size = static_cast<int>(d.screen().size());
    for (int row = 0; row < (bottom - top) / 2; ++row) {
        for (int col = 0; col < (right - left) / 2; ++col) {
            const int at = origin + (top + 2 * row) * w + left + 2 * col;
            if (at < 0 || at + w + 1 >= size || at + offset >= size) {
                continue;
            }
            const std::uint8_t c = from[at + offset];
            to[at] = c;
            to[at + 1] = c;
            to[at + w] = c;
            to[at + w + 1] = c;
        }
    }
}

void GlobeScreen::zoomFrom(int x, int y) {
    Display& d = g_.display();
    Platform& p = g_.platform();
    p.cursorShown = false;
    int left = x;
    int top = y;
    int right = x + 2;
    int bottom = y + 1;
    for (int step = 0; step < kZoomSteps; ++step) {
        left = std::max(left - 4, kZoomLimit.left);
        top = std::max(top - 2, kZoomLimit.top);
        right = std::min(right + 4, kZoomLimit.right);
        bottom = std::min(bottom + 2, kZoomLimit.bottom);
        Surface& s = d.screen();
        const int o = d.originRows();
        for (int i = left; i < right; ++i) {
            s.set(i, top + o, kZoomFrame);
            s.set(i, bottom - 1 + o, kZoomFrame);
        }
        for (int i = top; i < bottom; ++i) {
            s.set(left, i + o, kZoomFrame);
            s.set(right - 1, i + o, kZoomFrame);
        }
        // Within the frame, the picture in blocks, each block showing one
        // of its four pixels in turn, until the time is up.
        if ((bottom - top - 2) / 2 == 0) {
            continue;
        }
        const std::uint32_t start = p.ticks;
        for (int k = 0;; ++k) {
            mosaic(left + 1, top + 1, right - 1, bottom - 1, kMosaicOffsets[k % 4]);
            if (p.ticks - start >= kMosaicTicks) {
                break;
            }
            g_.pollInput();
            p.waitFrame();
        }
    }
}

bool GlobeScreen::command(std::uint16_t action) {
    if (!active_) {
        return false;
    }
    switch (action) {
    case kUp:
        globe_->tilt(kTiltStep);
        drawPlanet();
        return true;
    case kDown:
        globe_->tilt(-kTiltStep);
        drawPlanet();
        return true;
    case kRight:
        globe_->turn(-kTurnStep);
        drawPlanet();
        return true;
    case kLeft:
        globe_->turn(kTurnStep);
        drawPlanet();
        return true;
    case kTurnToPlayer:
        turnToPlayer();
        return true;
    case kExit:
        // The planet turns to Paul, and the map opens there.
        turnToPlayer();
        playerPoint(mapLongitude, mapLatitude);
        zoomFrom(Globe::kCentreX, Globe::kCentreY);
        leaving = true;
        return true;
    default:
        return false;
    }
}

void GlobeScreen::click(int x, int y) {
    if (!active_ || !onPlanet(x, y)) {
        return;
    }
    std::uint16_t lon = 0;
    int lat = 0;
    if (!globe_->mapPoint(x, y, lon, lat)) {
        return;
    }
    mapLongitude = lon;
    mapLatitude = lat;
    drawPlanet();
    zoomFrom(x, y);
    leaving = true;
}

bool GlobeScreen::pointAt(int x, int y) {
    if (!active_) {
        return false;
    }
    const bool over = onPlanet(x, y);
    if (over == overPlanet_) {
        return false;
    }
    setCommands(over);
    Display& d = g_.display();
    d.targetScreen();
    panel_.drawCommands();
    d.targetBack();
    panel_.drawCommands();
    return true;
}

void GlobeScreen::setCommands(bool overPlanet) {
    overPlanet_ = overPlanet;
    const ProgramFile& program = g_.resources().program();
    std::vector<Command> list;
    for (std::size_t at = (overPlanet ? kPlanetCommands : kGlobeCommands) + 2;; at += 4) {
        Command c{program.dataU16(at), program.dataU16(at + 2)};
        if (c.phrase == 0) {
            break;
        }
        list.push_back(c);
    }
    if (!overPlanet) {
        // The results, loading, saving and leaving the game: not yet.
        for (std::size_t i = 1; i < list.size(); ++i) {
            if (i == 1) {
                list[i].phrase = kSeeResults;
            }
            list[i].phrase = static_cast<std::uint16_t>(list[i].phrase | Command::kGrey);
        }
    }
    panel_.setCommands(std::move(list));
}

} // namespace mentat
