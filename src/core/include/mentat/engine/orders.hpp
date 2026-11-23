// SPDX-License-Identifier: GPL-3.0-or-later
//
// Giving orders to one of Paul's troops: the map around it, its chief in
// a box answering, and the orders as commands. See docs/rules/orders.md.
#pragma once

#include "mentat/engine/talk.hpp"
#include "mentat/gfx/scene.hpp"
#include "mentat/gfx/surface.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace mentat {

class Game;
class MapScreen;
class Panel;
class SpiceMap;
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
    static constexpr std::uint16_t kWindTrap = 0x6A2B;
    static constexpr std::uint16_t kAttack = 0x6A2F;
    static constexpr std::uint16_t kEspionage = 0x6A45;
    static constexpr std::uint16_t kSearchSpice = 0x776D;
    static constexpr std::uint16_t kSearchArmy = 0x7734;
    static constexpr std::uint16_t kSearchEcology = 0x775C;
    static constexpr std::uint16_t kCancel = 0xD2E2;

    Orders(Game& game, World& world, Panel& panel, MapScreen& map, Talk& talk, SpiceMap& spice);

    bool active() const { return troop_ != 0; }
    int troop() const { return troop_; }
    // The troop last given orders, 0 at first.
    int lastTroop() const { return last_; }

    // Opens the orders for troop `troop`, from a talk with its chief
    // (`fromRoom`) or from the map: the map around the place, dissolved
    // onto the screen, and the chief's box over it.
    void open(int troop, bool fromRoom = true);
    // The troop after `after` (by number, going round) of Paul's that the
    // map marks; 0 when none.
    int nextMarkedTroop(int after) const;
    // A command of the orders. False when it is not one.
    bool command(std::uint16_t action);
    // Closes the orders; the caller shows the room again.
    void close();

    // Whether the small map is open for the troop to be shown where to go.
    bool choosing() const { return choosing_; }
    // Whether the troop's equipment is being changed.
    bool equipping() const { return equipping_; }
    // The box of the equipment the place keeps, while equipping.
    ClipRect placeBox() const { return freeBox_; }
    // A click on the screen while choosing or equipping, not on the
    // commands.
    void click(int x, int y);

    // The chief's box, over the map.
    ClipRect box() const { return box_; }

    // The orders are over and Paul is back in the room.
    std::function<void()> finished;
    // The troop was sent somewhere, or left: the orders are over and the
    // map stays, with its own commands.
    std::function<void()> sent;

private:
    void drawBox();
    void setMenu();
    void nextTroop();
    // The orders end; the map stays, or with `leaveMap` (orders given in
    // the room) Paul goes back to the room.
    void leave(bool leaveMap);
    void occupationMenu();
    void specialize(int occupation);
    // An occupation of the troop's speciality: its bits 0 and 1.
    void specializeWithin(int occupation);
    // The troop spies out the nearest fort not found: it is asked to go there.
    void spy();
    // The chief answers whether his troop goes to `place`; on a yes it sets
    // off and the orders are over. False on a no.
    bool sendTo(std::size_t place);
    void say(int list);
    void chooseDestination();
    void stopChoosing();
    void confirmDestination(std::size_t place);
    void equipped();
    void searchEquipment(int first, int last);
    void answer(std::uint8_t situation);
    void openEquipment();
    void drawEquipment();
    void drawHeld();
    void closeEquipment();
    void clickEquipment(int x, int y);
    void clickSmallMap(int x, int y);
    // Where each piece of a list of equipment was drawn: its first column
    // and the column after it, or 0 and 0 when none was.
    using Spans = std::array<std::array<int, 2>, 7>;
    void drawEquipmentList(const std::array<int, 7>& counts, int x, int y, int bottom, std::uint16_t colour,
                           Spans& spans);

    Game& g_;
    World& world_;
    Panel& panel_;
    MapScreen& map_;
    Talk& talk_;
    SpiceMap& spice_;
    int troop_ = 0;
    int last_ = 0;
    bool fromRoom_ = true;
    // Which list of orders is shown (its data offset).
    std::size_t list_ = 0;
    bool choosing_ = false;
    ClipRect box_{};
    // The last line of news showed the troop's equipment.
    bool heldShown_ = false;
    bool equipping_ = false;
    std::array<int, 7> held_{};
    std::array<int, 7> free_{};
    Spans heldAt_{};
    Spans freeAt_{};
    ClipRect heldBox_{};
    ClipRect freeBox_{};
};

// The box a troop's chief talks in (border and fill from data offset
// 0x18E9, a framed place for the face at its left), drawn on the screen,
// the back buffer and the saved background at `where`, for portrait
// `face`; what Talk::startBox() needs to talk in it.
Talk::Box drawChiefBox(Game& game, const ClipRect& where, int face);
// The box at the top of the screen.
ClipRect chiefBoxHigh();

} // namespace mentat
