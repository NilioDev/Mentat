// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.hpp"

#include "mentat/engine/pad_buttons.hpp"
#include "mentat/engine/pointer_jump.hpp"

using namespace mentat;

namespace {

// Five lines of a list under each other, and an arrow to their right.
std::vector<ClipRect> panel() {
    return {
        {92, 155, 230, 163}, {92, 163, 230, 171}, {92, 171, 230, 179}, {92, 179, 230, 187}, {92, 187, 230, 195},
        {260, 160, 280, 175},
    };
}

} // namespace

MTT_TEST("pointer: down goes to the next line of a list") {
    const auto p = jumpTarget(panel(), 150, 159, 0, 1);
    MTT_CHECK(p && p->x == 160 && p->y == 166);
}

MTT_TEST("pointer: up from the first line finds nothing") {
    MTT_CHECK(!jumpTarget(panel(), 150, 159, 0, -1));
}

MTT_TEST("pointer: right goes to the arrow, left comes back to the list") {
    const auto p = jumpTarget(panel(), 150, 167, 1, 0);
    MTT_CHECK(p && p->x == 269 && p->y == 167);
    const auto q = jumpTarget(panel(), p->x, p->y, -1, 0);
    MTT_CHECK(q && q->x == 160 && q->y == 166);
}

MTT_TEST("pointer: from the scene, down reaches the nearest line") {
    const auto p = jumpTarget(panel(), 300, 40, 0, 1);
    MTT_CHECK(p && p->x == 269);
    MTT_CHECK(!jumpTarget(panel(), 300, 40, 0, 0));
}

MTT_TEST("gamepad: the buttons as Mentat comes, and as the player's file sets them") {
    PadButtons b;
    MTT_CHECK(b.action(0) == PadAction::Click && b.action(1) == PadAction::RightClick);
    MTT_CHECK(b.action(2) == PadAction::Click && b.action(3) == PadAction::RightClick);
    MTT_CHECK(b.action(4) == PadAction::None && b.action(11) == PadAction::Up && b.action(14) == PadAction::Right);
    MTT_CHECK(b.action(6) == PadAction::Key && b.action(99) == PadAction::Key);
    const int bad = b.read("# my pad\n"
                           "B = click\n"
                           "a = Right click   # swapped\n"
                           "Back = quit\r\n"
                           "left shoulder = jump left\n"
                           "start = fly\n"
                           "nonsense\n"
                           "\n");
    MTT_CHECK(bad == 2);
    MTT_CHECK(b.action(1) == PadAction::Click && b.action(0) == PadAction::RightClick);
    MTT_CHECK(b.action(4) == PadAction::Quit && b.action(9) == PadAction::Left && b.action(6) == PadAction::Key);
    MTT_CHECK(PadButtons::buttonNamed("DP-Up") == 11 && PadButtons::buttonNamed("z") == -1);
}
