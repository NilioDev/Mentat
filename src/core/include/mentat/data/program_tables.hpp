// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tables Mentat reads from the player's DNCDPRG.EXE. Each one is described
// in docs/formats/program.md.
#pragma once

#include "mentat/data/program_file.hpp"

#include <cstdint>
#include <string>

namespace mentat {

struct VideoInfo {
    std::uint8_t flags = 0;
    std::uint8_t delay = 0; // timer ticks between frames
    std::string name;       // empty for an unused slot

    // The original draws the full-motion videos (from Irulan's introduction
    // on) with each pixel in every second column and row.
    bool spaced = false;
};

namespace video {
inline constexpr int kMoon1 = 2;
inline constexpr int kTitle = 15;
inline constexpr int kCredits = 20;
inline constexpr int kVirgin = 21;
inline constexpr int kCryo = 22;
inline constexpr int kCryo2 = 23;
inline constexpr int kPresent = 24;
inline constexpr int kIrulan = 25;
inline constexpr int kFirstSpaced = 25;
inline constexpr int kCount = 37;
} // namespace video

// Video slot `index` (0..36). Throws FormatError.
VideoInfo videoInfo(const ProgramFile& program, int index);

} // namespace mentat
