# The control panel

Under the scene (rows 152 to 199) the game shows the control panel: a book
on the left with the clock and two slots, the list of commands in the
middle, and the arrows on the right. The panel's pictures are sprites of
picture file 0 (`ICONES.HSQ`).

## Zones

The program keeps a list of zones at data offset `0x1AE4`: a count (24),
then 14 bytes each.

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 × 4 | Left, top, right and bottom; right and bottom are one past the last column and row |
| 8 | 2 | Flags: `0x80` the mouse can use it, `0x40` blacked out before it is drawn, `0x20` not drawn |
| 10 | 2 | The sprite it shows, `0xFFFF` for none |
| 12 | 2 | The original's number for what a click does |

Drawing a zone draws its sprite with its top-left corner at the zone's
top-left corner. The zones that matter so far:

| Zones | What |
| --- | --- |
| 0 to 3 | The parts of the panel that change with what the player does |
| 4 to 6 | The frame of the list of commands and the two side pieces |
| 7 to 11 | The five lines of the list of commands |
| 12 to 17 | The arrows: the pad, then up, right, down, left, and the middle |
| 19, 20 | The scene |
| 21, 22 | The two slots |

When the game starts, the target is cleared and every zone is drawn, then
the ornament (below).

### Layouts

A layout gives zones 0 to 3 their flags and sprites, four (flags, sprite)
pairs, after which zones 0 to 2 are drawn again. While the player walks
around the layout is the one at data offset `0x1C36`.

## The ornament

At the top of the panel, sprite `0x0F` at (126, 148) holds sprite `0x10` +
n at (150, 137), for n from 0 to 10. When the game starts it opens: n rises
by one every 8 ticks until it is 10, drawn on the back buffer and the
screen.

## The clock

Only while zone 1 shows sprite 0. For the part of the day h (the clock's
low four bits), the table at data offset `0x1E7E` gives two (x, y) pairs:
where sprite `0x4A` (the sun) and sprite `0x4B` (the moon) go; x = 0 means
the sprite is not shown. Both are clipped to the rectangle at `0x1EFE`
((6, 181) to (30, 190)).

The day, from 1 (see [game.md](game.md)), is then written with the small
font (see [../formats/font.md](../formats/font.md)) in colour `0xFA` on
`0xF1`, at row 190: three characters, a nought before the first other
digit written as a space, and a space after. It starts at column 11, 2
further left when the day is below 100 and 2 more below 10.

When the part of the day changes, zone 1 and the clock are drawn on the
screen and the rectangle (6, 181) to (30, 198) is copied to the back
buffer.

## The slots

Zones 21 and 22 are drawn with sprite `0x40` (an empty slot), then with
sprite `0x41` + what the slot holds (the two bytes at state offset
`0x1152`; `0xFF`, nothing, gives `0x40` again).

## The arrows

The six zones of the arrows are first set from data offset `0x1C76`, then:

- **Inside**, except in rooms of kind `0x21`: the pad (zone 12) shows
  sprite `0x21`, or `0x22` in a place's first room. The middle (zone 17) is
  shown only in the palace, and not in its first room. Each arrow can be
  used when the room has a way out in its direction (see
  [places.md](places.md)), and is not drawn otherwise.
- **Otherwise** the pad shows sprite `0x23` and the four arrows sprites
  `0x1D` to `0x20`, all usable.

When zone 2 shows a sprite from 3 to 5, the rectangle at data offset
`0x2458` ((254, 162) to (296, 193)) is filled with colour `0xF0` first.
Then the six zones are drawn.

## The list of commands

The commands of a moment are (phrase, action) pairs. Phrase n is string
n − 1 of the command file (`COMMAND1.HSQ` for English). Bit 14 of the
phrase greys the command out, bit 15 lights it up.

The five lines are zones 7 to 11. With more than five commands, the fifth
line says "Others..." (phrase `0xA0`). Each line is written at column 93,
one row below the zone's top, with the small font:

- a line without a command is filled with colour `0xF3`;
- a greyed command is written in `0xF5` on `0xF3`, and cannot be used;
- otherwise in `0xFA` on `0xF3`, or `0xF3` on `0xFA` when lit, and the
  line's zone can be used.

The text starts with a space; the rest of the line up to column 227 (seven
rows) is filled with the background colour.

The command under the mouse is lit: the line it points at, or the line of
the person it points at in the scene.

## The mouse pointer

The program keeps its pointers as 68 bytes each from data offset `0x2584`:
the point (x, y), 16 words of screen pixels to keep (bit 15 the leftmost),
and 16 words of the pointer's own bits. A pixel not kept is colour `0x0F`
where the pointer's bit is set and 0 where it is not. The first pointer is
the arrow.

Its shape follows the mouse. In a room it is the arrow. On the map, the
rectangle is the map's view; on the globe, it is the planet's square at
data offset `0x2448`; in the ornithopter's cockpit, its small map (see
[cockpit.md](cockpit.md)). With (x, y) the mouse:

- Over the panel, from row `0x9B` down, it is the arrow.
- Strictly inside the rectangle, it is the hand (`0x25C8`).
- In the rectangle's rows: at most 49 pixels left of its left edge (or on
  it), it is the left arrow (`0x26D8`). At most 49 pixels right of its
  right edge (or on it), it is the right arrow (`0x2650`).
- Otherwise, in its columns: up to 24 rows above its top (or on it), it is
  the up arrow (`0x260C`). Up to 24 rows below its bottom (or on it), it
  is the down arrow (`0x2694`).
- Anywhere else, it is the arrow.
