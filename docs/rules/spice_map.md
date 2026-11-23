# The map of the regions

"SEE SPICE DENSITY" (action `0x53F1`, see [map.md](map.md)) opens a small
window over the map of Dune: the planet a screen pixel to a map point,
split into its regions, each region Paul knows coloured by its spice or
by the work of his troops there. Choosing the command again closes it.
Giving a troop somewhere to go uses the same window (see
[orders.md](orders.md)).

## The window

The window's top-left corner is kept in the state, words `0x11C1` (x)
and `0x11C3` (y); a new game has it at (`0x4B`, `0x0F`). The window is
`0xAA` × `0x6C` pixels:

- the frame, sprite `0x8D` of the map's picture file (`ONMAP.HSQ`),
  drawn at the corner;
- the map, `0xA0` × `0x59` pixels from (5, 7) into the frame;
- the bar, from (6, `0x62`) to 6 pixels from the right and 2 from the
  bottom, filled with colour `0xF5`.

The window is drawn over the map on the screen only. Before it is drawn,
the box that names the map and a place's label are taken away.

## The map

The window's middle is the main map's middle when it opens. A map point
(λ, φ) is at x = the view's left + 80 + ⌊d × 2w / 65536⌋, where d is λ
less the middle's longitude taken as a signed 16-bit number and w the
width of latitude φ, and at y = the view's top + 44 + (φ − the middle's
latitude), as on the ornithopter's small map (see
[cockpit.md](cockpit.md)). The rows are gathered the same way, with the
middle's latitude kept so that no row goes past latitude 86, and the
planet's rim drawn near the poles, but from `MAP2.HSQ`: each point is the
number of its region.

The rows are painted from the top. A point takes its region's colour when
the point to its right (none for the last point of a row) and the point
below it are of the same region; otherwise it is a border, in colour
`0x70`. What lies below the last row is not kept by the original; it is
taken as the last row again.

Each region starts with colour `0x70`, unknown. Then each place in the
order of the state, unless byte `0x0A` has bit 7 (hidden), gives its
region (byte `0x10`) a colour:

- **Spice density** (the window as it opens): when the place's byte
  `0x0A` has bit 6 (its spice found), `0x50` plus the high four bits of
  its byte `0x12`; otherwise `0x75`. (`0x78` under one of the original's
  start-up options.)
- **Troop occupation**: a pattern of four two-bit values, by Paul's
  troops (byte 3 below `0x80`) at the place (its chain, byte 9 and each
  troop's byte 1) or going there (byte 3 bit 6, its destination word 4,
  or word `0x0C` when bits 0 and 1 of byte 3 are both set), leaving out
  those waiting for orders (occupation 2, the low four bits of byte 3).
  Occupations 0 to 3 count as spice, 4 to 7 as army, the rest as ecology.
  With s, a and e the counts:

  | Kinds | Pattern |
  | --- | --- |
  | none | `0x00` |
  | spice | `0x55` |
  | army | `0xAA` |
  | spice, army | `0x66` when s = a; `0x9A` when s < a; else `0x65` |
  | ecology | `0xFF` |
  | spice, ecology | `0x77` when s = e; `0xDF` when s < e; else `0x75` |
  | army, ecology | `0xBB` when a = e; `0xEF` when a < e; else `0xBA` |
  | all three | s < a: `0xDE` when a < e, else `0x9B`; otherwise `0x7B` when s < e, else `0x67` |

  A point of a region with a pattern is in colour `0x71` plus the
  pattern's low two bits. After each row every pattern is turned two
  bits to the left, so the rows take the four values in turn.

Then, clipped to the view:

1. The places not hidden that fall inside the view, as on the
   ornithopter's small map: sprite `0x3A` of the panel's picture file and
   the steps of [map.md](map.md) by kind, a sietch further than Paul can
   see 5 further on, centred on the point.
2. The bar: the legend (below).
3. A dotted frame of what the main map shows: around the main map's
   middle as it falls on the window, from 40 pixels left to 39 right and
   20 up to 19 down, each side kept within the view; nothing when it
   comes to no height or width. Each side is drawn in colour `0xFB` from
   its top or left end, every other pixel starting with the second.
4. Paul's mark, as on the main map: sprite `0x4C` of the panel's file,
   13 pixels left of his point and its bottom just above it. When the
   window is open for a troop, that troop's mark instead: sprite `0x36`
   of the panel's file at the point of its place (or its own point while
   it moves), its bottom just above it.

## The bar

Text on the bar is in the panel's small font, colour `0xFE` on `0xF5`,
from (6, `0x62`) into the window.

- **The legend.** For the spice, phrase `0x65`, a "-" from `0x53` pixels
  further on, a "+" `0x41` pixels after the "-", and the scale: sprites
  `0x80` at (`0x5F`, `0x63`) and `0x81` at (`0x9B`, `0x63`) of the map's
  file. For the occupations, phrase `0x68` alone.
- **A place.** While the mouse points at a place (below), the bar is
  cleared and names it: phrase `0x44` plus its kind's word (as for a
  place's label, see [map.md](map.md)), then its name, " " or "-" (from
  the second word 3 on) and the second word. Except for the first two
  places and those of kind `0x21`, the troops follow from `0x71` pixels
  into the window, in colour `0xFB`: phrase `0x66` when there are none,
  otherwise for spice, army and ecology in turn, when there are some,
  sprite `0x82`, `0x83` or `0x84` of the map's file, then ":" and the
  digit of the count 6 pixels further on; each kind takes `0x12` pixels.
  When the mouse leaves the place, the bar is cleared and shows the
  legend again.

## The mouse

A place is pointed at when the mouse is inside the frame and the nearest
place shown (across plus down) is less than 9 pixels away; the first of
equals counts.

Otherwise, with the mouse inside the frame:

- On the bar (the frame's last 10 rows) the legend's phrase is written
  again with its colours swapped; leaving the bar writes it as before.
- On the map, showing the spice: the screen's colour under the mouse,
  less `0x50`, gives a level from 0 to 15 (any other colour gives none).
  When the level changes, the mark of the old one is flipped back and
  that of the new one drawn: a 5 × 7 outline at (`0x5E` + 4 × the level,
  `0x62`) in the window, each pixel's colour changed by exclusive-or with
  `0x0F`, kept within (4, 4) to (`0x13C`, `0x94`) of the screen.

A click inside the frame:

- in its first 7 rows: within its first 10 pixels it closes the window;
  further on it is where the window would be dragged (not built yet);
- on its last rows from the map's bottom on: the window switches between
  spice density and troop occupation, and is drawn again;
- on the map: when a place is pointed at, the window goes and the main
  map moves its middle to that place.

The arrows move the window's middle by their steps instead of the main
map's. The middle of the arrows puts the main map back on Paul, and the
window is drawn again over it.

## Not built yet

Dragging the window; the state's word `0x115A`, which changes the regions
of `MAP2.HSQ` as the story goes on.
