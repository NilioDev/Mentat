// SPDX-License-Identifier: GPL-3.0-or-later
//
// Playing with a gamepad: the pointer jumps between the things on the
// screen that can be clicked, as on the console versions of such games.
// See docs/gamepad.md.
#pragma once

#include "mentat/gfx/scene.hpp"

#include <optional>
#include <vector>

namespace mentat {

struct Point {
    int x = 0;
    int y = 0;
};

// Where the pointer goes from (x, y) when the player pushes direction
// (dx, dy), each -1, 0 or 1: the middle of the nearest target lying that
// way, a target counting as nearer the less it lies to the side. None when
// nothing lies that way. A target the pointer is already on is passed over.
std::optional<Point> jumpTarget(const std::vector<ClipRect>& targets, int x, int y, int dx, int dy);

} // namespace mentat
