# Fights

Paul's troops attack a place (occupation 5) and fight there (occupation
6) as part of their hourly work (see [work.md](work.md)). For the records
see [moves.md](moves.md) and [places_taken.md](places_taken.md).

## What a troop is worth

Twice its keenness (see [work.md](work.md#keenness)), as a byte, plus
its army skill, at most 255, times its men, shifted right by 4. Then for
each of bits 5, 4, 3 and 2 of its byte `0x19` (its weapons), that value
doubled once more for each bit looked at (twice, four, eight and sixteen
times the shifted value) is added when the bit is set, stopping at
`0xFFFF`. The worth is the high byte of the total, but at least 1 for a
troop with men.

## The forces at a place

The troops there, but those whose byte 3 has bit 5: the Harkonnen
troops' worth is summed at word `0x94`, Paul's at word `0x96`, and
Paul's troops' words `0x10` and `0x12` are put together (bit by bit) at
words `0x5C` and `0x5E`. The fight leans (byte `0x9C`): with the
stronger side's sum L and the weaker's S, L × 256 / S halved, at most
`0xFC` (`0xFC` when S is 0 or L / 256 is at least S); when the
Harkonnens are the stronger, 256 less that. It is Paul's chance, out of
256, to strike.

## Espionage (5)

With the hours since the troop's word `0x0A` and r = (`0x50` less its
army skill, as a signed byte) halved:

- Until its byte `0x10` has bit 6 (ready), once more than r / 2 hours
  have passed: when its word `0x0C` is 0, the troops at the place whose
  byte `0x10` has bit 4 (still) lose it and are counted; with none, the
  troop is ready; otherwise word `0x0C` takes the count. Then, when not
  ready yet and more than r hours have passed, the forces are weighed,
  word `0x0E` takes the Harkonnens' worth divided by word `0x0C`, and
  the troop is ready.
- A troop that is not the first of the place's chain, once more than
  half its army skill in hours has passed, flees (below) when a number
  from the second generator, kept to its low six bits, is at least its
  army skill.

## Attacking (6)

The state's byte `0x11BC` gets bit 0.

At the Harkonnens' palace (place 2): the story's byte `0xC2` grows by
one, every troop there takes byte 3 4, the Harkonnens there not Paul's
are sent away (again until none are left), every map point marked
`11` in bits 4 and 5 loses bit 4, and message `0x0F0A` is left for the
palace (see [work.md](work.md#the-chiefs-messages)).

Elsewhere the forces are weighed. With no Harkonnens' worth left the
place is taken (see [places_taken.md](places_taken.md)). Otherwise the
low byte of a number from the second generator decides: below the lean,
the troop strikes; otherwise it is struck.

### Striking

The troop's worth, divided by the number of Harkonnen troops there not
fled (byte 3 without bit 5), plus 1 as a byte (255 when that comes round
to 0), is the blow. Each Harkonnen troop there loses men: the blow times
(255 less twice its army skill, as a byte), shifted right by 8 (255 when
that passes `0xFFFF`), but no more than it has. The striking troop's
word `0x0C` grows by all the men they lost. A Harkonnen troop left with
none gets bit 5 of byte 3 and bit 4 of byte `0x10`; one time in four
(the first generator with mask 3 giving 0) it also loses bit 2 of byte
`0x19` and its equipment is counted one less at the place. When no
Harkonnen troop there has men left, the place is taken.

### Being struck

The Harkonnens' worth, divided by the state's byte `0x60` (Paul's troops
at the place the state last learned), is the blow; the troop loses men
as above, and its word `0x0E` grows by them. With none left it gets 30
plus a number from the first generator with mask `0x7F` and flees. When
then none of Paul's troops there fight any more (occupation 6, byte 3
without bit 5), the place is lost (below).

## Fleeing

A troop of Paul's (not a Harkonnen one, without bit 5 of byte 3) gets
bit 5 of byte 3, its byte `0x19` becomes 0 and word `0x0A` the clock, and
Paul's charisma (`0x29`) falls by 4, to 1 at least. For each multiple of
4 it fell past, every troop of Paul's (byte 3 with neither bit 5 nor bit
7) loses one point of spirits more (see [moves.md](moves.md)).

## A place lost

The place's byte `0x0A` loses bit 1. When it is the place Paul is at, the
game is lost (not built yet: the end shown). Otherwise:

- a sietch becomes a Harkonnen fort: its kind becomes `0x28` plus its low
  three bits, the count of sietches (`0x27`) goes down by one, and the
  first nine records at `0xFD8` that name it (see [work.md](work.md))
  get its kind in the high byte of their word 0 and 3 in the low one;
- the Harkonnen troops there get bit 4 of byte `0x10`, the others bit 5
  of byte 3;
- the land within 5 points around is marked `11` in bits 4 and 5 (as
  looking over, see [places_taken.md](places_taken.md#looking-over-the-land));
- the place loses bits 0 and 3 of byte `0x0A`, and the map is drawn
  again.

## Not built yet

The Harkonnens attacking Paul's place, and the end of the game when the
place Paul is at is lost.

## At Paul's place: the view and "Massive attack"

While Paul's place is at war (byte `0x2B`) and he is inside it, the view
is the battle seen from the place (see [attack.md](attack.md#at-war)),
the arrows give way to the zones at `0x1D72` (when byte `0x11CB` is set)
or `0x1D1E`, and the first room offers "Massive attack" and "Fight for a
whole day" (see [desert.md](desert.md#waiting)). When the fight starts or
ends with the hour, the view is drawn again, unless hours are being
passed at once.

"Massive attack" fights at once:

1. The forces are weighed; words `0x98` and `0x9A` keep the Harkonnens'
   and Paul's worth.
2. A number from the second generator, as a byte, against how the fight
   leans: below it every blow of the attack is Paul's, otherwise every
   blow is the Harkonnens'.
3. Up to 16 rounds: the forces are weighed again, then each of Paul's
   fighting troops there (byte 3 is 6) strikes (or takes the place when
   no Harkonnen worth is left) or is struck, as in an hour's fight. The
   rounds stop once either side's worth is 0.
4. The forces are weighed; `0x98` and `0x9A` become what each side's
   worth fell by, and the place is looked at as at the end of an hour
   (see [days.md](days.md#pauls-place-at-war)).

Then 20 blasts light the horizon, each 0x28 ticks (or 0x2A) after the
one before: the battle's flash counter starts again from 0x0B, or 0x11,
by the first generator's bits 0 and 9.
