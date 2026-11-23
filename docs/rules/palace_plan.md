# The palace's plan

The middle of the arrows (see [panel.md](panel.md)) shows a plan of the
palace over the right part of the room. It does nothing outside the
palace (rooms of kind `0x20`) or in its first room.

## Drawing it

On the back picture:

1. The rectangle (160, 0) to (320, 116) is filled with colour `0xF1`.
2. Four frames are drawn around (164, 4) to (316, 112), each a pixel
   further out: the first from (163, 3) to (316, 112), both corners
   included, in colour `0xF7`; then `0xF5`, `0xF3` and `0xF1`.
3. The plan's sprites, of picture file `0x21`: the list of (sprite, x, y)
   words at state offset `0x120B`, up to a word `0xFFFF`. The story removes
   its last parts as the palace opens (see
   [story_steps.md](story_steps.md)).
4. The people, counted by room. A character counts when byte 3 of its
   record (the high byte of its place) equals state byte 7 (that of
   Paul's place); not Gurney (character 4) from story step `0x15` to
   `0x1F`. It counts in its room (the low byte of its record's word 0)
   among those going with Paul when byte `0x0F` of its record has bit 6,
   else among the others. Paul counts in his room when it is `0x0C` or
   lower.

   For rooms 2 to 12, the table at data offset `0x1426` gives an (x, y)
   byte pair each, from the position of the plan's first sprite. From
   that point moved by (3, 2): sprite 2 once for each of the others, at
   most five, 4 pixels apart to the right; 7 rows lower the same for those
   going with Paul; and when Paul is there, sprite 1 at (+12, +5) from the
   room's point.

The plan then comes in by unrolling (transition `0x10` in the rectangle,
see below), and the commands become those at data offset `0x2012`
("Done").

## Closing it

"Done", any other command, a click anywhere or the middle again: the room
is drawn again on the back picture and the plan rolls up and away
(transition `0x12` in the rectangle); the room's commands come back.

## Transitions in a rectangle

The original has a second set of transitions that work inside a rectangle:

- **`0x10`, unroll.** Two rows of the back picture from the rectangle's
  bottom less 2 are copied to its top on the screen; then each step takes
  two more rows, starting two rows higher, until the start goes above the
  screen's first row. The rectangle is then copied from the back picture.
- **`0x12`, roll up.** With *n* the rectangle's height less 6: the screen's
  top *n* rows of the rectangle take the rows 6 below them, and the 6 rows
  under them come from the back picture; *n* goes down by 6 while it stays
  0 or more, with a last step at 0 when it did not land on 0.

The original runs both as fast as it can; Mentat does 16 steps a tick of
the unroll and 4 of the roll.
