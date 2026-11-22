// SPDX-License-Identifier: GPL-3.0-or-later
//
// Opens the developer's own DUNE.DAT for tests that need real data.
#pragma once

#include "mentat/data/dat_archive.hpp"
#include "mentat/data/program_file.hpp"
#include "mentat/game_files.hpp"

#include <optional>

namespace mentat::test {

inline std::optional<DatArchive> openGameData() {
    const auto files = findGameFiles({MENTAT_GAMEDATA_DIR});
    if (!files) {
        return std::nullopt;
    }
    return DatArchive::open(files->dat);
}

inline std::optional<ProgramFile> openProgram() {
    const auto files = findGameFiles({MENTAT_GAMEDATA_DIR});
    if (!files || files->program.empty()) {
        return std::nullopt;
    }
    return ProgramFile::open(files->program);
}

// Files in DUNE.DAT with the .HSQ extension that are not picture files.
inline bool isPictureFile(const std::string& name) {
    static const char* const notPictures[] = {
        "DIALOGUE", "COMMAND", "DNVGA", "DN386", "DNPCS", "DNADL", "DNADP", "DNADG", "DNMID", "DNSDB", "DNSBP",
        "CONDIT", "MAP.", "GLOBDATA", "FREQ", "PHRASE", "SN1", "SN2", "SN3", "SN4", "SN6", "SNA", "SEKENCE", "WATER",
        "WORMSUIT", "WORMINTR", "WARSONG", "MORNING", "SIETCHM", "BAGDAD", "ARRAKIS", "CRYOMUS", "MAP2",
    };
    if (name.size() < 4 || name.compare(name.size() - 4, 4, ".HSQ") != 0) {
        return false;
    }
    for (const char* prefix : notPictures) {
        if (name.rfind(prefix, 0) == 0) {
            return false;
        }
    }
    return true;
}

} // namespace mentat::test
