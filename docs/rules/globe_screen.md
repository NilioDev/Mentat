# The globe

The planet in the left part of the map's panel (zone 0, action `0xB8C6`)
opens the globe: the planet turning between the frescoes of the two
houses, with Paul's place marked on it. The planet itself is drawn as on
the story pages (see [globe.md](globe.md)). This page covers the globe as
it is opened, turned and left, its results, and the loading and saving
of games.

## Opening the globe

1. The planet faces the map's middle (the map's view, see
   [map.md](map.md)): rotation ⌊398 × longitude / 65536⌋, and the
   latitude as the tilt, with the story pages' rule for tilts closer to
   level than 32.
2. On the back buffer:
   - the scene's 320 × 152 rows are filled with colour `0xF0`;
   - sprite 2 of `FRESK.HSQ` (resource 1), the planet's frame, at
     (`0x5B`, `0x14`);
   - the whole planet;
   - sprites 0 and 1 of `FRESK.HSQ`, the frescoes, at (0, 0) and
     (`0xD6`, 0);
   - the ornament, as it is;
   - the globe's commands (below);
   - zones 0 to 2 as the map's layout leaves them, and the six zones at
     data offset `0x1DC6` in place of the arrows (see
     [panel.md](panel.md)).
3. The music for the globe is chosen (moment 1, see [game.md](game.md)).
4. The changed colours are sent, and the map rises away from the globe
   (transition 0 upwards, see [display.md](display.md)).
5. The waiting music starts, and a timer runs every tick (below).

### The timer

Every tick the next screen line of the planet is drawn in the back
buffer, as on the story pages. When a picture is finished:

1. Paul's place is found on the screen (below), with the planet as it was
   drawn.
2. The planet's area, (96, 15) to (234, 134), is copied to the screen.
3. When the place was found, sprite `0x36` of the panel's picture file
   marks it on the screen: its left edge at the point and its bottom row
   just above it (the row is counted in a byte).
4. The planet turns a step.

The same happens at once, after drawing the whole planet again, whenever
an arrow moves it.

### Where a map point shows on the planet

With the latitude's row (its absolute value) of width w and phase p (see
[globe.md](globe.md)):

1. The map point's number is round(2w × longitude / 65536), rounding up
   from a half. Its distance round from the face's middle is d = the
   number less p; a negative d is made positive and puts the point on
   the left. When d is w or more it becomes 2w − d and the side changes.
   When d is ⌊w / 2⌋ or more the point is behind the pole: d becomes
   2⌊w / 2⌋ − d.
2. The columns of `GLOBDATA.HSQ` are searched from the middle outwards for
   the height where each column crosses the row (an entry of the first
   half equal to twice the row). The first column is searched from height
   0; each next one from the height found in the one before. The column
   whose longitude at that height (its second half) is d or more gives
   the point's column and height. When a column never crosses the row,
   the height found in the column before is used with this column. After
   64 columns, or when the first column never crosses the row, the point
   is not shown.
3. The tilted view gives each value v of a screen line (−98 to 97) what
   it shows: whether u = v − tilt is above 0 (the side of negative
   latitudes), and |u| as a height, or past the pole (|u| of 99 or more)
   197 − |u| taken as negative. The values from −64 to 63 are searched
   for the first that shows the point's side and its height (negative
   when behind the pole). None: not shown. A value below 0 means the
   lower half of the planet; its size is the value looked for.
4. The screen lines, from the middle row outwards (at most 54), are
   searched while they are longer than the column: the line whose byte
   in that column is equal to the value, or the nearest above it (as an
   unsigned byte), is the row. When the nearest is more than 2 above,
   the point is not shown.
5. The point is (160 ± column, 79 ∓ the line's number), the column taken
   left or right and the line up or down as found.

### Which map point a screen point shows

For a point (x, y) with 96 ≤ x < 224 and 25 ≤ y < 134: line |y − 79|
of the list, and column |x − 160|; outside the line there is none. Its
byte v, taken as negative from row 79 down, gives with the tilted view
(above) the side and height. The column's first half at that height
gives the row, its second half how far round l it is; past the pole l
becomes w − l. Left of the middle l is negative. The point is p + l,
brought into the ring of 2w, and the longitude is that number × 65536 /
2w; the latitude is the row, negative on the side of negative
latitudes.

## The commands

The globe's commands are kept at data offset `0x204A` (two bytes, then
(phrase, action) pairs up to a phrase of 0): leaving the globe, the
results, loading, saving and leaving the game. Leaving the game is not
built yet and is greyed; so are loading and saving when there is nowhere to
keep games. The second line is
phrase `0xB1` with action `0xB96B` (the results), or, while the results
show, phrase `0xB2` with action `0xB961` (back to the standard view).

While the mouse is over the planet's square, (96, 25) to (224, 134), the
list is the one at data offset `0x2062`: a single lit line offering the
map of the area. Leaving the square brings the globe's list back.

## The zones

| Action | What |
| --- | --- |
| `0xBC81` | the button right of the list: leaving the globe |
| `0xB9B9` | up: the tilt grows by 8, at most 98 |
| `0xB9CC` | right: the planet turns back 32 steps |
| `0xB9C0` | down: the tilt falls by 8, at least −98 |
| `0xB9D3` | left: the planet turns on 32 steps |
| `0xBA9E` | the middle: the planet turns to Paul |

The four arrows act again while the button stays down, as on the map.

### Turning to Paul

The planet turns towards Paul's position in pictures:

1. The rotation wanted is ⌊398 × longitude / 65536⌋; the turn is the
   shorter way round to it (half way round: forwards). The tilt wanted is
   Paul's latitude.
2. The turn is held to 32 steps each way and the tilt's change to 24: a
   turn of 32 or more becomes 32 and one of −32 or less becomes −32, and
   the same for the tilt with 24. Each of these counts as cut, even when
   it was exactly at the limit.
3. The planet turns and tilts by those, is drawn whole and shown as
   above (turning a step more).
4. When anything was cut, all of it is done again.

## The results

The second command slides the frescoes away and shows how Paul is doing.

1. On the back buffer: the scene filled with `0xF0`, the planet's frame
   and the planet as on opening, then the sprites of `FRESK.HSQ` listed at
   data offset `0x2506` ((sprite, x, y) up to `0xFFFF`): the emblems of
   the two houses.
2. The results are counted (below) and written into their phrases.
3. The phrases listed at data offset `0x2482` are written in the large
   font, those at `0x2494` in the small one. Each entry is (phrase, x, y,
   colours) up to a phrase of `0xFFFF`; the colours' low byte is the
   letters', the high byte the background's (0: none). A byte `0x0D` starts
   a new line at the same x, 10 rows lower in the large font and 7 in the
   small; bytes from `0x80` show as '@'.
4. The bars start at 0, and grow (below) every 12 ticks, the first time
   at once, on the back buffer; after that on the screen.
5. The scene is kept, and the frescoes slide out: with a shift of 0, −16,
   −32 and on while it is above −`0x6A` (to −96), the kept scene comes
   back, fresco 0 is drawn at (shift, 0) and fresco 1 at (`0xD6` − shift,
   0), both cut to the scene, the ornament over them, and the scene goes
   to the screen. The original goes as fast as it can draw; here each step
   takes 4 ticks, and no timer runs meanwhile.
6. The planet is drawn whole and shown in the colours of who holds each
   point (see [globe.md](globe.md)), and the second command changes.

The second command again removes the bars' timer and slides the frescoes
back on the screen, 16 at a time up to 0, draws them at 0 on the back
buffer, and shows the planet in its usual colours.

### What is counted

In the state (as words unless said):

- **The areas held**: of the first `0xC5F9` map bytes, those whose bits
  `0x30` are both set (held), and those with only one of them (partly
  held). Each count c becomes a share of N = `0xC5F9` − `0x187`:
  ⌊c × 65536 / N⌋ × 100, divided by 65536 and rounded from a half. The
  held share goes to `0xA4`, the partly held one plus 1 to `0xA2`.
- **The best spice stock**: `0xA0` + `0x1172` − `0x1170` (at least 0)
  goes to `0xA6` when it is at least what `0xA6` holds.
- **The men**: over the troops (from `0x8AA`, `0x1B` bytes each, the first
  always and the others until one starts with 0) whose byte 3 does not
  have bit `0x20`: the byte at `0x1A` adds to `0xAC` when byte `0x10` has
  bit `0x80`, otherwise to `0xAA` when byte 3 does not have bit `0x80`.

### Writing the numbers

A number goes into the first run of digits found from a place in the
phrases, which lie one after the other, each ended by `0xFF`; the search
for the next number goes on from the end of that run, into the next
phrases. The phrases keep what is written into them.

- **Three digits**, ending just before the run's end: at most 999. A
  hundreds digit of 0 becomes a space, and so does then a tens digit of
  0; the units always show.
- **Five digits**: the thousands as two digits, a leading 0 a space, and
  when both are 0 both are spaces; then the last three as above, where the
  spaces go on only while all before were spaces.

The numbers:

1. Phrase `0xC2`: the day, ((clock + 3) >> 4) + 1, in three digits, and
   over the two bytes after them an ending from the four pairs at data
   offset `0x251A` + 8 × language: the first pair when the tens are '1' or
   the units 4 or more, also for units of 1 in the second language's
   table unless the tens are a space; otherwise the pair of the units.
2. Phrase `0xC3`: charisma, the state's byte `0x29` halved.
3. From phrase `0xC4` on: `0xA4` and `0xA2` in three digits, then `0xA8`,
   `0xA6`, `0xAC` and `0xAA` in five digits ending one before the run's
   end, whose last digit becomes 0 (so they show ten times).

Each of the six is kept at `0x115E` + 2i; when it differs from what was
kept, the byte at `0x116A` + i becomes 1 (more) or 2 (less); it stays as
it was when the same. The clock with its low four bits cleared goes to
`0x115C`.

### The bars

Each bar goes to a byte: `0xA4` / 2 + 1, `0xA2` / 2 + 1, `0xA8` / 16 + 1,
`0xA6` / 16 + 1, and the high bytes of `0xAC` and `0xAA` plus 1. Each step,
every bar that is not there yet moves one towards it; with h its size
(at most 30) and (x, y) from the six pairs at data offset `0x24EE`, sprite
`0x37` (`0x38` for the odd bars) of the panel's picture file goes at
(x, y − h) and sprite `0x39` at (x, y − h − 10). When the bar arrives, the
byte at `0x116A` + i is written as a letter of the small font at
(x + 4, y − h − 10), in colour `0x3F` (`0x25` for the odd bars), with no
background.

## Loading and saving

"SAVE GAME" shows the list at data offset `0x207A` (two bytes, then
(phrase, action) pairs as above): the two logs (phrases `0x10F` and
`0x110`, action `0xB35A`) and "Cancel" (phrase `0xA3`, action `0xD2E2`),
which brings the globe's list back. "LOAD GAME" shows the list at
`0x208A`: the four files (phrases `0x10F` to `0x112`, action `0xB3B0`) and
"Cancel". The n-th line is the file `DUNE37Sn.SAV` (see
[../formats/save.md](../formats/save.md)). While these lists show, the
mouse over the planet does not change them.

For each file that is there, its clock is read, and for the first two its
line is written into: with c the clock, the day ((c + 3) >> 4) + 1 goes in
three digits over the end of the line's second run of digits (leading 0s
as spaces, as in the results), and from three bytes after that run, the
ten letters at (c & 15) × 10 of phrase `0x117`, the times of day. The lines
keep what is written into them. When saving, a file that is there lights
its line; when loading, a file that is missing greys its line.

**Saving** into a log writes the line with the clock as it is now and
draws it, writes the file, and shows phrase `0x113` ("save successful"),
or `0x114` when the file could not be written, on the list's last line.
After 300 ticks, a saved game brings the globe's list back; otherwise the
list stays.

**Loading** a file puts back the state, the dialogue lists and who holds
each area; a file that cannot be read changes nothing. Then the globe
shows the game loaded: when the results show, the frescoes slide back in
first; the results are shown (above), and the planet turns to Paul.

## Leaving

Leaving the globe, or clicking on the planet where it shows a map point,
zooms into the map:

- **Leaving** turns the planet to Paul, makes Paul's position the map's
  middle and zooms from (160, 79).
- **A click on the planet** makes the map point under the mouse the
  map's middle, draws the planet again as an arrow does, and zooms from
  the mouse.

The zoom hides the mouse pointer and grows a frame 8 times from the
rectangle (x, y) to (x + 2, y + 1): each time its left edge goes 4
pixels left and its top 2 up, but not past (96, 15), and its right edge
4 right and its bottom 2 down, but not past (234, 134). Each time:

1. The frame is drawn on the screen in colour 7 (its right column and
   bottom row are inside the rectangle).
2. Inside it, the back buffer is shown in blocks of 2 × 2 pixels, from
   the top left corner inside the frame, as many whole blocks as fit.
   Each block takes one of its own pixels, the top left, then the bottom
   right, the top right and the bottom left in turn, again and again
   until 10 ticks have passed. Nothing is shown when no whole row of
   blocks fits.

Then the map's colours are sent at once, the map is drawn with that
middle on the back buffer, without its box, and it spirals in
(transition 2, see [display.md](display.md)); the planet's timer stops.
