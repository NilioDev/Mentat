// SPDX-License-Identifier: GPL-3.0-or-later
//
// Scene files (.SAL): how the rooms of the palace, the sietches, the
// villages and the Harkonnen fortress are put together from sprites, lines
// and shaded shapes. See docs/formats/scene.md and docs/rules/scenes.md.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace mentat {

class ProgramFile;
class SpriteSheet;
class Surface;

// A rectangle; right and bottom are one past the last row and column.
struct ClipRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

// Where drawing by position lands: a screen-sized surface, and how many
// rows everything is moved down (the original can shift all drawing down,
// which its opening uses to centre the scenes).
struct Canvas {
    Canvas(Surface& s, int rows = 0) : surface(s), originRows(rows) {} // NOLINT: a surface is a canvas at row 0
    Surface& surface;
    int originRows = 0;
};

// Values the drawing takes from the player's DNCDPRG.EXE.
struct SceneTables {
    // Divisors for the seven sizes a sprite can be shrunk to (index 0 is
    // full size), as 8.8 fixed point: 0x100 draws the sprite as it is.
    std::array<std::uint16_t, 8> scaleDivisors{};
    // Lines are clipped to this rectangle: the part of the screen above
    // the control panel.
    ClipRect lineClip;

    static SceneTables fromProgram(const ProgramFile& program); // throws FormatError
};

class SceneFile {
public:
    // Takes the unpacked bytes of a .SAL file. Throws FormatError.
    static SceneFile parse(std::vector<std::uint8_t> bytes);

    std::size_t size() const { return offsets_.size(); }
    // The bytes of scene `index`, from its start to the end of the file.
    std::span<const std::uint8_t> scene(std::size_t index) const; // throws FormatError

private:
    std::vector<std::uint8_t> bytes_;
    std::vector<std::size_t> offsets_;
};

// A room number as the game keeps it picks both a scene and a picture
// file: the low four bits of (room - 1) are the scene, the rest choose the
// picture file. Picture file 0 means "keep the one in use".
struct RoomNumber {
    int scene = 0;
    int pictureFile = 0; // 0, or the original's resource number (0x14 and up)
    static RoomNumber decode(int room);
};

// Called for each place in a scene where a person may stand, in drawing
// order, so they can be drawn between the scene's other parts. `slot`
// counts these places from 0.
using PersonSlotFn = std::function<void(int slot, int x, int y)>;

// Draws scene `scene` onto `to` (a 320 × 200 surface) with sprites from
// `sheet`. Returns the number of persons the scene has room for (the
// scene's first byte). Throws FormatError on damaged data.
int drawScene(Surface& to, std::span<const std::uint8_t> scene, const SpriteSheet& sheet,
              const SceneTables& tables, const PersonSlotFn& onPerson = {});
int drawScene(const Canvas& to, std::span<const std::uint8_t> scene, const SpriteSheet& sheet,
              const SceneTables& tables, const PersonSlotFn& onPerson = {});

// Draws a sprite named by a 16-bit sprite word, as scenes and the game's
// sprite lists do: bits 0-8 the sprite number, 10-12 the size, 13 upside
// down, 14 mirrored. A colour offset other than 0 replaces the sprite's mode.
void drawSpriteWord(const Canvas& to, const SpriteSheet& sheet, std::uint16_t word, int x, int y,
                    const SceneTables& tables, std::uint8_t modeOverride = 0);

// The pieces drawScene uses, open for tests and tools.

// Draws a sprite shrunk by an 8.8 divisor (0x100 keeps its size), the
// way the original does it for scenery that is further away.
void drawScaledSprite(const Canvas& to, const SpriteSheet& sheet, std::size_t index, std::uint8_t mode,
                      bool flipX, bool flipY, std::uint16_t divisor, int x, int y);

// A one-colour line from (x0, y0) to (x1, y1), clipped.
void drawSceneLine(const Canvas& to, int x0, int y0, int x1, int y1, std::uint8_t colour, const ClipRect& clip);

} // namespace mentat
