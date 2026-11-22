// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/actors.hpp"

#include "mentat/engine/game.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <algorithm>

namespace mentat {

void Actors::add(int sprite, int x, int y, std::uint16_t motion) {
    game_.selectSheet(sheet_);
    const SpriteView v = game_.sheet().sprite(static_cast<std::size_t>(sprite));
    const int width = v.header.widthAndFlags & 0x0FFF;
    const int height = v.header.height;
    Actor a;
    a.rect.left = x - width / 2;
    a.rect.top = y - height / 2;
    a.rect.right = a.rect.left + width;
    a.rect.bottom = a.rect.top + height;
    a.sprite = sprite;
    a.motion = motion;
    actors_.push_back(a);
}

void Actors::setSprite(std::size_t i, int sprite) { actors_.at(i).sprite = sprite; }

void Actors::move(std::size_t i, int dx, int dy) {
    Actor& a = actors_.at(i);
    ClipRect changed = a.rect;
    a.rect.left += dx;
    a.rect.top += dy;
    a.rect.right += dx;
    a.rect.bottom += dy;
    if (dx >= 0) {
        changed.right = a.rect.right;
    } else {
        changed.left = a.rect.left;
    }
    if (dy >= 0) {
        changed.bottom = a.rect.bottom;
    } else {
        changed.top = a.rect.top;
    }
    redraw(changed);
}

void Actors::remove(std::size_t i) {
    const ClipRect where = actors_.at(i).rect;
    actors_.erase(actors_.begin() + static_cast<std::ptrdiff_t>(i));
    redraw(where);
}

void Actors::redraw(ClipRect r) {
    game_.selectSheet(sheet_);
    r.left = std::max(r.left, clip_.left);
    r.top = std::max(r.top, clip_.top);
    r.right = std::min(r.right, clip_.right);
    r.bottom = std::min(r.bottom, clip_.bottom);
    if (r.right <= r.left || r.bottom <= r.top) {
        return;
    }
    Display& d = game_.display();
    d.copyRect(d.saved(), d.back(), r);
    const Canvas canvas(d.target(), d.originRows());
    for (const Actor& a : actors_) {
        if (a.rect.left < r.right && a.rect.top < r.bottom && a.rect.right > r.left && a.rect.bottom > r.top) {
            drawSpriteClipped(canvas, game_.sheet().sprite(static_cast<std::size_t>(a.sprite)), a.rect.left,
                              a.rect.top, r);
        }
    }
    d.copyRect(d.back(), d.screen(), r);
}

} // namespace mentat
