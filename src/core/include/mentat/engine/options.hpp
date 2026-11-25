// SPDX-License-Identifier: GPL-3.0-or-later
//
// Mentat's own options (not the original's mixer panel): which arrangement
// of the music plays. They are kept in a small text file beside the saved
// games, one "name = value" a line.
#pragma once

#include <string>
#include <string_view>

namespace mentat {

enum class MusicChoice {
    AdLib,     // the .HSQ music on the Sound Blaster player
    AdLibGold, // the .AGD music on the AdLib Gold player
    Mt32,      // the .M32 music on a Roland MT-32
};

struct Options {
    MusicChoice music = MusicChoice::AdLibGold;

    // Reads the file's text; what it does not understand keeps its default.
    static Options read(std::string_view text);
    std::string write() const;
};

// The word for a choice in the file ("adlib", "adlib-gold", "mt32").
std::string_view musicWord(MusicChoice choice);

} // namespace mentat
