// SPDX-License-Identifier: GPL-3.0-or-later
//
// Finding the player's own copy of the original game.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
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

// Copies DUNE.DAT and DNCDPRG.EXE into `folder` (made when missing) from
// `source`: an image of the CD (.iso or .bin), or a folder that holds them,
// such as the CD itself. `progress` is called now and then with the bytes
// copied so far and the bytes to copy in all. Each file is written under a
// passing name first, so a copy that stops halfway leaves nothing that looks
// whole. Throws FormatError with a message meant for the player.
using CopyProgress = std::function<void(std::uint64_t done, std::uint64_t total)>;
void importGameFiles(const std::filesystem::path& source, const std::filesystem::path& folder,
                     const CopyProgress& progress);

} // namespace mentat
