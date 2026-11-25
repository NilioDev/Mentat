// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/data/cd_image.hpp"

#include "mentat/data/reader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <span>

namespace mentat {

namespace {

constexpr std::uint32_t kRawSector = 2352;
constexpr std::uint32_t kFirstDescriptor = 16;
// No real top folder comes near this; it keeps a damaged one from making
// us read the whole image.
constexpr std::uint32_t kMaxFolderSize = 1u << 20;

std::string upper(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

// A raw sector starts with twelve sync bytes: 00, ten times FF, 00.
bool isSync(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 12 || bytes[0] != 0 || bytes[11] != 0) {
        return false;
    }
    return std::all_of(bytes.begin() + 1, bytes.begin() + 11, [](std::uint8_t b) { return b == 0xFF; });
}

} // namespace

CdImage CdImage::open(const std::filesystem::path& path) {
    CdImage image;
    image.file_.open(path, std::ios::binary);
    if (!image.file_) {
        throw FormatError("cannot open the CD image");
    }
    std::error_code ec;
    image.fileSize_ = std::filesystem::file_size(path, ec);
    if (ec) {
        throw FormatError("cannot read the CD image's size");
    }

    // Raw images: the first descriptor's sector begins with the sync bytes,
    // then the address and the mode, which says where the data starts.
    if (image.fileSize_ >= std::uint64_t{kFirstDescriptor + 1} * kRawSector) {
        std::array<std::uint8_t, 24> head{};
        image.file_.seekg(static_cast<std::streamoff>(kFirstDescriptor * kRawSector));
        image.file_.read(reinterpret_cast<char*>(head.data()), head.size());
        if (image.file_ && isSync(head)) {
            image.rawSize_ = kRawSector;
            image.dataStart_ = head[15] == 2 ? 24 : 16;
        }
        image.file_.clear();
    }

    // The volume descriptors follow one another until the closing one
    // (type 255); the primary one (type 1) says where the top folder is.
    std::array<std::uint8_t, kSectorData> sector{};
    std::optional<std::uint32_t> folderSector;
    std::uint32_t folderSize = 0;
    for (std::uint32_t s = kFirstDescriptor; s < kFirstDescriptor + 32 && !folderSector; ++s) {
        image.readSector(s, sector.data());
        if (std::memcmp(sector.data() + 1, "CD001", 5) != 0) {
            throw FormatError("not a CD image");
        }
        if (sector[0] == 255) {
            break;
        }
        if (sector[0] == 1) {
            Reader r(sector);
            r.seek(156 + 2);
            folderSector = r.u32();
            r.seek(156 + 10);
            folderSize = r.u32();
        }
    }
    if (!folderSector) {
        throw FormatError("the CD image has no file system");
    }
    if (folderSize > kMaxFolderSize) {
        throw FormatError("the CD image's folder is damaged");
    }

    // The folder's records never cross a sector's end; a zero length means
    // the rest of the sector is empty.
    const std::uint32_t sectors = (folderSize + kSectorData - 1) / kSectorData;
    for (std::uint32_t i = 0; i < sectors; ++i) {
        image.readSector(*folderSector + i, sector.data());
        std::size_t pos = 0;
        while (pos < kSectorData && sector[pos] != 0) {
            Reader r(std::span<const std::uint8_t>(sector).subspan(pos, sector[pos]));
            r.skip(2);
            const std::uint32_t at = r.u32();
            r.skip(4);
            const std::uint32_t size = r.u32();
            r.skip(25 - 14);
            const std::uint8_t flags = r.u8();
            r.skip(32 - 26);
            const std::uint8_t nameLength = r.u8();
            const auto name = r.bytes(nameLength);
            pos += sector[pos];
            // Skip folders and the records for "this folder" and "the one
            // above" (one byte names 0 and 1).
            if ((flags & 0x02) != 0 || nameLength <= 1) {
                continue;
            }
            std::string text(name.begin(), name.end());
            if (const auto semicolon = text.find(';'); semicolon != std::string::npos) {
                text.resize(semicolon);
            }
            if (!text.empty() && text.back() == '.') {
                text.pop_back();
            }
            const std::uint32_t used = (size + kSectorData - 1) / kSectorData;
            const std::uint64_t end =
                (std::uint64_t{at} + used - 1) * image.rawSize_ + image.dataStart_ + kSectorData;
            if (used > 0 && end > image.fileSize_) {
                throw FormatError("a file on the CD image goes past its end");
            }
            image.entries_.push_back(CdEntry{std::move(text), at, size});
        }
    }
    return image;
}

const CdEntry* CdImage::find(std::string_view name) const {
    const std::string wanted = upper(name);
    for (const auto& entry : entries_) {
        if (upper(entry.name) == wanted) {
            return &entry;
        }
    }
    return nullptr;
}

void CdImage::readSector(std::uint32_t sector, std::uint8_t* out) {
    const std::uint64_t at = std::uint64_t{sector} * rawSize_ + dataStart_;
    if (at + kSectorData > fileSize_) {
        throw FormatError("the CD image ends too soon");
    }
    file_.seekg(static_cast<std::streamoff>(at));
    file_.read(reinterpret_cast<char*>(out), kSectorData);
    if (!file_) {
        file_.clear();
        throw FormatError("cannot read the CD image");
    }
}

void CdImage::copyOut(const CdEntry& entry, const std::filesystem::path& to,
                      const std::function<void(std::uint64_t)>& progress) {
    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw FormatError("cannot write " + to.string());
    }
    std::vector<std::uint8_t> buffer;
    std::uint64_t done = 0;
    const std::uint32_t sectors = (entry.size + kSectorData - 1) / kSectorData;
    // Plain images are copied in large pieces, raw ones a sector at a time.
    constexpr std::uint32_t kPiece = 512;
    std::array<std::uint8_t, kSectorData> sector{};
    for (std::uint32_t i = 0; i < sectors;) {
        const std::uint32_t count = std::min(kPiece, sectors - i);
        buffer.clear();
        for (std::uint32_t k = 0; k < count; ++k) {
            readSector(entry.sector + i + k, sector.data());
            buffer.insert(buffer.end(), sector.begin(), sector.end());
        }
        i += count;
        const std::uint64_t keep = std::min<std::uint64_t>(buffer.size(), entry.size - done);
        out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(keep));
        if (!out) {
            throw FormatError("cannot write " + to.string() + " (is the disk full?)");
        }
        done += keep;
        if (progress) {
            progress(done);
        }
    }
}

std::vector<std::uint8_t> CdImage::read(const CdEntry& entry, std::uint64_t offset, std::size_t size) {
    std::vector<std::uint8_t> out;
    if (offset >= entry.size) {
        return out;
    }
    const std::uint64_t end = std::min<std::uint64_t>(entry.size, offset + size);
    out.reserve(static_cast<std::size_t>(end - offset));
    std::array<std::uint8_t, kSectorData> sector{};
    for (std::uint64_t at = offset; at < end;) {
        const auto index = static_cast<std::uint32_t>(at / kSectorData);
        readSector(entry.sector + index, sector.data());
        const auto from = static_cast<std::size_t>(at % kSectorData);
        const auto take = static_cast<std::size_t>(std::min<std::uint64_t>(kSectorData - from, end - at));
        out.insert(out.end(), sector.begin() + static_cast<std::ptrdiff_t>(from),
                   sector.begin() + static_cast<std::ptrdiff_t>(from + take));
        at += take;
    }
    return out;
}

std::vector<std::uint8_t> CdImage::firstSector() {
    std::vector<std::uint8_t> out(kSectorData);
    readSector(0, out.data());
    return out;
}

} // namespace mentat
