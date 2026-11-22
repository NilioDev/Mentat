// SPDX-License-Identifier: GPL-3.0-or-later
#include "mentat/data/save_file.hpp"

#include "mentat/data/reader.hpp"

namespace mentat {

namespace {

constexpr std::size_t kHeader = 4; // the marker and the last run's count, then the length
constexpr int kLongestRun = 0xFF;

} // namespace

std::vector<std::uint8_t> SaveFile::encode(std::uint16_t clock, std::span<const std::uint8_t> contents) {
    if (contents.size() != kSize) {
        throw FormatError("save: wrong size of contents");
    }
    std::vector<std::uint8_t> out{static_cast<std::uint8_t>(clock), static_cast<std::uint8_t>(clock >> 8), 0, 0, 0, 0};
    std::size_t i = 0;
    int run = 0;
    while (i < contents.size()) {
        const std::uint8_t b = contents[i];
        run = 1;
        while (i + static_cast<std::size_t>(run) < contents.size() && contents[i + static_cast<std::size_t>(run)] == b &&
               run < kLongestRun) {
            ++run;
        }
        i += static_cast<std::size_t>(run);
        // Runs of one or two stay as they are, unless they are the marker.
        if (b != kMarker && run <= 2) {
            out.insert(out.end(), static_cast<std::size_t>(run), b);
        } else {
            out.push_back(kMarker);
            out.push_back(static_cast<std::uint8_t>(run));
            out.push_back(b);
        }
    }
    const std::size_t length = out.size() - 2;
    if (length > 0xFFFF) {
        throw FormatError("save: packed state too long");
    }
    out[2] = kMarker;
    out[3] = static_cast<std::uint8_t>(run);
    out[4] = static_cast<std::uint8_t>(length);
    out[5] = static_cast<std::uint8_t>(length >> 8);
    return out;
}

std::uint16_t SaveFile::clock(std::span<const std::uint8_t> file) {
    Reader r(file);
    return r.u16();
}

std::vector<std::uint8_t> SaveFile::decode(std::span<const std::uint8_t> file) {
    Reader r(file);
    r.u16(); // the clock
    const std::uint8_t marker = r.u8();
    r.u8();
    const std::uint16_t length = r.u16();
    if (length < kHeader || static_cast<std::size_t>(length) + 2 > file.size()) {
        throw FormatError("save: bad length");
    }
    const std::size_t end = 2 + static_cast<std::size_t>(length);
    std::vector<std::uint8_t> out;
    out.reserve(kSize);
    std::size_t i = 2 + kHeader;
    while (i < end) {
        const std::uint8_t b = file[i++];
        if (b != marker) {
            out.push_back(b);
        } else {
            if (i + 2 > end) {
                throw FormatError("save: run cut short");
            }
            out.insert(out.end(), file[i], file[i + 1]);
            i += 2;
        }
        if (out.size() > kSize) {
            throw FormatError("save: too much state");
        }
    }
    if (out.size() != kSize) {
        throw FormatError("save: too little state");
    }
    return out;
}

void SaveFile::packMap(std::span<const std::uint8_t> map, std::span<std::uint8_t> to) {
    if (to.size() < kMapPart) {
        throw FormatError("save: map part too small");
    }
    for (std::size_t i = 0; i < kMapPart; ++i) {
        int packed = 0;
        for (std::size_t k = 0; k < 4; ++k) {
            const std::size_t at = 4 * i + k;
            packed = (packed << 2) | (at < map.size() ? (map[at] >> 4) & 3 : 0);
        }
        to[i] = static_cast<std::uint8_t>(packed);
    }
}

void SaveFile::unpackMap(std::span<const std::uint8_t> from, std::span<std::uint8_t> map) {
    if (from.size() < kMapPart) {
        throw FormatError("save: map part too small");
    }
    for (std::size_t i = 0; i < kMapPart; ++i) {
        for (std::size_t k = 0; k < 4 && 4 * i + k < map.size(); ++k) {
            const int bits = (from[i] >> (6 - 2 * k)) & 3;
            std::uint8_t& m = map[4 * i + k];
            m = static_cast<std::uint8_t>((m & ~0x30) | (bits << 4));
        }
    }
}

} // namespace mentat
