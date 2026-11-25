// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reading files from an image of the player's game CD (an ISO 9660 file
// system), so Mentat can copy its two game files out of it. See
// docs/formats/cd_image.md.
//
// Both kinds of image are read: plain ones with 2048 bytes a sector (.iso)
// and raw ones with 2352 (.bin). Only the top folder is searched, which is
// where the game's files are.
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mentat {

struct CdEntry {
    std::string name; // as on the CD, without the ";1" version
    std::uint32_t sector = 0;
    std::uint32_t size = 0;
};

class CdImage {
public:
    static constexpr std::uint32_t kSectorData = 2048;

    // Opens an image and reads its top folder. Throws FormatError when the
    // file is not a CD image or is damaged.
    static CdImage open(const std::filesystem::path& path);

    const std::vector<CdEntry>& entries() const { return entries_; }

    // Finds a file in the top folder by name, ignoring case.
    const CdEntry* find(std::string_view name) const;

    // Copies a file out to `to`, calling `progress` with the bytes copied so
    // far after each piece. Throws FormatError on a read or write failure.
    void copyOut(const CdEntry& entry, const std::filesystem::path& to,
                 const std::function<void(std::uint64_t)>& progress);

private:
    // Reads the 2048 data bytes of one sector.
    void readSector(std::uint32_t sector, std::uint8_t* out);

    std::ifstream file_;
    std::uint64_t fileSize_ = 0;
    std::uint32_t rawSize_ = kSectorData; // bytes a sector takes in the file
    std::uint32_t dataStart_ = 0;         // where its data starts in it
    std::vector<CdEntry> entries_;
};

} // namespace mentat
