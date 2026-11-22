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
inline constexpr int kVerses = 14;
inline constexpr int kTitle = 15;
inline constexpr int kDesert1 = 16;
inline constexpr int kDesert2 = 17;
inline constexpr int kDesert3 = 18;
inline constexpr int kPlant = 19;
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

// The original numbers the files it loads; this is the name in DUNE.DAT of
// resource `index` (0..255), empty for an unused number. Throws FormatError.
std::string resourceName(const ProgramFile& program, int index);

namespace resource {
inline constexpr int kFirstPortrait = 0x02; // LETO.HSQ; portrait n is kFirstPortrait + n
inline constexpr int kFirstRoomPictures = 0x13;
inline constexpr int kDawnPalace = 0x1B; // XPLAIN9.HSQ
inline constexpr int kPersons = 0x26;
inline constexpr int kSky = 0x28; // and 0x29 for the sky seen from inside a room
inline constexpr int kStars = 0x2C;
inline constexpr int kDesert = 0x2D; // INTDS.HSQ
inline constexpr int kSunrise = 0x2E;
inline constexpr int kBackgrounds = 0x30;
inline constexpr int kFirstIntroPicture = 0x56; // INT02.HSQ
inline constexpr int kPalacePicture = 0x62;
inline constexpr int kFirstSubtitles = 0x69; // IRUL1.HSQ, one per language
inline constexpr int kFirstScene = 0xA1;     // SIET.SAL
inline constexpr int kFirstMusic = 0xA4;     // track n is kFirstMusic + n
inline constexpr int kFont = 0xBB;           // DNCHAR.BIN
inline constexpr int kFirstCommands = 0xC0;  // COMMAND1.HSQ, one per language
} // namespace resource

} // namespace mentat
