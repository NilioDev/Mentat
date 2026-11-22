// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/attack_view.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

namespace mentat {

namespace {

// Places in the program's data.
constexpr std::size_t kSkyArea = 0x1582;      // left, top, right, bottom
constexpr std::size_t kGroundArea = 0x158A;
constexpr std::size_t kHorizon = 0x11DD;      // sprite list
constexpr std::size_t kFlyerSpeeds = 0x15A2;  // 4 speeds, x and y bytes
constexpr std::size_t kFlyerStarts = 0x1592;  // 4 places, x and y words

constexpr int kSkySprite = 2;
constexpr int kGroundSprite = 3;
constexpr int kTickTicks = 3;
constexpr int kBattleSound = 3;
constexpr int kFirstShot = 0x14;
constexpr int kFirstBlast = 0x1C;
constexpr int kLastBlast = 0x2D;
constexpr int kFlashColour = 128;
constexpr int kFlashColours = 28;
constexpr int kBrightFlash = 0x37;
constexpr int kDimFlash = 0x36;
constexpr int kNormalColours = 0x35;

int s8(int v) { return static_cast<std::int8_t>(static_cast<std::uint8_t>(v)); }

ClipRect readRect(const ProgramFile& p, std::size_t at) {
    return ClipRect{p.dataU16(at), p.dataU16(at + 2), p.dataU16(at + 4), p.dataU16(at + 6)};
}

// One speed component in 1/32 pixel, with its carried fraction: returns
// the whole pixels to move and keeps the new fraction.
int subpixelStep(int speed, std::uint8_t& fraction) {
    const bool back = speed < 0;
    const int sum = ((back ? -speed : speed) + fraction) & 0xFF;
    fraction = static_cast<std::uint8_t>(sum & 31);
    const int whole = sum >> 5;
    return back ? -whole : whole;
}

// A heading (0 to 255, a quarter turn every 64) as a speed of 32/32 of a
// pixel along its main direction.
void headingSpeed(int heading, int& dx, int& dy) {
    const int turned = (heading + 0x20) & 0xFF;
    int along = heading & 0xFF;
    if ((turned & 0x7F) >= 0x40) {
        dx = (turned & 0x80) != 0 ? -0x20 : 0x20;
        along = (along - 0x40) & 0xFF;
        if ((turned & 0x80) != 0) {
            along = (-((along - 0x80) & 0xFF)) & 0xFF;
        }
        dy = s8(along);
        return;
    }
    dy = -0x20;
    if ((turned & 0x80) != 0) {
        along = (-((along - 0x80) & 0xFF)) & 0xFF;
        dy = 0x20;
    }
    dx = s8(along);
}

} // namespace

void AttackView::setColours(int sprite, bool spare) {
    game_.selectSheet(kSheet);
    const SpriteView v = game_.sheet().sprite(static_cast<std::size_t>(sprite));
    Reader r(v.data);
    r.skip(2);
    const auto rgb = r.bytes(kFlashColours * 3);
    Display& d = game_.display();
    for (int i = 0; i < kFlashColours; ++i) {
        const Colour c{rgb[static_cast<std::size_t>(i * 3)], rgb[static_cast<std::size_t>(i * 3 + 1)],
                       rgb[static_cast<std::size_t>(i * 3 + 2)]};
        if (spare) {
            d.spare()[static_cast<std::size_t>(kFlashColour + i)] = c;
        } else {
            d.palette()[static_cast<std::size_t>(kFlashColour + i)] = c;
            d.markChanged(kFlashColour + i);
        }
    }
}

void AttackView::start() {
    const ProgramFile& p = game_.resources().program();
    Display& d = game_.display();
    game_.selectSheet(kSheet);
    actors_.useSheet(kSheet);
    actors_.clear();

    // The sky and the ground are one sprite each, repeated over their area.
    const auto tile = [&](std::size_t at, int sprite) {
        const ClipRect area = readRect(p, at);
        d.clearRect(d.target(), area);
        const Canvas canvas = game_.canvas();
        const SpriteView v = game_.sheet().sprite(static_cast<std::size_t>(sprite));
        const int w = v.header.width();
        const int h = v.header.height;
        if (w <= 0 || h <= 0) {
            return;
        }
        for (int y = area.top; y < area.bottom; y += h) {
            for (int x = area.left; x < area.right; x += w) {
                drawSpriteClipped(canvas, v, x, y, area);
            }
        }
    };
    tile(kSkyArea, kSkySprite);
    tile(kGroundArea, kGroundSprite);
    std::vector<std::uint16_t> list;
    for (std::size_t at = kHorizon;; at += 6) {
        const std::uint16_t word = p.dataU16(at);
        if (word == 0xFFFF || list.size() > 60) {
            break;
        }
        list.insert(list.end(), {word, p.dataU16(at + 2), p.dataU16(at + 4)});
    }
    list.push_back(0xFFFF);
    game_.drawSpriteList(list);
    actors_.setClip(game_.sceneTables().lineClip);

    nextFlyer_ = flyerWave_ = pause_ = shots_ = flash_ = 0;
    flyerKind_ = 0;
    game_.removeTimer(kTimer);
    game_.addTimer(kTimer, kTickTicks, [this] { tick(); });
    d.saveBackground();
    game_.playSound(kBattleSound);
    running_ = true;
}

void AttackView::stop() {
    if (!running_) {
        return;
    }
    running_ = false;
    game_.removeTimer(kTimer);
    game_.stopSound();
    actors_.clear();
}

bool AttackView::noiseBit() {
    const bool top = (noise_ & 0x8000) != 0;
    noise_ = static_cast<std::uint16_t>((noise_ << 1) | (top ? 1 : 0));
    return top;
}

// The horizon lights up: at 16 the bright colours, at 10 dimmer ones, and
// in between the colours fade back to the usual ones.
void AttackView::flash(int step) {
    if (step == 0x10 || step == 0x0A) {
        setColours(step == 0x10 ? kBrightFlash : kDimFlash, false);
        game_.display().flush();
        setColours(kNormalColours, true);
        return;
    }
    game_.display().fadeStep(kFlashColour, kFlashColours, step);
}

void AttackView::spawnFlyer() {
    if (--flyerWave_ < 0) {
        if ((noise_ & 3) == 0) {
            flash_ = 0x0B;
            if ((noise_ & 0x0C) == 0) {
                flash_ = 0x11;
            }
        }
        std::uint16_t kind = static_cast<std::uint16_t>(game_.random2());
        flyerWave_ = game_.random(7);
        if (flyerWave_ >= 4) {
            kind |= 0x4000;
        }
        flyerKind_ = kind;
    }
    nextFlyer_ = 8;

    const ProgramFile& p = game_.resources().program();
    const int low = flyerKind_ & 0xFF;
    int high = flyerKind_ >> 8;
    int sprite = 0;
    std::uint16_t motion = 0;
    if ((low & 0x10) == 0) {
        // Ornithopters on a straight line.
        const int lane = high & 6;
        motion = p.dataU16(kFlyerSpeeds + static_cast<std::size_t>(lane));
        sprite = 2 * lane + 4;
    } else {
        // Shots that may turn as they go.
        int heading = high & 0x3F;
        int turn = high & 0xC0;
        if ((turn & 0x40) != 0 && noiseBit()) {
            heading += (turn & 0x80) != 0 ? -10 : 10;
            if (s8(heading) < 0) {
                turn ^= 0x80;
                heading = 0;
            }
            if (heading >= 0x40) {
                heading = 0x3F;
                turn ^= 0x80;
            }
            high = turn | heading;
            flyerKind_ = static_cast<std::uint16_t>((high << 8) | low);
        }
        int dx = 0;
        int dy = 0;
        headingSpeed((heading + 0xE0) & 0xFF, dx, dy);
        motion = static_cast<std::uint16_t>(((dy & 0xFF) << 8) | (dx & 0xFF));
        sprite = kFirstShot;
    }
    const std::size_t start = kFlyerStarts + static_cast<std::size_t>(low & 0x0C);
    actors_.add(sprite, static_cast<std::int16_t>(p.dataU16(start)), static_cast<std::int16_t>(p.dataU16(start + 2)),
                motion);
}

void AttackView::tick() {
    noise_ = static_cast<std::uint16_t>(game_.random2());
    flash_ = (flash_ - 1) & 0xFF;
    if (flash_ <= 0x10) {
        flash(flash_);
    }

    if (--pause_ < 0) {
        if (--shots_ < 0) {
            const int r = game_.random2();
            shots_ = r & 0x7F;
            pause_ = (r >> 8) & 0xFF;
        } else {
            // A shot lands somewhere along the horizon.
            const int r = game_.random2();
            const int y = r & 0x7F;
            const int x = (((r & 0x80) != 0 ? 1 : 0) << 8) | ((r >> 8) & 0xFF);
            if (y >= 0x30 && y < 0x60 && x < 320) {
                actors_.add(kFirstBlast + (y & 7), x, y, 0);
            }
        }
    }
    if (--nextFlyer_ < 0) {
        spawnFlyer();
    }

    auto& list = actors_.list();
    const std::size_t count = list.size();
    for (std::size_t i = 0; i < count && i < list.size(); ++i) {
        Actor& a = list[i];
        int sprite = a.sprite;
        int dx = s8(a.motion & 0xFF);
        int dy = s8(a.motion >> 8);
        if (sprite < kFirstShot) {
            // Ornithopters: their wings beat through four sprites.
            sprite = (sprite & ~3) | (noiseBit() ? 2 : 0);
            sprite |= noiseBit() ? 1 : 0;
        } else if (sprite < kFirstBlast) {
            std::uint8_t fx = static_cast<std::uint8_t>(a.extra & 0xFF);
            std::uint8_t fy = static_cast<std::uint8_t>(a.extra >> 8);
            dx = subpixelStep(dx, fx);
            dy = subpixelStep(dy, fy);
            a.extra = static_cast<std::uint16_t>((fy << 8) | fx);
            int bits = 0;
            for (int b = 0; b < 3; ++b) {
                bits = (bits << 1) | (noiseBit() ? 1 : 0);
            }
            sprite = kFirstShot + bits;
        } else {
            if (++sprite > kLastBlast) {
                actors_.remove(i);
                continue; // the next one waits for the next tick, as in the original
            }
        }
        actors_.setSprite(i, sprite);
        actors_.move(i, dx, dy);
        const ClipRect r = list[i].rect;
        if (r.left < 0 || r.left >= 320 || r.right < 0 || r.bottom < 0) {
            actors_.remove(i);
        }
    }
}

} // namespace mentat
