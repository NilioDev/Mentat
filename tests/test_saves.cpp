// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_data.hpp"
#include "test.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/data/save_file.hpp"
#include "mentat/engine/dialogue.hpp"
#include "mentat/engine/saved_games.hpp"
#include "mentat/engine/world.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace mentat;

MTT_TEST("save file: runs of three or more, and the marker, are packed") {
    std::vector<std::uint8_t> c(SaveFile::kSize, 0);
    c[0] = 5;
    c[1] = 6;
    c[2] = 6;
    c[3] = SaveFile::kMarker;
    c[4] = 9;
    const auto file = SaveFile::encode(0x1234, c);
    MTT_CHECK(file[0] == 0x34 && file[1] == 0x12 && file[2] == SaveFile::kMarker);
    MTT_CHECK(file[6] == 5 && file[7] == 6 && file[8] == 6);
    MTT_CHECK(file[9] == SaveFile::kMarker && file[10] == 1 && file[11] == SaveFile::kMarker && file[12] == 9);
    // Then the zeros, 255 at most to a run.
    MTT_CHECK(file[13] == SaveFile::kMarker && file[14] == 0xFF && file[15] == 0);
    const std::size_t length = static_cast<std::size_t>(file[4] | (file[5] << 8));
    MTT_CHECK(length + 2 == file.size());
    MTT_CHECK(file[3] == (SaveFile::kSize - 5) % 255);
    MTT_CHECK(SaveFile::clock(file) == 0x1234 && SaveFile::decode(file) == c);

    // Short, cut and overlong files are refused.
    MTT_CHECK_THROWS(SaveFile::decode(std::vector<std::uint8_t>(file.begin(), file.begin() + 5)));
    MTT_CHECK_THROWS(SaveFile::decode(std::vector<std::uint8_t>(file.begin(), file.end() - 1)));
    auto longer = file;
    longer.insert(longer.end(), {SaveFile::kMarker, 0xFF, 0});
    longer[4] = static_cast<std::uint8_t>(longer.size() - 2);
    longer[5] = static_cast<std::uint8_t>((longer.size() - 2) >> 8);
    MTT_CHECK_THROWS(SaveFile::decode(longer));
    MTT_CHECK_THROWS(SaveFile::encode(0, std::vector<std::uint8_t>(10)));
}

MTT_TEST("save file: two bits of each map byte, four to a byte") {
    std::vector<std::uint8_t> map(SaveFile::kMapBytes, 0x05);
    map[0] = 0x35;
    map[1] = 0x1F;
    map[3] = 0x2A;
    std::vector<std::uint8_t> packed(SaveFile::kMapPart);
    SaveFile::packMap(map, packed);
    MTT_CHECK(packed[0] == 0xD2 && packed[1] == 0);
    std::vector<std::uint8_t> back(SaveFile::kMapBytes, 0xFF);
    SaveFile::unpackMap(packed, back);
    MTT_CHECK(back[0] == 0xFF && back[1] == 0xDF && back[2] == 0xCF && back[3] == 0xEF);
    // Past the end of the map, zeros.
    SaveFile::packMap(std::vector<std::uint8_t>(3, 0x30), packed);
    MTT_CHECK(packed[0] == 0xFC && packed[1] == 0);
    std::vector<std::uint8_t> small(10);
    MTT_CHECK_THROWS(SaveFile::packMap(map, small));
}

MTT_TEST("saved games: a game saved comes back as it was") {
    auto archive = test::openGameData();
    auto program = test::openProgram();
    if (!archive || !program) {
        MTT_SKIP("no game files in gamedata/");
    }
    World w(*program);
    Dialogue d = Dialogue::parse(archive->load("DIALOGUE.HSQ"));
    MTT_CHECK(d.bytes().size() <= SaveFile::kDialoguePart);
    std::vector<std::uint8_t> map(SaveFile::kMapBytes + 16, 0x03);
    const auto folder = std::filesystem::temp_directory_path() / "mentat-test-saves";
    std::filesystem::remove_all(folder);
    SavedGames games(folder, *program, w, d, [&]() -> std::vector<std::uint8_t>& { return map; });
    MTT_CHECK(!games.clockOf(1) && !games.load(1));

    w.setClock(0x25);
    map[7] = 0x33;
    d.markSaid(*d.list(0));
    MTT_CHECK(games.save(1) && games.clockOf(1) == 0x25);
    // The table of lists is kept as the original keeps it in memory.
    const auto contents = games.contents();
    const std::size_t at = SaveFile::kMapPart + SaveFile::kCodePart;
    MTT_CHECK((contents[at] | (contents[at + 1] << 8)) == ((d.bytes()[0] | (d.bytes()[1] << 8)) + 0xAA76) % 0x10000);

    const auto said = d.bytes();
    w.setClock(0x40);
    map[7] = 0x03;
    map[8] = 0x13;
    d = Dialogue::parse(archive->load("DIALOGUE.HSQ"));
    MTT_CHECK(games.load(1));
    MTT_CHECK(w.clock() == 0x25 && map[7] == 0x33 && map[8] == 0x03 && d.bytes() == said);

    // A damaged file changes nothing.
    {
        std::ofstream f(folder / "DUNE37S2.SAV", std::ios::binary);
        f << "xx";
    }
    MTT_CHECK(games.clockOf(2) && !games.load(2) && w.clock() == 0x25);
    std::filesystem::remove_all(folder);
}
