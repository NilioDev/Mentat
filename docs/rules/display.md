# The display

How the original draws to the screen and changes palettes. The numbers that
the original keeps in its video driver are read from the player's own
`DNVGA.HSQ` (see "Tables" below).

## Pictures

The original keeps a 320 × 200 back picture besides the screen itself.
Scenes, sprites and video frames are drawn into whichever of the two is the
current target; most screens are drawn into the back picture and then shown
with a transition.

A vertical origin, a number of rows, is added to every place that is drawn
to. The opening uses 24 rows, so its 152-row scene area sits in the middle
of the screen; the rest of the game uses 0.

## Palettes

The original keeps three palettes of 256 colours (6 bits per component):

- the **working** palette, which drawing code changes;
- the **spare** palette, which transitions fade toward;
- the **screen** palette, what the card really shows.

Changing a colour in the working palette marks it as changed. Sending
the palette copies only the changed colours to the screen and clears the
marks. Keeping the palette copies the working palette into the spare one.

A fade step moves colours of the working palette toward the spare one: each
component gains (spare − working) / divisor, rounded toward zero, and the
colours are sent at once.

## Transitions

Every step of a fade or dissolve is followed by a wait of 3 ticks. The waits
are timed from when the previous step was due, so the whole transition takes
its full time even when drawing is slow. After any transition the back
picture is copied to the screen and the changed colours are sent.

| Type | What happens |
| --- | --- |
| `0x00` | **Rise** (the program asks for it upwards). The old screen is kept. In 19 steps, with s from 8 to 152 by 8, the 152 rows at the origin show: the old rows from s on at the top, then 8 rows of colour 7, then the back picture in place. Each step is followed by a wait of 6 ticks. |
| `0x02` | **Spiral.** The back picture is copied to the screen in blocks of 8 × 4 pixels, in a square spiral from the block at (156, 74): 1 block right, 2 down, then for n = 2, 4, … 36: n left, n up, n + 1 right, n + 1 down, each side starting next to where the last one ended. Each side waits for the next tick. Blocks are placed by their offset in the screen, so one past the left or right edge runs on in the next or previous row. |
| `0x04` | **Squash.** The old screen is kept. 17 steps, numbered 0x11 down to 1, each followed by a wait of 6 ticks. Each step blacks out the 152 rows at the origin and draws each half from the middle out (rows 75 and up, rows 76 and down, 76 rows each) from the picture: a run of rows copied, then rows left out, again and again, until the half's 76 rows are used up (it also stops when the rows left are no more than those to leave out). Step 9 draws nothing: the changed colours are sent and from then on the back picture is drawn instead of the old one. Rows left out and run per step, from 0x11 down: 1/17, 1/7, 1/4, 1/2, 2/3, 5/4, 2/1, 5/1, (9), 5/1, 2/1, 5/4, 2/3, 1/2, 1/4, 1/7, 1/17. |
| `0x06` | **Mosaic.** The old screen is kept. Its 152 rows at the origin are drawn again and again in square blocks of 2, then 4, then 8 pixels, each block in the colour of one pixel of it (the old picture's), that pixel taking its turn from a fixed list per size; each size goes on until 0x24 ticks have passed since it began. The changed colours are sent, and the back picture comes the same way in blocks of 8, 4 and 2. The pixels, as (column, row) in the block: for 2, (0,0) (1,1) (1,0) (0,1); for 4, (1,1) (0,3) (3,2) (2,0) (0,1) (2,3) (0,0) (1,2) (3,0) (1,3) (2,1) (3,3) (2,2) (1,0) (3,1) (0,2); for 8, (3,3) (0,5) (5,4) (4,2) (0,3) (4,5) (0,2) (3,4) (5,2) (3,5) (4,3) (5,5) (4,4) (3,2) (5,3) (2,4). Mentat draws one turn per tick; the original as fast as it can. |
| `0x26` | **Short mosaic.** As `0x06` with blocks of 2 and 4 out, then 4 and 2 in. |
| `0x28` | **Fine mosaic.** As `0x06` with blocks of 2 three times out, then 2 in. |
| `0x08` | **Sparkle.** The 152 rows at the origin, counted as one run of pixels, come from the back picture in an order that looks random: a 16-bit number starts at 1; each step copies the pixel it names and the one 0x7FFE further on (when that is still in the rows), then shifts the number right by one, mixing in `0x4400` with exclusive or when the bit shifted out was 1. When the number is 1 again (32 767 steps), pixel 0 is copied. 150 steps a tick, about 218 ticks. |
| `0x22` | **Slide down.** The new picture comes down from the top over the 152 rows at the origin, pushing the old one down and out: a row more every 6 ticks, two when the ticks have run past the time for the row. |
| `0x10` | **Dissolve.** The 152 rows at the origin are blacked out through a 4 × 4 pattern: 16 steps, each doing one place of the pattern in every 4 × 4 block. Then the changed colours are sent and 16 more steps show the back picture through the same pattern. About 96 ticks. |
| `0x30` | **Cut.** The changed colours are sent and the 152 rows at the origin are copied from the back picture. |
| `0x34` | **Dissolve of the whole screen.** As `0x10`, but over all 200 rows. |
| `0x36` | **Fade in from black.** The working palette is kept in the spare one, colours 0 to 254 are set to black and sent, the back picture is shown, and the colours rise in 64 rounds (32 colours at a time, steps of 1). |
| `0x38` | **Fade to black.** The working palette is kept, colours fall in 32 rounds (64 colours at a time, steps of 2), the back picture is shown, the working palette is set back from the spare one, and only the changed colours are sent, so the screen mostly stays dark. |
| `0x3A` | **Fade through black.** Colours 1 to 254 of the working and spare palettes trade places, so the new colours wait in the spare one. The old picture fades out in 22 rounds (85 colours at a time, steps of 3), the back picture is shown and the colours rise to the new palette in 22 rounds the same way. About 400 ticks. |

The lowest bit of the type is ignored.

A fade down takes the step off every component (stopping at 0) and sends
the chunk. A fade up brings every component toward the spare palette so
that all arrive in the last round: with d the distance still to go, a
component rises only when the rounds left are no more than the number of
steps it still needs. It first rises by d modulo the step (when that is not
0) and then by whole steps.

## Zoom

The zoom enlarges a part of the back picture to the 320 × 152 scene rows
of the screen. Each level takes a part of a fixed size, its top-left corner
at the centre less half the size (never below 0), and repeats source
pixels in a fixed pattern:

| Level | Part | Columns | Rows |
| --- | --- | --- | --- |
| 1 | 280 × 133 | each group of 7 pixels gives 8, the first doubled | each group of 7 rows gives 8, the last doubled |
| 2 | 240 × 114 | each 3 give 4, the first doubled | each 3 give 4, the last doubled |
| 3 | 213 × 100 | each 2 give 3, the first doubled; the last pixel twice | each 2 give 3, the last doubled |
| 4 | 160 × 76 | each twice | each twice |
| 5 | 107 × 51 | each three times; the last twice | each three times; the last twice |
| 6 | 80 × 38 | each four times | each four times |
| 7 | 40 × 19 | each eight times | each eight times |

The source is read straight through memory, so a part that runs off the
right edge goes on at the start of the next row.

## Tables

| File | Offset | Size | What |
| --- | --- | --- | --- |
| `DNVGA.HSQ` (unpacked) | `0x2ED7` | 16 × 2, then `0xFFFF` | The dissolve order: the offset (row × 320 + column, both 0 to 3) of each place of the 4 × 4 pattern |
| `DNVGA.HSQ` (unpacked) | `0x7EF` | 8 + 56 + 56 | The flat map's rounded edge near the poles (see [map.md](map.md)) |
