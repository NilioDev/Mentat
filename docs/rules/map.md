# The map of Dune

"SEE DUNE MAP" swaps the room for a flat map of the planet around the
player. This page covers the map as it is drawn and moved; the
commands that give orders, show the spice and take an ornithopter come
later.

## Points of the map

The map is `MAP.HSQ` (see [globe.md](globe.md) for how its rows are laid
out with `TABLAT.BIN`). A point of the map is a byte:

- bits 0 to 3: its colour, 0 to 15;
- bits 4 and 5: `01` marks a spice field Paul has found;
- bit 6: a place stands on it (set when the program starts, below).

A position on the planet is a longitude, 65536 to a turn, and a latitude
from −98 (north pole) to 98 (south pole). The point nearest to longitude
λ on latitude φ is number round(λ × 2w / 65536) of its row, where w is the
row's `width`; the rounding is up from a half. A point covers 65536 / 2w
longitude units; the program keeps that, rounded to the nearest whole
number, for each latitude.

## When the program starts

Before anything else the program settles the places onto the map. For
each place (records at state offset `0x100`, see
[../formats/state.md](../formats/state.md)):

1. Its longitude becomes that of the nearest point of its latitude: the
   point's number × 65536 / 2w, rounded down.
2. The point's position in the map (its row's start plus its number, not
   brought back into the row) goes to the place's word at `0x06`, and the
   point gets bit 6.
3. `MAP2.HSQ`, as long as the map, gives each point a region number. The
   place's byte at `0x10` takes its point's region, and its byte at `0x11`
   the number of points in that region plus 7, divided by 16.

Then each troop of each place (the place's byte `0x09` names the first,
each troop's byte `0x01` the next) takes the place: the place's offset in
its word at `0x04`, the place's longitude and latitude in `0x06` and
`0x08`. Its byte at `0x12` keeps bits 4 to 6 and takes the low four bits
of the place's first byte, with bit 7 set when those are 4 or 5, or more
than 9.

## Opening the map

Choosing "SEE DUNE MAP" (action `0x186B`) in a room:

1. The view's middle is the player's position: inside a place the
   place's longitude and latitude; otherwise the state's word at `0x04`
   is the longitude and the low byte at `0x06` the latitude.
2. On the back buffer: four frames around the view's rectangle (data
   offset `0x1482`: (4, 4) to (316, 148)), from just outside it outwards,
   each a pixel further out, in colours `0xFC`, `0xFA`, `0xF8` and `0xF6`;
   the ornament fully open (see [panel.md](panel.md)); the view (below);
   the map's commands; the panel's layout at data offset `0x1C66` (a
   small globe on the left, which opens the globe, see
   [globe_screen.md](globe_screen.md)) and the six zones of the arrows taken whole
   from data offset `0x1CCA`.
3. The music for the map plays (moment 2, see [game.md](game.md)).
4. The whole screen dissolves in (transition `0x34`, see
   [display.md](display.md)). The colours are those of `ONMAP.HSQ`.
5. The box that names the map is written on the screen (below).

While the map is shown the clock runs on, but the sky does not fade.

### The view

The view's middle may not go further than latitude 75 north or south; a
latitude beyond is brought back to 75 first.

1. **The map**, 312 × 144 pixels from (4, 4), described below.
2. **Spice fields.** For each latitude from the middle's less 18
   downwards, while its row's middle stays inside the view: starting at
   the point nearest the middle's longitude, the points are looked at
   rightwards, one every 4 pixels until the right edge of the view, and
   then again from the same point leftwards until the left edge. Each
   point whose bits 4 and 5 are `01` gets sprite `0x78` of `ONMAP.HSQ`,
   or `0x79` when the next point in the map is one too. It is centred a
   little off the point: (b / 4) mod 4 pixels right, with b the point's
   position in the map plus 2w, and its position mod 4 pixels down.
3. **Places**, in the order of their records, except those with bit 7 of
   their byte at `0x0A`. Each goes where its longitude and latitude fall
   (below), if that is inside the view, centred: half the sprite's width
   (rounded down) left of the point and half its height up. The sprite of
   `ONMAP.HSQ` is `0x7A` plus one for each of these the place's kind (byte
   `0x08`) reaches: `0x20`, `0x21`, `0x28`, `0x30`. A place of a kind
   below `0x20` gets 5 more when it is further from the player than the
   state's word at `0x1176`: further, that is, in latitudes, or in
   longitude, counted in the points of the player's latitude.
4. **The player**, at the player's position: sprite `0x4C` of the panel's
   file, 13 pixels left of the point, its bottom row just above it.
5. **The ornament's head** (sprite `0x1A` at (150, 137)) again over the
   map.
6. **The troops' marks**: those at the places drawn and those on the
   move, see [orders.md](orders.md) and [moves.md](moves.md). They move
   while the map is shown.

All of these are clipped to the view.

A map position (λ, φ) is on the screen at x = 160 + ⌊d × 2w × 4 / 65536⌋,
where d is λ less the middle's longitude taken as a signed 16-bit number
and w the width of latitude φ, and y = 76 + 4 × (φ − the middle's
latitude).

### The map

Each map point becomes 4 × 4 pixels, and the pixels between points are
blended so the map looks smooth. The program works on lines of values,
where a point of colour c is the value 16c + 8, and every sum wraps round
at 256. A value v is shown in colour 16 + (v / 16 mod 16).

37 rows of the map are used, from the middle's latitude less 18 to the
middle's latitude plus 18. Each gives a **line**: with 2w points around
its latitude, take the point m nearest-below the middle's longitude
(⌊λ × 2w / 65536⌋) and q, the next two bits of that product (the quarter
of a point the middle is past m). When the row has 88 points or more, the
line holds 89 points from m − 44; otherwise the whole ring and one point
more, from m − w, the line then starting (88 − 2w) × 2 pixels further in.
Points before the first are counted round the ring. Each point p of the
line gives four values from its previous point's value a to its own value
b: a, a + s, a + 2s, a + 3s, where s is (b − a) taken as a signed byte
and divided by 4 rounding down. The previous point of the first point is
the byte just before it in the map, even when that belongs to another
row. The values are written from q values before the line's start, so
that the four leading up to point m begin at value 176 − q; value 176 is
the line's middle.

Each step from one row to the next makes four rows of pixels: the line of
the row nearer the pole, and three lines blended from it towards the line
of the row nearer the equator. The row nearer the pole has fewer points
(n against N), so the nearer-equator line is read faster: with r =
(N − n) × 65536 / n, a counter that adds r for every value read skips one
value of the wider line each time it passes 65536. Each blended line k
(1, 2, 3, counted from the narrower) takes the value a + k × s, where a is
the narrower line's value and s the difference to the wider line's value
taken as a signed byte, divided by 4 rounding down. Line k also writes a
value twice whenever its own counter, adding hi(k × r / 4 + 128) (the
high byte) to a byte, passes 256. This runs 176 values to the right of
the middle and then 176 to the left of it (starting just left of it), so
the three blended lines stay centred.

Going down the screen, north of the equator the earlier row is the
narrower; from the equator on, the later one is. The four rows are shown
from 20 values into the lines, 312 pixels wide. The wider line is then
kept as the next step's first line.

### Near the poles

When a step's row nearer the equator is at latitude 86 to 92 in the
north, or 87 to 93 in the south, the four pixel rows are rounded off
before they are shown. The player's video driver (`DNVGA.HSQ`, unpacked)
keeps, at file offset `0x7EF`, eight values: four for the left edge and
four for the right. Then two tables of seven latitudes each, 8 bytes a
latitude, two bytes (e, m) a pixel row: at `0x7F7` the north's (86 to
92), at `0x82F` the south's (87 to 93). Each row gets, from its left:
e − 4 values of 0 and the four left edge values (only the last e of them
when e is 4 or less), then 2m values left as they are, then the four
right edge values (only the first e) and e − 4 values of 0.

## The box

On the screen: the rectangle at data offset `0x194A` ((10, 10) to (190,
64)) is filled with the colour of its byte 9 and outlined with the colour
of its byte 8, inside the rectangle. Command phrase `0xE2` is then
written in the large font in colour `0xF0` from 10 pixels right and 8
down of its corner: each character after the last, a line break going
back to the left and 10 rows down. Before that, the number of rallied
troops (state byte `0x28`, at most 999) replaces the last three
characters of the phrase's first run of digits, a nought before the
first other digit written as a space.

The box goes 1000 ticks after the mouse button last went down or up, and
when an arrow moves the view: the view's rectangle under it is copied
from the back buffer to the screen.

## The commands

The map's commands are kept at data offset `0x20F2`: two bytes, then
(phrase, action) pairs up to a phrase of 0. Before they are shown:

- The second, giving orders: while the word at `0x1176` is below 2 it
  becomes phrase `0x93`, usable only when the player is in a place whose
  first troop has its byte 3 below `0x80`. Otherwise it keeps its phrase,
  greyed while the map shows no troop's mark.
- The third, the spice density, is greyed until the story (state byte
  `0x2A`) reaches 5; until then there is no fifth command.
  The spice density opens the map of the regions, see
  [spice_map.md](spice_map.md).
- The fourth, taking an ornithopter, is greyed (taking one from the map
  is not built yet).

The second command, "contact Fremen troops" (action `0x86CC`), does
nothing while no troop works for Paul (state byte `0x28` is 0). Otherwise
it opens the orders (see [orders.md](orders.md)), given from the map, of:
while Paul reaches no further than 1 (word `0x1176`, see
[jessica.md](jessica.md)), the first troop of his place; otherwise the
troop last given orders when the map marks it, else the troop the map
marks that "next troop" would come to from none. Troops numbered above
`0x43` are not given orders.

"EXIT MAPS" has action `0x186B` again: it closes the map. The room is
drawn as on entering it, with the panel's walking layout, and the screen
dissolves to it (transition `0x34`); then the ornament opens if it is not
already open.

## The arrows

The arrows' zones from `0x1CCA` act as follows; the four arrows have
flag `0x4000`: while the button stays down on one, it acts again each
time 50 ticks have passed since it last acted.

| Action | What |
| --- | --- |
| `0x8829` | up |
| `0x8824` | right |
| `0x882E` | down |
| `0x881F` | left |
| `0x5B05` | the middle: the view goes back to the player |

An arrow takes the box away, adds its step to the view's middle (four
(longitude, latitude) pairs at data offset `0x145E`, in the order up,
right, down, left) and draws the view again on the back buffer; the
view's rectangle is then copied to the screen. The middle does the same
without taking the box away.

## A place's label

The map keeps the places it draws, each with the point of the screen
where its sprite is centred. A click on the map (above the panel) first
takes the box away. The place drawn nearest to the click, counting the
distance across plus the distance down, the first of equal ones winning
and only places of a kind below `0x31`, is the one clicked when that
distance is below `0x14`. A click on a place shows its label; a click on
the place whose label is shown, or away from every place, takes the
label away by copying its rectangle from the back buffer to the screen.
Moving the view, its middle and leaving the map take it away too.

What a click on a place does while Paul is in a sietch is not built yet.

### Its size

What is inside a place is known when its byte `0x0A` has bit 1, or,
unless it is a fort that is safe (kind `0x28` or more, byte `0x0A` bit
3), when one of its troops (from its byte 9, each troop's byte 1 giving
the next) has job (byte 3) 6 and is not the Harkonnens' (byte `0x10`
bit 7). Its size is then 1. Otherwise it is 2, except for a safe fort
whose byte `0x0A` has bit 4: size 0 for kinds `0x28` to `0x2F`, 2 from
`0x30`.

### Where

The label is `0x6A` pixels wide; its height is state byte `0x11D0` +
its size. With the place at (x, y): its left is x + `0x0F`, less `0x82`
when that is `0xD2` or more; its top is y less half the height, at
least 4 and at most `0x94` less the height. On the screen only, it is
filled with the colour of byte 9 of data offset `0x1668` and outlined,
inside it, with the colour of byte 8.

### What it says

In the large font, on the fill's colour, from the label's left L and top T:

1. The kind's phrase as in the cockpit (see [cockpit.md](cockpit.md))
   at (L + `0x0C`, T + 4), colour `0x9A`.
2. The name as in the cockpit at (L + 4, T + 13), colour `0x96`.
   Size 2 stops here. The row r is now T + 13.
3. Size 0 only, when state byte `0x0A` has bit 5: in the small font,
   colour `0x90`, phrase `0x6C` ("water:") at (L + 4, r + `0x0A`). When
   the place's byte `0x0A` has bit 5 its water (byte `0x1B`) follows as
   three characters, noughts in front written as spaces, and that many
   of sprite `0x75` of `ONMAP.HSQ` go in a row from (L + 4,
   r + `0x11`); otherwise phrase `0x6D` ("No wind-trap") at (L + `0x0A`,
   r + `0x11`). The row moves on by `0x11`.
4. At (L + 4, r + `0x0C`), colour `0x9A`:
   - When its inside is known, phrase `0x4C` ("Battle:") and, at
     (L + `0x33`, r + `0x12`), sprite `0x8E` + the battle's gauge (below).
   - Otherwise phrase `0x6E` ("Equipment:"). From row r + `0x16`: when
     the seven bytes from the place's `0x14` are all 0, phrase `0x69` at
     (L + `0x10`, r + `0x1B`); otherwise, for each byte that is not 0, a
     column of that many of its sprite (data offset `0x192F` + which
     byte), starting at L + 4 and moving right by the sprite's width + 1
     for each column.

A row or column of n copies of a sprite fills the room from its start to
the label's right (or bottom) edge: one copy every size + 2 pixels; when
room / n is less than that, every (room − (size + 2)) / n pixels, and when
that is below 2, every 2 pixels and only (room − (size + 2)) / 2 copies.

### The battle's gauge

Over the troops at the place: the Harkonnens' men (byte `0x1A`) make H;
those with job 6 make M, and the words at `0x0C` and `0x0E` of those
make C and E. With a = 256 M / (M + E) and b = 256 H / (H + C) (0 when
the sum is 0) and m the larger, the value is (a − b) × 256, plus 255
when a − b is below 0, divided by m rounding towards 0 (just a − b when
m is 0); then halved rounding down, plus `0x80`, kept to a byte. The gauge is (value +
`0x0F`) / 32.
