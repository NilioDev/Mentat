// SPDX-License-Identifier: GPL-3.0-or-later
//
// Showing one of the original's videos from start to end.
#pragma once

#include "mentat/data/dat_archive.hpp"
#include "mentat/data/program_file.hpp"
#include "mentat/engine/platform.hpp"

namespace mentat {

// Plays video slot `index` full screen. Returns false when the player
// skipped it with a key or a click.
bool playVideo(Platform& platform, DatArchive& archive, const ProgramFile& program, int index);

} // namespace mentat
