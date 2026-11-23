// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/data/program_tables.hpp"
#include "mentat/engine/cockpit.hpp"
#include "mentat/engine/dialogue.hpp"
#include "mentat/engine/flat_map.hpp"
#include "mentat/engine/flight.hpp"
#include "mentat/engine/fiber.hpp"
#include "mentat/engine/game.hpp"
#include "mentat/engine/globe_screen.hpp"
#include "mentat/engine/intro.hpp"
#include "mentat/engine/map_screen.hpp"
#include "mentat/engine/panel.hpp"
#include "mentat/engine/places.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/engine/saved_games.hpp"
#include "mentat/engine/spice_map.hpp"
#include "mentat/engine/vision.hpp"
#include "mentat/engine/world.hpp"
#include "mentat/gfx/surface.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
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

MTT_TEST("flat map: the small map is a pixel a point, and the rim closes in near the poles") {
    // Each point's colour is its number around the ring, with the bits
    // above the colour set too.
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0);
    for (int r = -98; r <= 98; ++r) {
        const int start = kMiddle + (r < 0 ? -1 : 1) * std::abs(r) * 256;
        for (int point = 0; point < 200; ++point) {
            map[static_cast<std::size_t>(start + point)] = static_cast<std::uint8_t>(0x30 | (point & 0x0F));
        }
    }
    FlatMap::Edges edges;
    edges.poles.fill(10);
    const FlatMap m(smallLatitudes(), map, edges);
    Surface s(320, 200);
    MTT_CHECK(m.drawSmall(s, 0, 0, 160, 89, 0, 0) == 0);
    // The middle column shows point 0; the first, 80 points before it.
    MTT_CHECK(s.at(80, 44) == 16 && s.at(81, 44) == 17);
    MTT_CHECK(s.at(0, 44) == 16 + (120 & 0x0F) && s.at(0, 0) == s.at(0, 88));
    // Too near a pole: the middle goes no further than latitude 42, and
    // rows from latitude 70 have the rim.
    MTT_CHECK(m.drawSmall(s, 0, 0, 160, 89, 0, 60) == 42);
    const int y = 70 - (42 - 44);
    MTT_CHECK(s.at(80, y - 1) == 16);
    MTT_CHECK(s.at(5, y) == 0 && s.at(6, y) == 0x1C && s.at(9, y) == 0x17 && s.at(10, y) >= 16 && s.at(10, y) < 32);
    MTT_CHECK(s.at(150, y) == 0x17 && s.at(153, y) == 0x1C && s.at(154, y) == 0 && s.at(159, y) == 0);
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

MTT_TEST("flat map: a disc of radius 5 covers ten rows and wraps round the planet") {
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0);
    FlatMap m(smallLatitudes(), map, FlatMap::Edges{});
    m.disc(0, 0, 5, 0x5D, [](std::uint8_t& b) { b = 1; });
    // Points either side of the middle, from 5 rows above to 4 below.
    const int half[] = {3, 5, 5, 6, 6, 6, 6, 5, 5, 3};
    for (int row = -6; row <= 5; ++row) {
        const int h = row < -5 || row > 4 ? -1 : half[row + 5];
        for (int p = -8; p <= 8; ++p) {
            const int at = m.rowStart(row) + (p + 200) % 200;
            MTT_CHECK(m.bytes()[static_cast<std::size_t>(at)] == (std::abs(p) <= h ? 1 : 0));
        }
    }
}

MTT_TEST("world: a troop alone at a Harkonnen fort takes it") {
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0x05);
    FlatMap m(smallLatitudes(), map, FlatMap::Edges{});
    m.bytes()[static_cast<std::size_t>(m.rowStart(2) + 51)] = 0x15; // a spice field found
    World w;
    const std::size_t fort = World::placeRecord(1);
    w.setU8(fort, 0x01);
    w.setU16(fort + 2, 16384); // point 50
    w.setU16(fort + 4, 2);
    w.setU8(fort + 8, 0x2D);
    const std::size_t other = World::placeRecord(2);
    w.setU8(other + 8, 0x2D);
    w.setU8(other + 0x12, 0x40);
    w.setU8(other + 0x19, 12);
    w.setU8(World::placeRecord(3), 0xFF);
    w.setU16(0x02, 0x35); // day 3
    w.setU16(0x00, 2);    // no prisoner kept
    w.setU8(0x29, 1);
    for (int n = 1; World::troopRecord(n) < 0xFBB; ++n) {
        w.setU8(World::troopRecord(n), static_cast<std::uint8_t>(n));
        w.setU8(World::troopRecord(n) + 3, 0xA0); // nobody's
    }
    const std::size_t t = World::troopRecord(2);
    w.setU8(t + 3, 0x40); // on its way
    w.setU16(t + 4, static_cast<std::uint16_t>(fort));
    w.setU16(t + 6, 16384);
    w.setU16(t + 8, 2);
    w.setU16(t + 0x12, 0x2002);
    w.setU8(t + 0x15, 0x1A);
    w.setU8(t + 0x17, 0x3C);
    w.troopsWork(0, m);
    MTT_CHECK(w.u8(fort + 9) == 2);
    MTT_CHECK(w.u8(fort + 0x0A) == 0x08 && w.u8(fort + 0x0B) == 5);
    MTT_CHECK(w.u8(0x29) == 5);
    MTT_CHECK(w.u16(0x115A) == 0x0001);
    MTT_CHECK(w.u8(t + 3) == 4);
    MTT_CHECK(w.u16(t + 0x10) == 0x0520);
    MTT_CHECK(w.u16(t + 0x12) == 0x6002);
    MTT_CHECK(w.u8(t + 0x15) == 0x20 && w.u8(t + 0x17) == 0x3F);
    // One Harkonnen fort left: the story notes it, and the fort gets the
    // equipment kept everywhere, less 10.
    MTT_CHECK(w.u16(0xA8) == 8);
    MTT_CHECK(w.u8(0xC2) == 1);
    MTT_CHECK(w.u8(fort + 0x19) == 2);
    MTT_CHECK(m.bytes()[static_cast<std::size_t>(m.rowStart(2) + 50)] == 0x25);
    MTT_CHECK(m.bytes()[static_cast<std::size_t>(m.rowStart(2) + 51)] == 0x15);
    MTT_CHECK(m.bytes()[static_cast<std::size_t>(m.rowStart(2) + 57)] == 0x05);
}

MTT_TEST("world: a troop mining spice brings it in hour by hour") {
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0x05);
    FlatMap m(smallLatitudes(), map, FlatMap::Edges{});
    World w;
    const std::size_t place = World::placeRecord(1);
    w.setU8(place + 8, 0x01);
    w.setU8(place + 0x0A, 0x40); // prospected
    w.setU8(place + 0x11, 28);
    w.setU8(place + 0x12, 0x54);
    w.setU8(World::placeRecord(2), 0xFF);
    w.setU16(0x02, 0x05);
    for (int n = 1; World::troopRecord(n) < 0xFBB; ++n) {
        w.setU8(World::troopRecord(n), static_cast<std::uint8_t>(n));
        w.setU8(World::troopRecord(n) + 3, 0xA0);
    }
    const std::size_t t = World::troopRecord(1);
    w.setU8(t + 3, 0); // mining
    w.setU16(t + 4, static_cast<std::uint16_t>(place));
    w.setU8(t + 0x15, 50);
    w.setU8(t + 0x16, 0x20);
    w.setU8(t + 0x19, 0x80); // a harvester
    w.setU8(t + 0x1A, 100);
    // (50 + 0x20) × 100 men, its high byte 32, times 0x51, over 128: 20.
    w.troopsWork(0, m);
    MTT_CHECK(w.u16(t + 0x0C) == 20 && w.u16(t + 0x0E) == 20);
    MTT_CHECK(w.u16(t + 0x10) == 0x0108);
    MTT_CHECK(w.u16(0xA0) == 2);
    MTT_CHECK(w.u8(place + 0x13) == 20 && w.u8(place + 0x12) == 0x54);
    w.troopsWork(0, m);
    MTT_CHECK(w.u16(t + 0x0E) == 40 && w.u16(0xA0) == 4);
    MTT_CHECK(w.u8(place + 0x13) == 12 && w.u8(place + 0x12) == 0x53);
}

MTT_TEST("world: a strong troop fighting the last Harkonnens of a fort takes it") {
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0x05);
    FlatMap m(smallLatitudes(), map, FlatMap::Edges{});
    World w;
    const std::size_t fort = World::placeRecord(1);
    w.setU8(fort + 8, 0x2D);
    w.setU8(fort + 9, 1);
    w.setU8(World::placeRecord(2), 0xFF);
    for (int n = 1; World::troopRecord(n) < 0xFBB; ++n) {
        w.setU8(World::troopRecord(n), static_cast<std::uint8_t>(n));
        w.setU8(World::troopRecord(n) + 3, 0xA0);
    }
    const std::size_t ours = World::troopRecord(1);
    w.setU8(ours + 1, 2);
    w.setU8(ours + 3, 6); // fighting
    w.setU16(ours + 4, static_cast<std::uint16_t>(fort));
    w.setU8(ours + 0x15, 50);
    w.setU8(ours + 0x17, 0x20);
    w.setU8(ours + 0x1A, 100);
    const std::size_t theirs = World::troopRecord(2);
    w.setU8(theirs + 2, 9);
    w.setU8(theirs + 3, 0x80);
    w.setU16(theirs + 4, static_cast<std::uint16_t>(fort));
    w.setU8(theirs + 0x10, 0x80);
    w.setU8(theirs + 0x1A, 1);
    w.random2 = [] { return 0; }; // below the lean: Paul's troop strikes
    w.troopsWork(0, m);
    // Worth 3 against 1: the blow kills the last man, and the fort falls;
    // the Harkonnens there stay as Fremen.
    MTT_CHECK(w.u8(0x9C) == 0xFC);
    MTT_CHECK((w.u8(fort + 0x0A) & 0x08) != 0);
    MTT_CHECK(w.u8(ours + 3) == 4);
    MTT_CHECK(w.u8(theirs + 3) == 0xA0 && (w.u8(theirs + 0x10) & 0x80) == 0);
}

MTT_TEST("world: a new day keeps the spice accounts, and hour 15 the Harkonnens grow") {
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0x05);
    FlatMap m(smallLatitudes(), map, FlatMap::Edges{});
    World w;
    w.setU8(World::placeRecord(1), 0xFF);
    w.setU16(World::placeRecord(1), 0xFFFF);
    for (int n = 1; World::troopRecord(n) < 0xFBB; ++n) {
        w.setU8(World::troopRecord(n), static_cast<std::uint8_t>(n));
        w.setU8(World::troopRecord(n) + 3, 0xA0);
    }
    w.setU16(0x1174, 0x1F);
    w.setU16(0x02, 0x20); // day 2 begins
    w.setU16(0xA0, 500);  // the stock
    w.setU16(0x1170, 300); // the stock a day ago
    w.setU16(0x1172, 20);
    w.setU16(0xAE, 200); // what was made the day before
    w.hourPassed(0, m);
    MTT_CHECK(w.u16(0x1174) == 0x20 && w.u8(0xFF) == 1);
    MTT_CHECK(w.u16(0xA6) == 220 && w.u16(0xAE) == 220);
    MTT_CHECK(w.u16(0xB0) == 20 && w.u16(0xB2) == 0);
    MTT_CHECK(w.u16(0x1170) == 500 && w.u16(0x1172) == 0);
    MTT_CHECK(w.u16(0xA2) == 1 && w.u16(0xA4) == 0);

    const std::size_t harkonnens = World::troopRecord(5);
    w.setU8(harkonnens + 3, 0x80);
    w.setU8(harkonnens + 0x10, 0x80);
    w.setU8(harkonnens + 0x1A, 50);
    const std::size_t full = World::troopRecord(6);
    w.setU8(full + 3, 0x80);
    w.setU8(full + 0x10, 0x80);
    w.setU8(full + 0x1A, 200);
    w.setU16(0x02, 0x2F); // hour 15
    w.setU16(0x00, 0x8000); // the bit that comes round is set
    w.hourPassed(0, m);
    MTT_CHECK(w.u8(harkonnens + 0x1A) == 51 && w.u8(full + 0x1A) == 200);
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
    Panel panel(g);
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

MTT_TEST("map screen: a click on a place shows its label, another click takes it away") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables::fromDriver(archive->load("DNVGA.HSQ")), nullptr);
    World w(*program);
    Panel panel(g);
    MapScreen map(g, w, panel);
    map.setUp();
    map.enter();
    Display& d = g.display();
    d.copyRect(d.back(), d.screen(), map.view());
    const std::size_t palace = World::placeRecord(1);
    // The palace is in the middle; its label, the name alone, to its right.
    map.click(158, 75);
    MTT_CHECK(map.labelled() == palace);
    const ClipRect r = map.label();
    MTT_CHECK(r.left == 175 && r.top == 61 && r.right == 281 && r.bottom == 91);
    MTT_CHECK(d.screen().at(175, 61) != d.back().at(175, 61) || d.screen().at(200, 80) != d.back().at(200, 80));
    // Again on the same place: it goes, the map shows again.
    map.click(160, 76);
    MTT_CHECK(map.labelled() == 0 && d.screen().at(200, 80) == d.back().at(200, 80));
    map.click(160, 76);
    MTT_CHECK(map.labelled() == palace);
    // Far from any place: it goes.
    map.click(60, 40);
    MTT_CHECK(map.labelled() == 0);
    // A fort whose inside is shown gets the tallest label; one Paul knows
    // the inside of gets the middle one, with the battle.
    w.setU8(palace + 8, 0x28);
    w.setU8(palace + 0x0A, 0x18);
    map.click(160, 76);
    MTT_CHECK(map.label().bottom - map.label().top == w.u8(0x11D0));
    map.click(160, 76);
    w.setU8(palace + 0x0A, 0x02);
    map.click(160, 76);
    MTT_CHECK(map.label().bottom - map.label().top == w.u8(0x11D1));
    // Moving the map takes it away.
    MTT_CHECK(map.command(MapScreen::kUp) && map.labelled() == 0);
}

MTT_TEST("places: the palace's entrance looks outside, with its ornithopter in front") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables::fromDriver(archive->load("DNVGA.HSQ")), nullptr);
    Places& places = g.places();
    places.enter(0x2001, 0x0180);
    g.display().targetBack();
    const Surface& back = g.display().back();
    places.drawView();
    MTT_CHECK(back.at(10, 10) == 0 && back.at(210, 90) == 0);
    places.drawView([&] { places.drawOutside(0x20, 1); });
    MTT_CHECK(back.at(10, 10) != 0);
    const Surface before = back;
    places.drawOrnithopters(1, false);
    auto changed = [&](int left, int top, int right, int bottom) {
        int n = 0;
        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                n += back.at(x, y) != before.at(x, y) ? 1 : 0;
            }
        }
        return n;
    };
    MTT_CHECK(changed(200, 70, 320, 152) > 500);
    // The folded wings reach past the scene's right edge, and are cut there
    // rather than going on at the left.
    MTT_CHECK(changed(0, 75, 60, 120) == 0);
    // Taking off, it rises and goes; at the last step nothing of it is left.
    Surface& target = g.display().back();
    target = before;
    places.drawTakeOff(1, false, 16, 1);
    MTT_CHECK(changed(0, 0, 320, 152) > 500);
    target = before;
    places.drawTakeOff(1, false, 32, 1);
    MTT_CHECK(changed(0, 0, 320, 152) == 0);
}

MTT_TEST("cockpit: the map around Paul, the names of what the mouse points at, and Cancel") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables::fromDriver(archive->load("DNVGA.HSQ")), nullptr);
    World w(*program);
    w.goTo(0x2001, 0x0180);
    Panel panel(g);
    MapScreen map(g, w, panel);
    map.setUp();
    Cockpit cockpit(g, w, panel, map);
    cockpit.enter();
    cockpit.drawScene();
    MTT_CHECK(cockpit.active());
    const ClipRect r = cockpit.view();
    MTT_CHECK(r.left == 81 && r.top == 45 && r.right == 241 && r.bottom == 134);
    MTT_CHECK(panel.commands().size() == 1 && panel.commands()[0].action == Cockpit::kCancel);
    const int middle = g.display().back().at(100, 100);
    MTT_CHECK(middle >= 16 && middle < 32);
    cockpit.show();
    // The line above the map: the name of the palace, then the desert to
    // the east of it.
    auto nameShown = [&] {
        for (int x = 85; x < 240; ++x) {
            for (int y = 38; y < 45; ++y) {
                if (p.screen.at(x, y) == 0xFE) {
                    return true;
                }
            }
        }
        return false;
    };
    MTT_CHECK(!nameShown());
    cockpit.pointAt(161, 89);
    MTT_CHECK(nameShown());
    cockpit.pointAt(200, 87);
    MTT_CHECK(nameShown());
    cockpit.pointAt(20, 20);
    MTT_CHECK(!nameShown());
    MTT_CHECK(!cockpit.command(0x1234) && !cockpit.leaving);
    MTT_CHECK(cockpit.command(Cockpit::kCancel) && cockpit.leaving);
    cockpit.leave();
    MTT_CHECK(!cockpit.active() && !cockpit.command(Cockpit::kCancel));
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
    Panel panel(g);
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
    // The results: the frescoes slide away, and the second line goes back.
    MTT_CHECK(panel.commands()[1].action == GlobeScreen::kSeeResults);
    Fiber results([&] { globe.command(GlobeScreen::kSeeResults); });
    p.fiber = &results;
    while (!results.finished()) {
        results.resume();
        p.ticks += 1;
    }
    MTT_CHECK(globe.showingResults() && panel.commands()[1].action == GlobeScreen::kStandardVision);
    MTT_CHECK(back.at(5, 100) == 0xF0); // where the left fresco was
    Fiber standard([&] { globe.command(GlobeScreen::kStandardVision); });
    p.fiber = &standard;
    while (!standard.finished()) {
        standard.resume();
        p.ticks += 1;
    }
    MTT_CHECK(!globe.showingResults() && panel.commands()[1].action == GlobeScreen::kSeeResults);
    MTT_CHECK(back.at(5, 100) != 0xF0);
    // With nowhere to keep games, loading and saving are greyed.
    MTT_CHECK((panel.commands()[2].phrase & Command::kGrey) != 0 && (panel.commands()[3].phrase & Command::kGrey) != 0);
    auto run = [&](const std::function<void()>& f) {
        Fiber step(f);
        p.fiber = &step;
        while (!step.finished()) {
            step.resume();
            p.ticks += 1;
        }
        p.fiber = nullptr;
    };
    {
        const auto folder = std::filesystem::temp_directory_path() / "mentat-test-globe-saves";
        std::filesystem::remove_all(folder);
        Dialogue said = Dialogue::parse(archive->load("DIALOGUE.HSQ"));
        SavedGames saves(folder, *program, w, said, [&]() -> std::vector<std::uint8_t>& { return map.mapBytes(); });
        GlobeScreen withSaves(g, w, panel, map, &saves);
        withSaves.enter(map.longitude(), map.latitude());
        MTT_CHECK((panel.commands()[2].phrase & Command::kGrey) == 0 && panel.commands()[3].action == GlobeScreen::kSave);
        MTT_CHECK(withSaves.command(GlobeScreen::kSave) && panel.commands().size() == 3);
        MTT_CHECK((panel.commands()[0].phrase & Command::kLit) == 0);
        const std::uint16_t first = panel.commands()[0].phrase;
        run([&] { withSaves.command(GlobeScreen::kSaveSlot, first); });
        MTT_CHECK(saves.clockOf(1) == w.clock() && panel.commands().size() == 5);
        MTT_CHECK(withSaves.command(GlobeScreen::kLoad));
        MTT_CHECK((panel.commands()[0].phrase & Command::kGrey) == 0 && (panel.commands()[1].phrase & Command::kGrey) != 0);
        // The line shows the day the game was saved.
        MTT_CHECK(panel.phrases().at(first - 1u).find("DAY  1") != std::string::npos);
        const int clock = w.clock();
        w.setClock(clock + 40);
        run([&] { withSaves.command(GlobeScreen::kLoadSlot, first); });
        MTT_CHECK(withSaves.loaded && w.clock() == clock && withSaves.showingResults());
        withSaves.leave();
        std::filesystem::remove_all(folder);
        globe.enter(map.longitude(), map.latitude());
    }
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

MTT_TEST("game: an effect's parts play their counts, and a repeat can end early") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables::fromDriver(archive->load("DNVGA.HSQ")), nullptr);
    // Plays the effect out, the queue played at once each time, and
    // returns how many plays of sound were queued.
    auto plays = [&](int endAfter) {
        p.sound.clear();
        g.playSound(6);
        int n = 0;
        for (int i = 0; i < 1000; ++i) {
            for (const SoundCommand& c : p.sound) {
                n += c.channel == SoundChannel::Digital && !c.samples.empty() ? 1 : 0;
            }
            p.sound.clear();
            if (i == endAfter) {
                g.endSoundRepeat();
            }
            p.soundQueued[static_cast<int>(SoundChannel::Digital)] = 0;
            g.pollInput();
        }
        return n;
    };
    const int whole = plays(-1);
    MTT_CHECK(whole > 3);
    // Ending the first repeat early leaves out its other plays.
    MTT_CHECK(plays(0) < whole);
}

MTT_TEST("flight: to a place a step at a time, the corner's map over the video, and landing") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables::fromDriver(archive->load("DNVGA.HSQ")), nullptr);
    World w(*program);
    w.goTo(0x2001, 0x0180);
    Panel panel(g);
    MapScreen map(g, w, panel);
    map.setUp();
    // The nearest place shown on the map, other than the palace.
    const std::size_t palace = World::placeRecord(1);
    std::size_t to = 0;
    int best = 1 << 30;
    for (std::size_t r = World::kPlaces + World::kPlaceSize; w.u16(r) != 0xFFFF; r += World::kPlaceSize) {
        if (w.u8(r + 0x0A) & 0x80) {
            continue;
        }
        const int d = std::abs(static_cast<std::int16_t>(w.u16(r + 2) - w.u16(palace + 2))) / 128 +
                      std::abs(static_cast<std::int16_t>(w.u16(r + 4)) - static_cast<std::int16_t>(w.u16(palace + 4)));
        if (d < best) {
            best = d;
            to = r;
        }
    }
    MTT_CHECK(to != 0);
    const int ornithopters = w.u8(to + 0x15);
    Flight f(g, w, panel, map);
    f.setDestination(to);
    f.begin();
    MTT_CHECK(f.active() && w.flying() && w.placeHere() == 0);
    f.show();
    MTT_CHECK(panel.commands().size() == 2 && panel.commands()[0].action == Flight::kSkip &&
              panel.commands()[1].action == Flight::kChange);
    MTT_CHECK(f.videoSlot() == 2);
    // The corner: the small map framed, over the video's frame.
    const Surface& back = g.display().back();
    MTT_CHECK(back.at(200, 0) == 0xF6 && back.at(203, 3) == 0xFC);
    int mapColours = 0;
    for (int y = 6; y < 58; y += 4) {
        for (int x = 206; x < 314; x += 4) {
            mapColours += back.at(x, y) >= 16 && back.at(x, y) < 32 ? 1 : 0;
        }
    }
    MTT_CHECK(mapColours > 13 * 27 * 3 / 4);
    // Each step comes nearer, and the place is reached.
    int steps = 0;
    while (!f.arrived && steps < 500) {
        f.step();
        ++steps;
    }
    MTT_CHECK(f.arrived && f.approach && steps > 1 && steps < 500);
    f.land();
    MTT_CHECK(!f.active() && !w.flying() && w.inside() && (w.room() & 0xFF) == 1);
    MTT_CHECK(World::placeRecord(w.place() >> 8) == to && w.home() == to && w.placeHere() == to);
    MTT_CHECK(w.u8(to + 0x15) == ornithopters + 1);
    MTT_CHECK(f.approachVideo() >= 6 && f.approachVideo() <= 10);

    // Skipping goes straight there.
    w.goTo(0x2001, 0x0180);
    f.setDestination(to);
    f.begin();
    f.show();
    const int clock = w.clock();
    MTT_CHECK(f.command(Flight::kSkip) && f.arrived && !f.approach);
    MTT_CHECK(w.clock() >= clock);
    f.land();
    MTT_CHECK(World::placeRecord(w.place() >> 8) == to);
    MTT_CHECK(!f.command(Flight::kSkip));

    // A worm rides in its own video, and leaves the ornithopters alone.
    w.goTo(0x2001, 0x0180);
    const int before = w.u8(to + 0x15);
    f.worm = true;
    f.setDestination(to);
    f.begin();
    f.show();
    MTT_CHECK(f.videoSlot() == Flight::kWormVideo);
    MTT_CHECK(f.command(Flight::kSkip) && f.arrived);
    f.land();
    MTT_CHECK(World::placeRecord(w.place() >> 8) == to && w.u8(to + 0x15) == before);
}

MTT_TEST("world: flying takes the followers along, and landing finds a hidden place") {
    World w;
    w.goTo(0x2001, 0x0180);
    // Character 0 goes with the player; character 1 stays.
    w.setU16(0xFD8, 0x2001);
    w.setU16(0xFD8 + 2, 0x0180);
    w.setU8(0xFD8 + 0x0F, 0x40);
    w.setU16(0xFE8, 0x2001);
    w.setU16(0xFE8 + 2, 0x0180);
    w.flyOver(0x1234, -5);
    MTT_CHECK(w.flying() && !w.inside() && w.room() == 0x1234 && static_cast<std::int16_t>(w.place()) == -5);
    MTT_CHECK(w.character(0).room == 0x1234 && w.character(1).room == 0x2001);
    const std::size_t record = World::placeRecord(3);
    w.setU8(record + 8, 0x05);
    w.setU8(record + 0x0A, 0x80);
    w.enterPlace(record);
    MTT_CHECK(!w.flying() && w.room() == 0x0501 && w.place() == 0x0380 && w.home() == record);
    MTT_CHECK((w.u8(record + 0x0A) & 0x80) == 0 && w.u8(0x27) == 1);
    MTT_CHECK(w.character(0).room == 0x0501 && w.character(0).place == 0x0380);
}

MTT_TEST("flight: a click on the desert flies that way, the arrows turn, and back goes home") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables::fromDriver(archive->load("DNVGA.HSQ")), nullptr);
    World w(*program);
    w.goTo(0x2001, 0x0180);
    Panel panel(g);
    MapScreen map(g, w, panel);
    map.setUp();
    Cockpit cockpit(g, w, panel, map);
    cockpit.enter();
    cockpit.drawScene();
    // Off the map nothing is chosen; due west of the palace the desert is.
    MTT_CHECK(!cockpit.destinationAt(20, 20).chosen);
    const Cockpit::Destination west = cockpit.destinationAt(100, 89);
    MTT_CHECK(west.chosen && west.record == 0);
    cockpit.leave();
    Flight f(g, w, panel, map);
    f.setFree(west.longitude, west.latitude);
    MTT_CHECK(f.free() && f.heading() >= 0xB0 && f.heading() <= 0xD0);
    f.begin();
    f.show();
    MTT_CHECK(panel.commands().front().action == Flight::kBack && panel.commands().back().action == Flight::kChange);
    std::uint16_t lon = 0;
    int lat = 0;
    w.position(lon, lat);
    f.step();
    std::uint16_t lon2 = 0;
    w.position(lon2, lat);
    MTT_CHECK(static_cast<std::int16_t>(lon2 - lon) < 0); // westwards
    const int h = f.heading();
    MTT_CHECK(f.command(Flight::kLeft) && f.heading() == ((h - 4) & 0xFF));
    MTT_CHECK(f.command(Flight::kRight) && f.command(Flight::kRight) && f.heading() == ((h + 4) & 0xFF));
    f.step();
    MTT_CHECK(f.heading() == ((h + 4) & 0xFF)); // steered by hand, it keeps its heading
    MTT_CHECK(f.command(Flight::kBack) && !f.free() && f.destination() == w.home());
    MTT_CHECK(panel.commands().front().action == Flight::kSkip);
}

MTT_TEST("flight: a companion sees a hidden place ahead, and the Harkonnens' zone warns") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables::fromDriver(archive->load("DNVGA.HSQ")), nullptr);
    {
        World w(*program);
        w.goTo(0x2001, 0x0180);
        // The story at step 4, Gurney with Paul, and the sietch east of the
        // palace not found yet.
        w.setU8(0x2A, 4);
        w.setU8(0xFD8 + 16 * 4 + 0x0F, static_cast<std::uint8_t>(w.u8(0xFD8 + 16 * 4 + 0x0F) | 0x40));
        w.setU16(0x10, static_cast<std::uint16_t>(w.u16(0x10) | (1 << 4)));
        w.setU16(0x1152, 0xFF04);
        const std::size_t sietch = World::placeRecord(12);
        w.setU8(sietch + 0x0A, 0x80);
        w.setU8(sietch + 0x0B, 0);
        Panel panel(g);
        MapScreen map(g, w, panel);
        map.setUp();
        Cockpit cockpit(g, w, panel, map);
        cockpit.enter();
        cockpit.drawScene();
        const Cockpit::Destination east = cockpit.destinationAt(201, 89);
        cockpit.leave();
        Flight f(g, w, panel, map);
        f.setFree(east.longitude, east.latitude);
        f.begin();
        f.show();
        std::uint16_t lon = 0;
        int lat = 0;
        int steps = 0;
        while (f.notice == Flight::Notice::None && steps < 20) {
            f.step();
            ++steps;
        }
        w.position(lon, lat);
        // As the original: seen on the right, four points before reaching it.
        MTT_CHECK(f.notice == Flight::Notice::Sighting && f.noticedBy == 4 && f.paused);
        MTT_CHECK(lon == 0x22C0);
        MTT_CHECK(!(w.u8(sietch + 0x0A) & 0x80) && !f.free() && f.destination() == sietch);
        MTT_CHECK(w.u8(0x23) == 3 && w.u8(0xE1) == 1 && w.u16(0x11F3) == 0x48 && w.u16(0x11F5) == 0xD0);
        MTT_CHECK(panel.commands().front().action == Flight::kSkip);
        f.endNotice();
        MTT_CHECK(f.notice == Flight::Notice::None && w.u8(0x23) == 0);
    }
    {
        // Nobody on board, flying north: the cockpit warns at the zone's
        // edge, and again after a point outside it.
        World w(*program);
        w.goTo(0x2001, 0x0180);
        Panel panel(g);
        MapScreen map(g, w, panel);
        map.setUp();
        Cockpit cockpit(g, w, panel, map);
        cockpit.enter();
        cockpit.drawScene();
        const Cockpit::Destination north = cockpit.destinationAt(161, 62);
        cockpit.leave();
        Flight f(g, w, panel, map);
        f.setFree(north.longitude, north.latitude);
        f.begin();
        f.show();
        std::uint16_t lon = 0;
        int lat = 0;
        int steps = 0;
        while (f.notice == Flight::Notice::None && steps < 40) {
            f.step();
            ++steps;
        }
        w.position(lon, lat);
        MTT_CHECK(f.notice == Flight::Notice::Zone && f.noticedBy == -1 && lat == -0x17);
        f.endNotice();
        f.step();
        w.position(lon, lat);
        MTT_CHECK(lat == -0x18 && f.notice == Flight::Notice::None);
        // A step that stays at that latitude, then one into the zone again.
        f.step();
        w.position(lon, lat);
        MTT_CHECK(lat == -0x18 && f.notice == Flight::Notice::None);
        f.step();
        w.position(lon, lat);
        MTT_CHECK(lat == -0x19 && f.notice == Flight::Notice::Zone && !f.caught);
        // Ignoring the warnings, deeper into the zone: caught after eight
        // steps in a row inside it.
        int inZone = 0;
        while (!f.caught && inZone < 20) {
            f.endNotice();
            f.step();
            w.position(lon, lat);
            ++inZone;
        }
        MTT_CHECK(f.caught && f.paused);
        MTT_CHECK(inZone == 7);
    }
}

MTT_TEST("spice map: the regions around the map's middle, its bar and its clicks") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables{}, nullptr);
    World w(*program);
    Panel panel(g);
    MapScreen map(g, w, panel);
    map.setUp();
    const std::size_t sietch = World::placeRecord(12);
    w.enterPlace(sietch);
    g.display().targetBack();
    map.enter();
    SpiceMap spice(g, w, panel, map);
    spice.open();
    MTT_CHECK(spice.shown());
    // Where the state keeps it, a window of 170 x 108 pixels.
    const ClipRect f = spice.frame();
    MTT_CHECK(f.left == static_cast<int>(w.u16(0x11C1)) && f.top == static_cast<int>(w.u16(0x11C3)));
    MTT_CHECK(f.right - f.left == 0xAA && f.bottom - f.top == 0x6C);
    // The screen shows it; the back buffer keeps the map.
    const ClipRect v = spice.view();
    const Surface& screen = g.display().screen();
    int unknown = 0;
    for (int y = v.top; y < v.bottom; ++y) {
        for (int x = v.left; x < v.right; ++x) {
            unknown += screen.at(x, y) == 0x70 ? 1 : 0;
        }
    }
    MTT_CHECK(unknown > 1000);
    MTT_CHECK(g.display().back().at(v.left + 1, v.top + 1) != screen.at(v.left + 1, v.top + 1) ||
              screen.at(v.left + 1, v.top + 1) != 0x70);
    // Paul's sietch is in the middle: pointing at it names it on the bar.
    const int mx = v.left + 80;
    const int my = v.top + 44;
    spice.pointAt(mx, my);
    MTT_CHECK(spice.pointed() == sietch);
    std::size_t place = 0;
    MTT_CHECK(spice.click(mx, my, place) == SpiceMap::Click::Map && place == sietch);
    // The bar switches to the troops' work and back; the box at the top
    // left closes the window.
    MTT_CHECK(spice.click(f.left + 20, f.bottom - 4, place) == SpiceMap::Click::Toggle && spice.occupations);
    MTT_CHECK(spice.click(f.left + 20, f.bottom - 4, place) == SpiceMap::Click::Toggle && !spice.occupations);
    MTT_CHECK(spice.click(f.left + 30, f.top + 3, place) == SpiceMap::Click::Title);
    MTT_CHECK(spice.click(f.left + 4, f.top + 3, place) == SpiceMap::Click::Close);
    spice.close();
    MTT_CHECK(!spice.shown() && screen.at(v.left + 1, v.top + 1) == g.display().back().at(v.left + 1, v.top + 1));
}

MTT_TEST("messages: the queue gives its first message, and the vision shows the place and shimmers") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    Platform p;
    Game g(p, *archive, *program, DisplayTables::fromDriver(archive->load("DNVGA.HSQ")), nullptr);
    World w(*program);
    Panel panel(g);
    MapScreen map(g, w, panel);
    map.setUp();
    // Two messages wait; the first is given, then dropped.
    const std::size_t q = World::kMessageQueue;
    const auto place = static_cast<std::uint16_t>(World::placeRecord(5));
    w.setU8(q, 2);
    w.setU16(q + 1, 0x0F0C);
    w.setU16(q + 3, place);
    w.setU16(q + 5, 0x0F07);
    w.setU16(q + 7, static_cast<std::uint16_t>(World::placeRecord(6)));
    std::uint16_t message = 0;
    std::uint16_t at = 0;
    MTT_CHECK(w.firstMessage(message, at) && message == 0x0F0C && at == place);
    w.dropMessage();
    MTT_CHECK(w.u8(q) == 1 && w.firstMessage(message, at) && message == 0x0F07);
    w.dropMessage();
    MTT_CHECK(w.u8(q) == 0 && !w.firstMessage(message, at));

    // A companion's messages: the chiefs' only about Paul's place.
    w.setU8(q, 4);
    w.setU16(q + 1, 0x0F0C);
    w.setU16(q + 3, place);
    w.setU16(q + 5, 0x0507);
    w.setU16(q + 7, 0);
    w.setU16(q + 9, 0x0F07);
    w.setU16(q + 11, static_cast<std::uint16_t>(World::placeRecord(6)));
    w.setU16(q + 13, 0x0508);
    w.setU16(q + 15, place);
    w.setU16(0x114E, static_cast<std::uint16_t>(World::placeRecord(6)));
    MTT_CHECK(w.messageFrom(5, message, at) && message == 0x0507 && at == 0);
    MTT_CHECK(w.messageFrom(0x0F, message, at) && message == 0x0F07);
    MTT_CHECK(!w.messageFrom(4, message, at));
    w.dropMessagesFrom(5);
    MTT_CHECK(w.u8(q) == 2 && w.u16(q + 1) == 0x0F0C && w.u16(q + 5) == 0x0F07 && w.u16(q + 9) == 0);
    w.dropMessagesFrom(0x0F);
    MTT_CHECK(w.u8(q) == 1 && w.u16(q + 1) == 0x0F0C && w.u16(q + 3) == place);
    w.setU8(q, 0);

    // The vision of a place away from Paul has the small map in its frame.
    Vision v(g, w, map);
    v.draw(place);
    const Surface& back = g.display().back();
    std::set<int> inWindow;
    for (int y = 0x24; y < 0x54; y += 4) {
        for (int x = 0xB4; x < 0x11C; x += 4) {
            inWindow.insert(back.at(x, y));
        }
    }
    MTT_CHECK(inWindow.size() > 4);
    // The shimmer turns the colours 0x80 to 0xBF down by one.
    const Colour second = g.display().palette()[0x81];
    const Colour first = g.display().palette()[0x80];
    v.shimmer();
    MTT_CHECK(g.display().palette()[0x80].r == second.r && g.display().palette()[0x80].b == second.b);
    MTT_CHECK(g.display().palette()[0xBF].r == first.r && g.display().palette()[0xBF].g == first.g);
}

MTT_TEST("world: Duncan proposes amounts by the stock and the Emperor's demand") {
    World w;
    w.setU8(0x10D8, 0xFF); // no smugglers
    w.setU16(0xBC, 400);
    w.setU16(0xA0, 300);
    w.prepareShipment();
    MTT_CHECK(w.u8(0x9F) == 3 && w.u8(0x1A) == 0);
    MTT_CHECK(w.u16(0xB4) == 300 && w.u16(0xB6) == 225 && w.u16(0xB8) == 150 && w.u16(0xBA) == 75);
    MTT_CHECK((w.u8(0xBF) & 6) == 0);
    w.setU16(0xA0, 700); // between one and a half and twice the demand
    w.prepareShipment();
    MTT_CHECK(w.u16(0xB4) == 400 && w.u16(0xB6) == 700 && w.u16(0xB8) == 525 && w.u16(0xBA) == 600);
    MTT_CHECK((w.u8(0xBF) & 6) == 4);
    w.setU16(0xA0, 900);
    w.prepareShipment();
    MTT_CHECK(w.u16(0xB8) == 600 && w.u16(0xBA) == 800 && (w.u8(0xBF) & 6) == 6);
    w.setU16(0xA0, 0);
    w.prepareShipment();
    MTT_CHECK(w.u8(0x9F) == 0);
}

MTT_TEST("world: the second answer accepted picks the second amount") {
    World w;
    w.setU16(0xB4, 10);
    w.setU16(0xB6, 20);
    w.setU8(0x1A, 2);
    w.setU8(0x9F, 2); // refused: nothing
    w.decideShipment(false);
    MTT_CHECK(w.u16(0x1158) == 0);
    w.setU8(0x9F, 1);
    w.decideShipment(false);
    MTT_CHECK(w.u16(0xC0) == 20 && w.u16(0x1158) == 0xFFFF);
}

MTT_TEST("world: a smuggler's bill is paid from the stock, or waits") {
    World w;
    const std::size_t s = 0x10D8 + 0x11;
    w.setU8(0x10D8, 1);
    w.setU8(s, 2);
    w.setU8(s + 0x11, 0xFF);
    w.setU16(s + 0x0E, 40);
    w.setU8(s + 0x10, 0);
    w.setU16(2, 5 << 4); // day 5
    w.setU16(0xA0, 100);
    w.setU8(0x22, 1);
    w.prepareShipment();
    MTT_CHECK(w.u16(0x10B4) == s && w.u16(0x20) == 40 && w.u8(0x1F) == 5 && w.u16(0x11F7) == 2);
    w.setU8(0x9F, 3);
    w.decideShipment(true);
    MTT_CHECK((w.u8(s + 2) & 0x60) == 0x20);
    w.setU8(0x9F, 1);
    w.decideShipment(true);
    MTT_CHECK(w.u16(s + 0x0E) == 0 && w.u16(0xA0) == 60 && w.u16(0x1172) == 40 && w.u8(0x22) == 0);
}

MTT_TEST("world: the smuggler offers his next kind, and haggles") {
    World w;
    const std::size_t s = 0x10D8;
    w.setU8(s + 0x11, 0xFF);
    w.setU8(s + 1, 2);           // his patience
    w.setU8(s + 4 + 0, 3);       // three of the first kind
    w.setU8(s + 4 + 2, 1);       // one of the third
    w.setU8(s + 9 + 0, 0x8A);    // prices, the top bit not counted
    w.setU8(s + 9 + 2, 0x40);
    w.setU8(0x1141, 4);          // four kinds
    w.setU16(0x11F1, 0xE8);      // the first was offered last
    w.setU16(0x10B4, static_cast<std::uint16_t>(s));
    w.setU16(0x114E, 0x200);     // Paul's place
    w.setU16(2, 9 << 4);         // day 9
    std::vector<int> draws;
    w.random = [&](int mask) {
        const int n = draws.empty() ? 0 : draws.front();
        if (!draws.empty()) {
            draws.erase(draws.begin());
        }
        return n & mask;
    };
    draws = {2};
    w.smugglerOffers();
    MTT_CHECK(w.u16(0x11F1) == 0xEA && w.u8(0x9D) == 0x80 && w.u8(0x9E) == 2 && w.u8(s + 3) == 9);

    // Arguing: he gives way by an eighth while Paul has argued less than his
    // patience, then no more.
    w.setU8(0x1A, 0);
    draws = {2};
    MTT_CHECK(w.smugglerAnswered(3) == 3);
    MTT_CHECK(w.u8(0x9D) == 0x70 && w.u8(0x9E) == 3);
    w.setU8(0x1A, 1);
    draws = {1};
    MTT_CHECK(w.smugglerAnswered(3) == 3 && w.u8(0x9D) == 0);
    draws = {0};
    w.setU8(0x9D, 0x70);
    MTT_CHECK(w.smugglerAnswered(3) == 3 && w.u8(0x9D) == 0x70 && (w.u8(0x9E) & 0x10));

    // Refusing ends the offer, now and then he asks again.
    draws = {0};
    MTT_CHECK(w.smugglerAnswered(2) == 3 && w.u8(0x9D) == 0x70);
    draws = {5};
    MTT_CHECK(w.smugglerAnswered(2) == 2 && w.u8(0x9D) == 0);

    // Accepting puts the price on his bill and the equipment in Paul's place.
    w.setU8(0x9D, 0x70);
    w.setU8(s + 2, 0x60);
    MTT_CHECK(w.smugglerAnswered(1) == 1);
    MTT_CHECK(w.u16(s + 0x0E) == 0x70 && w.u16(0x20) == 0x70 && w.u8(0x22) == 1 && w.u8(s + 0x10) == 9);
    MTT_CHECK(w.u8(s + 6) == 0 && w.u8(0x200 + 0x16) == 1 && w.u8(s + 2) == 0 && w.u8(0x9D) == 0);

    // The next offer goes round to the first kind; with none left, nothing.
    w.smugglerOffers();
    MTT_CHECK(w.u16(0x11F1) == 0xE8 && w.u8(0x9D) == 0x14);
    w.setU8(s + 4, 0);
    w.setU8(0x9D, 0);
    w.smugglerOffers();
    MTT_CHECK(w.u16(0x11F1) == 0xE8 && w.u8(0x9D) == 0);
}

MTT_TEST("world: a shipment sets the Emperor's mood and the day of his next demand") {
    World w;
    w.setU16(0xBC, 400);
    w.setU16(0xA0, 2000);
    w.setU16(0xC0, 500);
    w.setU16(0x1158, 0xFFFF);
    w.setU8(0xBF, 0x90);
    w.setU8(0xFD8 + 0x30 + 0x0E, 3); // Duncan goes with Paul, first in the slots
    w.setU8(0xFD8 + 0x30 + 0x0F, 0x42);
    w.setU16(0x10, 8);
    w.setU8(0x1152, 3);
    w.setU8(0x1153, 1);
    w.sendShipment(500); // as the original did in a test: mood 0xA0, six days
    MTT_CHECK(w.u8(0xFD8 + 0x30 + 0x0F) == 0x02 && w.u16(0x10) == 0 && w.u8(0x1152) == 1 && w.u8(0x1153) == 0xFF);
    MTT_CHECK(w.u16(0xA0) == 1500 && w.u16(0x1172) == 500);
    MTT_CHECK(w.u8(0xBE) == 0xA0 && w.u8(0xBF) == 0xC0);
    MTT_CHECK(w.u16(0x118D) == 6 && w.u8(0xCF) == 6 && w.u16(0x1158) == 0);
    w.setU16(0x118D, 0);
    w.sendShipment(100); // short: four days
    MTT_CHECK(w.u8(0xBE) == 0x20 && w.u8(0xBF) == 0x88 && w.u16(0x118D) == 4);
    w.setU16(0x118D, 0);
    w.sendShipment(100); // short twice running: the mood falls to nothing
    MTT_CHECK(w.u8(0xBE) == 0);
}

MTT_TEST("world: the Emperor's reply goes to the news by his mood") {
    World w;
    w.setU8(0x2A, 0x0F);
    MTT_CHECK(!w.emperorReplies());
    MTT_CHECK((w.u8(0xFF7) & 0x10) != 0);
    w.setU8(0x2A, 0x10);
    w.setU8(0xBE, 0x85);
    w.setU16(0xC0, 5);
    MTT_CHECK(w.emperorReplies());
    MTT_CHECK(w.u16(0xC0) == 0 && (w.u8(0xBF) & 1) != 0);
    MTT_CHECK(w.u8(0xC8) == 1 && w.u16(0x1179) == 0x090B);
    w.setU8(0xBE, 0);
    w.emperorReplies();
    MTT_CHECK(w.u16(0x117B) == 0x0C0B && w.u8(0x11BB) == 1);
}

MTT_TEST("world: Stilgar's attack sends the army troops with weapons to the palace") {
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0x05);
    FlatMap m(smallLatitudes(), map, FlatMap::Edges{});
    World w;
    const std::size_t sietch = World::placeRecord(1);
    const std::size_t palace = World::placeRecord(2);
    MTT_CHECK(palace == 0x11C);
    w.setU8(sietch + 8, 0x01);
    w.setU16(sietch + 2, 0);
    w.setU16(sietch + 4, 2);
    w.setU8(palace + 8, 0x30);
    w.setU16(palace + 2, 30000);
    w.setU16(palace + 4, 40);
    w.setU8(World::placeRecord(3), 0xFF);
    for (int n = 1; World::troopRecord(n) < 0xFBB; ++n) {
        w.setU8(World::troopRecord(n) + 3, 0xA0); // nobody's
    }
    w.setU8(sietch + 9, 1);
    for (const int n : {1, 2}) {
        const std::size_t t = World::troopRecord(n);
        w.setU8(t, static_cast<std::uint8_t>(n));
        w.setU8(t + 1, n == 1 ? 2 : 0);
        w.setU8(t + 3, 4); // the army
        w.setU16(t + 4, static_cast<std::uint16_t>(sietch));
        w.setU16(t + 8, 2);
    }
    w.setU8(World::troopRecord(1) + 0x19, 0x04); // weapons
    w.setU8(0xC2, 6);
    w.launchAttack(m);
    MTT_CHECK(w.u8(0xC2) == 7);
    const std::size_t one = World::troopRecord(1);
    MTT_CHECK(w.u16(one + 4) == palace && (w.u8(one + 3) & 0x40) != 0);
    MTT_CHECK(w.u16(one + 8) != 2 || w.u16(one + 6) != 0); // it has made steps
    MTT_CHECK(w.u16(World::troopRecord(2) + 4) == sietch && w.u8(World::troopRecord(2) + 3) == 4);
}

MTT_TEST("world: a troop fetches equipment from the nearest place that has it free, and comes home") {
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0x05);
    FlatMap m(smallLatitudes(), map, FlatMap::Edges{});
    World w;
    for (int n = 1; World::troopRecord(n) < 0xFBB; ++n) {
        w.setU8(World::troopRecord(n) + 3, 0xA0); // nobody's
    }
    auto place = [&](int n, int kind, int lon, int ornithopters) {
        const std::size_t p = World::placeRecord(n);
        w.setU8(p, static_cast<std::uint8_t>(n));
        w.setU8(p + 8, static_cast<std::uint8_t>(kind));
        w.setU16(p + 2, static_cast<std::uint16_t>(lon));
        w.setU16(p + 4, 0x40);
        w.setU8(p + 0x15, static_cast<std::uint8_t>(ornithopters));
        return p;
    };
    const std::size_t home = place(3, 0x01, 0x1000, 0);
    const std::size_t sietch = place(4, 0x02, 0x1300, 1);
    place(5, 0x03, 0x1100, 0);                               // none there
    const std::size_t village = place(6, 0x21, 0x1800, 1);  // further, but counted a quarter
    place(7, 0x30, 0x1000, 3);                               // a fort: not searched
    w.setU16(World::placeRecord(8), 0xFFFF);
    MTT_CHECK(w.equipmentSource(home, 1) == village);
    MTT_CHECK(w.equipmentSource(home, 0) == 0);
    // At Paul's home an ornithopter stays until step 0x50.
    w.setU16(0x1150, static_cast<std::uint16_t>(village));
    w.setU8(0x2A, 0x10);
    MTT_CHECK(w.equipmentSource(home, 1) == sietch);
    w.setU16(0x1150, 0);
    // A troop already on its way there for one takes it.
    const std::size_t other = World::troopRecord(5);
    w.setU8(other + 3, 0x43);
    w.setU16(other + 4, static_cast<std::uint16_t>(village));
    w.setU16(other + 0x0C, static_cast<std::uint16_t>(home));
    w.setU8(other + 0x0E, 1);
    MTT_CHECK(w.equipmentSource(home, 1) == sietch);

    // Troop 1 goes to the sietch (at its own point) for an ornithopter.
    w.setU16(sietch + 2, 0x1000);
    const std::size_t t = World::troopRecord(1);
    w.setU8(t, 1);
    w.setU8(t + 3, 0x03);
    w.setU16(t + 4, static_cast<std::uint16_t>(home));
    w.setU16(t + 6, 0x1000);
    w.setU16(t + 8, 0x40);
    w.setU8(home + 9, 1);
    w.setU16(t + 0x0C, static_cast<std::uint16_t>(home));
    w.setU16(t + 0x0E, 0x4001);
    w.startMove(1, sietch, m);
    MTT_CHECK(w.u8(sietch + 0x15) == 0 && w.u8(t + 0x19) == 0x40 && w.u8(t + 0x0F) == 0);
    MTT_CHECK(w.u16(t + 4) == home && (w.u8(t + 3) & 0x40) && w.u8(sietch + 9) == 0);
    MTT_CHECK(!w.troopArrivedHere);
    // Home again, it settles with it and mines spice once more; Paul is
    // there, in the second room, where the troops stand: it is to be seen.
    w.setU16(0x114E, static_cast<std::uint16_t>(home));
    w.setU8(0x0B, 2);
    w.stepTroop(1, 4, m);
    MTT_CHECK(!(w.u8(t + 3) & 0x40) && (w.u8(t + 3) & 0x0F) == 0 && w.u8(home + 9) == 1);
    MTT_CHECK(w.u8(home + 0x15) == 1);
    MTT_CHECK(w.troopArrivedHere);
}

MTT_TEST("world: Jessica's powers let Paul reach his troops further off") {
    World w;
    w.setU16(0x1176, 1);
    w.setU8(0x29, 0x10);
    w.jessicaTeaches();
    MTT_CHECK(w.u16(0x1176) == 0x1E && w.u8(0x29) == 0x1A && w.u8(0xD5) == 0x80 - 5);
    w.jessicaTeaches();
    MTT_CHECK(w.u16(0x1176) == 0x32 && w.u8(0x29) == 0x1A && w.u8(0xD5) == 0x80 - 8);
    // After the water of life, everywhere.
    w.setU8(0x0A, 0x02);
    w.jessicaTeaches();
    MTT_CHECK(w.u16(0x1176) == 0xFFE2 && w.u8(0x29) == 0x42 && w.u8(0xD5) == 0);
    // The captain gives away the place last learnt.
    const std::size_t place = World::placeRecord(9);
    w.setU8(place + 0x0A, 0x81);
    w.setU16(0x11CE, static_cast<std::uint16_t>(place));
    w.revealLearnedPlace();
    MTT_CHECK(w.u8(place + 0x0A) == 0x01);
}

MTT_TEST("world: Harkonnen troops where character 4 is move the story on to 0x30") {
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0x05);
    FlatMap m(smallLatitudes(), map, FlatMap::Edges{});
    World w;
    for (int n = 1; World::troopRecord(n) < 0xFBB; ++n) {
        w.setU8(World::troopRecord(n) + 3, 0xA0);
    }
    const std::size_t place = World::placeRecord(5);
    w.setU8(place + 8, 0x30);
    w.setU16(World::placeRecord(9), 0xFFFF); // the last place
    w.setU16(0x101A, 0x0580); // character 4 in place 5
    w.setU8(0x2A, 0x2D);
    w.troopsWork(0, m);
    MTT_CHECK(w.u8(0x2A) == 0x2D); // nobody there
    // A Harkonnen army troop there.
    const std::size_t t = World::troopRecord(7);
    w.setU8(t, 7);
    w.setU8(t + 3, 0x84);
    w.setU8(t + 0x10, 0x80);
    w.setU16(t + 4, static_cast<std::uint16_t>(place));
    w.setU8(place + 9, 7);
    w.setU16(0x10, 0x10); // he goes with Paul: nothing
    w.troopsWork(0, m);
    MTT_CHECK(w.u8(0x2A) == 0x2D);
    w.setU16(0x10, 0);
    w.troopsWork(0, m);
    MTT_CHECK(w.u8(0x2A) == 0x30);
}

MTT_TEST("world: walking out into the desert and back in") {
    World w;
    const std::size_t place = World::placeRecord(3);
    w.setU16(World::placeRecord(5), 0xFFFF); // the last place
    w.setU16(place + 2, 0x2102);
    w.setU16(place + 4, static_cast<std::uint16_t>(-19));
    w.setU8(place + 8, 0x03); // a sietch
    w.enterPlace(place);
    MTT_CHECK(w.inside());
    w.setU8(0xE7, 1);
    // Out to the right: one step east of the sietch.
    MTT_CHECK(!w.walkDesert(2));
    MTT_CHECK(!w.inside());
    MTT_CHECK(w.room() == 0x2103 && w.place() == 0x00ED);
    MTT_CHECK(w.u8(0xE7) == 0 && w.u8(0x08) == 0xFF && w.placeHere() == 0);
    MTT_CHECK(w.desertSteps() == 0);
    // Up: a 256th of a latitude north.
    MTT_CHECK(!w.walkDesert(1));
    MTT_CHECK(w.room() == 0x2103 && w.place() == 0xFFEC);
    MTT_CHECK(w.desertSteps() == 1 && w.u8(0xF4) == 1);
    MTT_CHECK(!w.walkDesert(3));
    MTT_CHECK(w.place() == 0x00ED);
    // Left, onto the sietch's own point: in its first room again.
    MTT_CHECK(w.walkDesert(4));
    MTT_CHECK(w.inside() && w.room() == 0x0301 && (w.place() >> 8) == 3);
    MTT_CHECK(w.desertSteps() == 0);
}

MTT_TEST("world: found unconscious, Paul is carried home") {
    World w;
    const std::size_t place = World::placeRecord(2);
    w.setU16(World::placeRecord(5), 0xFFFF);
    w.setU16(place + 2, 0x4000);
    w.setU8(place + 8, 0x05);
    w.enterPlace(place);
    MTT_CHECK(!w.walkDesert(3));
    MTT_CHECK(w.place() == 0x0100); // a 256th south of latitude 0
    w.carriedHome();
    MTT_CHECK(w.inside() && w.room() == 0x0502 && w.u8(0xE7) == 1);
    // The tiredness of walking goes an hour at a time.
    w.setU8(0xF4, 0x12);
    w.setU8(0xF5, 3);
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0x05);
    FlatMap m(smallLatitudes(), map, FlatMap::Edges{});
    w.setU8(0xC2, 7); // the story is over: only the hour's own
    w.hourPassed(0, m);
    MTT_CHECK(w.u8(0xF4) == 0x11 && w.u8(0xF5) == 3);
    w.hourPassed(0, m);
    MTT_CHECK(w.u8(0xF4) == 0x10 && w.u8(0xF5) == 3);
    w.hourPassed(0, m);
    MTT_CHECK(w.u8(0xF4) == 0 && w.u8(0xF5) == 0);
}

MTT_TEST("world: a massive attack fights the rounds at once") {
    std::vector<std::uint8_t> map(2 * kMiddle + 512, 0x05);
    FlatMap m(smallLatitudes(), map, FlatMap::Edges{});
    World w;
    const std::size_t sietch = World::placeRecord(1);
    w.setU8(sietch + 8, 0x03);
    w.setU8(sietch + 9, 1);
    w.setU8(sietch + 0x0A, 0x02); // the Harkonnens are inside
    w.setU8(World::placeRecord(2), 0xFF);
    w.setU16(World::placeRecord(2), 0xFFFF);
    for (int n = 1; World::troopRecord(n) < 0xFBB; ++n) {
        w.setU8(World::troopRecord(n), static_cast<std::uint8_t>(n));
        w.setU8(World::troopRecord(n) + 3, 0xA0);
    }
    const std::size_t ours = World::troopRecord(1);
    w.setU8(ours + 1, 2);
    w.setU8(ours + 3, 6);
    w.setU16(ours + 4, static_cast<std::uint16_t>(sietch));
    w.setU8(ours + 0x15, 50);
    w.setU8(ours + 0x17, 0x20);
    w.setU8(ours + 0x1A, 100);
    const std::size_t theirs = World::troopRecord(2);
    w.setU8(theirs + 3, 0x80);
    w.setU16(theirs + 4, static_cast<std::uint16_t>(sietch));
    w.setU8(theirs + 0x10, 0x80);
    w.setU8(theirs + 0x15, 50);
    w.setU8(theirs + 0x17, 0x20);
    w.setU8(theirs + 0x1A, 200);
    w.enterPlace(sietch);
    w.setU8(0x2B, 1);
    w.random2 = [] { return 0xFF; }; // above the lean: Paul's troop is struck
    w.random = [](int) { return 1; };
    const int before = w.u8(ours + 0x1A);
    w.massiveAttack(m);
    // Struck round after round until it fled with a few.
    MTT_CHECK(w.u8(ours + 0x1A) != before);
    MTT_CHECK(w.u16(0x9A) != 0); // what Paul's side lost
    MTT_CHECK(w.u16(0x98) == 0); // the Harkonnens lost nothing
}

MTT_TEST("world: the prospectors go to the places of their route in turn") {
    World w;
    const std::size_t a = World::placeRecord(1);
    const std::size_t b = World::placeRecord(2);
    const std::size_t c = World::placeRecord(3);
    const std::size_t t = World::troopRecord(3);
    w.setU16(t + 4, static_cast<std::uint16_t>(a));
    // Their route starts where they stand: that place is taken off.
    w.setU16(0x11D3, static_cast<std::uint16_t>(a));
    w.setU16(0x11D5, static_cast<std::uint16_t>(b));
    w.setU16(0x11D7, static_cast<std::uint16_t>(c));
    MTT_CHECK(w.nextOnRoute(3) == b);
    MTT_CHECK(w.u16(0x11D3) == b && w.u16(0x11D5) == c && w.u16(0x11D7) == 0);
    // On the move, nothing is taken off.
    w.setU8(t + 3, 0x40);
    w.setU16(t + 4, static_cast<std::uint16_t>(b));
    MTT_CHECK(w.nextOnRoute(3) == b && w.u16(0x11D3) == b);
    // An empty route: they stay.
    w.setU8(t + 3, 0);
    w.setU16(0x11D3, 0);
    MTT_CHECK(w.nextOnRoute(3) == 0);
}
