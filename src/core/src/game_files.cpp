// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/game_files.hpp"

#include "mentat/data/cd_image.hpp"
#include "mentat/data/reader.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>
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

std::optional<Mt32Roms> findMt32Roms(const std::vector<std::filesystem::path>& folders) {
    for (const auto& folder : folders) {
        std::error_code ec;
        if (!std::filesystem::is_directory(folder, ec)) {
            continue;
        }
        for (const std::string_view kind : {"MT32", "CM32L"}) {
            auto control = findIn(folder, std::string(kind) + "_CONTROL.ROM");
            auto pcm = findIn(folder, std::string(kind) + "_PCM.ROM");
            if (!control.empty() && !pcm.empty()) {
                return Mt32Roms{std::move(control), std::move(pcm)};
            }
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

std::vector<std::string> importExtraFiles(const std::vector<std::filesystem::path>& files,
                                          const std::filesystem::path& folder, const NameFor& nameFor) {
    std::vector<std::filesystem::path> candidates;
    for (const auto& file : files) {
        std::error_code ec;
        if (std::filesystem::is_directory(file, ec)) {
            for (const auto& item : std::filesystem::directory_iterator(file, ec)) {
                if (item.is_regular_file(ec)) {
                    candidates.push_back(item.path());
                }
            }
        } else if (std::filesystem::is_regular_file(file, ec)) {
            candidates.push_back(file);
        }
    }
    std::vector<std::string> copied;
    for (const auto& from : candidates) {
        const std::string name = nameFor(from);
        if (name.empty() || std::find(copied.begin(), copied.end(), name) != copied.end()) {
            continue;
        }
        std::error_code ec;
        std::filesystem::create_directories(folder, ec);
        if (ec) {
            throw FormatError("cannot make the folder " + folder.string());
        }
        const auto part = folder / (name + ".part");
        dropOnFailure(part, [&] { copyFile(from, part, [](std::uint64_t) {}); });
        // A file found here with the same name in another case would be
        // found before the new one.
        for (const auto& item : std::filesystem::directory_iterator(folder, ec)) {
            if (hasName(item.path(), name) && item.path().filename() != name) {
                std::filesystem::remove(item.path(), ec);
            }
        }
        finish(part, folder / name);
        copied.push_back(name);
    }
    return copied;
}

namespace {

constexpr std::uint64_t kMcdFileTable = 0x19354;  // in the disc's DUNE.DAT
constexpr std::uint64_t kMcdPieceTable = 0x18168; // the pieces' file numbers
constexpr int kMcdPieces = 9;
constexpr std::uint32_t kMcdMainFile = 1; // the main processor's program
constexpr std::uint64_t kMcdProgramStart = 0x24AA;
constexpr std::size_t kMcdProgramSize = 0x1F73; // to the end of the file
constexpr std::uint32_t kMcdMaxPiece = 0x80F0;

std::string mcdSongName(int n) { return "MCDSONG" + std::to_string(n) + ".BIN"; }

void writeWhole(const std::filesystem::path& to, const std::vector<std::uint8_t>& bytes) {
    const auto part = std::filesystem::path(to.string() + ".part");
    dropOnFailure(part, [&] {
        std::ofstream out(part, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            throw FormatError("cannot write " + to.string());
        }
    });
    finish(part, to);
}

} // namespace

bool isMegaCdImage(const std::filesystem::path& source) {
    try {
        auto image = CdImage::open(source);
        const auto first = image.firstSector();
        return std::memcmp(first.data(), "SEGADISCSYSTEM", 14) == 0;
    } catch (const FormatError&) {
        return false;
    }
}

void importMegaCdMusic(const std::filesystem::path& source, const std::filesystem::path& folder) {
    auto image = CdImage::open(source);
    const CdEntry* dat = image.find("DUNE.DAT");
    if (dat == nullptr) {
        throw FormatError("DUNE.DAT is not on that Mega CD image");
    }
    const CdEntry data = *dat;
    const auto fileAt = [&](std::uint32_t n) {
        const auto entry = image.read(data, kMcdFileTable + 6ull * n, 6);
        if (entry.size() != 6) {
            throw FormatError("that Mega CD image is not the one Mentat knows");
        }
        const auto b = [&](std::size_t i) { return static_cast<std::uint32_t>(entry[i]); };
        const std::uint32_t sector = (b(0) << 16) | (b(1) << 8) | b(2);
        const std::uint32_t size = (b(3) << 16) | (b(4) << 8) | b(5);
        return std::pair<std::uint64_t, std::uint32_t>{std::uint64_t{sector} * CdImage::kSectorData, size};
    };

    const auto [mainAt, mainSize] = fileAt(kMcdMainFile);
    if (mainSize < kMcdProgramStart + kMcdProgramSize) {
        throw FormatError("that Mega CD image is not the one Mentat knows");
    }
    const auto program = image.read(data, mainAt + kMcdProgramStart, kMcdProgramSize);
    if (program.size() != kMcdProgramSize || program[0] != 0xC3) {
        throw FormatError("that Mega CD image is not the one Mentat knows");
    }
    const auto numbers = image.read(data, kMcdPieceTable, 2 * kMcdPieces);
    std::vector<std::vector<std::uint8_t>> pieces;
    for (int i = 0; i < kMcdPieces && numbers.size() == 2u * kMcdPieces; ++i) {
        const auto at2 = 2 * static_cast<std::size_t>(i);
        const std::uint32_t n = (static_cast<std::uint32_t>(numbers[at2]) << 8) | numbers[at2 + 1];
        const auto [at, size] = fileAt(n);
        auto piece = image.read(data, at, std::min(size, kMcdMaxPiece));
        if (piece.size() < 0x40 || piece[2] != 0x32 || piece[3] != 0x00) {
            throw FormatError("that Mega CD image is not the one Mentat knows");
        }
        pieces.push_back(std::move(piece));
    }
    if (pieces.size() != kMcdPieces) {
        throw FormatError("that Mega CD image is not the one Mentat knows");
    }
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    if (ec) {
        throw FormatError("cannot make the folder " + folder.string());
    }
    writeWhole(folder / "MCDPROG.BIN", program);
    for (int i = 0; i < kMcdPieces; ++i) {
        writeWhole(folder / mcdSongName(i + 1), pieces[static_cast<std::size_t>(i)]);
    }
}

std::optional<std::filesystem::path> findMegaCdMusic(const std::vector<std::filesystem::path>& folders) {
    for (const auto& folder : folders) {
        std::error_code ec;
        if (!std::filesystem::is_directory(folder, ec) || findIn(folder, "MCDPROG.BIN").empty()) {
            continue;
        }
        bool all = true;
        for (int i = 1; i <= kMcdPieces && all; ++i) {
            all = !findIn(folder, mcdSongName(i)).empty();
        }
        if (all) {
            return folder;
        }
    }
    return std::nullopt;
}

} // namespace mentat
