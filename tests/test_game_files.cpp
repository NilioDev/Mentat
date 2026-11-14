// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.hpp"

#include "mentat/game_files.hpp"

#include <filesystem>
#include <fstream>

using namespace mentat;

MTT_TEST("game files: finds dune.dat in any case and skips missing folders") {
    const auto root = std::filesystem::temp_directory_path() / "mtt_test_game_files";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "game");
    std::ofstream(root / "game" / "dune.dat") << "x";
    const auto found = findGameFiles({root / "missing", root / "game"});
    MTT_CHECK(found.has_value());
    MTT_CHECK(found && found->dat.filename() == "dune.dat");
    MTT_CHECK(!findGameFiles({root / "missing"}).has_value());
    std::filesystem::remove_all(root);
}
