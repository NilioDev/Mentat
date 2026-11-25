// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.hpp"

#include "mentat/data/reader.hpp"
#include "mentat/game_files.hpp"

#include <filesystem>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using namespace mentat;

MTT_TEST("game files: finds dune.dat in any case and skips missing folders") {
    const auto root = std::filesystem::temp_directory_path() / "mtt_test_game_files";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "game");
    std::ofstream(root / "game" / "dune.dat") << "x";
    const auto found = findGameFiles({root / "missing", root / "game"});
    MTT_CHECK(found.has_value());
    MTT_CHECK(found && found->dat.filename() == "dune.dat");
    MTT_CHECK(found && found->program.empty());
    std::ofstream(root / "game" / "DncdPrg.exe") << "x";
    const auto both = findGameFiles({root / "game"});
    MTT_CHECK(both && both->program.filename() == "DncdPrg.exe");
    MTT_CHECK(!findGameFiles({root / "missing"}).has_value());
    std::filesystem::remove_all(root);
}

namespace {

// A small CD image of our own: the descriptors, a top folder with the two
// game files and a folder, and the files' bytes (made up).
std::vector<std::uint8_t> makeImage(bool raw, std::uint32_t datSize) {
    constexpr std::uint32_t kData = 2048;
    const std::uint32_t datSectors = (datSize + kData - 1) / kData;
    const std::uint32_t sectors = 20 + datSectors;
    std::vector<std::vector<std::uint8_t>> data(sectors, std::vector<std::uint8_t>(kData, 0));
    auto put32 = [](std::vector<std::uint8_t>& s, std::size_t at, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            s[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i));
        }
    };
    auto descriptor = [&](std::uint32_t at, std::uint8_t type) {
        data[at][0] = type;
        std::memcpy(data[at].data() + 1, "CD001", 5);
        data[at][6] = 1;
    };
    descriptor(16, 1);
    descriptor(17, 255);
    data[16][156] = 34;
    put32(data[16], 156 + 2, 18);
    put32(data[16], 156 + 10, kData);
    std::size_t pos = 0;
    auto record = [&](std::string_view name, std::uint32_t sector, std::uint32_t size, std::uint8_t flags) {
        auto& s = data[18];
        const auto length = static_cast<std::uint8_t>(33 + name.size() + (name.size() % 2 == 0 ? 1 : 0));
        s[pos] = length;
        put32(s, pos + 2, sector);
        put32(s, pos + 10, size);
        s[pos + 25] = flags;
        s[pos + 32] = static_cast<std::uint8_t>(name.size());
        std::memcpy(s.data() + pos + 33, name.data(), name.size());
        pos += length;
    };
    record(std::string_view("\0", 1), 18, kData, 2);
    record(std::string_view("\1", 1), 18, kData, 2);
    record("DNCDPRG.EXE;1", 19, 10, 0);
    record("EXTRAS", 18, kData, 2);
    record("DUNE.DAT;1", 20, datSize, 0);
    for (std::uint32_t i = 0; i < 10; ++i) {
        data[19][i] = static_cast<std::uint8_t>('a' + i);
    }
    for (std::uint32_t i = 0; i < datSize; ++i) {
        data[20 + i / kData][i % kData] = static_cast<std::uint8_t>(i * 7);
    }
    std::vector<std::uint8_t> out;
    for (std::uint32_t s = 0; s < sectors; ++s) {
        if (raw) {
            const std::uint8_t head[16] = {0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 2, 0, 1};
            out.insert(out.end(), head, head + 16);
        }
        out.insert(out.end(), data[s].begin(), data[s].end());
        if (raw) {
            out.insert(out.end(), 288, 0);
        }
    }
    return out;
}

std::vector<std::uint8_t> readAll(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

void writeAll(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),
                                                static_cast<std::streamsize>(bytes.size()));
}

} // namespace

MTT_TEST("game files: copies both files out of a plain and a raw CD image") {
    const auto root = std::filesystem::temp_directory_path() / "mtt_test_cd_image";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    constexpr std::uint32_t kDatSize = 2048 * 600 + 77; // more than one piece
    for (const bool raw : {false, true}) {
        const auto image = root / (raw ? "game.bin" : "game.iso");
        writeAll(image, makeImage(raw, kDatSize));
        const auto into = root / (raw ? "raw" : "plain");
        std::uint64_t last = 0;
        std::uint64_t whole = 0;
        importGameFiles(image, into, [&](std::uint64_t done, std::uint64_t total) {
            MTT_CHECK(done >= last);
            last = done;
            whole = total;
        });
        MTT_CHECK(whole == kDatSize + 10);
        MTT_CHECK(last == whole);
        const auto dat = readAll(into / "DUNE.DAT");
        MTT_CHECK(dat.size() == kDatSize);
        MTT_CHECK(dat.size() == kDatSize && dat[kDatSize - 1] == static_cast<std::uint8_t>((kDatSize - 1) * 7));
        MTT_CHECK(readAll(into / "DNCDPRG.EXE") == std::vector<std::uint8_t>({'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
                                                                              'i', 'j'}));
        MTT_CHECK(!std::filesystem::exists(into / "DUNE.DAT.part"));
        const auto found = findGameFiles({into});
        MTT_CHECK(found && !found->program.empty());
    }
    // From a folder, such as the CD itself.
    importGameFiles(root / "plain", root / "copy", {});
    MTT_CHECK(readAll(root / "copy" / "DUNE.DAT").size() == kDatSize);
    std::filesystem::remove_all(root);
}

MTT_TEST("game files: refuses images that are not CDs, are cut short or lack a file") {
    const auto root = std::filesystem::temp_directory_path() / "mtt_test_cd_bad";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    auto fails = [&](const std::filesystem::path& source) {
        try {
            importGameFiles(source, root / "out", {});
        } catch (const FormatError&) {
            return true;
        }
        return false;
    };
    writeAll(root / "junk.iso", std::vector<std::uint8_t>(40000, 0x41));
    MTT_CHECK(fails(root / "junk.iso"));
    MTT_CHECK(fails(root / "missing.iso"));
    auto image = makeImage(false, 5000);
    image.resize(image.size() - 2048); // the last sector of DUNE.DAT is gone
    writeAll(root / "short.iso", image);
    MTT_CHECK(fails(root / "short.iso"));
    MTT_CHECK(!std::filesystem::exists(root / "out" / "DUNE.DAT"));
    MTT_CHECK(!std::filesystem::exists(root / "out" / "DUNE.DAT.part"));
    image = makeImage(false, 5000);
    image[18 * 2048 + 34 * 2 + 33] = 'X'; // DNCDPRG.EXE is now XNCDPRG.EXE
    writeAll(root / "lacking.iso", image);
    MTT_CHECK(fails(root / "lacking.iso"));
    std::filesystem::create_directories(root / "empty");
    MTT_CHECK(fails(root / "empty"));
    std::filesystem::remove_all(root);
}

MTT_TEST("game files: finds the MT-32 ROMs in any case, as a pair") {
    const auto root = std::filesystem::temp_directory_path() / "mtt_test_mt32_roms";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "half");
    std::filesystem::create_directories(root / "both");
    std::ofstream(root / "half" / "MT32_CONTROL.ROM") << "x";
    std::ofstream(root / "both" / "mt32_control.rom") << "x";
    std::ofstream(root / "both" / "Mt32_Pcm.Rom") << "x";
    const auto found = findMt32Roms({root / "missing", root / "half", root / "both"});
    MTT_CHECK(found && found->control.filename() == "mt32_control.rom" && found->pcm.filename() == "Mt32_Pcm.Rom");
    MTT_CHECK(!findMt32Roms({root / "half"}).has_value());
    std::filesystem::remove_all(root);
}

MTT_TEST("game files: copies the player's extra files under the names they are wanted by") {
    const auto root = std::filesystem::temp_directory_path() / "mtt_test_extra_files";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "from" / "inner");
    std::filesystem::create_directories(root / "to");
    std::ofstream(root / "from" / "control.bin") << "control";
    std::ofstream(root / "from" / "pcm.bin") << "pcm";
    std::ofstream(root / "from" / "notes.txt") << "notes";
    std::ofstream(root / "from" / "inner" / "deep.bin") << "control";
    std::ofstream(root / "to" / "mt32_pcm.rom") << "old";
    const NameFor nameFor = [](const std::filesystem::path& file) -> std::string {
        std::ifstream in(file, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(in)), {});
        if (text == "control") {
            return "MT32_CONTROL.ROM";
        }
        if (text == "pcm") {
            return "MT32_PCM.ROM";
        }
        return {};
    };
    // A file by itself and the files in a folder, but not in its folders;
    // the second control ROM is left alone.
    const auto names =
        importExtraFiles({root / "from" / "pcm.bin", root / "from", root / "missing"}, root / "to", nameFor);
    MTT_CHECK(names == std::vector<std::string>({"MT32_PCM.ROM", "MT32_CONTROL.ROM"}));
    const auto roms = findMt32Roms({root / "to"});
    MTT_CHECK(roms && roms->control.filename() == "MT32_CONTROL.ROM" && roms->pcm.filename() == "MT32_PCM.ROM");
    {
        std::ifstream in(roms->pcm, std::ios::binary); // closed before the folder goes
        MTT_CHECK(std::string((std::istreambuf_iterator<char>(in)), {}) == "pcm");
    }
    MTT_CHECK(importExtraFiles({root / "from" / "notes.txt"}, root / "to", nameFor).empty());
    MTT_CHECK(!std::filesystem::exists(root / "to" / "MT32_PCM.ROM.part"));
    std::filesystem::remove_all(root);
}
