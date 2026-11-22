// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/small_map.hpp"

#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <cstdlib>

namespace mentat {

namespace {
// The places: sprites of the panel's picture file.
constexpr std::uint16_t kPlaceSprite = 0x3A;
constexpr std::uint8_t kPlaceHidden = 0x80;
} // namespace

void SmallMap::drawMap() {
    latitude = map_.flatMap().drawSmall(g_.display().target(), view.left, view.top, view.right, view.bottom, longitude,
                                        latitude);
}

bool SmallMap::toScreen(std::uint16_t lon, int lat, int& x, int& y) const {
    const int middleX = view.left + (view.right - view.left) / 2;
    const int middleY = view.top + (view.bottom - view.top - 1) / 2;
    const int ring = 2 * map_.flatMap().width(std::abs(lat));
    const long along = static_cast<std::int16_t>(static_cast<std::uint16_t>(lon - longitude));
    // The whole part of the offset in points, rounded down.
    x = static_cast<int>((along * ring) >> 16) + middleX;
    y = lat - latitude + middleY;
    return x >= view.left && x < view.right && y >= view.top && y < view.bottom;
}

void SmallMap::drawPlaces() {
    shown_.clear();
    const SpriteSheet& sheet = g_.sheet();
    for (std::size_t record = World::kPlaces; world_.u16(record) != 0xFFFF; record += World::kPlaceSize) {
        if (world_.u8(record + 0x0A) & kPlaceHidden) {
            continue;
        }
        int x = 0;
        int y = 0;
        if (!toScreen(world_.u16(record + 2), static_cast<std::int16_t>(world_.u16(record + 4)), x, y)) {
            continue;
        }
        shown_.push_back(Shown{record, x, y});
        const SpriteView v = sheet.sprite(map_.placeSprite(record, kPlaceSprite));
        drawSpriteClipped(g_.canvas(), v, x - v.header.width() / 2, y - v.header.height / 2, view);
    }
}

} // namespace mentat
