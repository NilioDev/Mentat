// SPDX-License-Identifier: GPL-3.0-or-later
#include "melange/data/dat_archive.hpp"

#include "melange/data/hsq.hpp"
#include "melange/data/reader.hpp"

#include <algorithm>
#include <cctype>

namespace melange {

namespace {

char normalise(char c) {
    if (c == '/') {
        return '\\';
    }
    return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

bool sameName(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return normalise(x) == normalise(y); });
}

bool safeName(std::string_view name) {
    if (name.empty() || name.front() == '\\' || name.front() == '/') {
        return false;
    }
    if (name.find(':') != std::string_view::npos || name.find("..") != std::string_view::npos) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u > 0x20 && u < 0x7F;
    });
}

} // namespace

DatDirectory DatDirectory::parse(std::span<const std::uint8_t> directory, std::uint64_t archiveSize) {
    if (directory.size() < kDirectorySize || archiveSize < kDirectorySize) {
        throw FormatError("DUNE.DAT is shorter than its directory");
    }
    Reader r(directory.first(kDirectorySize));
    const std::size_t slots = r.u16();
    if (slots > (kDirectorySize - 2) / kEntrySize) {
        throw FormatError("DUNE.DAT lists more slots than its directory holds");
    }
    DatDirectory dir;
    for (std::size_t i = 0; i < slots; ++i) {
        const auto raw = r.bytes(kNameSize);
        const auto end = std::find(raw.begin(), raw.end(), std::uint8_t{0});
        std::string name(raw.begin(), end);
        DatEntry entry;
        entry.size = r.u32();
        entry.offset = r.u32();
        entry.flag = r.u8();
        if (name.empty()) {
            break; // the rest of the slots are unused
        }
        if (!safeName(name)) {
            throw FormatError("DUNE.DAT holds a file with an unsafe name");
        }
        if (entry.offset < kDirectorySize || entry.offset > archiveSize ||
            entry.size > archiveSize - entry.offset) {
            throw FormatError("DUNE.DAT entry " + name + " lies outside the archive");
        }
        std::transform(name.begin(), name.end(), name.begin(), normalise);
        entry.name = std::move(name);
        dir.entries_.push_back(std::move(entry));
    }
    return dir;
}

const DatEntry* DatDirectory::find(std::string_view name) const {
    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const DatEntry& e) { return sameName(e.name, name); });
    return it == entries_.end() ? nullptr : &*it;
}

DatArchive DatArchive::open(const std::filesystem::path& path) {
    DatArchive archive;
    archive.file_.open(path, std::ios::binary);
    if (!archive.file_) {
        throw FormatError("cannot open " + path.string());
    }
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        throw FormatError("cannot read the size of " + path.string());
    }
    std::vector<std::uint8_t> head(DatDirectory::kDirectorySize);
    archive.file_.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
    if (static_cast<std::size_t>(archive.file_.gcount()) != head.size()) {
        throw FormatError("DUNE.DAT is shorter than its directory");
    }
    archive.directory_ = DatDirectory::parse(head, size);
    return archive;
}

std::vector<std::uint8_t> DatArchive::read(const DatEntry& entry) {
    std::vector<std::uint8_t> bytes(entry.size);
    file_.clear();
    file_.seekg(static_cast<std::streamoff>(entry.offset));
    file_.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file_ || static_cast<std::size_t>(file_.gcount()) != bytes.size()) {
        throw FormatError("cannot read " + entry.name + " from DUNE.DAT");
    }
    return bytes;
}

std::vector<std::uint8_t> DatArchive::read(std::string_view name) {
    const DatEntry* entry = directory_.find(name);
    if (entry == nullptr) {
        throw FormatError("DUNE.DAT has no file " + std::string(name));
    }
    return read(*entry);
}

std::vector<std::uint8_t> DatArchive::load(std::string_view name) {
    auto bytes = read(name);
    const bool packedName = name.size() > 4 && sameName(name.substr(name.size() - 4), ".HSQ");
    if (packedName && isHsq(bytes)) {
        return unpackHsq(bytes);
    }
    return bytes;
}

} // namespace melange
