# Drawing scenes

How the commands of a scene file (see [../formats/scene.md](../formats/scene.md))
end up on the screen. Mentat's drawing matches the original's palette index
for palette index in all 371 combinations of scene and picture file that
were measured; the details below are what that took.

## The screen as one row

The original treats the 320 × 200 screen as 64 000 bytes in a row, behind a
16-bit address. Each sprite row and each row of a shape starts at
y × 320 + x, where a y of 200 or more, or below zero, is taken as 199;
everything after that start only moves the address along. So a picture
that runs past the right edge goes on at the left edge one row lower, and
an address past 65 535 comes back to 0, the top of the screen. Bytes between
64 000 and 65 535 are not on the screen.

Only lines are clipped.

## Sprites

A sprite at full size is drawn as any sprite (see
[../formats/sprite.md](../formats/sprite.md)), with the screen addressing
above. The mirror and upside-down bits in the scene are added to the
sprite's own.

A shrunk sprite is always read as four bits per pixel, whatever its mode.
With the divisor *d* (8.8 fixed point):

- Its drawn width is (the width with its two bytes swapped) / *d*, and its
  drawn height (height × 256) / *d*, keeping the low eight bits. For widths
  below 256 the swap is just width × 256.
- Row *i* of the drawn sprite shows source row (*i* × *d*) / 256, and column
  *j* shows source column (*j* × *d*) / 256, both counted in 16 bits.
- A mirrored sprite starts at x + drawn width − 1 and goes left; an upside
  down one starts at y + drawn height − 1 and goes up.
- A nibble of 0 is see-through; others get the colour offset added.

## Lines

A line is clipped to the rectangle from the program (see
[../formats/program.md](../formats/program.md)).

- **Flat** (both ends on one row): every pixel from the left end to the
  right end, both included.
- **Upright** (both ends in one column): every pixel from the top end to the
  bottom end, both included. When the top is below the screen the line is
  skipped; when it is above, the line starts at row 0.
- **Any other**: the first end is not drawn. The longer of the x and y
  distances (*major*) gives the number of steps, the shorter one *minor*.
  A counter starts at *major* / 2. Each step adds *minor* to it; when it
  reaches *major* or more, *major* is taken off and the step moves along both
  axes, otherwise only along the longer one. Each new point is drawn.

## Shaded shapes

### Finding the edges

Each side of the shape is followed from point to point, and every time it
moves one row down an x is written for that row. The first side writes the
left column of a list of rows and the second side the right column; which
of the two is really further left does not matter, as the filling sorts
them out. From a point (x, y) to the next (x′, y′):

- **Flat** (y′ = y): one x is written: x′ when it is further left and not
  below 0, otherwise x. The flat stretch takes up a row of its own.
- **Upright** (x′ = x): x is written |y′ − y| + 1 times, once for each row
  including both ends.
- **Any other**: steps as for a line, without clipping. Before each step that
  goes one row down, the x from before that step is written. Steps that go
  up write nothing.

### Filling

The shape covers as many rows as the bottom point is below the top point.
For each row, the two x values are sorted and every pixel from the smaller
to the larger, both included, is set; left to right, or right to left when
flag bit 0 is set.

The shade is a 16-bit number whose high byte is a colour. It starts at the
base colour × 256 and goes up by the change per row (× 16) for each row.
Within a row it starts at the row's shade and goes up by the change per
pixel (× 16) for each pixel drawn, in drawing order.

Each pixel also gets a speckle from a 16-bit shift register that lasts for
the whole shape. When the shape has grain (any of flag bits 1–5), the
register starts at 1, and its feedback value is (grain bits) × 256 + 2;
without grain it starts at 0 and stays there. For each pixel, the register
is shifted right by one; when the bit shifted out was 1, the feedback value
is mixed in with exclusive or. The pixel's colour is then the shade's high
byte + (register & 3) − 1, kept to eight bits.
