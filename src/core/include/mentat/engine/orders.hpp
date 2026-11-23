// SPDX-License-Identifier: GPL-3.0-or-later
//
// Giving orders to one of Paul's troops: the map around it, its chief in
// a box answering, and the orders as commands. See docs/rules/orders.md.
#pragma once

#include "mentat/gfx/scene.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace mentat {

class Game;
class MapScreen;
class Panel;
class Talk;
class World;

class Orders {
public:
    // The original's numbers for what the orders' commands do.
    static constexpr std::uint16_t kInformation = 0x7BED;
    static constexpr std::uint16_t kOccupation = 0x69B3;
    static constexpr std::uint16_t kEquipment = 0x7CBB;
    static constexpr std::uint16_t kMove = 0x8064;
    static constexpr std::uint16_t kNoMoreOrders = 0x8763;
    static constexpr std::uint16_t kNextTroop = 0x86FA;
    static constexpr std::uint16_t kLeaveTroop = 0x8770;
    static constexpr std::uint16_t kSpice = 0x6A71;
    static constexpr std::uint16_t kArmy = 0x6A83;
    static constexpr std::uint16_t kEcology = 0x6A87;
    static constexpr std::uint16_t kCancel = 0xD2E2;

    Orders(Game& game, World& world, Panel& panel, MapScreen& map, Talk& talk);

    bool active() const { return troop_ != 0; }
    int troop() const { return troop_; }

    // Opens the orders for troop `troop` from a talk with its chief: the
    // map around the place, dissolved onto the screen, and the chief's
    // box over it.
    void open(int troop);
    // A command of the orders. False when it is not one.
    bool command(std::uint16_t action);
    // Closes the orders; the caller shows the room again.
    void close();

    // The orders are over and Paul is back in the room.
    std::function<void()> finished;

private:
    void drawBox();
    void setMenu();
    void occupationMenu();
    void specialize(int occupation);
    void say(int list);
    std::array<int, 7> freeEquipment(std::size_t place) const;

    Game& g_;
    World& world_;
    Panel& panel_;
    MapScreen& map_;
    Talk& talk_;
    int troop_ = 0;
    ClipRect box_{};
};

} // namespace mentat
