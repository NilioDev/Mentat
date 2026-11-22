// SPDX-License-Identifier: GPL-3.0-or-later
//
// Saving and loading games in the original's files, DUNE37S1.SAV and on, so
// that games saved by either can be loaded by the other. See
// docs/formats/save.md.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace mentat {

class Dialogue;
class ProgramFile;
class World;

class SavedGames {
public:
    // `folder` empty: there is nowhere to save, and nothing to load.
    // `map` gives the map, whose holders are kept.
    using MapFn = std::function<std::vector<std::uint8_t>&()>;
    SavedGames(std::filesystem::path folder, const ProgramFile& program, World& world, Dialogue& dialogue, MapFn map);

    bool available() const { return !folder_.empty(); }
    // The clock of the game in slot n (from 1), or nothing when there is
    // none there or it cannot be read.
    std::optional<std::uint16_t> clockOf(int slot) const;
    // False when the file could not be written.
    bool save(int slot);
    // False when there is no game there, or it is not one: then nothing
    // changes.
    bool load(int slot);

    // The file's contents for the game as it is, and putting them back.
    std::vector<std::uint8_t> contents() const;
    void restore(std::span<const std::uint8_t> contents);

private:
    std::filesystem::path pathOf(int slot) const;
    std::optional<std::vector<std::uint8_t>> read(int slot) const;

    std::filesystem::path folder_;
    const ProgramFile& program_;
    World& world_;
    Dialogue& dialogue_;
    MapFn map_;
};

} // namespace mentat
