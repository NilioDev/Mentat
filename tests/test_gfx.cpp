// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/gfx/font.hpp"
#include "mentat/gfx/palette.hpp"
#include "mentat/gfx/png.hpp"
#include "mentat/gfx/sprite.hpp"
#include "mentat/gfx/sprite_sheet.hpp"
#include "mentat/gfx/surface.hpp"

#include <cstdint>
#include <string>
#include <vector>

using namespace mentat;

namespace {

template <typename F>
bool throwsFormatError(F&& f) {
    try {
        f();
    } catch (const FormatError&) {
        return true;
    }
    return false;
}

} // namespace

MTT_TEST("palette: applies blocks, keeps colour 0 for a lone colour-0 block") {
    Palette p;
    p[0] = Colour{1, 2, 3};
    const std::vector<std::uint8_t> blocks = {
        0x00, 0x01, 9, 9, 9,          // colour 0 alone: skipped
        0x10, 0x02, 63, 0, 0, 0, 63, 0, // colours 16 and 17
        0xFF, 0xFF,
    };
    p.applyBlocks(blocks);
    MTT_CHECK(p[0] == (Colour{1, 2, 3}));
    MTT_CHECK(p[16] == (Colour{63, 0, 0}));
    MTT_CHECK(p[17] == (Colour{0, 63, 0}));
    MTT_CHECK(Palette::to8Bit(63) == 255);
    MTT_CHECK(Palette::to8Bit(0) == 0);
}

MTT_TEST("palette: a count of 0 means 256 colours; damaged blocks are refused") {
    std::vector<std::uint8_t> all = {0x00, 0x00};
    for (int i = 0; i < 256; ++i) {
        all.insert(all.end(), {static_cast<std::uint8_t>(i / 4), 0, 0});
    }
    all.insert(all.end(), {0xFF, 0xFF});
    Palette p;
    p.applyBlocks(all);
    MTT_CHECK(p[255].r == 63);
    MTT_CHECK(throwsFormatError([] {
        Palette q;
        q.applyBlocks(std::vector<std::uint8_t>{0xF0, 0x20, 1, 2, 3});
    }));
    MTT_CHECK(throwsFormatError([] {
        Palette q;
        q.applyBlocks(std::vector<std::uint8_t>{0x10, 0x01, 1, 2});
    }));
}

MTT_TEST("sprite: four-bit rows add the offset and keep pixel 0 see-through") {
    // 3 pixels wide → 4 stored pixels (2 bytes) per row, 2 rows, offset 0x20.
    const std::vector<std::uint8_t> bytes = {
        0x03, 0x00, 0x02, 0x20, // header
        0x21, 0x03,             // row 0: pixels 1 2 3 0
        0x00, 0x50,             // row 1: pixels 0 0 0 5
    };
    Surface s(8, 4);
    s.fill(0xAA);
    drawSprite(s, readSprite(bytes), 1, 1);
    MTT_CHECK(s.at(1, 1) == 0x21);
    MTT_CHECK(s.at(2, 1) == 0x22);
    MTT_CHECK(s.at(3, 1) == 0x23);
    MTT_CHECK(s.at(4, 1) == 0xAA);
    MTT_CHECK(s.at(1, 2) == 0xAA);
    MTT_CHECK(s.at(4, 2) == 0x25); // the padding column is drawn too
}

MTT_TEST("sprite: packed rows repeat and copy bytes") {
    // 8-bit, see-through colour 0, 4 wide, 1 row: run of 3 × 7, then 1 literal 0.
    const std::vector<std::uint8_t> bytes = {
        0x04, 0x80, 0x01, 0xFF, // packed, width 4, height 1, mode 0xFF
        0xFE, 0x07,             // repeat 257 - 0xFE = 3 times 7
        0x00, 0x00,             // 1 literal byte: 0
    };
    Surface s(6, 1);
    s.fill(1);
    drawSprite(s, readSprite(bytes), 0, 0);
    MTT_CHECK(s.at(0, 0) == 7 && s.at(1, 0) == 7 && s.at(2, 0) == 7);
    MTT_CHECK(s.at(3, 0) == 1);
    // The solid mode draws colour 0 too.
    auto solid = bytes;
    solid[3] = 0xFE;
    drawSprite(s, readSprite(solid), 0, 0);
    MTT_CHECK(s.at(3, 0) == 0);
}

MTT_TEST("sprite: mirrored sprites start at their header width") {
    const std::vector<std::uint8_t> bytes = {0x03, 0x00, 0x01, 0xFE, 1, 2, 3};
    Surface s(4, 1);
    drawSprite(s, readSprite(bytes), 0, 0, DrawFlags{true, false});
    MTT_CHECK(s.at(0, 0) == 3 && s.at(1, 0) == 2 && s.at(2, 0) == 1);
    auto flagged = bytes;
    flagged[1] |= 0x40; // the header's own mirror flag, undone by the draw flag
    drawSprite(s, readSprite(flagged), 0, 0, DrawFlags{true, false});
    MTT_CHECK(s.at(0, 0) == 1 && s.at(2, 0) == 3);
}

MTT_TEST("sprite: refuses runs that cross a row and data that ends early") {
    MTT_CHECK(throwsFormatError([] {
        decodeSprite(readSprite(std::vector<std::uint8_t>{0x02, 0x80, 0x01, 0xFE, 0xFD, 0x01}));
    }));
    MTT_CHECK(throwsFormatError([] {
        decodeSprite(readSprite(std::vector<std::uint8_t>{0x04, 0x00, 0x02, 0xFE, 1, 2, 3}));
    }));
    MTT_CHECK(throwsFormatError([] { readSprite(std::vector<std::uint8_t>{0x04, 0x00}); }));
}

MTT_TEST("sprite sheet: reads the palette and the sprite list") {
    std::vector<std::uint8_t> file = {
        0x09, 0x00,                   // sprite list at 9
        0x05, 0x01, 10, 20, 30,       // colour 5
        0xFF, 0xFF,                   // end of palette
        0x04, 0x00, 0x0A, 0x00,       // two sprites, at 9+4 and 9+10
        0x02, 0x00, 0x01, 0xFE, 1, 2, // sprite 0
        0x01, 0x00, 0x01, 0xFE, 9,    // sprite 1
    };
    const auto sheet = SpriteSheet::parse(file);
    MTT_CHECK(sheet.size() == 2);
    Palette p;
    sheet.applyPalette(p);
    MTT_CHECK(p[5] == (Colour{10, 20, 30}));
    MTT_CHECK(sheet.sprite(1).header.width() == 1);
    MTT_CHECK(throwsFormatError([&] { (void)sheet.sprite(2); }));
    file[11] = 0x40; // sprite 1 far outside the file
    MTT_CHECK(throwsFormatError([&] { (void)SpriteSheet::parse(file); }));
}

MTT_TEST("font: draws set bits from the left and advances by the width") {
    std::vector<std::uint8_t> file(Font::kFileSize, 0);
    file['A'] = 3;
    file[256 + 'A' * 9 + 0] = 0xC0;
    file[256 + 'A' * 9 + 8] = 0x80; // the ninth row
    const Font font = Font::parse(file);
    Surface s(8, 9);
    MTT_CHECK(font.draw(s, 1, 0, "AA", 7) == 7);
    MTT_CHECK(s.at(1, 8) == 7 && font.row(0xFF, 0) == 0);
    MTT_CHECK(s.at(1, 0) == 7 && s.at(2, 0) == 7 && s.at(3, 0) == 0 && s.at(4, 0) == 7);
    MTT_CHECK(font.width("AAA") == 9);
    MTT_CHECK(throwsFormatError([] { (void)Font::parse(std::vector<std::uint8_t>(10)); }));
}

MTT_TEST("png: writes a valid signature and image header") {
    Surface s(2, 1);
    Palette p;
    const auto png = encodePng(s, p);
    MTT_CHECK(png.size() > 33);
    MTT_CHECK(png[0] == 0x89 && png[1] == 'P' && png[12] == 'I' && png[15] == 'R');
    MTT_CHECK(png[png.size() - 8] == 'I' && png[png.size() - 5] == 'D');
}

MTT_TEST("sprite sheet: every picture in the player's DUNE.DAT decodes") {
    auto dat = test::openGameData();
    if (!dat) {
        MTT_SKIP("no DUNE.DAT in gamedata/");
    }
    int files = 0;
    int sprites = 0;
    for (const auto& e : dat->directory().entries()) {
        if (!test::isPictureFile(e.name)) {
            continue;
        }
        const auto sheet = SpriteSheet::parse(dat->load(e.name));
        Palette p;
        sheet.applyPalette(p);
        for (std::size_t i = 0; i < sheet.size(); ++i) {
            const auto px = decodeSprite(sheet.sprite(i));
            MTT_CHECK(px.width >= px.drawnWidth);
            ++sprites;
        }
        ++files;
    }
    MTT_CHECK(files == 135);
    MTT_CHECK(sprites > 2000);
}

MTT_TEST("font: both fonts in the player's DUNE.DAT load") {
    auto dat = test::openGameData();
    if (!dat) {
        MTT_SKIP("no DUNE.DAT in gamedata/");
    }
    const Font a = Font::parse(dat->load("DNCHAR.BIN"));
    const Font b = Font::parse(dat->load("DNCHAR2.BIN"));
    MTT_CHECK(a.width('A') > 0);
    MTT_CHECK(b.width('A') > 0);
}
