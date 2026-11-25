// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/game_files.hpp"

#include "mentat/data/cd_image.hpp"
#include "mentat/data/reader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
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

void copyFile(const std::filesystem::path& from, const std::filesystem::path& to,
              const std::function<void(std::uint64_t)>& progress) {
    std::ifstream in(from, std::ios::binary);
    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    if (!in || !out) {
        throw FormatError("cannot copy " + from.filename().string());
    }
    std::vector<char> buffer(1u << 20);
    std::uint64_t done = 0;
    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto got = in.gcount();
        if (got <= 0) {
            break;
        }
        out.write(buffer.data(), got);
        if (!out) {
            throw FormatError("cannot write " + to.string() + " (is the disk full?)");
        }
        done += static_cast<std::uint64_t>(got);
        progress(done);
    }
    if (in.bad()) {
        throw FormatError("cannot read " + from.filename().string());
    }
}

// Runs a copy, and removes what it wrote when it fails.
template <typename Copy>
void dropOnFailure(const std::filesystem::path& part, Copy&& copy) {
    try {
        copy();
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove(part, ec);
        throw;
    }
}

// Puts a finished copy in its place.
void finish(const std::filesystem::path& part, const std::filesystem::path& whole) {
    std::error_code ec;
    std::filesystem::rename(part, whole, ec);
    if (ec) {
        std::filesystem::remove(part, ec);
        throw FormatError("cannot write " + whole.string());
    }
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

void importGameFiles(const std::filesystem::path& source, const std::filesystem::path& folder,
                     const CopyProgress& progress) {
    static constexpr std::array<std::string_view, 2> kNames{"DUNE.DAT", "DNCDPRG.EXE"};
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    if (ec) {
        throw FormatError("cannot make the folder " + folder.string());
    }
    const auto report = progress ? progress : CopyProgress([](std::uint64_t, std::uint64_t) {});
    const auto partOf = [&](std::string_view name) { return folder / (std::string(name) + ".part"); };

    if (std::filesystem::is_directory(source, ec)) {
        std::array<std::filesystem::path, 2> from;
        std::uint64_t total = 0;
        for (std::size_t i = 0; i < kNames.size(); ++i) {
            from[i] = findIn(source, kNames[i]);
            if (from[i].empty()) {
                throw FormatError(std::string(kNames[i]) + " is not in that folder");
            }
            total += std::filesystem::file_size(from[i], ec);
        }
        std::uint64_t before = 0;
        for (std::size_t i = 0; i < kNames.size(); ++i) {
            dropOnFailure(partOf(kNames[i]), [&] {
                copyFile(from[i], partOf(kNames[i]), [&](std::uint64_t done) { report(before + done, total); });
            });
            before += std::filesystem::file_size(from[i], ec);
            finish(partOf(kNames[i]), folder / kNames[i]);
        }
        return;
    }

    auto image = CdImage::open(source);
    std::array<CdEntry, 2> entries;
    std::uint64_t total = 0;
    for (std::size_t i = 0; i < kNames.size(); ++i) {
        const CdEntry* entry = image.find(kNames[i]);
        if (entry == nullptr) {
            throw FormatError(std::string(kNames[i]) + " is not on that CD image");
        }
        entries[i] = *entry;
        total += entry->size;
    }
    std::uint64_t before = 0;
    for (std::size_t i = 0; i < kNames.size(); ++i) {
        dropOnFailure(partOf(kNames[i]), [&] {
            image.copyOut(entries[i], partOf(kNames[i]), [&](std::uint64_t done) { report(before + done, total); });
        });
        before += entries[i].size;
        finish(partOf(kNames[i]), folder / kNames[i]);
    }
}

} // namespace mentat
