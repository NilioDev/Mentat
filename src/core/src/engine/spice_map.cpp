// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/spice_map.hpp"

#include "mentat/data/program_tables.hpp"
#include "mentat/data/string_table.hpp"
#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/font.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace mentat {

namespace {

constexpr int kPanelSheet = 0;

// Where the window stands: the state keeps its top-left corner.
constexpr std::size_t kWindowX = 0x11C1;
constexpr std::size_t kWindowY = 0x11C3;
// The map inside the frame.
constexpr int kViewX = 5;
constexpr int kViewY = 7;
constexpr int kViewWidth = 0xA0;
constexpr int kViewHeight = 0x59;
// The frame: sprite 0x8D of the map's picture file.
constexpr std::uint16_t kFrame = 0x8D;
// The close box: this far into the top strip.
constexpr int kCloseWidth = 10;

// The colours of the regions.
constexpr std::uint8_t kUnknown = 0x70;
constexpr std::uint8_t kNoSpiceKnown = 0x75;
constexpr std::uint8_t kFirstDensity = 0x50;
constexpr std::uint8_t kFirstWork = 0x71;
constexpr std::uint8_t kPlaceHidden = 0x80; // place byte 0x0A
constexpr std::uint8_t kSpiceFound = 0x40;  // place byte 0x0A

// The bar at the bottom.
constexpr int kBarX = 6;
constexpr int kBarY = 0x62;
constexpr int kBarRight = 6;  // in from the frame's right
constexpr int kBarBottom = 2; // in from the frame's bottom
constexpr int kBarStrip = 10; // the frame's last rows: the bar
constexpr std::uint8_t kBarFill = 0xF5;
constexpr std::uint8_t kBarText = 0xFE;
constexpr std::uint8_t kBarCount = 0xFB;
constexpr std::uint16_t kSpiceLegend = 0x65;
constexpr std::uint16_t kWorkLegend = 0x68;
constexpr std::uint16_t kNoTroops = 0x66;
constexpr int kMinusX = 0x53; // from the bar's left
constexpr int kPlusX = 0x41;  // from the end of the minus
constexpr std::uint16_t kScaleLeft = 0x80;
constexpr std::uint16_t kScaleRight = 0x81;
constexpr int kScaleX = 0x5F;
constexpr int kScaleY = 0x63;
constexpr int kScaleStep = 0x3C;
// The mark on the scale: a 5 × 7 outline, colours flipped by 0x0F.
constexpr int kDensityX = 0x5E;
constexpr int kDensityStep = 4;
constexpr std::uint8_t kDensityFlip = 0x0F;
// What a place is, then its name; then the troops there.
constexpr std::uint16_t kKindPhrase = 0x44;
constexpr std::uint16_t kSecondWordPhrase = 0x0C;
constexpr int kHyphenFrom = 3;
constexpr int kTroopsX = 0x71;
constexpr std::uint16_t kWorkSprite = 0x82;
constexpr int kCountX = 6;
constexpr int kCountStep = 0x12;
constexpr std::size_t kFirstCountedPlace = 0x138; // the two palaces have no counts

// The dotted frame of what the main map shows.
constexpr int kMainHalfWidth = 0x28;
constexpr int kMainHalfHeight = 0x14;
constexpr std::uint8_t kMainFrame = 0xFB;

// Marks: Paul (0x4C of the panel's file, 13 left of the point and its
// bottom at it) or the troop given orders (0x36, its bottom at its point).
constexpr std::uint16_t kPlayerMark = 0x4C;
constexpr int kPlayerMarkLeft = 13;
constexpr std::uint16_t kTroopMark = 0x36;

// The troop's way: dotted, in colour 0x0C; a line this wide or wider
// goes round the planet, 400 pixels.
constexpr std::uint8_t kRouteColour = 0x0C;
constexpr std::uint16_t kRoutePattern = 0x5555;
constexpr int kWrapAcross = 0x50;
constexpr int kRingAcross = 0x190;

// The end of the troops' records.
constexpr std::size_t kTroopsEnd = 0xFBB;

// A place is pointed at within this distance (across plus down).
constexpr int kReach = 9;

std::int16_t s16(std::uint16_t v) { return static_cast<std::int16_t>(v); }

} // namespace

SpiceMap::SpiceMap(Game& game, World& world, Panel& panel, MapScreen& map)
    : g_(game), world_(world), panel_(panel), map_(map), small_(game, world, map) {}

ClipRect SpiceMap::view() const {
    return ClipRect{left_ + kViewX, top_ + kViewY, left_ + kViewX + kViewWidth, top_ + kViewY + kViewHeight};
}

void SpiceMap::open() { openAt(0, s16(world_.u16(kWindowX)), s16(world_.u16(kWindowY))); }

void SpiceMap::openFor(int troop, int left, int top) { openAt(troop, left, top); }

void SpiceMap::openAt(int troop, int left, int top) {
    troop_ = troop;
    left_ = left;
    top_ = top;
    small_.longitude = map_.longitude();
    small_.latitude = map_.latitude();
    shown_ = true;
    draw();
}

void SpiceMap::close() {
    if (!shown_) {
        return;
    }
    shown_ = false;
    pointed_ = 0;
    Display& d = g_.display();
    d.copyRect(d.back(), d.screen(), frame());
}

void SpiceMap::redraw() {
    if (shown_) {
        draw();
    }
}

void SpiceMap::scroll(int longitude, int latitude) {
    small_.longitude = static_cast<std::uint16_t>(small_.longitude + longitude);
    small_.latitude += latitude;
    redraw();
}

void SpiceMap::draw() {
    Display& d = g_.display();
    const bool wasBack = &d.target() == &d.back();
    d.targetScreen();
    const int previous = g_.sheetResource();
    g_.selectSheet(resource::kMapSprites);
    drawSpriteClipped(g_.canvas(), g_.sheet().sprite(kFrame), left_, top_, ClipRect{0, 0, 320, 200});
    small_.view = view();
    drawRegions();
    g_.selectSheet(kPanelSheet);
    small_.drawPlaces();
    pointed_ = 0;
    density_ = -1;
    legendLit_ = false;
    drawBar();
    drawScale();
    drawMainView();
    drawMarker();
    drawRoute();
    // The panel's ornament stays in front.
    const ClipRect f = frame();
    const ClipRect m = map_.view();
    map_.drawHead(ClipRect{std::max(f.left, m.left), std::max(f.top, m.top), std::min(f.right, m.right),
                           std::min(f.bottom, m.bottom)});
    g_.selectSheet(previous);
    if (wasBack) {
        d.targetBack();
    }
}

// --- The regions

// The colour of each region, by its number.
std::vector<std::uint8_t> SpiceMap::colours() const {
    std::vector<std::uint8_t> table(256, kUnknown);
    for (std::size_t record = World::kPlaces; world_.u16(record) != 0xFFFF; record += World::kPlaceSize) {
        const std::uint8_t flags = world_.u8(record + 0x0A);
        if (flags & kPlaceHidden) {
            continue;
        }
        const int region = world_.u8(record + 0x10);
        std::uint8_t c = 0;
        if (occupations) {
            // Two bits for each of four rows in turn, by which kinds of
            // work Paul's troops there do and which do most.
            int spice = 0;
            int army = 0;
            int ecology = 0;
            counts(record, spice, army, ecology);
            const int kinds = (spice ? 1 : 0) | (army ? 2 : 0) | (ecology ? 4 : 0);
            switch (kinds) {
            case 1:
                c = 0x55;
                break;
            case 2:
                c = 0xAA;
                break;
            case 3:
                c = spice == army ? 0x66 : spice < army ? 0x9A : 0x65;
                break;
            case 4:
                c = 0xFF;
                break;
            case 5:
                c = spice == ecology ? 0x77 : spice < ecology ? 0xDF : 0x75;
                break;
            case 6:
                c = army == ecology ? 0xBB : army < ecology ? 0xEF : 0xBA;
                break;
            case 7:
                if (spice < army) {
                    c = army < ecology ? 0xDE : 0x9B;
                } else {
                    c = spice < ecology ? 0x7B : 0x67;
                }
                break;
            default:
                c = 0;
                break;
            }
        } else if (flags & kSpiceFound) {
            c = static_cast<std::uint8_t>(kFirstDensity + (world_.u8(record + 0x12) >> 4));
        } else {
            c = kNoSpiceKnown;
        }
        table[static_cast<std::size_t>(region)] = c;
    }
    return table;
}

// Paul's troops at a place or going there, by kind of work: spice
// (occupations 0 to 3), army (4 to 7) and ecology (the rest); those
// waiting for orders are left out.
void SpiceMap::counts(std::size_t record, int& spice, int& army, int& ecology) const {
    spice = 0;
    army = 0;
    ecology = 0;
    auto count = [&](std::size_t t) {
        const int job = world_.u8(t + 3);
        if (job >= 0x80) {
            return;
        }
        const int occupation = job & 0x0F;
        if (occupation == 2) {
            return;
        }
        switch (occupation >> 2) {
        case 0:
            ++spice;
            break;
        case 1:
            ++army;
            break;
        default:
            ++ecology;
            break;
        }
    };
    for (int troop = world_.u8(record + 9); troop != 0;) {
        const std::size_t t = World::troopRecord(troop);
        count(t);
        troop = world_.u8(t + 1);
    }
    for (std::size_t t = World::kTroops; t < kTroopsEnd; t += World::kTroopSize) {
        const int job = world_.u8(t + 3);
        if (!(job & 0x40)) {
            continue;
        }
        const std::size_t to = (job & 3) == 3 ? world_.u16(t + 0x0C) : world_.u16(t + 4);
        if (to == record) {
            count(t);
        }
    }
}

void SpiceMap::drawRegions() {
    FlatMap& m = map_.flatMap();
    const std::vector<std::uint8_t> regions = g_.resources().load(resource::kRegions);
    std::vector<std::uint8_t> work;
    small_.latitude = m.gatherSmall(regions, kViewWidth, kViewHeight, small_.longitude, small_.latitude, work);
    // What lies below the last row is not kept; it is taken as that row.
    const int stride = FlatMap::smallStride();
    std::copy_n(work.begin() + stride * (kViewHeight - 1), stride, work.begin() + stride * kViewHeight);
    std::vector<std::uint8_t> table = colours();
    const ClipRect v = view();
    const int first = small_.latitude - (kViewHeight - 1) / 2;
    // A point takes its region's colour when the point right of it and the
    // one below are of the same region; otherwise it is a border.
    auto same = [&](const std::uint8_t* p, bool last) { return (last || p[0] == p[1]) && p[0] == p[stride]; };
    if (!occupations) {
        m.paintSmall(g_.display().target(), v.left, v.top, kViewWidth, kViewHeight, first, work, 1,
                     [&](int, const std::uint8_t* p, bool last) { return same(p, last) ? table[*p] : kUnknown; });
        return;
    }
    // By rows: each row turns every pattern two bits on.
    int turned = 0;
    m.paintSmall(g_.display().target(), v.left, v.top, kViewWidth, kViewHeight, first, work, 1,
                 [&](int row, const std::uint8_t* p, bool last) {
                     while (turned < row) {
                         for (std::uint8_t& c : table) {
                             if (c != kUnknown) {
                                 c = static_cast<std::uint8_t>((c << 2) | (c >> 6));
                             }
                         }
                         ++turned;
                     }
                     if (!same(p, last)) {
                         return kUnknown;
                     }
                     const std::uint8_t c = table[*p];
                     return c == kUnknown ? kUnknown : static_cast<std::uint8_t>(kFirstWork + (c & 3));
                 });
}

// --- The bar

void SpiceMap::drawBar() {
    Surface& to = g_.display().target();
    const int x = left_ + kBarX;
    const int y = top_ + kBarY;
    to.fillRect(x, y, kWidth - kBarRight - kBarX, kHeight - kBarBottom - kBarY, kBarFill);
}

void SpiceMap::drawLegend(bool lit) {
    legendLit_ = lit;
    Surface& to = g_.display().target();
    const Font font = Font::parse(g_.resources().load(g_.fontFile()), Font::Size::Small);
    const std::uint8_t ink = lit ? kBarFill : kBarText;
    const std::uint8_t paper = lit ? kBarText : kBarFill;
    const StringTable& phrases = panel_.phrases();
    font.draw(to, left_ + kBarX, top_ + kBarY, phrases.at((occupations ? kWorkLegend : kSpiceLegend) - 1u), ink,
              paper);
}

// The legend with, for the spice, its scale from - to +.
void SpiceMap::drawScale() {
    drawLegend(false);
    if (occupations) {
        return;
    }
    Surface& to = g_.display().target();
    const Font font = Font::parse(g_.resources().load(g_.fontFile()), Font::Size::Small);
    const int x = left_ + kBarX;
    const int y = top_ + kBarY;
    const int after = font.draw(to, x + kMinusX, y, "-", kBarText, kBarFill);
    font.draw(to, after + kPlusX, y, "+", kBarText, kBarFill);
    const int previous = g_.sheetResource();
    g_.selectSheet(resource::kMapSprites);
    g_.drawSprite(kScaleLeft, left_ + kScaleX, top_ + kScaleY);
    g_.drawSprite(kScaleRight, left_ + kScaleX + kScaleStep, top_ + kScaleY);
    g_.selectSheet(previous);
}

void SpiceMap::drawPlaceInfo(std::size_t record) {
    Surface& to = g_.display().target();
    const Font font = Font::parse(g_.resources().load(g_.fontFile()), Font::Size::Small);
    const StringTable& phrases = panel_.phrases();
    auto phrase = [&](int n) -> const std::string& { return phrases.at(static_cast<std::size_t>(n) - 1u); };
    const int kind = world_.u8(record + 8);
    int what = 0;
    if (kind >= 0x20) {
        what = kind < 0x21 ? 1 : kind < 0x28 ? 2 : kind < 0x30 ? 3 : 1;
    }
    const int y = top_ + kBarY;
    int x = font.draw(to, left_ + kBarX, y, phrase(kKindPhrase + what), kBarText, kBarFill);
    x = font.draw(to, x, y, phrase(world_.u8(record)), kBarText, kBarFill);
    const int second = world_.u8(record + 1);
    x = font.draw(to, x, y, second >= kHyphenFrom ? "-" : " ", kBarText, kBarFill);
    x = font.draw(to, x, y, phrase(kSecondWordPhrase + second), kBarText, kBarFill);
    if (record < kFirstCountedPlace || kind == 0x21) {
        return;
    }
    int spice = 0;
    int army = 0;
    int ecology = 0;
    counts(record, spice, army, ecology);
    x = left_ + kTroopsX;
    if (spice + army + ecology == 0) {
        font.draw(to, x, y, phrase(kNoTroops), kBarCount, kBarFill);
        return;
    }
    const int previous = g_.sheetResource();
    g_.selectSheet(resource::kMapSprites);
    const int n[3] = {spice, army, ecology};
    for (int k = 0; k < 3; ++k) {
        if (n[k] == 0) {
            continue;
        }
        g_.drawSprite(static_cast<std::uint16_t>(kWorkSprite + k), x, y);
        const char text[2] = {':', static_cast<char>('0' + n[k])};
        font.draw(to, x + kCountX, y, std::string_view(text, 2), kBarCount, kBarFill);
        x += kCountStep;
    }
    g_.selectSheet(previous);
}

// Flips the outline that marks spice level `index` on the scale.
void SpiceMap::markDensity(int index) {
    if (index < 0) {
        return;
    }
    Surface& to = g_.display().screen();
    const int x0 = left_ + kDensityX + kDensityStep * index;
    const int y0 = top_ + kBarY;
    auto flip = [&](int x, int y) {
        if (x >= 4 && x <= 0x13C && y >= 4 && y <= 0x94) {
            to.set(x, y, static_cast<std::uint8_t>(to.at(x, y) ^ kDensityFlip));
        }
    };
    for (int x = 0; x < 5; ++x) {
        flip(x0 + x, y0);
        flip(x0 + x, y0 + 6);
    }
    for (int y = 1; y < 6; ++y) {
        flip(x0, y0 + y);
        flip(x0 + 4, y0 + y);
    }
}

// --- What else the window shows

// A line of dots: every other pixel from the line's top or left end.
namespace {
void dottedLine(Surface& to, int x0, int y0, int x1, int y1, std::uint8_t colour, const ClipRect& clip) {
    const bool across = y0 == y1;
    int a = across ? std::min(x0, x1) : std::min(y0, y1);
    const int b = across ? std::max(x0, x1) : std::max(y0, y1);
    for (int k = 0; a <= b; ++a, ++k) {
        if (!(k & 1)) {
            continue;
        }
        const int x = across ? a : x0;
        const int y = across ? y0 : a;
        if (x >= clip.left && x < clip.right && y >= clip.top && y < clip.bottom) {
            to.set(x, y, colour);
        }
    }
}

// A line as the original draws it, through a pattern of 16 bits turned
// one to the left for each pixel: a pixel is drawn when the bit that
// comes round is set. Across or down it goes from the lower end; slanted
// it starts at (x0, y0), which is not drawn.
void patternLine(Surface& to, int x0, int y0, int x1, int y1, std::uint8_t colour, std::uint16_t pattern,
                 const ClipRect& clip) {
    auto next = [&pattern] {
        const bool on = (pattern & 0x8000) != 0;
        pattern = static_cast<std::uint16_t>((pattern << 1) | (on ? 1 : 0));
        return on;
    };
    auto inClip = [&](int x, int y) { return x >= clip.left && x < clip.right && y >= clip.top && y < clip.bottom; };
    const int dy = y1 - y0;
    const int dx = x1 - x0;
    if (dy == 0) {
        if (y0 < clip.top || y0 >= clip.bottom) {
            return;
        }
        for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x) {
            if (next() && inClip(x, y0)) {
                to.set(x, y0, colour);
            }
        }
        return;
    }
    if (dx == 0) {
        int y = std::min(y0, y1);
        int count = std::abs(dy) + 1;
        if (y >= 200) {
            return;
        }
        if (y < 0) {
            count += y;
            y = 0;
        }
        if (x0 < clip.left || x0 >= clip.right) {
            return;
        }
        for (; count > 0; --count, ++y) {
            if (next() && inClip(x0, y)) {
                to.set(x0, y, colour);
            }
        }
        return;
    }
    const int sx = dx > 0 ? 1 : -1;
    const int sy = dy > 0 ? 1 : -1;
    int major = std::abs(dx);
    int minor = std::abs(dy);
    int straightX = sx;
    int straightY = 0;
    if (std::abs(dx) <= std::abs(dy)) {
        std::swap(major, minor);
        straightX = 0;
        straightY = sy;
    }
    int x = x0;
    int y = y0;
    int error = major / 2;
    for (int n = major; n > 0; --n) {
        error += minor;
        if (error >= major) {
            error -= major;
            x += sx;
            y += sy;
        } else {
            x += straightX;
            y += straightY;
        }
        if (next() && inClip(x, y)) {
            to.set(x, y, colour);
        }
    }
}

} // namespace

// The troop's way: a dotted line from where it is to its place, or to
// where it goes. A line that would cross most of the window goes the
// other way round the planet.
void SpiceMap::drawRoute() {
    if (troop_ == 0) {
        return;
    }
    struct Point {
        int x;
        int y;
        std::uint16_t lon;
    };
    std::vector<Point> points;
    auto add = [&](std::uint16_t lon, int lat) {
        Point p{0, 0, lon};
        small_.toScreen(lon, lat, p.x, p.y);
        points.push_back(p);
    };
    const std::size_t t = World::troopRecord(troop_);
    add(world_.u16(t + 6), s16(world_.u16(t + 8)));
    const std::size_t place = world_.u16(t + 4);
    add(world_.u16(place + 2), s16(world_.u16(place + 4)));
    const ClipRect v = view();
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        Point a = points[i];
        Point b = points[i + 1];
        int across = a.x - b.x;
        const auto turned = static_cast<std::int16_t>(static_cast<std::uint16_t>(a.lon - b.lon));
        if ((turned < 0) != (across < 0) && std::abs(across) >= kWrapAcross) {
            if (a.x >= v.left && a.x < v.right) {
                std::swap(a, b);
                across = -across;
            }
            a.x += across < 0 ? kRingAcross : -kRingAcross;
        }
        patternLine(g_.display().target(), a.x, a.y, b.x, b.y, kRouteColour, kRoutePattern, v);
    }
}


void SpiceMap::drawMainView() {
    int x = 0;
    int y = 0;
    small_.toScreen(map_.longitude(), map_.latitude(), x, y);
    const ClipRect v = view();
    auto clampX = [&](int a) { return std::clamp(a, v.left, v.left + kViewWidth - 1); };
    auto clampY = [&](int a) { return std::clamp(a, v.top, v.top + kViewHeight - 1); };
    const int l = clampX(x - kMainHalfWidth);
    const int r = clampX(x + kMainHalfWidth - 1);
    const int t = clampY(y - kMainHalfHeight);
    const int b = clampY(y + kMainHalfHeight - 1);
    if (t == b || l == r) {
        return;
    }
    Surface& to = g_.display().target();
    dottedLine(to, l, t, r, t, kMainFrame, v);
    dottedLine(to, l, b, r, b, kMainFrame, v);
    dottedLine(to, l, t, l, b, kMainFrame, v);
    dottedLine(to, r, t, r, b, kMainFrame, v);
}

void SpiceMap::drawMarker() {
    std::uint16_t lon = 0;
    int lat = 0;
    std::uint16_t sprite = kPlayerMark;
    int left = kPlayerMarkLeft;
    if (troop_ != 0) {
        const std::size_t t = World::troopRecord(troop_);
        if (world_.u8(t + 3) & 0x40) {
            lon = world_.u16(t + 6);
            lat = s16(world_.u16(t + 8));
        } else {
            const std::size_t place = world_.u16(t + 4);
            lon = world_.u16(place + 2);
            lat = s16(world_.u16(place + 4));
        }
        sprite = kTroopMark;
        left = 0;
    } else {
        world_.position(lon, lat);
    }
    int x = 0;
    int y = 0;
    if (!small_.toScreen(lon, lat, x, y)) {
        return;
    }
    g_.selectSheet(kPanelSheet);
    const SpriteView s = g_.sheet().sprite(sprite);
    drawSpriteClipped(g_.canvas(), s, x - left, (y - s.header.height) & 0xFF, view());
}

// --- The mouse

std::size_t SpiceMap::nearestPlace(int x, int y) const {
    std::size_t nearest = 0;
    int best = 0xFFFF;
    for (const SmallMap::Shown& s : small_.shown()) {
        const int d = std::abs(s.x - x) + std::abs(s.y - y);
        if (d < best) {
            best = d;
            nearest = s.record;
        }
    }
    return best < kReach ? nearest : 0;
}

void SpiceMap::pointAt(int x, int y) {
    if (!shown_) {
        return;
    }
    const ClipRect f = frame();
    const bool inside = x > f.left && x < f.right && y > f.top && y < f.bottom;
    Display& d = g_.display();
    const bool wasBack = &d.target() == &d.back();
    d.targetScreen();
    const std::size_t place = inside ? nearestPlace(x, y) : 0;
    if (place != pointed_) {
        pointed_ = place;
        density_ = -1;
        drawBar();
        if (place != 0) {
            drawPlaceInfo(place);
        } else {
            drawScale();
        }
    }
    if (place == 0) {
        const bool onBar = inside && y >= f.bottom - kBarStrip;
        if (onBar != legendLit_) {
            drawLegend(onBar);
        }
        if (inside && !onBar && !occupations) {
            const int level = d.screen().at(x, y) - kFirstDensity;
            const int now = level >= 0 && level < 16 ? level : -1;
            if (now != density_) {
                markDensity(density_);
                markDensity(now);
                density_ = now;
            }
        }
    }
    if (wasBack) {
        d.targetBack();
    }
}

SpiceMap::Click SpiceMap::click(int x, int y, std::size_t& place) {
    place = 0;
    const ClipRect f = frame();
    if (!shown_ || !(x > f.left && x < f.right && y > f.top && y < f.bottom)) {
        return Click::Outside;
    }
    if (y - f.top < kViewY) {
        return x - f.left < kCloseWidth ? Click::Close : Click::Title;
    }
    if (y - f.top >= kViewY + kViewHeight) {
        occupations = !occupations;
        draw();
        return Click::Toggle;
    }
    pointAt(x, y);
    place = pointed_;
    return Click::Map;
}

} // namespace mentat
