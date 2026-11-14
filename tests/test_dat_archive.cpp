// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.hpp"

#include "mentat/data/dat_archive.hpp"
#include "mentat/data/hsq.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/game_files.hpp"

#include <cstring>
#include <string>
#include <vector>

using namespace mentat;

namespace {

void putEntry(std::vector<std::uint8_t>& dir, std::size_t slot, const char* name, std::uint32_t size,
              std::uint32_t offset) {
    std::uint8_t* p = dir.data() + 2 + slot * DatDirectory::kEntrySize;
    std::memcpy(p, name, std::strlen(name));
    for (int i = 0; i < 4; ++i) {
        p[16 + i] = static_cast<std::uint8_t>(size >> (8 * i));
        p[20 + i] = static_cast<std::uint8_t>(offset >> (8 * i));
    }
}

std::vector<std::uint8_t> directoryWith(std::size_t slots) {
    std::vector<std::uint8_t> dir(DatDirectory::kDirectorySize, 0);
    dir[0] = static_cast<std::uint8_t>(slots);
    dir[1] = static_cast<std::uint8_t>(slots >> 8);
    return dir;
}

bool refuses(const std::vector<std::uint8_t>& dir, std::uint64_t size) {
    try {
        DatDirectory::parse(dir, size);
    } catch (const FormatError&) {
        return true;
    }
    return false;
}

} // namespace

MTT_TEST("dat: reads entries up to the first empty name") {
    auto dir = directoryWith(10);
    putEntry(dir, 0, "TABLAT.BIN", 100, 65536);
    putEntry(dir, 1, "pa\\PA1.VOC", 50, 65636);
    putEntry(dir, 3, "AFTER.GAP", 1, 65536); // after the empty slot: unused
    const auto d = DatDirectory::parse(dir, 70000);
    MTT_CHECK(d.entries().size() == 2);
    MTT_CHECK(d.entries()[1].name == "PA\\PA1.VOC");
    MTT_CHECK(d.find("pa/pa1.voc") != nullptr);
    MTT_CHECK(d.find("tablat.bin")->size == 100);
    MTT_CHECK(d.find("AFTER.GAP") == nullptr);
}

MTT_TEST("dat: refuses entries outside the archive or with unsafe names") {
    auto inside = directoryWith(1);
    putEntry(inside, 0, "A.BIN", 10, 100); // inside the directory area
    MTT_CHECK(refuses(inside, 70000));

    auto past = directoryWith(1);
    putEntry(past, 0, "A.BIN", 10000, 65536);
    MTT_CHECK(refuses(past, 70000));

    auto dots = directoryWith(1);
    putEntry(dots, 0, "..\\A.BIN", 1, 65536);
    MTT_CHECK(refuses(dots, 70000));

    auto many = directoryWith(60000);
    MTT_CHECK(refuses(many, 70000));

    MTT_CHECK(refuses(std::vector<std::uint8_t>(100, 0), 100));
}

MTT_TEST("dat: the player's DUNE.DAT opens and every HSQ file unpacks") {
    const auto files = findGameFiles({MENTAT_GAMEDATA_DIR});
    if (!files) {
        MTT_SKIP("no DUNE.DAT in gamedata/");
    }
    auto archive = DatArchive::open(files->dat);
    const auto& entries = archive.directory().entries();
    MTT_CHECK(entries.size() == 2549);
    std::size_t unpacked = 0;
    for (const auto& e : entries) {
        if (e.name.size() > 4 && e.name.substr(e.name.size() - 4) == ".HSQ") {
            const auto bytes = archive.read(e);
            MTT_CHECK(isHsq(bytes));
            MTT_CHECK(!unpackHsq(bytes).empty());
            ++unpacked;
        }
    }
    MTT_CHECK(unpacked == 188);
}
