# Text in a box

How the original lays a piece of text out in a box on the screen. Mentat
uses it for the story pages (see [story.md](story.md)) and will use it
for the rest of the game's text.

## Breaking into lines

The words are the runs of characters between spaces and line breaks
(byte `0x0D`). A word's width is the sum of its characters' widths in the
font (see [../formats/font.md](../formats/font.md)). The natural gap
between two words is 6 pixels; spaces themselves are not drawn.

Words go onto a line while they fit with a gap after each, the last word
of a line needing no gap after it. A word that does not fit starts the
next line. A line break ends the line. A word wider than the whole box
stops the layout: nothing is drawn.

## Stretching

The room left on a line is spread over its gaps: each gap becomes 6 plus
the room divided by the number of gaps, and the first (room mod gaps)
gaps get one pixel more, so the line reaches the box's right edge.
A line of one word gets the room as its gap, which matters only for the
check below.

These lines keep the natural gap of 6:

- the last line;
- a line that a line break ends;
- every line, when any line would get a gap of 30 pixels or more.

## Drawing

Each line is 10 rows high. The block of lines is centred in the box's
height (rounded down), starting at the box's left edge.
