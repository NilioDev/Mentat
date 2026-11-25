// SPDX-License-Identifier: GPL-3.0-or-later
//
// mentat-music: plays one of the original's pieces of music from the
// developer's own DUNE.DAT into a stereo WAV file, or prints the chip
// register writes per timer tick to compare with the original. A .AGD name
// plays it as on the AdLib Gold, a .HSQ name as on the Sound Blaster Pro.
//
//   mentat-music DUNE.DAT ARRAKIS.AGD out.wav [seconds]
//   mentat-music DUNE.DAT ARRAKIS.HSQ --trace [ticks]
//   mentat-music --megacd-import MEGACD.iso folder
//   mentat-music --megacd folder TRACK out.wav [seconds]
//
// The last two copy the Mega CD version's music out of an image of its disc
// and play the piece it has for the CD version's track 1-10.
#include "mentat/audio/music.hpp"
#include "mentat/audio/music_driver.hpp"
#include "mentat/audio/opl.hpp"
#include "mentat/data/dat_archive.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/engine/platform.hpp"
#include "mentat/game_files.hpp"
#ifdef MENTAT_WITH_MEGACD
#include "mentat/audio/megacd_music.hpp"
#endif

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace mentat;

namespace {

struct Trace : OplSink {
    std::uint32_t tick = 0;
    void writeRegister(std::uint16_t reg, std::uint8_t value) override {
        std::printf("%u %03x %02x\n", tick, reg, value);
    }
    void writeControl(std::uint8_t reg, std::uint8_t value) override {
        std::printf("%u control %02x %02x\n", tick, reg, value);
    }
    void writeSurround(std::uint8_t reg, std::uint8_t value) override {
        std::printf("%u surround %02x %02x\n", tick, reg, value);
    }
};

void put32(std::ofstream& out, std::uint32_t v) {
    const char b[4] = {static_cast<char>(v), static_cast<char>(v >> 8), static_cast<char>(v >> 16),
                       static_cast<char>(v >> 24)};
    out.write(b, 4);
}

void put16(std::ofstream& out, std::uint16_t v) {
    const char b[2] = {static_cast<char>(v), static_cast<char>(v >> 8)};
    out.write(b, 2);
}

void writeWav(const char* path, const std::vector<std::int16_t>& samples, int rate) {
    std::ofstream out(path, std::ios::binary);
    out.write("RIFF", 4);
    put32(out, static_cast<std::uint32_t>(36 + samples.size() * 2));
    out.write("WAVEfmt ", 8);
    put32(out, 16);
    put16(out, 1);
    put16(out, 2);
    put32(out, static_cast<std::uint32_t>(rate));
    put32(out, static_cast<std::uint32_t>(rate) * 4);
    put16(out, 4);
    put16(out, 16);
    out.write("data", 4);
    put32(out, static_cast<std::uint32_t>(samples.size() * 2));
    for (const auto s : samples) {
        put16(out, static_cast<std::uint16_t>(s));
    }
}

int megaCd(int argc, char** argv) {
    const std::string mode = argv[1];
    if (mode == "--megacd-import" && argc >= 4) {
        importMegaCdMusic(argv[2], argv[3]);
        std::printf("copied the Mega CD music to %s\n", argv[3]);
        return 0;
    }
#ifdef MENTAT_WITH_MEGACD
    if (mode == "--megacd" && argc >= 5) {
        MegaCdMusic music(readMegaCdMusic(argv[2]));
        const double seconds = argc > 5 ? std::atof(argv[5]) : 60;
        std::vector<std::int16_t> samples;
        music.runTo(0, samples);
        music.setVolume(0xFF, 0x78);
        music.startPiece(std::atoi(argv[3]), 0);
        const auto ticks = static_cast<std::uint32_t>(seconds * kTicksPerSecond);
        for (std::uint32_t t = 1; t <= ticks; ++t) {
            music.runTo(t, samples);
        }
        writeWav(argv[4], samples, music.rate());
        std::printf("track %s: %.1f s\n", argv[3], static_cast<double>(samples.size()) / 2 / music.rate());
        return 0;
    }
#endif
    std::fprintf(stderr, "usage: mentat-music --megacd-import IMAGE folder | --megacd folder TRACK out.wav [seconds]\n");
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]).rfind("--megacd", 0) == 0) {
        try {
            return megaCd(argc, argv);
        } catch (const FormatError& e) {
            std::fprintf(stderr, "%s\n", e.what());
            return 1;
        }
    }
    if (argc < 4) {
        std::fprintf(stderr, "usage: mentat-music DUNE.DAT NAME.HSQ out.wav [seconds] | --trace [ticks]\n");
        return 2;
    }
    try {
        DatArchive dat = DatArchive::open(argv[1]);
        const std::string name = argv[2];
        const bool gold = name.size() > 4 && (name.substr(name.size() - 4) == ".AGD" || name.substr(name.size() - 4) == ".agd");
        const MusicTables tables = MusicTables::load(dat, gold ? MusicCard::AdLibGold : MusicCard::SoundBlasterPro);
        auto song = std::make_shared<const Song>(Song::parse(dat.load(argv[2])));
        if (std::string(argv[3]) == "--trace") {
            const long ticks = argc > 4 ? std::atol(argv[4]) : 2000;
            Trace trace;
            MusicDriver driver(tables);
            driver.init(trace);
            driver.start(trace, song, 0);
            for (long t = 1; t <= ticks; ++t) {
                trace.tick = static_cast<std::uint32_t>(t);
                driver.tick(trace);
            }
            return 0;
        }
        const double seconds = argc > 4 ? std::atof(argv[4]) : 60;
        FmMusic music(tables);
        std::vector<std::int16_t> samples;
        music.runTo(0, samples);
        music.driver().start(music.opl(), song, 1);
        const auto ticks = static_cast<std::uint32_t>(seconds * kTicksPerSecond);
        for (std::uint32_t t = 1; t <= ticks && music.driver().status().playing(); ++t) {
            music.runTo(t, samples);
        }
        writeWav(argv[3], samples, Opl::kRate);
        std::printf("%s: %.1f s\n", argv[2], static_cast<double>(samples.size()) / 2 / Opl::kRate);
    } catch (const FormatError& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
    return 0;
}
