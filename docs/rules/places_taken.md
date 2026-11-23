# Places taken

A troop of Paul's that arrives at a place where no other troop is (the
first of the place's chain), a fort not guarded or a place where fighting
is known (see [moves.md](moves.md#arriving)), takes it. The words
of a place's record used here: byte 0 (its low four bits the place's
name), byte 8 its kind (from `0x28` a fort, below that a sietch), byte 9
its first troop, byte `0x0A` its flags, byte `0x0B` a day, byte `0x12`
its spice and byte `0x19` its equipment kept. For troops see
[moves.md](moves.md).

Unless the place is the one Paul is at, the chief's message 7 is left
for the place (see [work.md](work.md#the-chiefs-messages)); not when the
place is taken by the green spreading (see [work.md](work.md)).

## A sietch

Bit 1 of the place's byte `0x0A` (what is inside is known) is cleared,
and Paul's troops there are glad of it (below).

## A fort

1. The land around the fort, 5 points about, is looked over (below).
2. The fort's byte `0x0B` becomes the day (the clock word `0x02` shifted
   right by 4) plus 2.
3. Paul's charisma grows by 4 (below), then every troop of his takes
   heart once more (spirits up by 1).
4. The fort's byte `0x0A` gets bit 3: it is Paul's now.
5. The word at `0x115A` takes `0x8000` turned left by the place's name
   (the low four bits of byte 0), for the regions of the small map.
6. Paul's troops there are glad of it (below).
7. Then each Harkonnen troop there (byte 3 from `0x80` and byte `0x10`
   bit 7) in the chain's order: it leaves the chain, loses bit 7 of byte
   `0x10` and joins the chain again. When its turn there is 8 or more it
   is sent away (below). Otherwise it stays as Fremen not yet Paul's:
   byte 3 `0xA0`; from a first draw of the first generator with mask
   `0x0F7F`, byte `0x1A` takes the low byte plus `0x64` and byte `0x15`
   the high byte plus `0x14`; from a second with mask `0x1F1F`, byte
   `0x16` the low byte plus `0x0A` and byte `0x17` the high byte plus
   `0x0A`; byte `0x19` becomes 0 and byte `0x10` loses bit 4. The next
   troop looked at is the one this troop names after all this, so a
   troop that joined at the end ends the round.

## Then, for both

**Prisoners.** One Harkonnen troop is kept when the state's word 0 has
neither bit 0 nor bit 1, otherwise none. The Harkonnen troops there are
looked at in the chain's order: those up to that number become
prisoners (byte 3 `0xAC`, byte `0x10` bit 4, byte `0x1A` 0, word `0x0C`
0, byte `0x19` 0), the others are sent away. This is done again until no
more Harkonnen troops than are kept are left.

**The last forts.** The forts still the Harkonnens' (kind from `0x28`
without bit 3 of byte `0x0A`) are counted, and the spice they bring is
kept at word `0xA8`: the sum of their bytes `0x12` each shifted right by
3, plus a number from 0 to that sum shifted right by 4, from the third
generator. When one such fort or none is left: byte `0xC2` becomes 1,
the bytes at `0xFF7` and `0x1007` lose bit 1, and when the sum of all
places' bytes `0x19` (kept to a byte) is 10 or more, the place taken
gets that sum less 10 more in its byte `0x19`.

When the map is showing and the place is not hidden, the map is drawn
again.

## Glad troops

Each troop of Paul's at the place (byte 3 below `0x80`) in the chain's
order:

- troop 3 (the one shown three sietches) loses bit 5 of byte 3 and has
  whether it can work found again (see [orders.md](orders.md));
- a troop with bit 5 of byte 3 gets byte 3 `0x22`;
- any other: its word `0x10` gets bit 10 (and bit 5 at a fort), its
  spirits (byte `0x15`) grow by 4, to 100 at most, its byte `0x17` by 3,
  to `0x5F` at most, and when that changes its high four bits the low
  two bits of byte `0x10` become `10`. Then its occupation is set to 4,
  the army (see [orders.md](orders.md)).

## Sent away

The troop leaves the chain; its word 4 becomes `0xFBC`, byte 3 `0xA0`
and byte `0x1A` 0.

## Charisma

Paul's charisma is the byte at `0x29`, 200 at most. Each time it grows
past a multiple of 4, every troop of Paul's (byte 3 with neither bit 5
nor bit 7) takes heart: its spirits grow by 1, to 100 at most.

## Looking over the land

Around a place's point (its words 2 and 4), the land within a radius r
is marked: every map point there whose bits 4 and 5 are not `01` (a
spice field found) gets `10` in them. Rows beyond latitude `0x5D` north
or south are left alone. Each row of the disc wraps round the planet.

The rows run from r above the place to r − 1 below it, the top and
bottom row of each pair as wide as each other. With c = r, e = r, a
step count k = 0 and a half-width h = 0, the rows are worked from the
outside in:

1. While the rows are still in the caps: take k from e, and while e is
   not negative add one to k and h and take k from e again. Mark the
   pair of rows (top and bottom), h points either side of the middle
   point, then move both rows one in and add c to e. When e is negative
   the caps are done; otherwise c goes down by one, and when it is 0 the
   caps are done too; otherwise k and h grow by one and this step
   starts again.
2. Then k grows by one and c goes down by one, and for each pair of rows
   in the middle: mark them, move both in, add c to e (as 16-bit words).
   When that addition did not carry, c goes down by one and, unless it
   is now 0, the next pair follows. Otherwise (it carried, or c reached
   0) c goes down by one; when it is below 0 the disc is done; otherwise
   h grows by one, k is taken from e, k grows by one and the next pair
   follows.

A point may be marked more than once. For the two radii used:

- radius 2: four rows, from 2 above to 1 below, each 3 points either
  side;
- radius 5: ten rows, from 5 above to 4 below, with 3, 5, 5, 6, 6, 6, 6,
  5, 5 and 3 points either side.

When a troop is enlisted (see [people.md](people.md)) the land around
its place is looked over with radius 2; when a fort is taken, with
radius 5.

## Not built yet

The regions of the small map changing with the word at `0x115A`.
