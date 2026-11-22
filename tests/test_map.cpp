// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/data/program_tables.hpp"
#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/fiber.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/globe_screen.hpp"
#include "mentat/engine/intro.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/surface.hpp"

#include <cstdint>
#include <set>
#include <vector>

using namespace mentat;

namespace {

// Latitude tables for a small planet: every latitude has 100 points each
// side (200 around), rows 256 bytes apart.
std::vector<std::uint8_t> smallLatitudes() {
    std::vector<std::uint8_t> t(FlatMap::kRows * 8, 0);
    for (int r = 0; r < FlatMap::kRows; ++r) {
        const int base = r * 256;
        t[static_cast<std::size_t>(8 * r)] = static_cast<std::uint8_t>(base >> 8);
        t[static_cast<std::size_t>(8 * r + 1)] = static_cast<std::uint8_t>(base);
        t[static_cast<std::size_t>(8 * r + 3)] = 100;
    }
    return t;
}

constexpr int kMiddle = 0x62FC;

} // namespace

MTT_TEST("flat map: one colour stays one colour, and the edge rounds the poles") {
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0x05);
    FlatMap::Edges edges;
    edges.fade = {0x10, 0x20, 0x30, 0x40, 0x40, 0x30, 0x20, 0x10};
    for (std::size_t i = 0; i < edges.north.size(); i += 2) {
        edges.north[i] = 10;      // 10 at each side
        edges.north[i + 1] = 146; // 292 in the middle
    }
    const FlatMap m(smallLatitudes(), map, edges);
    MTT_CHECK(m.width(5) == 100 && m.pointSize(5) == 328);
    MTT_CHECK(m.rowStart(-3) == kMiddle - 768 && m.rowStart(3) == kMiddle + 768);
    MTT_CHECK(m.pointAt(0x8000, 0) == 100);
    MTT_CHECK(m.pointAt(0xFFFF, 0) == 200); // rounds up past the last point

    Surface s(320, 200);
    m.draw(s, 4, 4, 0x1234, 10);
    std::set<int> colours;
    for (int y = 4; y < 4 + FlatMap::kHeight; ++y) {
        for (int x = 4; x < 4 + FlatMap::kWidth; ++x) {
            colours.insert(s.at(x, y));
        }
    }
    MTT_CHECK(colours == std::set<int>{16 + 5});
    MTT_CHECK(s.at(3, 3) == 0 && s.at(316, 148) == 0);

    // At the north pole the top rows have their sides cut off.
    m.draw(s, 4, 4, 0, -90);
    MTT_CHECK(s.at(4, 4) == 16);              // a 0 value
    MTT_CHECK(s.at(4 + 6, 4) == 16 + 1);      // the first edge value
    MTT_CHECK(s.at(4 + 9, 4) == 16 + 4);
    MTT_CHECK(s.at(4 + 10, 4) == 16 + 5);     // the map
    MTT_CHECK(s.at(4 + 10 + 292, 4) == 16 + 4); // the right edge
    MTT_CHECK(s.at(4 + 311, 4) == 16);
}

MTT_TEST("flat map: four pixels to a point, stepping from the point before") {
    // Colour 0 and 8 in turns along every row: each point's four pixels
    // run from the colour before it to its own.
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0);
    for (std::size_t i = 0; i < map.size(); ++i) {
        map[i] = (i % 2 == 0) ? 0 : 8;
    }
    const FlatMap m(smallLatitudes(), map, FlatMap::Edges{});
    Surface s(320, 200);
    m.draw(s, 0, 0, 0, 0);
    std::set<int> colours;
    for (int x = 0; x < FlatMap::kWidth; ++x) {
        colours.insert(s.at(x, 72));
    }
    // 8 is the value 0x88, 0 is 0x08. Their difference, 0x80 either way, is
    // -128 as a signed byte: steps of -32 that wrap round below 0.
    MTT_CHECK(colours == (std::set<int>{16, 18, 20, 22, 24, 26, 28, 30}));
}

MTT_TEST("world: places settle onto the map's points when the program starts") {
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0x05);
    std::vector<std::uint8_t> regions(map.size(), 3);
    regions[static_cast<std::size_t>(kMiddle + 2 * 256 + 50)] = 9;
    FlatMap m(smallLatitudes(), map, FlatMap::Edges{});
    World w;
    const std::size_t p = World::placeRecord(1);
    w.setU8(p, 0x14);                // kind 4 for its troops
    w.setU16(p + 2, 50 * 328 + 100); // a little past point 50
    w.setU16(p + 4, 2);
    w.setU8(p + 9, 2);               // troop 2, then troop 1
    w.setU8(World::troopRecord(2) + 1, 1);
    w.setU8(World::troopRecord(1) + 0x12, 0x7F);
    w.setU8(World::placeRecord(2), 0xFF);
    w.setUpPlaces(m, regions);
    // Point 50 of 200 starts at 50 × 65536 / 200.
    MTT_CHECK(w.u16(p + 2) == 16384);
    MTT_CHECK(w.u16(p + 6) == kMiddle + 2 * 256 + 50);
    MTT_CHECK((m.bytes()[static_cast<std::size_t>(kMiddle + 2 * 256 + 50)] & 0x40) != 0);
    MTT_CHECK(w.u8(p + 0x10) == 9 && w.u8(p + 0x11) == 0); // (1 + 7) / 16
    for (const int t : {1, 2}) {
        const std::size_t r = World::troopRecord(t);
        MTT_CHECK(w.u16(r + 4) == p && w.u16(r + 6) == 16384 && w.u16(r + 8) == 2);
    }
    MTT_CHECK(w.u8(World::troopRecord(1) + 0x12) == (0x70 | 0x04 | 0x80));
    MTT_CHECK(w.u8(World::troopRecord(2) + 0x12) == (0x04 | 0x80));
}

MTT_TEST("map screen: opens on the palace and the arrows move it") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables::fromDriver(archive->load("DNVGA.HSQ")), nullptr);
    World w(*program);
    Panel panel(g, kEnglish);
    MapScreen map(g, w, panel);
    map.setUp();
    const std::uint16_t palace = w.u16(World::placeRecord(1) + 2);
    map.enter();
    MTT_CHECK(map.active());
    MTT_CHECK(map.longitude() == palace && map.latitude() == static_cast<std::int16_t>(w.u16(World::placeRecord(1) + 4)));
    MTT_CHECK(panel.commands().size() == 4 && panel.commands()[0].action == MapScreen::kToggle);
    MTT_CHECK((panel.commands()[2].phrase & Command::kGrey) != 0);
    // The view is mostly the map's colours, with the places over it, framed.
    int mapColours = 0;
    int samples = 0;
    for (int y = 4; y < 148; y += 8) {
        for (int x = 4; x < 316; x += 8) {
            const int c = g.display().back().at(x, y);
            mapColours += c >= 16 && c < 32 ? 1 : 0;
            ++samples;
        }
    }
    MTT_CHECK(mapColours * 10 > samples * 9);
    MTT_CHECK(g.display().back().at(0, 0) == 0xF6 && g.display().back().at(3, 3) == 0xFC);
    MTT_CHECK(map.command(MapScreen::kRight) && map.longitude() == static_cast<std::uint16_t>(palace + 0x1002));
    MTT_CHECK(map.command(MapScreen::kUp) && map.latitude() == static_cast<std::int16_t>(w.u16(World::placeRecord(1) + 4)) - 12);
    MTT_CHECK(map.command(MapScreen::kCentre) && map.longitude() == palace);
    MTT_CHECK(!map.command(0x1234));
    map.leave();
    MTT_CHECK(!map.active() && !map.command(MapScreen::kUp));
}

MTT_TEST("globe screen: opens on the map's middle, turns with the arrows and goes back to Paul") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables::fromDriver(archive->load("DNVGA.HSQ")), nullptr);
    World w(*program);
    Panel panel(g, kEnglish);
    MapScreen map(g, w, panel);
    map.setUp();
    map.enter();
    map.leave();
    GlobeScreen globe(g, w, panel, map);
    globe.enter(map.longitude(), map.latitude());
    MTT_CHECK(globe.active());
    // The planet's frame and the frescoes, with the planet's colours inside.
    const Surface& back = g.display().back();
    const int middle = back.at(160, 75);
    MTT_CHECK(middle >= 0x10 && middle < 0x2C);
    MTT_CHECK(panel.commands().size() == 5 && panel.commands()[0].action == GlobeScreen::kExit);
    MTT_CHECK((panel.commands()[0].phrase & Command::kGrey) == 0 && (panel.commands()[2].phrase & Command::kGrey) != 0);
    MTT_CHECK(panel.zones()[Panel::kArrows].action == GlobeScreen::kExit);
    MTT_CHECK(panel.zones()[Panel::kArrows + 1].action == GlobeScreen::kUp);
    // Over the planet the list offers the map of the area.
    MTT_CHECK(globe.pointAt(160, 80) && panel.commands().size() == 1);
    MTT_CHECK(!globe.pointAt(161, 80));
    MTT_CHECK(globe.pointAt(20, 80) && panel.commands().size() == 5);
    MTT_CHECK(globe.command(GlobeScreen::kUp) && globe.command(GlobeScreen::kLeft));
    MTT_CHECK(!globe.command(0x1234));
    MTT_CHECK(!globe.leaving);
    Fiber fiber([&] { globe.command(GlobeScreen::kExit); });
    p.fiber = &fiber;
    while (!fiber.finished()) {
        fiber.resume();
        p.ticks += 3;
    }
    p.fiber = nullptr;
    std::uint16_t lon = 0;
    int lat = 0;
    w.position(lon, lat);
    MTT_CHECK(globe.leaving && globe.mapLongitude == lon && globe.mapLatitude == lat);
    globe.leave();
    MTT_CHECK(!globe.active() && !globe.command(GlobeScreen::kUp));
}
