// SPDX-License-Identifier: GPL-3.0-or-later
//
// The original's numbered resources: files in DUNE.DAT named by the
// program's resource table (see docs/formats/program.md).
#pragma once

#include "mentat/data/dat_archive.hpp"
#include "mentat/data/program_file.hpp"
#include "mentat/gfx/sprite_sheet.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mentat {

class Resources {
public:
    Resources(DatArchive& archive, const ProgramFile& program) : archive_(archive), program_(program) {}

    DatArchive& archive() { return archive_; }
    const ProgramFile& program() const { return program_; }

    std::string name(int index) const; // throws FormatError for an unused number
    // The file's bytes, unpacked when it is packed. Throws FormatError.
    std::vector<std::uint8_t> load(int index);
    // A picture file, kept once loaded. Throws FormatError.
    const SpriteSheet& sheet(int index);

private:
    DatArchive& archive_;
    const ProgramFile& program_;
    std::map<int, std::unique_ptr<SpriteSheet>> sheets_;
};

} // namespace mentat
