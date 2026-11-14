// SPDX-License-Identifier: GPL-3.0-or-later
//
// DUNE.DAT, the CD version's archive. See docs/formats/dat.md.
//
// The archive is large (about 400 MB), so only its directory is read at
// first; files are read from disk when they are asked for.
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace melange {

struct DatEntry {
    std::string name; // upper case, folders separated by '\'
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    std::uint8_t flag = 0;
};

class DatDirectory {
public:
    // The directory area at the start of the archive.
    static constexpr std::size_t kDirectorySize = 65536;
    static constexpr std::size_t kEntrySize = 25;
    static constexpr std::size_t kNameSize = 16;

    // Reads the directory from the first bytes of the archive and checks
    // every entry against the archive's real size. Throws FormatError.
    static DatDirectory parse(std::span<const std::uint8_t> directory, std::uint64_t archiveSize);

    const std::vector<DatEntry>& entries() const { return entries_; }

    // Finds a file by name, ignoring case; '/' and '\' are the same.
    const DatEntry* find(std::string_view name) const;

private:
    std::vector<DatEntry> entries_;
};

class DatArchive {
public:
    // Opens the archive and reads its directory. Throws FormatError.
    static DatArchive open(const std::filesystem::path& path);

    const DatDirectory& directory() const { return directory_; }

    // The raw bytes of a file, as stored. Throws FormatError when the file
    // is missing or cannot be read.
    std::vector<std::uint8_t> read(const DatEntry& entry);
    std::vector<std::uint8_t> read(std::string_view name);

    // Like read(), but unpacks HSQ files.
    std::vector<std::uint8_t> load(std::string_view name);

private:
    std::ifstream file_;
    DatDirectory directory_;
};

} // namespace melange
