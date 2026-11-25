// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/engine/options.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace mentat {

namespace {

constexpr std::array<std::pair<std::string_view, MusicChoice>, 3> kMusic{{
    {"adlib", MusicChoice::AdLib},
    {"adlib-gold", MusicChoice::AdLibGold},
    {"mt32", MusicChoice::Mt32},
}};

// Lower case, without spaces or tabs.
std::string plain(std::string_view text) {
    std::string out;
    for (const char c : text) {
        if (c != ' ' && c != '\t' && c != '\r') {
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return out;
}

} // namespace

std::string_view musicWord(MusicChoice choice) {
    for (const auto& [word, c] : kMusic) {
        if (c == choice) {
            return word;
        }
    }
    return "adlib-gold";
}

Options Options::read(std::string_view text) {
    Options options;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        std::string line = plain(text.substr(start, end - start));
        start = end + 1;
        if (const auto hash = line.find('#'); hash != std::string::npos) {
            line.resize(hash);
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string name = line.substr(0, equals);
        const std::string value = line.substr(equals + 1);
        if (name == "music") {
            for (const auto& [word, c] : kMusic) {
                if (value == word) {
                    options.music = c;
                }
            }
        }
    }
    return options;
}

std::string Options::write() const {
    return "# Mentat's options (F2, or Start on a gamepad, opens them)\nmusic = " + std::string(musicWord(music)) +
           "\n";
}

} // namespace mentat
