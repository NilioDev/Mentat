// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/game_files.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace mentat {

namespace {

bool hasName(const std::filesystem::path& p, std::string_view wanted) {
    std::string name = p.filename().string();
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return name == wanted;
}

std::filesystem::path findIn(const std::filesystem::path& folder, std::string_view wanted) {
    std::error_code ec;
    for (const auto& item : std::filesystem::directory_iterator(folder, ec)) {
        if (item.is_regular_file(ec) && hasName(item.path(), wanted)) {
            return item.path();
        }
    }
    return {};
}

} // namespace

std::optional<GameFiles> findGameFiles(const std::vector<std::filesystem::path>& folders) {
    for (const auto& folder : folders) {
        std::error_code ec;
        if (!std::filesystem::is_directory(folder, ec)) {
            continue;
        }
        auto dat = findIn(folder, "DUNE.DAT");
        if (!dat.empty()) {
            return GameFiles{std::move(dat), findIn(folder, "DNCDPRG.EXE")};
        }
    }
    return std::nullopt;
}

} // namespace mentat
