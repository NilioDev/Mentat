// SPDX-License-Identifier: GPL-3.0-or-later
//
// The marks of Paul's troops on the map: at their places, by what they do,
// and on their way. See docs/rules/orders.md and docs/rules/moves.md.
#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace mentat {

namespace {

constexpr std::size_t kMarkOffsets = 0x1672; // a troop's mark off its place, by its turn there
// The marks' pictures: by occupation while working (0x179C) and while it
// cannot or is stopped (0x16B6); spice miners holding equipment; Fremen
// not yet Paul's; the ring around the troop given orders; troops on their
// way, by the kind of their occupation and where they head (0x18BF).
constexpr std::size_t kWorkMarks = 0x179C;
constexpr std::size_t kStoppedMarks = 0x16B6;
constexpr std::array<std::size_t, 3> kHeldMarks{0x1813, 0x1817, 0x181B};
constexpr std::array<std::size_t, 3> kMiningMarks{0x17BC, 0x17C9, 0x1806};
constexpr std::size_t kOthersMark = 0x181F;
constexpr std::size_t kStandingMark = 0x1732;
constexpr std::size_t kStandingInside = 0x16AA;
constexpr std::size_t kTurnedMark = 0x1774;
constexpr std::size_t kChosenMark = 0x18FD;
constexpr std::size_t kMovingMarks = 0x18BF;
constexpr std::uint32_t kMarkTicks = 0x0F;
// A troop on its way is marked while its point is this near the screen.
constexpr int kNear = -0x10;
constexpr int kFarRight = 0x148;
constexpr int kFarDown = 0xA0;
constexpr std::size_t kTroopsBegin = 0x8AA;
constexpr std::size_t kTroopsEnd = 0xFBB;
constexpr std::size_t kTroopSize = 0x1B;

} // namespace

// What a troop's mark looks like, by what it does; 0 when it has none.
std::size_t MapScreen::markFrames(int troop) const {
    const ProgramFile& program = g_.resources().program();
    const std::size_t t = World::troopRecord(troop);
    const std::size_t place = world_.u16(t + 4);
    const int job = world_.u8(t + 3);
    const int flags = world_.u8(t + 0x10);
    const int held = world_.u8(t + 0x19) & 0xC0;
    if (flags & 0x10) {
        return 0;
    }
    if (!(flags & 0x80) && (job & 0x80)) {
        // Fremen not yet Paul's show only where he knows what is inside.
        return (world_.u8(place + 0x0A) & 0x10) ? kOthersMark : 0;
    }
    if (job & 0x40) {
        return movingFrames(troop);
    }
    const int occupation = job & 0x0F;
    if (job & 0x30) {
        if (occupation == 0 && held != 0) {
            return held == 0x80 ? kHeldMarks[0] : held == 0x40 ? kHeldMarks[1] : kHeldMarks[2];
        }
        return program.dataU16(kStoppedMarks + 2 * static_cast<std::size_t>(occupation));
    }
    if (occupation == 0 && held != 0) {
        return held == 0x80 ? kMiningMarks[0] : held == 0x40 ? kMiningMarks[1] : kMiningMarks[2];
    }
    std::size_t frames = program.dataU16(kWorkMarks + 2 * static_cast<std::size_t>(occupation));
    if (frames == kStandingMark && (world_.u8(place + 0x0A) & 2)) {
        return kStandingInside;
    }
    if (frames == kStandingMark || frames == kTurnedMark) {
        // Turned by the troop's turn at the place.
        const int turn = (world_.u8(t + 2) - 1) & 7;
        if (turn >= 3) {
            frames += 10;
        }
        if (turn >= 4) {
            frames += 10;
        }
        if (turn >= 5) {
            frames += 10;
        }
    }
    return frames;
}

// A troop on its way: by the kind of its occupation, and by where it
// heads, down, right, up or left, whichever way is the longer (the
// longitude counted in whole 256ths).
std::size_t MapScreen::movingFrames(int troop) const {
    const ProgramFile& program = g_.resources().program();
    const std::size_t t = World::troopRecord(troop);
    const std::size_t place = world_.u16(t + 4);
    const int across = static_cast<std::int8_t>(
        static_cast<std::uint16_t>(world_.u16(place + 2) - world_.u16(t + 6)) >> 8);
    const int down = static_cast<std::int16_t>(static_cast<std::uint16_t>(world_.u16(place + 4) - world_.u16(t + 8)));
    int way = 2;
    int along = down;
    if (std::abs(across) >= std::abs(down)) {
        way = 1;
        along = across;
    }
    if (along < 0) {
        way ^= 2;
    }
    way += 4 * ((world_.u8(t + 3) & 0x0F) >> 2);
    return program.dataU16(kMovingMarks + 2 * static_cast<std::size_t>(way));
}

// Where a troop's mark goes: off its place by its turn there.
bool MapScreen::markPoint(int troop, int& x, int& y) const {
    const ProgramFile& program = g_.resources().program();
    const std::size_t t = World::troopRecord(troop);
    const std::size_t place = world_.u16(t + 4);
    int placeX = 0;
    int placeY = 0;
    if (!placeOnScreen(place, placeX, placeY)) {
        return false;
    }
    int turn = (world_.u8(t + 2) - 1) & 0x0F;
    if (world_.u8(place + 0x0A) & 2) {
        turn ^= 8;
    }
    x = placeX + static_cast<std::int8_t>(program.dataU8(kMarkOffsets + 2 * static_cast<std::size_t>(turn)));
    y = placeY + static_cast<std::int8_t>(program.dataU8(kMarkOffsets + 2 * static_cast<std::size_t>(turn) + 1));
    return true;
}

// A troop on its way is marked at its own point, while that is near.
bool MapScreen::movingPoint(int troop, int& x, int& y) const {
    const std::size_t t = World::troopRecord(troop);
    toScreen(world_.u16(t + 6), static_cast<std::int16_t>(world_.u16(t + 8)), x, y);
    return x > kNear && y > kNear && x < kFarRight && y < kFarDown;
}

// The mark is centred on its point, half its size rounded down.
ClipRect MapScreen::markRect(int sprite, int x, int y) const {
    const SpriteView v = g_.sheet().sprite(static_cast<std::size_t>(sprite));
    const int w = v.header.width();
    const int h = v.header.height;
    return ClipRect{x - w / 2, y - h / 2, x - w / 2 + w, y - h / 2 + h};
}

void MapScreen::addMark(int troop, std::size_t frames, int x, int y, bool chosen) {
    const ProgramFile& program = g_.resources().program();
    Mark m;
    m.troop = troop;
    m.frames = frames;
    m.next = frames;
    m.chosen = chosen;
    m.sprite = program.dataU8(frames);
    m.rect = markRect(m.sprite, x, y);
    // A mark of more than one frame goes on from one of them at random.
    int more = 0;
    while (program.dataU8(frames + 3 * static_cast<std::size_t>(more + 1)) != 0) {
        ++more;
    }
    if (more > 0) {
        int mask = 0;
        while (mask < more) {
            mask = mask * 2 + 1;
        }
        int r = 0;
        do {
            r = g_.random2() & mask;
        } while (r > more);
        m.next = frames + 3 * static_cast<std::size_t>(r);
        m.animated = true;
    }
    marks_.push_back(m);
}

// The marks of the troops at the places in sight, place by place, then of
// the troops on their way, then the ring around the troop given orders;
// drawn on the back buffer.
void MapScreen::drawMarks() {
    marks_.clear();
    const int previous = g_.sheetResource();
    g_.selectSheet(resource::kMapSprites);
    for (const Shown& s : shown_) {
        for (int troop = world_.u8(s.record + 9); troop != 0;) {
            const std::size_t t = World::troopRecord(troop);
            const std::size_t frames = markFrames(troop);
            int x = 0;
            int y = 0;
            if (frames != 0 && markPoint(troop, x, y)) {
                addMark(troop, frames, x, y, false);
            }
            troop = world_.u8(t + 1);
        }
    }
    for (std::size_t t = kTroopsBegin; t < kTroopsEnd; t += kTroopSize) {
        if ((world_.u8(t + 0x10) & 0x10) || !(world_.u8(t + 3) & 0x40)) {
            continue;
        }
        const int troop = world_.u8(t);
        int x = 0;
        int y = 0;
        if (movingPoint(troop, x, y)) {
            addMark(troop, movingFrames(troop), x, y, false);
        }
    }
    if (chosen_ != 0) {
        for (const Mark& m : std::vector<Mark>(marks_)) {
            if (m.troop == chosen_) {
                const int x = (m.rect.left + m.rect.right) / 2;
                const int y = (m.rect.top + m.rect.bottom) / 2;
                addMark(chosen_, kChosenMark, x, y, true);
                break;
            }
        }
    }
    const ClipRect v = view();
    for (const Mark& m : marks_) {
        drawSpriteClipped(g_.canvas(), g_.sheet().sprite(static_cast<std::size_t>(m.sprite)), m.rect.left, m.rect.top, v);
    }
    g_.selectSheet(previous);
}

// The next frame of a mark: its sprite, moved by the frame's offsets.
void MapScreen::stepMark(Mark& m) {
    const ProgramFile& program = g_.resources().program();
    std::size_t at = m.next;
    if (program.dataU8(at) == 0 && at != m.frames) {
        at = m.frames;
    }
    const ClipRect old = m.rect;
    m.sprite = program.dataU8(at);
    const int dx = static_cast<std::int8_t>(program.dataU8(at + 1));
    const int dy = static_cast<std::int8_t>(program.dataU8(at + 2));
    m.next = at + 3;
    m.rect = ClipRect{m.rect.left + dx, m.rect.top + dy, m.rect.right + dx, m.rect.bottom + dy};
    redrawMarks(ClipRect{std::min(old.left, m.rect.left), std::min(old.top, m.rect.top),
                         std::max(old.right, m.rect.right), std::max(old.bottom, m.rect.bottom)});
}

void MapScreen::stepMarks() {
    if (!active_ || marks_.empty() || g_.platform().ticks - lastStep_ < kMarkTicks) {
        return;
    }
    lastStep_ = g_.platform().ticks;
    ++steps_;
    // The ring moves at each step, the other marks at every fourth.
    for (Mark& m : marks_) {
        if (m.animated && (m.chosen || (steps_ & 3) == 0)) {
            stepMark(m);
        }
    }
}

namespace {

// The parts of `r` outside `cover`.
void cut(const ClipRect& r, const ClipRect& cover, std::vector<ClipRect>& out) {
    const ClipRect& b = cover;
    if (r.right <= b.left || r.left >= b.right || r.bottom <= b.top || r.top >= b.bottom) {
        out.push_back(r);
        return;
    }
    if (r.top < b.top) {
        out.push_back(ClipRect{r.left, r.top, r.right, b.top});
    }
    if (r.bottom > b.bottom) {
        out.push_back(ClipRect{r.left, b.bottom, r.right, r.bottom});
    }
    const int top = std::max(r.top, b.top);
    const int bottom = std::min(r.bottom, b.bottom);
    if (r.left < b.left) {
        out.push_back(ClipRect{r.left, top, b.left, bottom});
    }
    if (r.right > b.right) {
        out.push_back(ClipRect{b.right, top, r.right, bottom});
    }
}

} // namespace

namespace {

std::vector<ClipRect> cutAll(std::vector<ClipRect> parts, const std::vector<ClipRect>& over) {
    for (const ClipRect& c : over) {
        std::vector<ClipRect> left;
        for (const ClipRect& p : parts) {
            cut(p, c, left);
        }
        parts = std::move(left);
    }
    return parts;
}

} // namespace

// A part of the map comes back with its marks, except under the windows
// over it, on the back buffer and on the screen, except under what covers
// the map there.
void MapScreen::redrawMarks(const ClipRect& region) {
    const ClipRect v = view();
    const ClipRect r{std::max(region.left, v.left), std::max(region.top, v.top), std::min(region.right, v.right),
                     std::min(region.bottom, v.bottom)};
    if (r.right <= r.left || r.bottom <= r.top) {
        return;
    }
    const std::vector<ClipRect> parts = cutAll({r}, windows ? windows() : std::vector<ClipRect>{});
    Display& d = g_.display();
    const int previous = g_.sheetResource();
    g_.selectSheet(resource::kMapSprites);
    for (const ClipRect& part : parts) {
        d.copyRect(base_, d.back(), part);
        for (const Mark& m : marks_) {
            drawSpriteClipped(Canvas(d.back()), g_.sheet().sprite(static_cast<std::size_t>(m.sprite)), m.rect.left,
                              m.rect.top, part);
        }
    }
    g_.selectSheet(previous);
    std::vector<ClipRect> over = covers ? covers() : std::vector<ClipRect>{};
    if (boxShown_) {
        over.push_back(boxRect());
    }
    if (labelled_ != 0) {
        over.push_back(label_);
    }
    for (const ClipRect& part : cutAll(parts, over)) {
        d.copyRect(d.back(), d.screen(), part);
    }
}

int MapScreen::findMark(int troop) const {
    for (std::size_t i = 0; i < marks_.size(); ++i) {
        if (marks_[i].troop == troop && !marks_[i].chosen) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void MapScreen::markChanged(int troop) {
    if (!active_) {
        return;
    }
    const int at = findMark(troop);
    if (at < 0) {
        return;
    }
    const ClipRect old = marks_[static_cast<std::size_t>(at)].rect;
    marks_.erase(marks_.begin() + at);
    redrawMarks(old);
    const std::size_t frames = markFrames(troop);
    int x = 0;
    int y = 0;
    const bool moving = (world_.u8(World::troopRecord(troop) + 3) & 0x40) != 0;
    if (frames == 0 || !(moving ? movingPoint(troop, x, y) : markPoint(troop, x, y))) {
        return;
    }
    const int previous = g_.sheetResource();
    g_.selectSheet(resource::kMapSprites);
    addMark(troop, frames, x, y, false);
    g_.selectSheet(previous);
    redrawMarks(marks_.back().rect);
}

void MapScreen::markMoved(int troop) {
    if (!active_) {
        return;
    }
    int x = 0;
    int y = 0;
    const bool near = movingPoint(troop, x, y);
    const int at = findMark(troop);
    const int previous = g_.sheetResource();
    g_.selectSheet(resource::kMapSprites);
    if (near && at >= 0) {
        // The mark keeps its picture and goes to the new point.
        Mark& m = marks_[static_cast<std::size_t>(at)];
        const ClipRect old = m.rect;
        m.rect = markRect(m.sprite, x, y);
        redrawMarks(ClipRect{std::min(old.left, m.rect.left), std::min(old.top, m.rect.top),
                             std::max(old.right, m.rect.right), std::max(old.bottom, m.rect.bottom)});
    } else if (near) {
        addMark(troop, movingFrames(troop), x, y, false);
        redrawMarks(marks_.back().rect);
    } else if (at >= 0) {
        const ClipRect old = marks_[static_cast<std::size_t>(at)].rect;
        marks_.erase(marks_.begin() + at);
        redrawMarks(old);
    }
    g_.selectSheet(previous);
}

} // namespace mentat
