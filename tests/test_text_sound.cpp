// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/program_tables.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/data/string_table.hpp"
#include "mentat/data/voc.hpp"
#include "mentat/engine/text_box.hpp"
#include "mentat/gfx/font.hpp"
#include "mentat/gfx/surface.hpp"

#include <cstring>
#include <string>
#include <vector>

using namespace mentat;

MTT_TEST("strings: offsets, end bytes and bad tables") {
    // Two strings: "Hi" and "" (offsets 4 and 7).
    const std::vector<std::uint8_t> bytes{4, 0, 7, 0, 'H', 'i', 0xFF, 0xFF};
    const auto table = StringTable::parse(bytes);
    MTT_CHECK(table.size() == 2);
    MTT_CHECK(table.at(0) == "Hi");
    MTT_CHECK(table.at(1).empty());
    MTT_CHECK_THROWS(table.at(2));
    const std::vector<std::uint8_t> noEnd{2, 0, 'x'};
    MTT_CHECK_THROWS(StringTable::parse(noEnd));
    const std::vector<std::uint8_t> outside{2, 0};
    MTT_CHECK_THROWS(StringTable::parse(outside));
}

namespace {

std::vector<std::uint8_t> vocHeader() {
    std::vector<std::uint8_t> v(26);
    std::memcpy(v.data(), "Creative Voice File\x1A", 20);
    v[20] = 26;
    v[22] = 0x0A;
    v[23] = 0x01;
    return v;
}

void block(std::vector<std::uint8_t>& v, std::uint8_t type, std::vector<std::uint8_t> body) {
    v.push_back(type);
    v.push_back(static_cast<std::uint8_t>(body.size()));
    v.push_back(static_cast<std::uint8_t>(body.size() >> 8));
    v.push_back(0);
    v.insert(v.end(), body.begin(), body.end());
}

} // namespace

MTT_TEST("voc: sound, text and repeat blocks") {
    auto v = vocHeader();
    block(v, 5, {1, 2, 3});
    block(v, 6, {2, 0});
    block(v, 1, {211, 0, 0x80, 0x90});
    block(v, 7, {});
    block(v, 1, {6, 0, 0x10});
    v.push_back(0);
    const auto sound = Voc::parse(v);
    MTT_CHECK(sound.text.size() == 3);
    MTT_CHECK(sound.parts.size() == 2);
    MTT_CHECK(sound.parts[0].rate == 22222);
    MTT_CHECK(sound.parts[0].plays == 3);
    MTT_CHECK(sound.parts[0].samples.size() == 2);
    MTT_CHECK(sound.parts[1].rate == 4000);
    MTT_CHECK(sound.parts[1].plays == 1);

    auto bad = vocHeader();
    bad[0] = 'X';
    MTT_CHECK_THROWS(Voc::parse(bad));
    auto cut = vocHeader();
    block(cut, 1, {211, 0, 1, 2, 3});
    cut.resize(cut.size() - 2);
    MTT_CHECK_THROWS(Voc::parse(cut));
}

MTT_TEST("voc: 4-bit packed samples step and adapt") {
    // Start at 128; +7 at level 0 moves 7 and raises the level; then +1 at
    // level 1 moves 3; then 0 still moves 1 at level 1 and lowers the level,
    // and 0 at level 0 does not move.
    const std::vector<std::uint8_t> data{128, 0x71, 0x00};
    const auto out = Voc::unpack4Bit(data);
    MTT_CHECK(out.size() == 5);
    MTT_CHECK(out[1] == 135);
    MTT_CHECK(out[2] == 138);
    MTT_CHECK(out[3] == 139);
    MTT_CHECK(out[4] == 139);
    // Never leaves 0..255.
    const std::vector<std::uint8_t> down{2, 0xFF, 0xFF};
    MTT_CHECK(Voc::unpack4Bit(down).back() == 0);
}

MTT_TEST("program: refuses anything but the known version") {
    std::vector<std::uint8_t> bytes(ProgramFile::kKnownSize, 0);
    MTT_CHECK_THROWS(ProgramFile::parse(bytes));
    MTT_CHECK_THROWS(ProgramFile::parse({}));
    const std::vector<std::uint8_t> abc{'a', 'b', 'c'};
    MTT_CHECK(crc32(abc) == 0x352441C2u);
}

MTT_TEST("real data: every string table and voice file reads") {
    auto archive = test::openGameData();
    if (!archive) {
        MTT_SKIP("no game data");
    }
    int tables = 0;
    int sounds = 0;
    for (const auto& entry : archive->directory().entries()) {
        const std::string& name = entry.name;
        if (name.rfind("PHRASE", 0) == 0 || name.rfind("COMMAND", 0) == 0) {
            const auto table = StringTable::parse(archive->load(name));
            MTT_CHECK(table.size() > 300);
            ++tables;
        } else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".VOC") == 0) {
            const auto sound = Voc::parse(archive->read(entry));
            MTT_CHECK(!sound.parts.empty());
            ++sounds;
        }
    }
    MTT_CHECK(tables == 21);
    MTT_CHECK(sounds == 2288);
    const auto english = StringTable::parse(archive->load("PHRASE11.HSQ"));
    MTT_CHECK(english.size() == 479);
}

MTT_TEST("real data: program file splits into code and data") {
    auto program = test::openProgram();
    if (!program) {
        MTT_SKIP("no DNCDPRG.EXE");
    }
    MTT_CHECK(program->code().size() == ProgramFile::kDataStart);
    MTT_CHECK(program->data().size() == ProgramFile::kDataSize);
    MTT_CHECK_THROWS(program->dataU8(ProgramFile::kDataSize));
    MTT_CHECK_THROWS(program->codeU16(ProgramFile::kDataStart - 1));
}

MTT_TEST("real data: the video table names every video") {
    auto program = test::openProgram();
    if (!program) {
        MTT_SKIP("no DNCDPRG.EXE");
    }
    MTT_CHECK(videoInfo(*program, 0).name.empty());
    MTT_CHECK(videoInfo(*program, video::kVirgin).name == "VIRGIN.HNM");
    MTT_CHECK(videoInfo(*program, video::kIrulan).spaced);
    MTT_CHECK(!videoInfo(*program, video::kTitle).spaced);
    MTT_CHECK_THROWS(videoInfo(*program, video::kCount));
}

namespace {

Font evenFont() {
    std::vector<std::uint8_t> file(Font::kFileSize, 0);
    for (int c = 0; c < 128; ++c) {
        file[static_cast<std::size_t>(c)] = 4;
    }
    return Font::parse(file);
}

} // namespace

MTT_TEST("text box: words wrap and lines stretch to the width") {
    const Font font = evenFont();
    auto lines = layoutText(font, "aaaa bb cccc d", 40);
    MTT_CHECK(lines.size() == 2);
    MTT_CHECK(lines[0].words == 2 && lines[0].gap == 16 && lines[0].extra == 0);
    MTT_CHECK(lines[1].words == 2 && lines[1].gap == 6); // the last line is not stretched
    // A line break ends a line without stretching it.
    lines = layoutText(font, "a b\rc", 40);
    MTT_CHECK(lines.size() == 2 && lines[0].gap == 6 && lines[0].words == 2);
    // A gap of 30 or more anywhere: no line is stretched.
    lines = layoutText(font, "aaaa bbbbbbbbbbbbbbbb", 80);
    MTT_CHECK(lines.size() == 2 && lines[0].gap == 6);
    // A word wider than the box.
    MTT_CHECK(layoutText(font, "aaaaaaaaaaaa", 40).empty());
    Surface s(320, 200);
    MTT_CHECK(!drawTextBox(s, font, "aaaaaaaaaaaa", ClipRect{0, 0, 40, 20}, 6));
}

MTT_TEST("real data: the last story page lays out as in the original") {
    auto archive = test::openGameData();
    if (!archive) {
        MTT_SKIP("no game data");
    }
    const Font font = Font::parse(archive->load("DNCHAR.BIN"));
    const auto text = StringTable::parse(archive->load("COMMAND1.HSQ"));
    const auto lines = layoutText(font, text.at(0x11E), 320);
    MTT_CHECK(lines.size() == 2);
    MTT_CHECK(lines[0].words == 12 && lines[0].gap == 7 && lines[0].extra == 9);
    MTT_CHECK(lines[1].words == 10 && lines[1].gap == 6 && lines[1].extra == 0);
}
