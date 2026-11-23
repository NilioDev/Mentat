// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/pad_buttons.hpp"

#include <cctype>
#include <utility>

namespace mentat {

namespace {

// SDL's gamepad buttons, in its order.
enum Button : std::size_t {
    kSouth,
    kEast,
    kWest,
    kNorth,
    kBack,
    kGuide,
    kStart,
    kLeftStick,
    kRightStick,
    kLeftShoulder,
    kRightShoulder,
    kDpadUp,
    kDpadDown,
    kDpadLeft,
    kDpadRight,
};

// Lower case, without spaces, dashes or underscores.
std::string plain(std::string_view s) {
    std::string out;
    for (const char c : s) {
        if (c == ' ' || c == '\t' || c == '-' || c == '_' || c == '\r') {
            continue;
        }
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

} // namespace

PadButtons::PadButtons() {
    actions_.fill(PadAction::Key);
    actions_[kSouth] = PadAction::Click;
    actions_[kEast] = PadAction::RightClick;
    actions_[kBack] = PadAction::Quit;
    actions_[kDpadUp] = PadAction::Up;
    actions_[kDpadDown] = PadAction::Down;
    actions_[kDpadLeft] = PadAction::Left;
    actions_[kDpadRight] = PadAction::Right;
}

int PadButtons::buttonNamed(std::string_view name) {
    static const std::pair<const char*, int> kNames[] = {
        {"a", kSouth},
        {"south", kSouth},
        {"cross", kSouth},
        {"b", kEast},
        {"east", kEast},
        {"circle", kEast},
        {"x", kWest},
        {"west", kWest},
        {"square", kWest},
        {"y", kNorth},
        {"north", kNorth},
        {"triangle", kNorth},
        {"back", kBack},
        {"select", kBack},
        {"guide", kGuide},
        {"start", kStart},
        {"leftstick", kLeftStick},
        {"rightstick", kRightStick},
        {"leftshoulder", kLeftShoulder},
        {"lb", kLeftShoulder},
        {"rightshoulder", kRightShoulder},
        {"rb", kRightShoulder},
        {"dpup", kDpadUp},
        {"up", kDpadUp},
        {"dpdown", kDpadDown},
        {"down", kDpadDown},
        {"dpleft", kDpadLeft},
        {"left", kDpadLeft},
        {"dpright", kDpadRight},
        {"right", kDpadRight},
    };
    const std::string n = plain(name);
    for (const auto& [text, button] : kNames) {
        if (n == text) {
            return button;
        }
    }
    return -1;
}

bool PadButtons::actionNamed(std::string_view name, PadAction& out) {
    static const std::pair<const char*, PadAction> kNames[] = {
        {"none", PadAction::None},
        {"nothing", PadAction::None},
        {"click", PadAction::Click},
        {"leftclick", PadAction::Click},
        {"rightclick", PadAction::RightClick},
        {"key", PadAction::Key},
        {"skip", PadAction::Key},
        {"jumpup", PadAction::Up},
        {"jumpdown", PadAction::Down},
        {"jumpleft", PadAction::Left},
        {"jumpright", PadAction::Right},
        {"quit", PadAction::Quit},
    };
    const std::string n = plain(name);
    for (const auto& [text, action] : kNames) {
        if (n == text) {
            out = action;
            return true;
        }
    }
    return false;
}

int PadButtons::read(std::string_view text) {
    int bad = 0;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        if (const std::size_t hash = line.find('#'); hash != std::string_view::npos) {
            line = line.substr(0, hash);
        }
        if (plain(line).empty()) {
            continue;
        }
        const std::size_t eq = line.find('=');
        PadAction a = PadAction::None;
        const int button = eq == std::string_view::npos ? -1 : buttonNamed(line.substr(0, eq));
        if (button < 0 || !actionNamed(line.substr(eq + 1), a)) {
            ++bad;
            continue;
        }
        actions_[static_cast<std::size_t>(button)] = a;
    }
    return bad;
}

} // namespace mentat
