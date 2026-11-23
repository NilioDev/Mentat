# The troops' work, hour by hour

Each time the clock moves on (see [timing.md](timing.md)) the state's
word `0x1174` takes the clock, and a new day has come when the high four
bits of its low byte (the day) changed. Unless the story's byte `0xC2`
is 7 or more, the troops then work (for the rest of the hour see
[days.md](days.md)). For the records of troops and places
see [moves.md](moves.md) and [places_taken.md](places_taken.md); a
troop's skills are its bytes `0x16` (spice), `0x17` (army) and `0x18`
(ecology), its men byte `0x1A`.

## The troops in turn

Before the round, when the low byte of the state's word `0x101A` is
`0x80`, its high byte names the place the Harkonnens are going for (place
1 the first). Nothing is busy yet.

Each troop record from the first to the last, in turn:

1. When its word `0x12` has any of bits 4, 5 and 10: a troop on the move
   (byte 3 bit 6) steps on (below). Otherwise, while the state's byte
   `0xFA` is 0 the troop is passed over; when it is not, word `0x12`
   loses bits 4 and 5, and unless it has bit 10 the troop goes on to the
   next step.
2. A troop with fewer than 20 men may join another (below); then it is
   done.
3. A troop whose byte 3 has bit 5 or 7 is passed over; one on the move
   steps on: 4 steps, or 8 when its byte `0x19` has bit 6 (see
   [moves.md](moves.md)), unless it is the troop given orders just then.
4. Otherwise it does the work of its occupation (the low four bits of
   byte 3) at its place, below: 0 spice mining, 1 prospecting, 4 the army,
   5 espionage and 6 attacking (see [fights.md](fights.md)), 8 ecology, 9
   a wind-trap, 10 ecology waiting for water. The others do nothing
   here. Then it may forget (below).

After the round the state's byte `0xFA` is `0xFF` when a troop worked at
ecology, otherwise 0.

### Joining another troop

Only a troop whose byte 3 has none of bits 0, 1, 5, 6 and 7, that is not
a Harkonnen troop and not troop 3. Of Paul's other troops at its place
(byte 3 below `0x80` and without bits 5 and 7, not troop 3), it joins the
one with the fewest men that has room for its own (at most 255 together;
the last of those with as few). That troop gets its men, the bits of
equipment of both (byte `0x19`), and bit 9 of word `0x12`; the troop that
joined keeps the bits both had and is sent away (see
[places_taken.md](places_taken.md#sent-away)).

### Forgetting

When the clock's low six bits are all 0, a troop loses one point (not
below 0) of the skills of the specialities it has worked at (bits 13 to
15 of word `0x12`, for spice, the army and ecology) that its own
speciality (bits 2 and 3 of byte 3) does not keep: a spice troop the army
and ecology skills, an army troop the ecology skill, the others none.

### Skills

A skill grows by some amount to `0x5F` at most; when that changes its
high four bits, the low two bits of the troop's byte `0x10` become 1 for
spice, 2 for the army, 3 for ecology.

### Keenness

How keen a troop is: its spirits (byte `0x15`), plus 20 while the state's
byte `0xFA` is not 0. A fighting troop (occupation 6) at Paul's place
gets 30 more, and is kept to 100; at another place it is not kept. A
troop at ecology (8 or 9) is 100. Any other is kept to 100. While the
story's step (byte `0x2A`) is from `0x64` to `0x67`, 40 is taken off,
leaving at least 10.

## A new day at work

For mining, the army and ecology after its first hour, the work starts
with what the new day brings, when one has come:

1. A fort Paul took (byte `0x0A` bit 3): unless the day is still before
   its byte `0x0B` (the difference, as a byte, `0xFE` or more), it becomes
   a sietch: it loses bit 3, its kind keeps only its low three bits, the
   state's count of sietches (`0x27`) grows by one, and its byte `0x0B`
   becomes 5. The twelve records at `0xFD8` (16 bytes each) whose word 2
   names the place (its number in the high byte, `0x80` in the low one)
   get its new kind in the high byte of their word 0 and 1 or 2 in the low
   one (1 when it was 1). When word 6 of the state names it, word 4 is
   changed the same way, and bytes `0x0B` and 8 take its two bytes. Every
   troop there with bit 5 of word `0x10` loses it and gets bit 12 of word
   `0x12`; then the Harkonnen troops there not Paul's are sent away, again
   until none are left.
   While the fort's day has not come, nothing more of the new day
   happens.
2. A sietch (kind below `0x20`) whose byte `0x0B` is below 12 and whose
   byte `0x0A` lacks bit 0: byte `0x0B` grows by one, and the land within
   that many points around is looked over (see
   [places_taken.md](places_taken.md#looking-over-the-land)).
3. When more than 8 days have passed since the troop's byte `0x14`, its
   spirits go down by 1 (see [moves.md](moves.md), setting off).
4. When the troop is the first of its place's chain: Paul's troops there
   are looked at, unless the place is Paul's or its kind is `0x21` or
   more; of those with spirits below 40 and none of bits 0 to 3 and 5 of
   byte 3, some lack bit 7 of byte `0x12` and some have it. When there
   are both, each of Paul's troops there with spirits below 40 and none of
   bits 0, 1, 3 and 5 of byte 3 gets bit 4 of byte 3 and of byte `0x12`
   and message `0x0302` is left for the place (below).

## Spice mining (0)

After the new day: when word `0x10` has bit 9 (stopped), the troop goes
on only in the hour whose low four bits are those of its number (byte
0); in the others its word `0x0C` becomes 0 and byte 3 gets bit 4. In
that hour bit 9 is cleared and, when it can work (see
[orders.md](orders.md)), it mines without what may befall it.

Otherwise, when it cannot work, words `0x0C` and `0x0E` become 0 and
byte 3 gets bit 4. When it can, what may befall it (below), then it
mines:

1. Word `0x10` gets bit 8.
2. The spice it brings: its keenness plus the high four bits of its spice
   skill (as a byte), times its men; a quarter of that without a
   harvester (byte `0x19` bit 7). The high byte of that times (the
   place's byte `0x12` with its low four bits cleared, plus 1), shifted
   right by 7, kept to nine bits. Word `0x0C` takes it; when it grew, the
   bits 2 and 3 of byte `0x10` become `10`, when it fell `01`.
3. Word `0x0E` grows by it; when that changed any of its bits from 7 up,
   the spice skill grows by 1.
4. A tenth of it, with what was left over the last time (kept outside
   the state), goes to the state's word `0xA0`.
5. With the place's byte `0x13` it is divided by the place's byte `0x11`;
   the rest is kept in byte `0x13`, and the place's byte `0x12` loses the
   quotient, not below 0.

### What may befall a harvester

Only while the state's byte `0x0A` has bit 0, for a troop with a
harvester, in the hour of its number:

- From story step `0x35`, for a troop with bit 6 of word `0x12`: the
  state's word 0 is turned left three times; when its low three bits are
  then all 0, the Harkonnens see it: word `0x10` gets bits 9 and 15, byte
  3 bit 4, and the place's byte `0x0A` bit 2; message `0x0F03` is left
  for the place.
- Then a number from the second generator: when its high byte is not
  above the byte at `0x1141` plus the place's byte 0, word `0x10` gets bit
  14, and unless the troop's byte `0x19` has bit 6, by the low two bits of
  that high byte: 1, word `0x10` gets bit 13 and the troop loses 2 men
  unless it has 2 or fewer; 2, it stops (word `0x10` bit 9, byte 3 bit
  4); 3, a worm takes the harvester: word `0x10` gets bit 12, byte `0x19`
  loses bit 7 and the place's byte `0x14` one; message `0x0F06` is left
  for the place.

## Prospecting (1)

When the place's byte `0x0A` has bit 1, nothing happens. When it lacks
bit 6 (not prospected yet) and the troop can work: word `0x0C` is the
time it needs, worked out the first time as the place's byte `0x11`
times 16 divided by the troop's spirits plus spice skill (as a byte).
While more hours than that have not passed since word `0x0A`, word `0x0E`
shows how far it is in hundredths (but in the first hour). Once they
have: the spice skill grows by 2 and the place's byte `0x0A` gets bit 6.

A prospected place: the troop finds again whether it can work (it
cannot), word `0x0E` becomes 100, and unless its orders are being shown,
it sets off for its own place when the state's word `0x11D3` is not 0;
otherwise, when it could work before, message `0x0F0E` is left for the
place.

## The army (4)

After the new day, word `0x10` loses bit 9.

At a place the Harkonnens watch (byte `0x0A` bit 2): word `0x0E` counts
down from `0xFF00` plus `0x40` less the army skill (as a byte), one an
hour, in its low byte. When that goes below 0, the watch is over: the
place loses bit 2, each of Paul's troops there with bit 6 of word `0x12`
loses it, and when there was one, this troop's word `0x12` gets bit 8;
word `0x0E` becomes 0.

Elsewhere word `0x0C` counts down; when it goes below 0 the troop has a
lesson. At the place the Harkonnens go for its word `0x10` gets bit 11
and the level is `0xA0`; otherwise the level is the army skills of the
troops there with occupation 4 (Paul's), summed and divided by how many
there are (just the sum when none). Twice what the level is above the
troop's army skill, plus its keenness (at least 30), divides 200 with a
fremkit (byte `0x19` bit 2 or 3), 250 with bit 4, 300 with bit 5, 400
otherwise, and word `0x0C` takes that. Then the army skill grows by 1.

## Ecology (8)

Not while byte 3 has bit 4, nor when the troop cannot work. Ecology is
busy this hour; word `0x10` gets bit 8.

The first time at a place (its byte `0x0A` without bit 0): the place gets
bit 0, its spice (byte `0x12`) becomes 0, its word `0x0C` takes its
longitude and word `0x0E` the low byte of its latitude, byte `0x0B`
becomes 4, and the green spreads (below).

After that, after the new day: the place's byte `0x0F` grows by a quarter
of the troop's ecology skill (at least 1). When that passes 255: the
ecology skill grows by 1. With less than 12 in the place's byte `0x1B`
(its water) that becomes 0 and the troop's byte 3 gets bit 4; otherwise
the place loses 12 of it, its byte `0x0B` grows by one up to 12, its byte
`0x0E` (taken as a signed byte) goes 2 further north unless that would
pass −82, and the green spreads.

### The green spreading

When the map is showing and the place is not hidden, it is drawn again.
Around the place's word `0x0C` and byte `0x0E` (as a signed latitude),
within its byte `0x0B` points, on rows within 86 of the equator, each map
point as the disc walks it (see
[places_taken.md](places_taken.md#looking-over-the-land)), but those
marked `01` in bits 4 and 5:

- a point where a place stands (bit 6): the place whose word 6 names the
  point gets 0 spice; when it is a fort not Paul's, not one of the first
  two, it is shown and taken (see [places_taken.md](places_taken.md));
- the point gets `10` in bits 4 and 5; when its bits 1 to 3 make less than
  8, a byte starting at `0x44` for each spreading is turned left, and when
  the bit that comes round is set the point gets `01` instead.

## A wind-trap (9)

While the place's byte `0x0A` lacks bit 5: the troop's spirits grow by 1
to 100; twice its keenness (as a byte) plus its spice skill, at most 255,
times its men, shifted right by 12, is added to the place's byte `0x1B`.
When that passes 255 the wind-trap stands: the place gets bit 5 of byte
`0x0A` and bit 3 of its kind, and byte `0x1B` becomes 5. Once it stands,
the troop takes its byte 3 without bits 0 and 1 as its occupation (see
[orders.md](orders.md)).

## Ecology waiting for water (10)

While the place's byte `0x1A` is 0, the state's byte `0xEC` grows by one
an hour; when it comes round to 0, the place's byte `0x1A` becomes 16.
Once that is not 0, the troop's occupation becomes ecology (8).

## The chiefs' messages

A message (a word: the message in the low byte, `0x0F` or another
number in the high byte) is left for a place only while the state's
byte `0x0A` has bit 0. They wait at `0x1190`: a count, then up to ten
of the message word and the place's record word each. A message already
waiting for the same place is not left again. With ten waiting, the
oldest goes first: the count goes down (and the byte before it becomes 0
when that leaves none), the others move up and the last four bytes
become 0.

Those left here: `0x0302` when the troops low in spirits stop working,
`0x0F03` when the Harkonnens see a harvester, `0x0F06` when a worm takes
one, `0x0F0E` when a troop able to work finishes prospecting and does not
move on, `0x0F07` when a place is taken (see
[places_taken.md](places_taken.md)) and `0x0F0A` when the Harkonnens'
palace falls (see [fights.md](fights.md)). How they are shown is in
[messages.md](messages.md).

## Not built yet

What the original
does when a fort is taken in the middle of the green spreading (its walk
then goes on as a looking over around the fort). The daily accounts and
the Harkonnens' and smugglers' hours are in [days.md](days.md).
