# Orders

Paul gives orders to a troop of his through its chief: "give orders to
troop" in a talk with the chief (see [people.md](people.md)) ends the talk
and opens the orders.

## Opening

The state's byte `0xFB`, whose sign tells whether the map is shown, is
complemented (going back to the room through the map's own way negates it,
as leaving the map does). The map opens around Paul's place as "see Dune
map" does, without its box, with the orders as its commands, and is
dissolved onto the screen. Then the chief's box is drawn over it and the
chief says his first line.

## The chief's box

The box is the program's box at `0x18E9`: from column 5 to 232, 0x43 rows
high, its border and fill colours from bytes 8 and 9 of the record. It
stands at row 5 when the troop's mark on the map starts at row `0x4C` or
lower, and at row `0x50` otherwise. The mark stands off the place by the
troop's turn there (its byte 2, less one, with bit 3 flipped when the
place's byte `0x0A` has bit 1), from the table of signed pairs at
`0x1672`; the mark is 15 rows high, so its top is 7 rows above that point.

In the box, 4 columns and 3 rows in, is the face's frame (the record at
`0x18F3` for its colours), `0x3D` square. The face is drawn inside it,
clipped one pixel within the border. Each piece of the face goes where it
would in a talk, less the face's focus point, plus the frame's inside
corner: the focus is the pair of words at `0x22B9` plus four times the face
less `0x0E` (the three Fremen faces). The face's set is the troop's, as in
talks; it moves as in talks, with no voice.

The text is laid out like a box's text (see [text.md](text.md)) in colour
`0xF0` over the box's fill, from `0x49` columns into the box and 4 rows
down, to 6 columns from the right and 3 rows from the bottom.

## What the chief says

The chief is character 15. Before each line the state learns his troop
(see [people.md](people.md)). His news is list 2 of his group: each line
is the next that may be said after the last one, passing over lines said
before only when they have flag bit 5, and starting over at the list's
start when nothing more is found. "Ask for more information" says the next.

## The commands

The orders are the list at `0x210A`: ask for more information, change the
troop's occupation, modify its equipment, move it, and no more orders.

- The occupation, the equipment and the move start greyed, and stay so
  while the troop's word `0x12` has bit 10.
- Otherwise the occupation is offered unless the troop prospects (its
  occupation, the low four bits of byte 3, is 1). A troop waiting for
  orders (occupation 2) is offered "select troop occupation" (phrase
  `0x56`) instead, and nothing more.
- Once the story is at 5 or more, the troop can be moved.
- From 4 on, when the troop's word `0x10` lacks bit 9 and its place has
  bit 3 of byte `0x0A` or is of a kind below `0x28`, it can change its
  equipment when the place keeps equipment no troop there holds or the
  troop holds some (its byte `0x19`). What the place keeps is its seven
  counts from byte `0x14`, less one for each of its troops holding that
  piece: bit 7 of the troop's byte `0x19` is the first count.

"No more orders" closes the orders and the map, back to the room.

## The occupations

Choosing the occupation lists what the troop can take up, by its
occupation now:

| Now | List | |
| --- | --- | --- |
| waiting (2) | `0x215A` | spice, army, ecology |
| a spice one (bits 2 and 3 clear) | `0x216E` | search for equipment, army, ecology |
| an army one | `0x2182` | search for equipment, espionage, spice, ecology |
| attacking (5) | `0x219A` | attack |
| an ecology one | `0x21A6` | search for equipment, wind-trap, spice, army |

Each ends with "cancel", which brings the orders back. In the lists but
the first, searching for equipment is greyed while the story is below
`0x10`. Espionage is greyed while the state's word `0xE2` is below `0x1E`;
ecology is greyed unless the state's byte `0x0A` has bit 5.

### Taking up a speciality

"Specialize in spice" asks for occupation 0, "in army" 4 and "in
ecology" 8. When the troop has it already nothing happens. Otherwise
the new occupation is set (below), the answer is made a yes, the state
learns the troop, the situation at `0x23` becomes `0x0A`, and the chief
answers with the first line of his list 4 that may be said (passing over
lines said before only when they have flag bit 5); the next line of news
starts list 2 over. On a yes, when the new speciality (bits 2 and 3) is
not spice, bit 7 of the troop's byte `0x19` is cleared. On a no the
troop's word `0x12` comes back as it was and its old byte 3 is set again
the same way. Either way the orders come back.

Setting an occupation, when the low four bits of byte 3 differ from it:
ecology at place `0x7C8` while that place's byte `0x1A` is 0 becomes
`0x0A`. Byte 3 takes the value; the troop's word `0x12` loses bits 4 and
5 and its word `0x10` bit 8. The troop's ability to do the work is
checked (below); when it can, word `0x10` gets bit 8. The work starts
now: word `0x0A` takes the clock, words `0x0C` and `0x0E` become 0. Unless
the troop waits, word `0x12` gets bit 13 plus its speciality (bits 2 and 3
of byte 3).

### Whether a troop can do its work

By the occupation in the low four bits of byte 3, at the place its word 4
names:

- spice mining (0): not when word `0x10` has bit 9, word `0x12` has bit 4
  or 5, the place's byte `0x12` is 0, or the place's byte `0x0A` does not
  have bit 6 or has bit 0;
- prospecting (1): not when the place's byte `0x0A` has bit 6 or bit 0;
- ecology (8): not when word `0x12` has bit 4 or 5, the place's byte
  `0x1B` is 0, its byte `0x0A` lacks bit 5, or the troop's byte `0x19`
  lacks bit 1.

Any other occupation can always be done, and nothing more happens. For
these three, bit 4 of byte 3 keeps whether the troop cannot; when that
changes from cannot to can, the work starts again from now as above.

## The troops' marks

While the orders are shown, the map shows the troops at the places in
sight, place by place in the order of the map's places, each place's
troops in the order of its chain (byte 9, then each troop's byte 1). A
troop's mark is a list of frames in the program, three bytes each: a
sprite of the map's sheet and how far the mark moves (signed column and
row), ending at a sprite 0 after the first frame. Which list:

- none for a troop whose byte `0x10` has bit 4;
- Fremen not yet Paul's (byte 3 from `0x80`, byte `0x10` without bit
  7): the list at `0x181F` when their place's byte `0x0A` has bit 4,
  else none;
- a troop on the move (byte 3 bit 6): not built yet;
- when byte 3 has bit 4 or 5: for spice mining (occupation 0) with
  equipment in bits 7 and 6 of byte `0x19`, `0x1813` for bit 7 alone,
  `0x1817` for bit 6 alone, `0x181B` for both; otherwise the word at
  `0x16B6` plus twice the occupation;
- otherwise, for spice mining with that equipment, `0x17BC`, `0x17C9`
  or `0x1806` the same way; else the word at `0x179C` plus twice the
  occupation. When that is `0x1732` and the place's byte `0x0A` has bit
  1, it is `0x16AA`. When it is `0x1732` or `0x1774`, the troop's turn
  at the place (byte 2, less one, low three bits) moves it on by 10
  bytes for turn 3, 20 for 4 and 30 for 5 to 7.

The mark is centred (half its width and height, rounded down, to the
left and up) on the place's point moved by the turn's pair at `0x1672`
(see the box above), and clipped to the map's view. After the marks,
the troop given orders gets a ring around it: the list at `0x18FD`,
centred on the same point.

A mark with more than one frame shows its first and goes on from a
frame picked at random: the second generator's numbers, masked to the
smallest run of low bits that holds the count of further frames, until
one is not above it. Every `0x0F` ticks the ring shows its next frame,
and at every fourth of these steps the other moving marks do too; after
the last frame the list starts over. Each step moves the mark by the
frame's offsets and draws the map again over where it was and is, with
the marks, except under the chief's box.

## Not built yet

Modifying equipment, moving a troop,
searching for equipment, espionage, attacking, the wind-trap, the orders
from the map for troops on the move, and what troops do day by day.
