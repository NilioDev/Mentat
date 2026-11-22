// SPDX-License-Identifier: GPL-3.0-or-later
//
// Finding the player's own copy of the original game.
#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace mentat {

struct GameFiles {
    std::filesystem::path dat;     // DUNE.DAT
    std::filesystem::path program; // DNCDPRG.EXE, empty when not found
};

// Looks for DUNE.DAT (in any case) directly in each folder, in order, and
// for DNCDPRG.EXE in the same folder.
std::optional<GameFiles> findGameFiles(const std::vector<std::filesystem::path>& folders);

} // namespace mentat
