// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/game_files.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace mentat {

namespace {

bool isDatName(const std::filesystem::path& p) {
    std::string name = p.filename().string();
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return name == "DUNE.DAT";
}

} // namespace

std::optional<GameFiles> findGameFiles(const std::vector<std::filesystem::path>& folders) {
    for (const auto& folder : folders) {
        std::error_code ec;
        if (!std::filesystem::is_directory(folder, ec)) {
            continue;
        }
        for (const auto& item : std::filesystem::directory_iterator(folder, ec)) {
            if (item.is_regular_file(ec) && isDatName(item.path())) {
                return GameFiles{item.path()};
            }
        }
    }
    return std::nullopt;
}

} // namespace mentat
