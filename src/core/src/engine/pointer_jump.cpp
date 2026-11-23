// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/pointer_jump.hpp"

#include <cstdlib>
#include <limits>

namespace mentat {

namespace {

// How much more a step to the side counts than a step the way pushed.
constexpr long kSideWeight = 3;

bool inside(const ClipRect& r, int x, int y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }

} // namespace

std::optional<Point> jumpTarget(const std::vector<ClipRect>& targets, int x, int y, int dx, int dy) {
    if (dx == 0 && dy == 0) {
        return std::nullopt;
    }
    std::optional<Point> best;
    long bestScore = std::numeric_limits<long>::max();
    for (const ClipRect& r : targets) {
        if (r.right <= r.left || r.bottom <= r.top || inside(r, x, y)) {
            continue;
        }
        const Point middle{(r.left + r.right - 1) / 2, (r.top + r.bottom - 1) / 2};
        // The nearest point of the target, so a wide target close by wins
        // over a small one far away.
        const int nearX = x < r.left ? r.left : (x >= r.right ? r.right - 1 : x);
        const int nearY = y < r.top ? r.top : (y >= r.bottom ? r.bottom - 1 : y);
        const long along = static_cast<long>(nearX - x) * dx + static_cast<long>(nearY - y) * dy;
        if (along <= 0) {
            continue;
        }
        const long side = dx != 0 ? std::labs(middle.y - y) : std::labs(middle.x - x);
        const long score = along + kSideWeight * side;
        if (score < bestScore) {
            bestScore = score;
            best = middle;
        }
    }
    return best;
}

} // namespace mentat
