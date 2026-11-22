// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/data/hnm.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/gfx/surface.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace mentat;

namespace {

void put16(std::vector<std::uint8_t>& v, unsigned w) {
    v.push_back(static_cast<std::uint8_t>(w));
    v.push_back(static_cast<std::uint8_t>(w >> 8));
}

// A header with one palette colour, then one frame holding a sound chunk, a
// palette chunk and an unpacked 2×1 sprite at (5, 6).
std::vector<std::uint8_t> tinyHnm() {
    std::vector<std::uint8_t> f;
    put16(f, 2 + 5 + 2 + 1);
    f.insert(f.end(), {0x07, 0x01, 1, 2, 3, 0xFF, 0xFF, 0xFF});
    std::vector<std::uint8_t> frame;
    frame.insert(frame.end(), {'s', 'd'});
    put16(frame, 6);
    frame.insert(frame.end(), {0x11, 0x22});
    frame.insert(frame.end(), {'p', 'l'});
    put16(frame, 4 + 7);
    frame.insert(frame.end(), {0x08, 0x01, 4, 5, 6, 0xFF, 0xFF});
    put16(frame, 0x0002);        // width 2
    put16(frame, 0x1001);        // height 1, 4-bit with offset 0x10
    put16(frame, 5);             // x
    put16(frame, 6);             // y
    frame.insert(frame.end(), {0x21, 0x00});
    put16(f, static_cast<unsigned>(frame.size() + 2));
    f.insert(f.end(), frame.begin(), frame.end());
    return f;
}

} // namespace

MTT_TEST("hnm: splits frames into sound, palette and picture") {
    const auto hnm = HnmFile::parse(tinyHnm());
    MTT_CHECK(hnm.frameCount() == 1);
    Palette p;
    p.applyBlocks(hnm.headerPalette());
    MTT_CHECK(p[7] == (Colour{1, 2, 3}));
    const auto frame = hnm.frame(0);
    MTT_CHECK(frame.sound.size() == 2 && frame.sound[0] == 0x11);
    p.applyBlocks(frame.palette);
    MTT_CHECK(p[8] == (Colour{4, 5, 6}));
    const auto pic = HnmFile::decodePicture(frame.video);
    MTT_CHECK(pic.kind == HnmPicture::Kind::Sprite);
    MTT_CHECK(pic.x == 5 && pic.y == 6 && pic.width == 2 && pic.height == 1);
    Surface s;
    drawHnmPicture(s, pic);
    MTT_CHECK(s.at(5, 6) == 0x11 && s.at(6, 6) == 0x12);
    Surface spaced;
    drawHnmPicture(spaced, pic, true);
    MTT_CHECK(spaced.at(5, 6) == 0x11 && spaced.at(6, 6) == 0 && spaced.at(7, 6) == 0x12);
}

MTT_TEST("hnm: refuses frames that run past the file") {
    auto bytes = tinyHnm();
    bytes[10] = 0xFF; // frame size
    bool threw = false;
    try {
        (void)HnmFile::parse(bytes);
    } catch (const FormatError&) {
        threw = true;
    }
    MTT_CHECK(threw);
}

MTT_TEST("hnm: every video in the player's DUNE.DAT parses and its first frames decode") {
    auto dat = test::openGameData();
    if (!dat) {
        MTT_SKIP("no DUNE.DAT in gamedata/");
    }
    int videos = 0;
    for (const auto& e : dat->directory().entries()) {
        if (e.name.size() < 4 || e.name.compare(e.name.size() - 4, 4, ".HNM") != 0) {
            continue;
        }
        const auto hnm = HnmFile::parse(dat->read(e));
        MTT_CHECK(hnm.frameCount() > 0);
        Surface s;
        for (std::size_t i = 0; i < std::min<std::size_t>(hnm.frameCount(), 8); ++i) {
            const auto frame = hnm.frame(i);
            Palette p;
            if (!frame.palette.empty()) {
                p.applyBlocks(frame.palette);
            }
            drawHnmPicture(s, HnmFile::decodePicture(frame.video));
        }
        ++videos;
    }
    MTT_CHECK(videos == 37);
}
