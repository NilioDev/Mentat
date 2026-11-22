# The planet

How the original draws the turning planet: the map of Dune laid on a
sphere through precomputed tables. The story pages show it (see
[story.md](story.md)); the game's map view will use it too.

## The files

- `MAP.HSQ` (resource `0xBF`), the map: one byte per point. The equator
  row starts at byte `0x62FC`, the middle of the file. Each latitude row
  north of it starts `base` bytes after that point, each row south of it
  `base` bytes before it, and holds 2 × `width` points around the planet.
- `TABLAT.BIN` (resource `0xBA`): 99 records of four 16-bit numbers,
  high byte first, one per latitude from the equator (0) to the pole (98).
  The first number is the row's `base`, the second its `width`; the other
  two are room for the game's own use.
- `GLOBDATA.HSQ` (resource `0x92`):
  - byte 0: the length of the first line of the list below (with its end
    byte), as a negative number;
  - from byte 1: a list of screen lines, each a run of bytes below `0x80`
    ended by a byte of `0x80` or more. A line that is only an end byte ends
    the list. Byte x of a line belongs to the screen points x to the left
    and x to the right of the middle column; its value is how far that
    point is from the planet's middle, up or down along the sphere;
  - from byte `0xCDA`: 64 columns of 200 bytes, one for each x. Entry j
    (0 to 99) of a column is twice the latitude row of the point at height
    j in that column, and entry 100 + j its longitude, counted from the
    middle of the planet's face.

## Turning and tilting

A turn has 398 steps. For rotation r, the program takes
f = (r × 65536 + 32768) / 398, rounded down, and each row's phase is
2 × `width` × f / 65536, rounded down.

The tilt t moves the view up or down the sphere (−98 to 98). A tilt the
program keeps closer to level than 32 is used as 32 (from 0 up) or −32
(below 0).

## Drawing

The picture's middle is between columns 159 and 160. The upper half is
drawn from row 79 upwards, a list line a row; then the lower half from
row 80 downwards, starting again from the second line of the list (the
first line is the middle row) and taking each value as negative.

For each byte v at place x of a line:

1. u = v − t. The point is south of the equator when u is above 0. Its
   height j is |u|; when j is 99 or more, the point is past the pole:
   j becomes 197 − j and the longitude is mirrored (below).
2. The column for x gives the latitude row and the longitude l at height
   j. Past the pole, l becomes the row's `width` − l.
3. With the row's phase p and 2 × `width` points around, the left point
   (column 159 − x) shows map point (p − l) and the right point (column
   160 + x) shows map point (p + l), each brought into 0 to 2 × `width` − 1
   once, in the row north or south of the equator.
4. A map byte m gives colour 16 + (m & 15), plus 12 when (m & `0x30`) is
   `0x10` and (m & 15) is below 8. While the globe shows its results (see
   [globe_screen.md](globe_screen.md)), the colour shows who holds the
   point instead: 16 + (m & 15), plus 16 when (m & `0x30`) is `0x10` or
   `0x20`, and plus 32 when it is `0x30`.

## Turning on screen

The whole planet is drawn once in the back picture. Then a timer, every
tick, draws the next line in the back picture. A call that finds the end
of the lower half draws nothing; it copies the planet's part of the back
picture to the screen, turns the planet one step, and the next call
starts at the top again. With 55 lines above and 54 below, the planet
turns a step every 110 ticks.
