# The people of a place

Besides the characters of the story, four characters stand for the troops
of the place Paul is in: 12, the Harkonnens' captain; 13, the smuggler of
a village; 14, the Fremen of a troop that does not work for Paul yet; 15,
the chief of a troop of his. Each time the player's room is set up (see
[game.md](game.md)) they are put where their troops are, and the state
learns about the place.

## Where they stand

The place words (offset 2) of characters 12 to 15 become `0x7F80`
(nowhere). Inside a place (the low byte of the place word at `0x06` is
`0x80`), each troop of the place is looked at, from the place's byte 9,
each troop's byte 1 naming the next (troop n's record is at `0x8AA` +
`0x1B` × (n − 1)):

- A troop on the move (byte 3 bit 5) stands in room 2, or in room 3 in a
  place of kind `0x28` or more.
- Otherwise the Harkonnens' troops (byte `0x10` bit 7) are not shown;
  the others stand in room 2, or in room 1 while state byte `0x2B` is
  set.

A troop standing in the player's room (the low byte of the room word at
`0x04`) puts a character in that room of the place: the captain for a
Harkonnen troop (its byte 3 bit 4 copied to bit 4 of the captain's flags,
the word at `0xEE` cleared, and `0xED` set to `0xFF` with that bit or to
the troop's byte `0x15` without it); a chief for a troop whose byte 3 is
below `0x80`, the troops being counted up to 8 (troop 3's chief is the
prospectors'); otherwise the Fremen. The last troop of each sort found is
the one the character stands for.

In a village (kind `0x21`) the smuggler stands in the room too, and the
smuggler's record of the village (from `0x10D8`, `0x11` bytes each, by its
first byte, the village's first name) is kept at `0x10B4`: `0x1C` gets
its byte 2, the word `0x20` its word `0x0E`, `0x1F` the day (clock / 16)
less its byte `0x10` when that word is not 0 (else 0), `0x1D` its byte 1,
and `0x1E` the day less its byte 3, or 1 the first time (bit 3 of its
byte 2 is then set). `0x9D` and `0x9F` are cleared, and the word `0x11F1`
becomes `0xE8` + (the word at 0 and 7, less state byte `0x1141` until
below it).

### Their names and looks

In the list of commands the people are named by phrase `0x78` + the
character. With several chiefs, the first is `0x87` and each further one
`0x88`, `0x89`, …; from story step 5 the prospectors' chief is `0x8F`
instead. Further chiefs take spots as characters 16 on, after the others
(see [places.md](places.md)).

Each is shown, in the room and in a talk, with the persons' sprites and
portrait of a look:

- The smuggler: its own, the portrait's set (place's byte 0 / 2) + 1.
- The Fremen and a chief (unless the story is at step `0xC8`): by the
  troop's byte 0, the name n: look `0x0E` + n mod 3, set (n / 3, less
  `0x0F` while it is that or more, or `0x11` when n mod 3 is not 0) + 1.
- Character 4 between story steps `0x15` and `0x1F`: look `0x11`,
  `0x35` rows lower.
- The captain once overpowered: look `0x12`.

The spots turn by state byte `0xC5`.

## What the state learns about the place

With the place's record P, kept at `0x11CE`:

- `0x4D` its kind, `0x4E` its second word, `0x4F` its first word, `0x50`
  the byte at `0x1141` + its second word, `0x51` its byte `0x0A`, `0x52`
  its byte `0x12`, `0x54` its byte `0x1B`, `0x55` to `0x5B` its bytes
  `0x14` to `0x1A` (its equipment).
- The fight: for each troop of the place not on the move, its worth
  (below) is added to the word at `0x94` for the Harkonnens or `0x96`
  for Paul's, whose words `0x10` and `0x12` are also or-ed into `0x5C`
  and `0x5E`. Byte `0x9C`: with the larger of the two worths B and the
  smaller S, `0xFC` when S is 0 or B / 256 is S or more, otherwise the
  smaller of 256 B / S halved and `0xFC`; negated (as a byte) when the
  Harkonnens' is the larger.
- Bytes `0x60` to `0x92` are cleared, then each troop of the place, and
  each troop anywhere on its way there (byte 3 bit 6, going to its word
  4, or to its word `0x0C` when byte 3 & 3 is 3), is counted unless on
  the move: with base `0x61`, or `0x7F` on its way, Harkonnens count at
  the base and Paul's one before it, except that a troop of Paul's whose
  byte 3 is `0x80` only counts at `0x90`; then at base + 1 + its job
  (byte 3 & `0x0F`, with the low two bits cleared when they are both
  set), and, when that is below `0x7F`, at `0x71` + its word `0x12` &
  `0x0F`. Then `0x91` = `0x60` + `0x7E` and `0x92` = `0x61` + `0x7F`.
- `0x53`: of the place's seven sorts of equipment, those left after each
  troop of the place takes one of each sort its byte `0x19` names (bit 7
  for the first): bit 7 − i for sort i.
- `0xF7`: when P is not Paul's home (`0x1150`), bit n for each of
  characters 4, 5 and 7 whose place word is P's.
- The nearest places: of all places but P, by the larger of the high byte
  of the longitude's distance and the low byte of the latitude's (the
  whole latitude distance when that is larger), the nearest overall
  (`0xCA`, place at `0xCC`), the nearest shown place of a kind below
  `0x28` (`0xD0`/`0xD2`), the nearest hidden one the story has come far enough
  for (its byte `0x0B` not above the story's step; `0xD6`/`0xD8`), the nearest
  shown fort (`0xDC`/`0xDE`) and hidden fort (`0xE2`/`0xE4`). After each
  place word comes the direction to it, 0 to 7,
  and the word `0x11FD` is `0xDA` + the direction to the hidden sietch.

### A troop's worth

Its keenness k is its byte `0x15`, `0x14` more while `0xFA` is set; for a
troop harvesting spice (job 6) at the player's place `0x1E` more, for one
elsewhere no more than that, for jobs 8 and 9 just 100; otherwise at most
100. While the story is at steps `0x64` to `0x67` it is `0x28` less, at
least 10.

Its worth: (2k as a byte, plus byte `0x17`, at most 255) × its men (byte
`0x1A`) / 16 = v, plus 2v, 4v, 8v and 16v for each of bits 5, 4, 3 and 2
of byte `0x19` (`0xFFFF` on overflow), divided by 256; 1 when that is 0
and it has men.

## Talking to a troop's people

Before a talk with the Fremen or a chief, the state learns about the
troop T (for a chief, the one whose line was chosen: the line's phrase
less `0x87`, the prospectors' line meaning the prospectors' chief):

- `0x2C` its place, `0x2E` its byte 0, `0x30` its byte 3 (the job), `0x2F`
  its job & `0x0F`, the word `0x11F3` `0x18` + that.
- `0x42` the clock less its word `0x0A` (since it does what it does) and
  `0x41` that / 16; the word `0x11F5` is phrase `0x74` when its job has
  bit 4, else `0x70` below 3, `0x71` below 16, `0x72` below 32, and `0x73`
  otherwise (the original then writes the days into that phrase's text).
- For a troop harvesting spice (job 0): `0x4A` its word `0x0E` / `0x42`
  rounded (0 when `0x42` is 0), and its harvest (below) at `0x48`; for
  others `0x48` is 0 and bits 2 and 3 of its word `0x10` are cleared.
- `0x32` and `0x34` its words `0x10` and `0x12`, `0x31` and the word
  `0x11FF` the latter & `0x0F`; `0x36` its keenness; `0x38` to `0x3A` its
  bytes `0x16` to `0x18`, the words `0x11F7` to `0x11FB` `0xD1` + each /
  16; `0x44` and `0x46` its words `0x0C` and `0x0E`, the word `0x11F1`
  `0xE8` + the low byte of the latter; `0x37` its byte `0x16` + job / 4;
  `0x3B` and `0x3C` its bytes `0x19` and `0x1A`; `0x40` the day less its
  byte `0x14`.
- Then what the state learns about its place, as above.

### A harvest

(k + its byte `0x16` & `0xF0`, as a byte) × its men, divided by 4 unless
its byte `0x19` has bit 7; the high byte of that × (its place's byte
`0x12` & `0xF0`, plus 1), / 128, kept to 9 bits, is its new word `0x0C`.
When it changed, bit 3 (up) or bit 2 (down) of its word `0x10` is set in
place of both.

## Working for Paul

When the Fremen agree to "work for me" (see [talking.md](talking.md)),
their troop, if not Paul's yet and not the Harkonnens':

1. counts at `0x28`; when that reaches the byte at `0x1178` the story
   moves on to step `0x4C`;
2. raises Paul's charisma (`0x29`) by 1, at most `0xC8`; each time it
   passes a multiple of 4, the keenness of every troop whose byte 3 has
   neither bit 7 nor bit 5 grows by one, at most 100;
3. gets job 2 (keeping bit 5 of byte 3), its word `0x0A` the clock, its
   words `0x0C` and `0x0E` 0, and its byte `0x14` the day;
4. when its place's byte `0x0B` is 0, that becomes 2 and the land around
   the place is looked over for spice (not built yet).

The room's people are then set up again; for troop 3 its keenness and
`0x36` grow by `0x18`.

### Directions

From P's point to another's: with d the latitude difference and e the
longitude difference (as signed words), both halved when d is outside
−128 to 127; D = (low byte of d) × 256 as a signed word. When |e| ≥ |D|:
0 when e is 0, else 32 D / e plus `0x40` (e positive) or `0xC0`; otherwise
32 e / D, which for a positive D is taken from `0x80`, then negated. The
direction is the top three bits of that byte plus `0x10`.
