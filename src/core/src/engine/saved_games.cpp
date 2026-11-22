// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/saved_games.hpp"

#include "mentat/data/program_file.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/data/save_file.hpp"
#include "mentat/engine/dialogue.hpp"
#include "mentat/engine/world.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace mentat {

namespace {

// A saved game is never bigger than its contents unpacked plus a little.
constexpr std::uintmax_t kLargestFile = 0x10000 + 8;

// The original keeps the dialogue lists at this data offset, with the
// offsets of its table counted from the data part's start.
constexpr std::uint16_t kDialogueAt = 0xAA76;

// Adds `by` to each word of the lists' table, whose first word (before the
// change) gives its size.
void moveTable(std::span<std::uint8_t> lists, std::uint16_t first, int by) {
    const std::size_t words = std::min<std::size_t>(first / 2u, lists.size() / 2);
    for (std::size_t i = 0; i < words; ++i) {
        const auto w = static_cast<std::uint16_t>(lists[2 * i] | (lists[2 * i + 1] << 8));
        const auto moved = static_cast<std::uint16_t>(w + by);
        lists[2 * i] = static_cast<std::uint8_t>(moved);
        lists[2 * i + 1] = static_cast<std::uint8_t>(moved >> 8);
    }
}

} // namespace

SavedGames::SavedGames(std::filesystem::path folder, const ProgramFile& program, World& world, Dialogue& dialogue,
                       MapFn map)
    : folder_(std::move(folder)), program_(program), world_(world), dialogue_(dialogue), map_(std::move(map)) {}

std::filesystem::path SavedGames::pathOf(int slot) const {
    return folder_ / ("DUNE37S" + std::to_string(slot) + ".SAV");
}

std::optional<std::vector<std::uint8_t>> SavedGames::read(int slot) const {
    if (folder_.empty()) {
        return std::nullopt;
    }
    std::filesystem::path path = pathOf(slot);
    std::error_code e;
    if (!std::filesystem::is_regular_file(path, e)) {
        // The original's own files may have lower-case names.
        path = folder_ / ("dune37s" + std::to_string(slot) + ".sav");
        if (!std::filesystem::is_regular_file(path, e)) {
            return std::nullopt;
        }
    }
    const std::uintmax_t size = std::filesystem::file_size(path, e);
    if (e || size > kLargestFile) {
        return std::nullopt;
    }
    std::ifstream f(path, std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (!f.good() && !f.eof()) {
        return std::nullopt;
    }
    return bytes;
}

std::optional<std::uint16_t> SavedGames::clockOf(int slot) const {
    const auto bytes = read(slot);
    if (!bytes || bytes->size() < 2) {
        return std::nullopt;
    }
    return SaveFile::clock(*bytes);
}

std::vector<std::uint8_t> SavedGames::contents() const {
    std::vector<std::uint8_t> out(SaveFile::kSize, 0);
    std::span<std::uint8_t> at(out);
    SaveFile::packMap(map_(), at.subspan(0, SaveFile::kMapPart));
    at = at.subspan(SaveFile::kMapPart);
    // The original keeps a few bytes of its program here, and puts them back
    // on loading: they are written as the program holds them.
    const auto code = program_.code();
    if (code.size() < SaveFile::kCodeOffset + SaveFile::kCodePart) {
        throw FormatError("save: program too short");
    }
    std::copy_n(code.begin() + SaveFile::kCodeOffset, SaveFile::kCodePart, at.begin());
    at = at.subspan(SaveFile::kCodePart);
    const auto& said = dialogue_.bytes();
    const std::size_t n = std::min(said.size(), SaveFile::kDialoguePart);
    std::copy_n(said.begin(), n, at.begin());
    if (n >= 2) {
        moveTable(at.subspan(0, n), static_cast<std::uint16_t>(said[0] | (said[1] << 8)), kDialogueAt);
    }
    at = at.subspan(SaveFile::kDialoguePart);
    const auto& state = world_.bytes();
    std::copy_n(state.begin(), std::min(state.size(), SaveFile::kStatePart), at.begin());
    return out;
}

void SavedGames::restore(std::span<const std::uint8_t> contents) {
    if (contents.size() != SaveFile::kSize) {
        throw FormatError("save: wrong size of contents");
    }
    const auto saved = contents.subspan(SaveFile::kMapPart + SaveFile::kCodePart, SaveFile::kDialoguePart);
    const auto state = contents.subspan(SaveFile::kMapPart + SaveFile::kCodePart + SaveFile::kDialoguePart);
    std::vector<std::uint8_t> said(saved.begin(), saved.begin() + static_cast<std::ptrdiff_t>(std::min(saved.size(), dialogue_.bytes().size())));
    if (said.size() >= 2) {
        const auto first = static_cast<std::uint16_t>((said[0] | (said[1] << 8)) - kDialogueAt);
        moveTable(said, first, -kDialogueAt);
    }
    // Everything is checked before anything changes.
    Dialogue lists = dialogue_;
    lists.restore(said);
    std::vector<std::uint8_t>& map = map_();
    world_.restore(state);
    dialogue_ = std::move(lists);
    SaveFile::unpackMap(contents.subspan(0, SaveFile::kMapPart), map);
}

bool SavedGames::save(int slot) {
    if (folder_.empty()) {
        return false;
    }
    const std::vector<std::uint8_t> file = SaveFile::encode(static_cast<std::uint16_t>(world_.clock()), contents());
    std::error_code e;
    std::filesystem::create_directories(folder_, e);
    std::ofstream f(pathOf(slot), std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    return f.good();
}

bool SavedGames::load(int slot) {
    const auto bytes = read(slot);
    if (!bytes) {
        return false;
    }
    try {
        restore(SaveFile::decode(*bytes));
    } catch (const FormatError&) {
        return false;
    }
    return true;
}

} // namespace mentat
