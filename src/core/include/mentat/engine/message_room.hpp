// SPDX-License-Identifier: GPL-3.0-or-later
//
// The palace's message room: the device that lists the news Paul has been
// sent, and the hologram of whoever sent the one he picks, who then says
// it. See docs/rules/news.md.
#pragma once

#include "mentat/engine/panel.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace mentat {

class Game;
class Talk;
class World;

class MessageRoom {
public:
    // The original's numbers for the room's commands.
    static constexpr std::uint16_t kNew = 0x283A;
    static constexpr std::uint16_t kSeen = 0x283E;
    static constexpr std::uint16_t kPick = 0x290B;
    static constexpr std::uint16_t kCancel = 0x29D4;

    MessageRoom(Game& game, World& world, Panel& panel, Talk& talk);

    // A command of the room or of its lists; `line` is the panel's line
    // that was clicked. False when it is not one.
    bool command(std::uint16_t action, int line);
    // The list of news or a hologram's talk is shown.
    bool active() const { return open_; }
    // Paul goes elsewhere (the map, a game loaded): the list is left.
    void leave() {
        open_ = false;
        hologram_ = 0;
    }

    // Draws the room as it is, with its commands, on the back buffer.
    std::function<void()> drawRoom;
    // The room's commands come back after the news; the scene stays.
    std::function<void()> backToRoom;
    // The news said the Emperor's troops come: the game ends.
    std::function<void(int reason)> ended;

private:
    void open(bool seen);
    void pick(int line);
    void viewed();
    void cancel();
    void say(int phrase);
    // The device on the table, drawn on the screen.
    void drawDevice(int glow);
    void drawGlow(int glow);
    void sparkle(int frame);
    void step(int frame);
    void wait(std::uint32_t& due);
    void showHologram(int kind);
    void hideHologram();
    void drawHologram();

    Game& g_;
    World& world_;
    Panel& panel_;
    Talk& talk_;
    bool open_ = false;
    bool seen_ = false;
    int hologram_ = 0; // who is shown, 0 for nobody
    // The device's sprites: (sprite, x, y). The program changes those of
    // the side pieces as they turn, and they stay so.
    std::array<std::array<std::uint16_t, 3>, 3> device_{};
};

} // namespace mentat
