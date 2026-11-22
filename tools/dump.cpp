// SPDX-License-Identifier: GPL-3.0-or-later
//
// mentat-dump: writes the pictures and video frames of the developer's own
// DUNE.DAT as PNG files, to check the readers against the original.
//
//   mentat-dump DUNE.DAT out-folder [name...]
//
// Without names it writes every picture file and the first frame of every
// video. Output stays on the developer's machine; never commit it.
#include "mentat/data/dat_archive.hpp"
#include "mentat/data/hnm.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/gfx/png.hpp"
#include "mentat/gfx/sprite_sheet.hpp"
#include "mentat/gfx/surface.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace mentat;

namespace {

bool endsWith(const std::string& s, const char* tail) {
    const std::string t(tail);
    return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}

// Lays out all sprites of a sheet in a grid on one picture.
void dumpSheet(const std::string& name, const SpriteSheet& sheet, const std::filesystem::path& out) {
    Palette pal;
    for (int i = 0; i < 256; ++i) {
        pal[static_cast<std::size_t>(i)] = Colour{static_cast<std::uint8_t>(i / 4), static_cast<std::uint8_t>(i / 4), static_cast<std::uint8_t>(i / 4)};
    }
    sheet.applyPalette(pal);
    int cellW = 8;
    int cellH = 8;
    std::vector<SpritePixels> pix;
    for (std::size_t i = 0; i < sheet.size(); ++i) {
        try {
            pix.push_back(decodeSprite(sheet.sprite(i)));
        } catch (const FormatError&) {
            pix.emplace_back();
        }
        cellW = std::max(cellW, pix.back().width + 2);
        cellH = std::max(cellH, pix.back().height + 2);
    }
    const int cols = std::max(1, std::min<int>(static_cast<int>(pix.size()), 1024 / cellW));
    const int rows = (static_cast<int>(pix.size()) + cols - 1) / cols;
    Surface s(cols * cellW, std::max(1, rows * cellH));
    for (std::size_t i = 0; i < pix.size(); ++i) {
        if (pix[i].width > 0) {
            drawSprite(s, pix[i], static_cast<int>(i) % cols * cellW, static_cast<int>(i) / cols * cellH, false, false);
        }
    }
    writePng(out / (name + ".png"), s, pal);
}

void dumpHnm(const std::string& name, std::vector<std::uint8_t> bytes, const std::filesystem::path& out, std::size_t frames) {
    const HnmFile hnm = HnmFile::parse(std::move(bytes));
    Palette pal;
    pal.applyBlocks(hnm.headerPalette());
    Surface screen;
    for (std::size_t i = 0; i < std::min(frames, hnm.frameCount()); ++i) {
        const HnmFrame f = hnm.frame(i);
        if (!f.palette.empty()) {
            pal.applyBlocks(f.palette);
        }
        drawHnmPicture(screen, HnmFile::decodePicture(f.video));
        writePng(out / (name + "_" + std::to_string(i) + ".png"), screen, pal);
    }
    std::printf("%s: %zu frames\n", name.c_str(), hnm.frameCount());
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: mentat-dump DUNE.DAT out-folder [name...]\n");
        return 2;
    }
    try {
        DatArchive dat = DatArchive::open(argv[1]);
        const std::filesystem::path out(argv[2]);
        std::filesystem::create_directories(out);
        std::vector<std::string> names;
        for (int i = 3; i < argc; ++i) {
            names.emplace_back(argv[i]);
        }
        const bool all = names.empty();
        if (all) {
            for (const auto& e : dat.directory().entries()) {
                names.push_back(e.name);
            }
        }
        for (const auto& name : names) {
            try {
                if (endsWith(name, ".HNM")) {
                    dumpHnm(name, dat.read(name), out, all ? 1 : 100000);
                } else if (endsWith(name, ".HSQ")) {
                    auto bytes = dat.load(name);
                    dumpSheet(name, SpriteSheet::parse(std::move(bytes)), out);
                }
            } catch (const FormatError& e) {
                std::printf("%s: %s\n", name.c_str(), e.what());
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
    return 0;
}
