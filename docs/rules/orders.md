# Orders

Paul gives orders to a troop of his through its chief: "give orders to
troop" in a talk with the chief (see [people.md](people.md)) ends the talk
and opens the orders. From the map, "contact Fremen troops" opens them
too (see [map.md](map.md#the-commands)), and "next troop" goes from one
troop to the next.

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
talks; it moves as in talks.

The text is laid out like a box's text (see [text.md](text.md)) in colour
`0xF0` over the box's fill, from `0x49` columns into the box and 4 rows
down, to 6 columns from the right and 3 rows from the bottom.

## What the chief says

The chief is character 15. Before each line the state learns his troop
(see [people.md](people.md)). His news is list 2 of his group: each line
is the next that may be said after the last one, passing over lines said
before only when they have flag bit 5, and starting over at the list's
start when nothing more is found. "Ask for more information" says the next.
When a line leaves the answer "maybe" (`0x80`, see
[talking.md](talking.md)), the chief shows what his troop holds in his
box, as when modifying it (below).

After each line, when there is a recording of it (counted as in talks,
see [talking.md](talking.md#voices)), the face starts a new idle sequence,
its calm part as long as the words of the page, and the chief's voice says
it, whatever is chosen for the subtitles: the text stays in the box.

## The commands

Which list of orders is shown depends on the troop. How far it is from
Paul is the larger of the longitudes' difference (taken as a signed
number, made positive) divided by the size of a point at Paul's latitude
(see [map.md](map.md)) and the latitudes' difference.

- Further than Paul reaches (word `0x1176`, see
  [jessica.md](jessica.md)), or with bit 5 of byte 3 and byte 3 not
  `0x22`: the list at `0x2122`, "next troop" and "no more orders" (which
  leaves the troop, the map staying).
- On the move (byte 3 bit 6): the list at `0x214A`, "change destination"
  (as "move troop"), "next troop" and "cancel", which leaves the troop,
  the map staying.
- Otherwise the list at `0x210A`: ask for more information, change the
  troop's occupation, modify its equipment, move it, and no more orders;
  given from the map, the last is phrase `0x53`, "cut contact", instead.

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

In the list at `0x210A`, "no more orders" closes the orders and the map,
back to the room, when they were given in the room; given from the map,
"cut contact" closes the orders and the map stays with its commands. Any
click while the orders are shown that is not on a command that can be
used (on a greyed one, the map, the arrows or the panel) does the same.

### Next troop

"Next troop" looks through the troops the map marks, Paul's only (byte 3
below `0x80`), for the one whose number comes next after the troop's,
going round after 255. With none, nothing happens; while Paul reaches no
further than 1, nor when that troop is not at Paul's place. Otherwise the
troop is left as below and the orders open for the next one.

When the orders end, by any way, the troop is left: when its word `0x12`
has bit 11 and the story is at `0x60` to `0x63`, the story moves on to
`0x64`. Its word `0x10` keeps only bits 4 to 9, its word `0x12` loses
bits 9, 11 and 12, and its byte `0x14` takes the clock's day (the
clock, word 2, shifted right by 4).

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

"Wind-trap" asks for the troop's speciality (bits 2 and 3 of byte 3) with
1 in the low two bits, and "attack" with 2, the same way.

### Espionage

"Espionage" first asks for the speciality with 1, as above, and whatever
the answer goes on: the troop's byte `0x10` loses bit 6 (it is not ready),
the state learns the troop's place (see
[people.md](people.md#what-the-state-learns-about-the-place)), and the
chief is asked to go to the nearest fort not found yet (word `0xE4`) as
when a place is pointed at for "move troop": on a yes the troop sets off
and the orders end. On a no its occupation loses bits 0 and 1 and the
orders come back.

### Searching for equipment

"Go & search for equipment" looks for the first kind of the speciality's
equipment the troop does not hold (bits of byte `0x19`, kind k for bit
`0x80` >> k): kinds 0 and 1 for spice, 2 to 5 for the army, 6 for
ecology. When it holds them all, the orders come back and the chief
answers in situation `0x0F`. Otherwise word `0x1203` takes `0xE8` + k.

When the troop's own place has a piece of that kind free (as for
modifying the equipment; two for an ornithopter, kind 1, at Paul's home,
word `0x1150`), the troop takes it there and then, as if given it by
modifying the equipment.

Otherwise the nearest place to fetch it from is looked for, among the
places from the third on (to the first whose word 0 is `0xFFFF`), but
those hidden (byte `0x0A` bit 7), forts (kind from `0x28`) and the
troop's own place. The distance is the larger of the high byte of the
longitudes' and the low byte of the latitudes' (the whole latitude
distance when that is larger); a place is passed over when its low byte
is `0x32` or more, and from kind `0x21` on it counts a quarter. Of the
nearer ones, a place with a piece of the kind free, after taking off
one for each troop on the move there for that kind (byte 3 with bits 0,
1 and 6, word 4 the place, word `0x0C` not the place, byte `0x0E` the
kind) and, at Paul's home before story step `0x50`, one ornithopter
(never below 0).

With none, the orders come back and the chief answers in situation
`0x0E`. Otherwise the troop is asked to take up its speciality with 3
in the low two bits, as for "wind-trap". On a yes, its word `0x0E`
keeps the kind (low byte) and its bit (high byte), its word `0x0C` its
place, and it sets off for the place found (see [moves.md](moves.md));
the orders end.

### Setting an occupation

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

## Modifying the equipment

"Modify equipment" sets the commands to the list at `0x2012` (only
"done"). Unless the chief's last line showed what his troop holds, he says
the next line of his news, again up to four lines in all, until one does.

Then two boxes are drawn on the screen, from the chief's box (its left,
top, right and bottom): the troop's from left plus `0x7D` and top plus
`0x2B` to the chief's right and bottom, and the place's from left plus
`0x30` and the chief's bottom plus 1 (less `0x6D` when that is `0x70` or
more), `0x28` rows high, to the same right. Their border and fill colours
are the data bytes `0x193E` and `0x193F`, and `0x1948` and `0x1949`.

What the troop holds is the bits of its byte `0x19` from bit 7 down to
bit 1, one piece each; the state's byte `0x3D` keeps the byte. What the
place has spare is its counts less those its troops hold (see above). Each
list is drawn as on the map's label (see [map.md](map.md)): for each kind
with a count, its sprite (data `0x192F` on) in a column of that many,
the columns side by side, one pixel apart; "none" (phrase `0x69`) moved 12
right and 5 down when the list is empty. The troop's starts at the
chief's left plus `0x80` and top plus `0x2D`, its columns ending at the
chief's bottom, "none" in colour `0xF0`. The place's starts at its box's
left plus `0x50` and top plus 4, ending at its bottom, in the colour
of its border on the ground of its fill; then phrase `0x6F` in the same
colours from its left plus 8 and that row (5 lower when it has nothing),
each new line 10 rows lower.

A click inside the chief's box on a column of the troop's list (from its
left up to its sprite's width plus one) gives that piece back to the
place; elsewhere in the chief's box it does nothing. A click inside the
place's box on a column takes that piece when the troop holds none of
that kind. Either way both boxes are drawn again. The insides of the
boxes are what lies strictly between their edges. Any other click is
"done".

The original finds the place's spare again from the records whenever the
place is learned (each hour, see [days.md](days.md)), so a piece given
back in the same hour may not show; Mentat keeps the counts as clicked.

"Done": both boxes are taken off the screen, the troop's byte `0x19`
takes the pieces it holds (bit 0 clear), and its mark is found again. The
state's byte `0x3D` becomes the bits it gained, `0x3E` those it gave
back, and `0x3F` is `0x40` when it gained an ornithopter (bit 6) at Paul's
home (`0x1150`), else 0. A troop in ecology (occupation 8) has its ability
to work checked (below). Then the state learns the troop, the situation
at `0x23` becomes `0x0C`, and the chief answers with the first line of his
list 4 that may be said; with none, he says the next two lines of his
news. The orders come back.

## Moving the troop

"Move troop" asks the chief where to: he says phrase `0x54` of the
command file in the box (see below), the commands become the list at
`0x212E`, "cancel" alone, and the small map of the regions opens over the
map as for "see spice density" (see [spice_map.md](spice_map.md)),
showing the spice, at column `0x5C`: at row `0x1E` when the box is at
row 5, at row `0x0E` when it is low. It marks the troop and its way
instead of Paul. The troop of record `0x8E0` (troop 3, the prospectors)
is shown up to three sietches in turn instead (below).

The phrase goes in the box's text area, which is cleared whole: laid out
from one row higher than a line of news, `0x19` rows deep, and `0x26`
rows further down when the text area starts at row `0x32` or lower. Its
recording, were it spoken, is the chief's `0x10A` on from the phrase.

While the small map is open:

- the mouse points at places in it, and a click on its bar switches its
  colouring, as for "see spice density";
- "cancel", a click outside it or on its close box closes it; the chief
  says his next line of news, which starts list 2 over, and the orders
  come back;
- a click on a place pointed at closes it, the chief says his next line
  of news, and then answers. When the place is where the troop is (or
  goes), the situation at `0x23` is `0x10`. Otherwise, while word 4 of the
  troop names the place, the state learns the troop and names the place
  as a line does (its name to word `0x11ED`, its kind's word, `0x0C` on
  from byte 1, to word `0x11EF`); word 4 comes back, and the situation is
  `0x0B`. The answer is made a yes, and the chief answers with list 4 as
  for an occupation.

On a no the orders come back, the answer left in the box. On a yes the
troop sets off (see [moves.md](moves.md)) and the orders end: the box
goes, and the map stays where it was, drawn again with the map's own
commands, as "see Dune map" shows it but without its box. (The original
first draws squares closing in on the troop; that is not built yet.)

## The troops' marks

The map shows the troops at the places in sight, place by place in the order of the map's places, each place's
troops in the order of its chain (byte 9, then each troop's byte 1). A
troop's mark is a list of frames in the program, three bytes each: a
sprite of the map's sheet and how far the mark moves (signed column and
row), ending at a sprite 0 after the first frame. Which list:

- none for a troop whose byte `0x10` has bit 4;
- Fremen not yet Paul's (byte 3 from `0x80`, byte `0x10` without bit
  7): the list at `0x181F` when their place's byte `0x0A` has bit 4,
  else none;
- a troop on the move (byte 3 bit 6): see [moves.md](moves.md);
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
(see the box above), and clipped to the map's view. Then come the troops
on the move, in the order of their records (from `0x8AA`, `0x1B` bytes
each, up to `0xFBB`), but those whose byte `0x10` has bit 4. While the
orders are shown, the troop given orders then gets a ring around it: the
list at `0x18FD`, centred on the same point.

A mark with more than one frame shows its first and goes on from a
frame picked at random: the second generator's numbers, masked to the
smallest run of low bits that holds the count of further frames, until
one is not above it. Every `0x0F` ticks the ring shows its next frame,
and at every fourth of these steps the other moving marks do too; after
the last frame the list starts over. Each step moves the mark by the
frame's offsets and draws the map again over where it was and is, with
the marks, except under the chief's box; on the screen it stays under the
map's box, a place's label and the small map of the regions too.

When a troop's occupation is set, its ability to work changes, or it
sets off or is given another destination, its mark (not the ring), if it
has one, goes; a new one, as above, takes its place at the end of the
marks when the troop is on the move and near, or its place is in sight.

## The prospectors' route

For troop 3, "Move troop" takes the three words at state `0x11D3` as the
route so far (up to the first that is 0), the chief says phrase `0x55`
instead, and the commands are the list at `0x2136`: ADD A DESTINATION
(`0x80C7`, greyed when the route is empty or full, and doing nothing),
GIVE NEW DESTINATIONS (`0x80D9`), Done (`0x8214`) and Cancel. The small
map opens as above; the troop's way is drawn from its point through each
place of the route in turn.

- A click on a place that is a sietch (kind below `0x20`), or whose byte
  `0x0A` has bit 3, adds it to the route; a full route starts over first.
  The small map and the commands are drawn again. With three places, after
  `0x32` ticks, it is as Done.
- GIVE NEW DESTINATIONS empties the route and draws again.
- Done keeps the route at `0x11D3` (all three words). With none, as
  Cancel. Otherwise the chief answers as for a place clicked, about the
  route's first place (the situation always `0x0B`, never "where it is").
  How the troop then goes is in [moves.md](moves.md#the-prospectors-route).
