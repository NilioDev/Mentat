// SPDX-License-Identifier: GPL-3.0-or-later
//
// Finding the player's own copy of the original game.
#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace melange {

struct GameFiles {
    std::filesystem::path dat; // DUNE.DAT
};

// Looks for DUNE.DAT (in any case) directly in each folder, in order.
std::optional<GameFiles> findGameFiles(const std::vector<std::filesystem::path>& folders);

} // namespace melange
