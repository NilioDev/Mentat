// SPDX-License-Identifier: GPL-3.0-or-later
//
// Finding the player's own copy of the original game.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace mentat {

struct GameFiles {
    std::filesystem::path dat;     // DUNE.DAT
    std::filesystem::path program; // DNCDPRG.EXE, empty when not found
};

// Looks for DUNE.DAT (in any case) directly in each folder, in order, and
// for DNCDPRG.EXE in the same folder.
std::optional<GameFiles> findGameFiles(const std::vector<std::filesystem::path>& folders);

// The player's own Roland MT-32 ROMs, for the MT-32 music.
struct Mt32Roms {
    std::filesystem::path control;
    std::filesystem::path pcm;
};

// Looks in each folder, in order, for MT32_CONTROL.ROM and MT32_PCM.ROM (in
// any case), or else CM32L_CONTROL.ROM and CM32L_PCM.ROM.
std::optional<Mt32Roms> findMt32Roms(const std::vector<std::filesystem::path>& folders);

// Copies DUNE.DAT and DNCDPRG.EXE into `folder` (made when missing) from
// `source`: an image of the CD (.iso or .bin), or a folder that holds them,
// such as the CD itself. `progress` is called now and then with the bytes
// copied so far and the bytes to copy in all. Each file is written under a
// passing name first, so a copy that stops halfway leaves nothing that looks
// whole. Throws FormatError with a message meant for the player.
using CopyProgress = std::function<void(std::uint64_t done, std::uint64_t total)>;
void importGameFiles(const std::filesystem::path& source, const std::filesystem::path& folder,
                     const CopyProgress& progress);

// Copies the player's own files that are not on the game CD, such as the
// MT-32 ROMs, into `folder` (made when missing). Each of `files` is looked
// at, and for a folder each file directly in it; `nameFor` gives the name a
// file is kept under, or an empty name for a file that is not wanted.
// Returns the names copied, in order, each once. Throws FormatError when a
// wanted file cannot be copied.
using NameFor = std::function<std::string(const std::filesystem::path&)>;
std::vector<std::string> importExtraFiles(const std::vector<std::filesystem::path>& files,
                                          const std::filesystem::path& folder, const NameFor& nameFor);

} // namespace mentat
