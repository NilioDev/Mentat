// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/gfx/surface.hpp"

#include <algorithm>
#include <cstring>

namespace mentat {

void Surface::fillRect(int x, int y, int w, int h, std::uint8_t colour) {
    const int x0 = std::max(x, 0);
    const int y0 = std::max(y, 0);
    const int x1 = std::min(x + w, width_);
    const int y1 = std::min(y + h, height_);
    for (int yy = y0; yy < y1; ++yy) {
        for (int xx = x0; xx < x1; ++xx) {
            pixels_[index(xx, yy)] = colour;
        }
    }
}

void Surface::blit(const Surface& from, int fromX, int fromY, int w, int h, int toX, int toY) {
    for (int dy = 0; dy < h; ++dy) {
        for (int dx = 0; dx < w; ++dx) {
            const int sx = fromX + dx;
            const int sy = fromY + dy;
            if (from.contains(sx, sy)) {
                set(toX + dx, toY + dy, from.at(sx, sy));
            }
        }
    }
}

} // namespace mentat
