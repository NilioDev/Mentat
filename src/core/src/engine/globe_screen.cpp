// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/globe_screen.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/engine/saved_games.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/data/string_table.hpp"
#include "mentat/gfx/scene.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <algorithm>
#include <cstdlib>
#include <string_view>

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
// The lists of saved games: the same form.
constexpr std::size_t kSaveCommands = 0x207A;
constexpr std::size_t kLoadCommands = 0x208A;
// The first saved game's line; the others follow it.
constexpr std::uint16_t kFirstSlotPhrase = 0x10F;
// Only the first two lines show the day and time of their game.
constexpr int kDatedSlots = 2;
// The times of day, ten letters each.
constexpr std::uint16_t kTimesPhrase = 0x117;
constexpr int kTimeLength = 10;
// After saving: a line of its own, for a while.
constexpr std::uint16_t kSavedPhrase = 0x113;
constexpr std::uint16_t kSaveErrorPhrase = 0x114;
constexpr int kMessageLine = 4;
constexpr int kMessageTicks = 300;

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

// The second command: the results, or back from them.
constexpr std::uint16_t kSeeResultsPhrase = 0xB1;
constexpr std::uint16_t kStandardVisionPhrase = 0xB2;

constexpr ClipRect kScene{0, 0, 320, 152};

// The results. The frescoes slide out 16 pixels a step while they are
// further in than this.
constexpr int kSlideStep = 16;
constexpr int kSlideEnd = -0x6A;
constexpr std::uint32_t kSlideTicks = 4; // the original goes as fast as it draws
constexpr std::size_t kEmblems = 0x2506;    // (sprite, x, y) of FRESK.HSQ up to 0xFFFF
constexpr std::size_t kTitleTexts = 0x2482; // (phrase, x, y, colours) up to 0xFFFF
constexpr std::size_t kResultTexts = 0x2494;
constexpr std::size_t kBarPlaces = 0x24EE; // six (x, y)
constexpr std::size_t kOrdinals = 0x251A;  // per language: four pairs of letters
constexpr std::size_t kFrenchOrdinals = kOrdinals + 8;
constexpr std::uint16_t kDayPhrase = 0xC2;
constexpr std::uint16_t kCharismaPhrase = 0xC3;
constexpr std::uint16_t kFirstNumberPhrase = 0xC4;
constexpr std::uint16_t kFirstText = 0xAF;
constexpr std::uint16_t kLastText = 0xCC;
constexpr int kBarsTimer = 0xBE57;
constexpr int kBarTicks = 12;
constexpr int kBarMost = 30;
constexpr std::uint16_t kBar = 0x37; // and 0x38 for the second of a pair
constexpr std::uint16_t kBarTop = 0x39;
constexpr std::uint8_t kBarColours[2] = {0x3F, 0x25};

// In the state.
constexpr std::size_t kCharisma = 0x29;
constexpr std::size_t kSpiceTotal = 0xA0;
constexpr std::size_t kHeldOther = 0xA2; // the areas held, in per cent, plus one
constexpr std::size_t kHeldFull = 0xA4;
constexpr std::size_t kSpiceBest = 0xA6;
constexpr std::size_t kSpiceProduction = 0xA8;
constexpr std::size_t kMenOurs = 0xAA;
constexpr std::size_t kMenTheirs = 0xAC;
constexpr std::size_t kResultsDay = 0x115C;
constexpr std::size_t kLastResults = 0x115E; // six words
constexpr std::size_t kResultArrows = 0x116A; // six bytes: 1 up, 2 down, 3 the same
constexpr std::size_t kSpiceIn = 0x1170;
constexpr std::size_t kSpiceOut = 0x1172;
constexpr std::size_t kMapCounted = 0xC5F9; // map bytes counted for the areas
constexpr int kMapCountedLess = 0x187;     // the share is out of the count less this

} // namespace

GlobeScreen::GlobeScreen(Game& game, World& world, Panel& panel, MapScreen& map, int language, SavedGames* saves)
    : g_(game), world_(world), panel_(panel), map_(map), language_(language), saves_(saves) {}

Globe& GlobeScreen::globe() { return *globe_; }

void GlobeScreen::playerPoint(std::uint16_t& longitude, int& latitude) const { world_.position(longitude, latitude); }

void GlobeScreen::enter(std::uint16_t longitude, int latitude) {
    active_ = true;
    leaving = false;
    loaded = false;
    list_ = List::Globe;
    overPlanet_ = false;
    results_ = false;
    sideShift_ = 0;
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
    drawSides(0);
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
    g_.removeTimer(kBarsTimer);
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

bool GlobeScreen::command(std::uint16_t action, std::uint16_t phrase) {
    if (!active_) {
        return false;
    }
    const int slot = (phrase & Command::kPhrase) - kFirstSlotPhrase + 1;
    switch (action) {
    case kSave:
        openList(List::Save);
        return true;
    case kLoad:
        openList(List::Load);
        return true;
    case kCancel:
        openList(List::Globe);
        return true;
    case kSaveSlot:
        if (list_ == List::Save && slot >= 1) {
            int line = 0;
            const auto& lines = panel_.commands();
            while (static_cast<std::size_t>(line) < lines.size() &&
                   (lines[static_cast<std::size_t>(line)].phrase & Command::kPhrase) != (phrase & Command::kPhrase)) {
                ++line;
            }
            saveIn(slot, line);
        }
        return true;
    case kLoadSlot:
        if (list_ == List::Load && slot >= 1) {
            loadFrom(slot);
        }
        return true;
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
    case kSeeResults:
        if (!results_) {
            showResults();
        }
        return true;
    case kStandardVision:
        if (results_) {
            standardVision();
        }
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
    if (over == overPlanet_ || list_ != List::Globe) {
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
    std::size_t table = overPlanet ? kPlanetCommands : kGlobeCommands;
    if (list_ == List::Save) {
        table = kSaveCommands;
    } else if (list_ == List::Load) {
        table = kLoadCommands;
    }
    if (list_ != List::Globe) {
        overPlanet = false;
    }
    for (std::size_t at = table + 2;; at += 4) {
        Command c{program.dataU16(at), program.dataU16(at + 2)};
        if (c.phrase == 0) {
            break;
        }
        list.push_back(c);
    }
    if (list_ != List::Globe) {
        // A line for each saved game: those there are lit for saving, those
        // missing greyed for loading, and the first two show their day.
        for (Command& c : list) {
            if (c.action != kSaveSlot && c.action != kLoadSlot) {
                continue;
            }
            const int slot = (c.phrase & Command::kPhrase) - kFirstSlotPhrase + 1;
            const auto clock = saves_ != nullptr ? saves_->clockOf(slot) : std::nullopt;
            if (clock) {
                writeSlot(slot, *clock);
            }
            if (list_ == List::Save && clock) {
                c.phrase = static_cast<std::uint16_t>(c.phrase | Command::kLit);
            }
            if (list_ == List::Load && !clock) {
                c.phrase = static_cast<std::uint16_t>(c.phrase | Command::kGrey);
            }
        }
    } else if (!overPlanet) {
        // The second line shows the results, or goes back from them.
        if (list.size() > 1) {
            list[1] = results_ ? Command{kStandardVisionPhrase, kStandardVision} : Command{kSeeResultsPhrase, kSeeResults};
        }
        // Loading and saving need somewhere to keep the games; leaving the
        // game is not there yet.
        const bool canSave = saves_ != nullptr && saves_->available();
        for (std::size_t i = 2; i < list.size(); ++i) {
            if (!canSave || (list[i].action != kLoad && list[i].action != kSave)) {
                list[i].phrase = static_cast<std::uint16_t>(list[i].phrase | Command::kGrey);
            }
        }
    }
    panel_.setCommands(std::move(list));
}

namespace {

// Writes v (at most 999) into the three bytes before `end`: leading 0s
// become spaces, but the last digit always shows.
void writeThree(std::string& s, std::size_t end, int v, bool keep = false) {
    v = std::min(v, 999);
    char hundreds = static_cast<char>('0' + v / 100);
    if (hundreds == '0' && !keep) {
        hundreds = ' ';
    } else {
        keep = true;
    }
    char tens = static_cast<char>('0' + (v / 10) % 10);
    if (!keep && tens == '0') {
        tens = ' ';
    }
    s[end - 3] = hundreds;
    s[end - 2] = tens;
    s[end - 1] = static_cast<char>('0' + v % 10);
}

// The same with five digits.
void writeFive(std::string& s, std::size_t end, int v) {
    const int thousands = v / 1000;
    char high = static_cast<char>('0' + thousands / 10);
    char low = static_cast<char>('0' + thousands % 10);
    bool keep = true;
    if (high == '0') {
        high = ' ';
        if (low == '0') {
            low = ' ';
            keep = false;
        }
    }
    s[end - 5] = high;
    s[end - 4] = low;
    writeThree(s, end, v % 1000, keep);
}

} // namespace

// --- Loading and saving

void GlobeScreen::drawList() {
    Display& d = g_.display();
    d.targetScreen();
    panel_.drawCommands();
    d.targetBack();
    panel_.drawCommands();
}

void GlobeScreen::showLoadList() { openList(List::Load); }

bool GlobeScreen::loadSlot(std::uint16_t phrase) {
    const int slot = (phrase & Command::kPhrase) - kFirstSlotPhrase + 1;
    if (saves_ == nullptr || slot < 1 || !saves_->load(slot)) {
        return false;
    }
    list_ = List::Globe;
    return true;
}

void GlobeScreen::openList(List list) {
    list_ = list;
    setCommands(false);
    drawList();
}

// Writes the day and time of the game in `slot` into its line, as the
// original does: the day over the second number, the time three letters on.
void GlobeScreen::writeSlot(int slot, std::uint16_t clock) {
    if (slot > kDatedSlots) {
        return;
    }
    std::string& line = panel_.editPhrase(static_cast<std::uint16_t>(kFirstSlotPhrase + slot - 1));
    const std::string& times = panel_.phrases().at(kTimesPhrase - 1u);
    auto digit = [&](std::size_t i) { return i < line.size() && line[i] >= '0' && line[i] <= '9'; };
    std::size_t end = 0;
    for (int run = 0; run < 2; ++run) {
        while (end < line.size() && !digit(end)) {
            ++end;
        }
        while (digit(end)) {
            ++end;
        }
    }
    if (end < 3 || end > line.size()) {
        return;
    }
    writeThree(line, end, ((clock + 3) >> 4) + 1);
    const std::size_t from = static_cast<std::size_t>(clock & 0x0F) * kTimeLength;
    for (std::size_t i = 0; i < static_cast<std::size_t>(kTimeLength); ++i) {
        if (end + 3 + i < line.size() && from + i < times.size()) {
            line[end + 3 + i] = times[from + i];
        }
    }
}

void GlobeScreen::saveIn(int slot, int line) {
    if (saves_ == nullptr) {
        return;
    }
    // The line shows the game as it is now, then a word on how it went.
    writeSlot(slot, static_cast<std::uint16_t>(world_.clock()));
    Display& d = g_.display();
    d.targetScreen();
    panel_.drawLine(line, static_cast<std::uint16_t>(kFirstSlotPhrase + slot - 1));
    const bool saved = saves_->save(slot);
    panel_.drawLine(kMessageLine, saved ? kSavedPhrase : kSaveErrorPhrase);
    d.targetBack();
    g_.waitTicks(kMessageTicks);
    if (saved) {
        openList(List::Globe);
    } else {
        drawList();
    }
}

void GlobeScreen::loadFrom(int slot) {
    if (saves_ == nullptr || !saves_->load(slot)) {
        openList(List::Load);
        return;
    }
    loaded = true;
    list_ = List::Globe;
    g_.removeTimer(kBarsTimer);
    globe_->setMap(map_.mapBytes());
    // The globe shows the results of the game loaded, turned to Paul.
    if (results_) {
        standardVision();
    }
    showResults();
    turnToPlayer();
}

// --- The results

void GlobeScreen::drawSides(int shift) {
    g_.selectSheet(resource::kFresk);
    const Canvas to = g_.canvas();
    drawSpriteClipped(to, g_.sheet().sprite(kLeftSide), shift, 0, kScene);
    drawSpriteClipped(to, g_.sheet().sprite(kRightSide), kRightSideX - shift, 0, kScene);
}

void GlobeScreen::slideStep() {
    // The original goes as fast as it can draw, a few ticks a step, and
    // its timers wait until the frescoes have stopped.
    Platform& p = g_.platform();
    const std::uint32_t start = p.ticks;
    while (p.ticks - start < kSlideTicks) {
        p.waitFrame();
    }
}

void GlobeScreen::showResults() {
    Display& d = g_.display();
    const ProgramFile& program = g_.resources().program();
    // The picture without the results, then the emblems of the two houses,
    // the numbers and the first step of the bars, all on the back buffer.
    d.targetBack();
    d.target().fillRect(0, d.originRows(), Surface::kScreenWidth, 152, kFill);
    g_.selectSheet(resource::kFresk);
    g_.drawSprite(kFrameSprite, kFrameX, kFrameY);
    globe_->drawAll(d.target());
    g_.selectSheet(resource::kFresk);
    for (std::size_t at = kEmblems; program.dataU16(at) != 0xFFFF; at += 6) {
        g_.drawSprite(program.dataU16(at), program.dataU16(at + 2), program.dataU16(at + 4));
    }
    countResults();
    writeResults();
    drawTexts(kTitleTexts, true);
    barNow_.fill(0);
    drawTexts(kResultTexts, false);
    // The bars grow on the screen, but their first step comes at once, on
    // the picture.
    g_.addTimer(kBarsTimer, kBarTicks, [this] {
        Display& screen = g_.display();
        screen.targetScreen();
        growBars();
        screen.targetBack();
    });
    growBars();
    // The frescoes slide out to the sides.
    const Surface scene = d.back();
    for (sideShift_ = 0; sideShift_ > kSlideEnd; sideShift_ -= kSlideStep) {
        d.copyRect(scene, d.back(), kScene);
        d.targetBack();
        drawSides(sideShift_);
        panel_.drawOrnament();
        d.copyRect(d.back(), d.screen(), kScene);
        slideStep();
    }
    // The planet shows who holds each area.
    results_ = true;
    globe_->showHolders(true);
    drawPlanet();
    setCommands(overPlanet_);
    d.targetScreen();
    panel_.drawCommands();
    d.targetBack();
    panel_.drawCommands();
}

void GlobeScreen::standardVision() {
    Display& d = g_.display();
    g_.removeTimer(kBarsTimer);
    // The frescoes slide back in over the results, on the screen.
    d.targetScreen();
    while (sideShift_ < 0) {
        sideShift_ += kSlideStep;
        drawSides(sideShift_);
        panel_.drawOrnament();
        slideStep();
    }
    d.targetBack();
    drawSides(sideShift_);
    panel_.drawOrnament();
    results_ = false;
    globe_->showHolders(false);
    drawPlanet();
    setCommands(overPlanet_);
    d.targetScreen();
    panel_.drawCommands();
    d.targetBack();
    panel_.drawCommands();
}

namespace {

// A share of the planet in per cent, rounded: count out of `of`, as the
// original works it out in sixteenths of sixteen bits.
int share(int count, int of) {
    if (of <= 0 || count >= of) {
        return 100;
    }
    const std::uint32_t part = (static_cast<std::uint32_t>(count) << 16) / static_cast<std::uint32_t>(of);
    return static_cast<int>((part * 100 + 0x8000) >> 16);
}

} // namespace

void GlobeScreen::countResults() {
    // The areas: map points held outright, and those only partly held.
    const std::vector<std::uint8_t>& map = map_.mapBytes();
    const std::size_t n = std::min(map.size(), kMapCounted);
    int full = 0;
    int other = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const int held = map[i] & 0x30;
        if (held == 0x30) {
            ++full;
        } else if (held != 0) {
            ++other;
        }
    }
    const int of = static_cast<int>(kMapCounted) - kMapCountedLess;
    world_.setU16(kHeldFull, static_cast<std::uint16_t>(share(full, of)));
    world_.setU16(kHeldOther, static_cast<std::uint16_t>(share(other, of) + 1));
    // The best spice stock so far.
    const int stock = std::max(0, world_.u16(kSpiceTotal) + world_.u16(kSpiceOut) - world_.u16(kSpiceIn));
    if (stock >= world_.u16(kSpiceBest)) {
        world_.setU16(kSpiceBest, static_cast<std::uint16_t>(stock));
    }
    // The men of the troops: working for Paul, or for the enemy.
    int ours = 0;
    int theirs = 0;
    for (std::size_t at = World::kTroops;; at += World::kTroopSize) {
        const int men = world_.u8(at + 0x1A);
        if ((world_.u8(at + 3) & 0x20) == 0) {
            if ((world_.u8(at + 0x10) & 0x80) != 0) {
                theirs += men;
            } else if ((world_.u8(at + 3) & 0x80) == 0) {
                ours += men;
            }
        }
        if (world_.u8(at + World::kTroopSize) == 0) {
            break;
        }
    }
    world_.setU16(kMenOurs, static_cast<std::uint16_t>(ours));
    world_.setU16(kMenTheirs, static_cast<std::uint16_t>(theirs));
}


std::size_t GlobeScreen::afterNumber(std::size_t at) const {
    auto digit = [&](std::size_t i) { return i < texts_.size() && texts_[i] >= '0' && texts_[i] <= '9'; };
    while (at < texts_.size() && !digit(at)) {
        ++at;
    }
    while (digit(at)) {
        ++at;
    }
    return at;
}

void GlobeScreen::loadTexts() {
    if (!texts_.empty()) {
        return;
    }
    // The phrases lie one after the other, each ending in 0xFF, as in the
    // original, so that a search for a number can run on into the next.
    const StringTable& phrases = panel_.phrases();
    for (std::uint16_t p = kFirstText; p <= kLastText; ++p) {
        textStart_.push_back(texts_.size());
        texts_ += phrases.at(p - 1u);
        texts_ += static_cast<char>(StringTable::kEnd);
    }
}

void GlobeScreen::loadFonts() {
    if (!fonts_) {
        const std::vector<std::uint8_t> bytes = g_.resources().load(resource::kFont);
        fonts_.emplace(Font::parse(bytes, Font::Size::Large), Font::parse(bytes, Font::Size::Small));
    }
}

std::size_t GlobeScreen::textAt(std::uint16_t phrase) const { return textStart_[phrase - kFirstText]; }

void GlobeScreen::writeResults() {
    loadTexts();
    // The day, with its ending from the language's four.
    const ProgramFile& program = g_.resources().program();
    std::size_t end = afterNumber(textAt(kDayPhrase));
    if (end >= 3 && end + 2 <= texts_.size()) {
        writeThree(texts_, end, ((world_.clock() + 3) >> 4) + 1);
        const std::size_t ordinals = kOrdinals + 8 * static_cast<std::size_t>(language_);
        const char tens = texts_[end - 2];
        const int units = texts_[end - 1] & 0x0F;
        std::size_t pick = ordinals;
        if (tens != '1' && units < 4 && !(units == 1 && ordinals == kFrenchOrdinals && tens != ' ')) {
            pick += 2 * static_cast<std::size_t>(units);
        }
        texts_[end] = static_cast<char>(program.dataU8(pick));
        texts_[end + 1] = static_cast<char>(program.dataU8(pick + 1));
    }
    // Charisma.
    end = afterNumber(textAt(kCharismaPhrase));
    if (end >= 3) {
        writeThree(texts_, end, world_.u8(kCharisma) >> 1);
    }
    // How far each bar grows.
    barTarget_ = {(world_.u16(kHeldFull) >> 1) + 1,  (world_.u16(kHeldOther) >> 1) + 1,
                  (world_.u16(kSpiceProduction) >> 4) + 1, (world_.u16(kSpiceBest) >> 4) + 1,
                  (world_.u16(kMenTheirs) >> 8) + 1,      (world_.u16(kMenOurs) >> 8) + 1};
    for (int& t : barTarget_) {
        t &= 0xFF; // kept in a byte
    }
    // The six results, each with an arrow for how it went since the last day
    // they were looked at.
    world_.setU16(kResultsDay, static_cast<std::uint16_t>(world_.clock() & 0xFFF0));
    const std::size_t order[6] = {kHeldFull, kHeldOther, kSpiceProduction, kSpiceBest, kMenTheirs, kMenOurs};
    std::size_t at = textAt(kFirstNumberPhrase);
    for (int i = 0; i < 6; ++i) {
        const int v = world_.u16(order[i]);
        at = afterNumber(at);
        if (at >= texts_.size()) {
            break;
        }
        if (i < 2) {
            if (at >= 3) {
                writeThree(texts_, at, v);
            }
        } else if (at >= 6) {
            // Tens of what is counted: the last digit stays 0.
            texts_[at - 1] = '0';
            writeFive(texts_, at - 1, v);
        }
        const std::size_t last = kLastResults + 2 * static_cast<std::size_t>(i);
        const int before = world_.u16(last);
        world_.setU16(last, static_cast<std::uint16_t>(v));
        if (v != before) {
            world_.setU8(kResultArrows + static_cast<std::size_t>(i), v > before ? 1 : 2);
        }
    }
}

void GlobeScreen::drawTexts(std::size_t list, bool large) {
    loadTexts();
    loadFonts();
    const Font& font = large ? fonts_->first : fonts_->second;
    const int lineStep = large ? 10 : 7;
    const ProgramFile& program = g_.resources().program();
    Display& d = g_.display();
    for (std::size_t at = list; program.dataU16(at) != 0xFFFF; at += 8) {
        const std::uint16_t phrase = program.dataU16(at);
        const int left = program.dataU16(at + 2);
        int x = left;
        int y = program.dataU16(at + 4) + d.originRows();
        const std::uint16_t colours = program.dataU16(at + 6);
        if (phrase < kFirstText || phrase > kLastText) {
            continue;
        }
        for (std::size_t i = textAt(phrase); i < texts_.size(); ++i) {
            const auto c = static_cast<std::uint8_t>(texts_[i]);
            if (c == StringTable::kEnd) {
                break;
            }
            if (c == 0x0D) {
                x = left;
                y += lineStep;
                continue;
            }
            const char ch = static_cast<char>(c);
            x = font.draw(d.target(), x, y, std::string_view(&ch, 1), static_cast<std::uint8_t>(colours),
                          static_cast<std::uint8_t>(colours >> 8));
        }
    }
}

void GlobeScreen::growBars() {
    const ProgramFile& program = g_.resources().program();
    Display& d = g_.display();
    g_.selectSheet(kPanelSheet);
    for (int i = 0; i < 6; ++i) {
        const auto k = static_cast<std::size_t>(i);
        if (barNow_[k] == barTarget_[k]) {
            continue;
        }
        barNow_[k] += barNow_[k] < barTarget_[k] ? 1 : -1;
        const int h = std::min(barNow_[k], kBarMost);
        const int x = program.dataU16(kBarPlaces + 4 * k);
        const int y = program.dataU16(kBarPlaces + 4 * k + 2) - h;
        g_.drawSprite(static_cast<std::uint16_t>(kBar + (i & 1)), x, y);
        g_.drawSprite(kBarTop, x, y - 10);
        if (barNow_[k] == barTarget_[k]) {
            // The arrow for how it went.
            loadFonts();
            const char arrow = static_cast<char>(world_.u8(kResultArrows + k));
            fonts_->second.draw(d.target(), x + 4, y - 10 + d.originRows(), std::string_view(&arrow, 1), kBarColours[i & 1]);
        }
    }
}

} // namespace mentat
