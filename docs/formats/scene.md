# Scene files (.SAL)

The rooms of the palace, the sietches, the villages and the Harkonnen
fortress are not stored as pictures. Each is a short list of drawing
commands that puts sprites from a picture file (see [sprite.md](sprite.md))
on the screen, draws thin lines, and fills shaded shapes. There are four
scene files: `SIET.SAL`, `PALACE.SAL`, `VILG.SAL` and `HARK.SAL`. Like most
files in `DUNE.DAT` they are packed (see [hsq.md](hsq.md)).

How the commands are drawn, step by step, is in
[../rules/scenes.md](../rules/scenes.md).

All numbers are little-endian.

## Layout

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 × n | Scene list: n offsets from the start of the file |
| … | | The scenes |

The first offset is also the size of the list, so there are first / 2
scenes. A scene starts with one byte, the number of persons that can stand
in it, followed by commands. A command starts with a 16-bit word; the word
`0xFFFF` ends the scene.

## Which scene, with which pictures

The game keeps a room as one number *r* from 1 up. Of *r* − 1, the low four
bits pick the scene in the file. The rest, when not zero, pick the picture
file whose sprites the scene uses: 1 is `PROUGE.HSQ`, 2 `COMM.HSQ` and so on
through the original's list of resources (resource `0x13` + that number).
When the rest is zero, the picture file in use stays.

So the same scene can be drawn with different picture files: the palace
rooms share their layouts and differ in their pictures.

## Commands

### Sprite: word below `0x8000`

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 | Sprite word |
| 2 | 1 | x, low eight bits |
| 3 | 1 | y |
| 4 | 1 | Colour offset; 0 keeps the sprite's own |

Bit 9 of the word (`0x200`) is bit 8 of x. Take the word without that bit
and subtract one; of the result:

| Bits | What |
| --- | --- |
| 0–8 | Sprite number in the picture file |
| 10–12 | Size: 0 is full size, 1–7 shrink the sprite by the divisor of that number (see [program.md](program.md)) |
| 13 | Drawn upside down |
| 14 | Drawn mirrored |

A word whose low nine bits are 1 is not a sprite but a place where a person
can stand, at that x and y. Persons are drawn there in the order these
places appear, so scenery that comes later in the list is drawn in front
of them.

The colour offset, when not zero, takes the place of the sprite's mode: a
four-bit sprite then gets that value added to its pixels instead of its
own. This lets one sprite be drawn in more than one set of colours.

### Line: word `0xC0nn`

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 | `0xC0` and the colour *nn* |
| 2 | 2 | x of the first end |
| 4 | 2 | y of the first end |
| 6 | 2 | x of the second end |
| 8 | 2 | y of the second end |

### Shaded shape: any other word from `0x8000`

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 | Low byte: base colour. High byte: flags |
| 2 | 1 | Change of shade per pixel to the right, signed, in sixteenths of a colour |
| 3 | 1 | Change of shade per row down, signed, in sixteenths of a colour |
| 4 | 2 | x of the top point |
| 6 | 2 | y of the top point |
| 8 | 4 × … | First side: points as x, y pairs |
| … | 4 × … | Second side: points as x, y pairs, may be absent |

Both sides run from the top point down to one bottom point. The first side
lists its points up to and including the bottom point, which has bit 14
(`0x4000`) set in its x. When that x also has bit 15 set, the second side
is a straight edge from the top point to the bottom point and nothing more
follows. Otherwise the second side lists its points from the top down, the
last one with bit 15 (`0x8000`) set in its x, and then runs straight to the
bottom point. The two high bits are not part of x.

Flags:

| Bit | What |
| --- | --- |
| 0 | Rows are filled from right to left |
| 1–5 | Grain: when any is set, a speckle pattern is added to the shading |
| 7 | Always set (marks the command as a shape) |

## Reading it safely

Mentat checks that the scene list points inside the file, stops with an
error when a command runs past the end of the file or names a sprite the
picture file does not have, and refuses a shape taller than 1024 rows.
