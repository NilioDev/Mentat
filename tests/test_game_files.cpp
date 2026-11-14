// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.hpp"

#include "melange/game_files.hpp"

#include <filesystem>
#include <fstream>

using namespace melange;

MLG_TEST("game files: finds dune.dat in any case and skips missing folders") {
    const auto root = std::filesystem::temp_directory_path() / "mlg_test_game_files";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "game");
    std::ofstream(root / "game" / "dune.dat") << "x";
    const auto found = findGameFiles({root / "missing", root / "game"});
    MLG_CHECK(found.has_value());
    MLG_CHECK(found && found->dat.filename() == "dune.dat");
    MLG_CHECK(!findGameFiles({root / "missing"}).has_value());
    std::filesystem::remove_all(root);
}
