// SPDX-License-Identifier: GPL-3.0-or-later
//
// What each gamepad button does, and the player's own choices for them read
// from a small text file. See docs/gamepad.md.
#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace mentat {

enum class PadAction {
    None,       // nothing
    Click,      // the left mouse button
    RightClick, // the right mouse button
    Key,        // a key press: skips a video or a pause
    Up,         // the pointer jumps that way
    Down,
    Left,
    Right,
    Quit,       // Mentat closes
};

class PadButtons {
public:
    // The buttons, in SDL's order of gamepad buttons.
    static constexpr std::size_t kButtons = 26;

    // As Mentat comes: A and X click, B and Y right-click, the direction
    // pad jumps, Back does nothing and the rest press a key.
    PadButtons();

    PadAction action(std::size_t button) const {
        return button < kButtons ? actions_[button] : PadAction::Key;
    }
    void set(std::size_t button, PadAction a) {
        if (button < kButtons) {
            actions_[button] = a;
        }
    }

    // Reads lines of "button = action" (names as in docs/gamepad.md; case
    // and spaces do not matter, "#" starts a comment). Lines it does not
    // understand are left out; returns how many were.
    int read(std::string_view text);

    // The button's number for a name ("a", "south", "dpup", ...), or -1.
    static int buttonNamed(std::string_view name);
    // The action for a name ("click", "jump up", ...); false for none.
    static bool actionNamed(std::string_view name, PadAction& out);

private:
    std::array<PadAction, kButtons> actions_{};
};

} // namespace mentat
