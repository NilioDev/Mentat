// SPDX-License-Identifier: GPL-3.0-or-later
//
// mentat-scene: draws one scene of a .SAL file from the developer's own
// DUNE.DAT, to check the scene drawing against the original.
//
//   mentat-scene DUNE.DAT DNCDPRG.EXE PALACE.SAL index POR.HSQ out.png|out.raw
//
// A .raw file holds the 320 × 200 palette indices, one byte each. Output
// stays on the developer's machine; never commit it.
#include "mentat/data/dat_archive.hpp"
#include "mentat/data/program_file.hpp"
#include "mentat/data/reader.hpp"
#include "mentat/gfx/png.hpp"
#include "mentat/gfx/scene.hpp"
#include "mentat/gfx/sprite_sheet.hpp"
#include "mentat/gfx/surface.hpp"

#include <cstdio>
#include <fstream>
#include <string>

using namespace mentat;

int main(int argc, char** argv) {
    if (argc != 7) {
        std::fprintf(stderr, "usage: mentat-scene DUNE.DAT DNCDPRG.EXE NAME.SAL index SHEET.HSQ out.png|out.raw\n");
        return 2;
    }
    try {
        DatArchive dat = DatArchive::open(argv[1]);
        const ProgramFile program = ProgramFile::open(argv[2]);
        const SceneTables tables = SceneTables::fromProgram(program);
        const SceneFile scenes = SceneFile::parse(dat.load(argv[3]));
        const SpriteSheet sheet = SpriteSheet::parse(dat.load(argv[5]));
        Surface screen;
        const int persons = drawScene(screen, scenes.scene(static_cast<std::size_t>(std::stoul(argv[4]))), sheet, tables,
                                      [](const PersonSlot& p) { std::printf("person %d at %d,%d\n", p.slot, p.x, p.y); });
        std::printf("room for %d persons\n", persons);
        const std::string out = argv[6];
        if (out.size() > 4 && out.substr(out.size() - 4) == ".raw") {
            std::ofstream f(out, std::ios::binary);
            f.write(reinterpret_cast<const char*>(screen.data()), static_cast<std::streamsize>(screen.size()));
        } else {
            Palette pal;
            sheet.applyPalette(pal);
            writePng(out, screen, pal);
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "mentat-scene: %s\n", e.what());
        return 1;
    }
}
