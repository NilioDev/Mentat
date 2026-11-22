// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/gfx/scene.hpp"
#include "mentat/gfx/sprite_sheet.hpp"
#include "mentat/gfx/surface.hpp"

#include <cstdint>
#include <span>
#include <vector>

using namespace mentat;

namespace {

void put16(std::vector<std::uint8_t>& v, int x) {
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
}

// A picture file without palette holding two copies of one 4-bit sprite,
// 4 × 2 pixels, mode 0x10: nibbles 1 2 3 4 / 5 6 7 8. Scenes cannot name
// sprite 0, so they draw sprite 1.
SpriteSheet tinySheet() {
    std::vector<std::uint8_t> b;
    put16(b, 2); // sprite list right after this word
    put16(b, 4); // two sprites, both 4 bytes after the list start
    put16(b, 4);
    put16(b, 4);  // width 4
    b.push_back(2);
    b.push_back(0x10);
    b.insert(b.end(), {0x21, 0x43, 0x65, 0x87});
    return SpriteSheet::parse(std::move(b));
}

SceneTables tables() {
    SceneTables t;
    t.scaleDivisors = {0x100, 0x200, 0x140, 0x160, 0x180, 0x1C0, 0x200, 0x280};
    t.lineClip = {0, 0, 320, 152};
    return t;
}

std::vector<std::uint8_t> scene(std::vector<std::uint8_t> body, int persons = 0) {
    std::vector<std::uint8_t> s;
    s.push_back(static_cast<std::uint8_t>(persons));
    s.insert(s.end(), body.begin(), body.end());
    put16(s, 0xFFFF);
    return s;
}

} // namespace

MTT_TEST("scene file: reads the scene list and refuses bad offsets") {
    std::vector<std::uint8_t> f;
    put16(f, 4);
    put16(f, 6);
    put16(f, 0xFFFF);
    f.push_back(0);
    f.push_back(0xFF);
    f.push_back(0xFF);
    const SceneFile sf = SceneFile::parse(f);
    MTT_CHECK(sf.size() == 2);
    MTT_CHECK(sf.scene(1).size() == 3);
    MTT_CHECK_THROWS(sf.scene(2));
    MTT_CHECK_THROWS(SceneFile::parse({3, 0}));
    MTT_CHECK_THROWS(SceneFile::parse({2, 0}));
}

MTT_TEST("scene: room numbers pick a scene and a picture file") {
    MTT_CHECK(RoomNumber::decode(1).scene == 0 && RoomNumber::decode(1).pictureFile == 0);
    MTT_CHECK(RoomNumber::decode(0x61).scene == 0 && RoomNumber::decode(0x61).pictureFile == 0x19);
    MTT_CHECK(RoomNumber::decode(0x1F).scene == 14 && RoomNumber::decode(0x1F).pictureFile == 0x14);
}

MTT_TEST("scene: sprites, colour offset, mirroring and person places") {
    const SpriteSheet sheet = tinySheet();
    std::vector<std::uint8_t> body;
    put16(body, 2); // sprite 1
    body.insert(body.end(), {10, 20, 0});
    put16(body, 2 | 0x4000 | 0x200); // sprite 1 mirrored, x + 256
    body.insert(body.end(), {10, 20, 0x30});
    put16(body, 1); // a person's place
    body.insert(body.end(), {50, 60, 0});
    const auto s = scene(body, 3);
    Surface screen;
    int calls = 0;
    const int persons = drawScene(screen, s, sheet, tables(), [&](int slot, int x, int y) {
        MTT_CHECK(slot == 0 && x == 50 && y == 60);
        ++calls;
    });
    MTT_CHECK(persons == 3);
    MTT_CHECK(calls == 1);
    MTT_CHECK(screen.at(10, 20) == 0x11 && screen.at(13, 20) == 0x14 && screen.at(13, 21) == 0x18);
    // Mirrored: the first pixel lands at x + width - 1, with the new offset.
    MTT_CHECK(screen.at(269, 20) == 0x31 && screen.at(266, 20) == 0x34);
}

MTT_TEST("scene: a sprite past the right edge goes on one row lower at the left") {
    const SpriteSheet sheet = tinySheet();
    std::vector<std::uint8_t> body;
    put16(body, 2 | 0x200);
    body.insert(body.end(), {62, 5, 0}); // x = 318
    Surface screen;
    drawScene(screen, scene(body), sheet, tables());
    MTT_CHECK(screen.at(318, 5) == 0x11 && screen.at(319, 5) == 0x12);
    MTT_CHECK(screen.at(0, 6) == 0x13 && screen.at(1, 6) == 0x14);
}

MTT_TEST("scene: shrunk sprites take every n-th pixel") {
    const SpriteSheet sheet = tinySheet();
    Surface screen;
    drawScaledSprite(screen, sheet, 1, 0x10, false, false, 0x200, 5, 5);
    MTT_CHECK(screen.at(5, 5) == 0x11 && screen.at(6, 5) == 0x13);
    MTT_CHECK(screen.at(7, 5) == 0 && screen.at(5, 6) == 0);
    Surface flipped;
    drawScaledSprite(flipped, sheet, 1, 0x20, true, false, 0x200, 5, 5);
    MTT_CHECK(flipped.at(6, 5) == 0x21 && flipped.at(5, 5) == 0x23);
}

MTT_TEST("scene: lines leave out their first point and are clipped") {
    Surface s;
    const ClipRect clip{0, 0, 320, 152};
    drawSceneLine(s, 0, 0, 4, 2, 9, clip);
    MTT_CHECK(s.at(0, 0) == 0);
    MTT_CHECK(s.at(1, 1) == 9 && s.at(2, 1) == 9 && s.at(3, 2) == 9 && s.at(4, 2) == 9);
    MTT_CHECK(s.at(1, 0) == 0);
    drawSceneLine(s, 20, 10, 15, 10, 7, clip);
    for (int x = 15; x <= 20; ++x) {
        MTT_CHECK(s.at(x, 10) == 7);
    }
    drawSceneLine(s, 30, 150, 30, 155, 5, clip);
    MTT_CHECK(s.at(30, 150) == 5 && s.at(30, 151) == 5 && s.at(30, 152) == 0);
}

MTT_TEST("scene: shapes fill between their sides; a flat stretch takes a row") {
    std::vector<std::uint8_t> body;
    put16(body, 0x8000 | 50); // flags 0x80: no grain, left to right
    body.insert(body.end(), {0, 0});
    put16(body, 10);
    put16(body, 10);
    put16(body, 10 | 0x4000); // first side: straight down to the bottom point
    put16(body, 14);
    put16(body, 13); // second side: right along the top...
    put16(body, 10);
    put16(body, 13 | 0x8000); // ...then down, then left to the bottom point
    put16(body, 14);
    Surface s;
    drawScene(s, scene(body), tinySheet(), tables());
    // Without grain a pixel is the base colour - 1.
    MTT_CHECK(s.at(10, 10) == 49 && s.at(11, 10) == 0);
    MTT_CHECK(s.at(10, 11) == 49 && s.at(13, 11) == 49 && s.at(14, 11) == 0);
    MTT_CHECK(s.at(13, 13) == 49 && s.at(10, 14) == 0);
}

MTT_TEST("scene: grain speckles the shading") {
    std::vector<std::uint8_t> body;
    put16(body, 0x8200 | 50); // grain bit 1
    body.insert(body.end(), {16, 0}); // one colour more per pixel to the right
    put16(body, 0);
    put16(body, 0);
    put16(body, 0);
    put16(body, 1);
    put16(body, 7 | 0x4000 | 0x8000);
    put16(body, 1);
    Surface s;
    drawScene(s, scene(body), tinySheet(), tables());
    // Rows 0: x 0 to 7 (the sides are sorted).
    MTT_CHECK(s.at(0, 0) == 51 && s.at(1, 0) == 51 && s.at(2, 0) == 53 && s.at(3, 0) == 53);
}

MTT_TEST("scene: refuses damaged scenes") {
    const SpriteSheet sheet = tinySheet();
    Surface s;
    std::vector<std::uint8_t> body;
    put16(body, 4); // sprite 3: not in the sheet
    body.insert(body.end(), {0, 0, 0});
    MTT_CHECK_THROWS(drawScene(s, scene(body), sheet, tables()));
    const std::vector<std::uint8_t> cut = {0, 0x02, 0x00, 1};
    MTT_CHECK_THROWS(drawScene(s, cut, sheet, tables()));
    Surface small(10, 10);
    MTT_CHECK_THROWS(drawScene(small, scene({}), sheet, tables()));
}

MTT_TEST("scene: real rooms match the original's drawing") {
    auto dat = test::openGameData();
    auto program = test::openProgram();
    if (!dat || !program) {
        test::reportSkip("no game data");
        return;
    }
    const SceneTables t = SceneTables::fromProgram(*program);
    MTT_CHECK(t.scaleDivisors[0] == 0x100 && t.lineClip.bottom == 152);
    struct Case {
        const char* file;
        std::size_t scene;
        const char* sheet;
        int persons;
        std::uint32_t crc; // of the palette indices the original draws
    };
    const Case cases[] = {
        {"PALACE.SAL", 0, "POR.HSQ", 9, 0x376AA566},
        {"SIET.SAL", 5, "SIET1.HSQ", 8, 0x4A53CEB2},
    };
    for (const Case& c : cases) {
        const SceneFile f = SceneFile::parse(dat->load(c.file));
        const SpriteSheet sheet = SpriteSheet::parse(dat->load(c.sheet));
        Surface s;
        MTT_CHECK(drawScene(s, f.scene(c.scene), sheet, t) == c.persons);
        MTT_CHECK(crc32(std::span<const std::uint8_t>(s.data(), s.size())) == c.crc);
    }
}
