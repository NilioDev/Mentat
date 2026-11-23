# Troops on the move

A troop is sent somewhere from its orders (see [orders.md](orders.md)).
It leaves its place's chain, walks across the map a few steps an hour,
and joins the chain of the place it reaches. The words of a troop's
record used here: byte 1 the next troop in its place's chain, byte 2 its
turn at the place, byte 3 its occupation and flags (bit 6 on the move),
word 4 its place or, on the move, its destination, words 6 and 8 its own
point (longitude and latitude), byte `0x15` its spirits, byte `0x19` the
equipment it holds.

## The chain of a place

A place's byte 9 is its first troop, and each troop's byte 1 the next
one; 0 ends the chain.

- **Leaving** a place: unless the troop is on the move, it is taken out
  of the chain of its word 4 (the first troop, or the byte 1 of the troop
  before it, takes its byte 1). Its byte 1 becomes 0.
- **Joining** a place: into an empty chain it comes first, with turn 1,
  or 9 for a Harkonnen troop (byte `0x10` bit 7). Otherwise it goes at
  the end of the chain and takes the first of the turns 1 to 30 that no
  troop there has (from 9 for a Harkonnen troop), or 30 when all are
  taken.

## Setting off

For a troop already on the move only the destination changes: word 4
takes it, byte 3 loses bits 0 and 1 when it has both, and its mark is
made again.

Otherwise:

1. Those of Paul's troops (byte 3 below `0x80`) at its place whose word
   `0x12` has bit 4 have their occupation set again (see
   [orders.md](orders.md)), even though they have it.
2. The troop leaves its place's chain.
3. When its byte 3 is 6 (fighting) and the destination's kind (byte 8)
   is below `0x28`: when the destination's byte `0x0A` has bit 1 nothing
   more happens, the troop in no chain; otherwise its spirits go down by
   3, not below 0. Spirits below 5 become 4, byte 3 gets bit 4 and word
   `0x12` bit 5.
4. Word 4 takes the destination, byte 3 gets bit 6, byte 2 becomes 0.
5. Each piece of equipment it holds (bits 7 to 1 of byte `0x19`, for
   the place's counts from byte `0x14` on) is counted one less at the
   place it left, not below 0.
6. Its mark is made again; then, unless its byte `0x10` has bit 4, it
   makes up to 7 steps at once (below), its mark following them.

## A step

With the troop's point (λ, φ) and the destination's (λ', φ'): the size
of a point at latitude |φ| (see [map.md](map.md)) is s; the distance
across is a = |λ' − λ| / s (the difference taken as a signed 16-bit
number), and down d = |φ' − φ|.

- When a ≥ d: the troop arrives when a < 7. Otherwise λ moves s towards
  λ', and φ one towards φ' (none when d is 0) only when the bit that comes
  round as the state's word 0 is turned one bit to the left is set.
- When d > a: the troop arrives when d < 7. Otherwise φ moves one
  towards φ'. When a is 0, λ takes λ'; otherwise λ moves s towards λ'
  only when the bit that comes round as word 0 turns is set.

The original draws word 0 afresh from the second generator over and over
in its main loop, so each hour's steps start from a new value.

## Each hour

When the clock moves on, each troop on the move but the one given orders
just then makes 4 steps, or 8 when its byte `0x19` has bit 6. Troops
whose word `0x12` has none of bits 4, 5 and 10 are passed over when their
byte 3 has bit 5 or 7. A troop still on its way after its steps has its
mark follow it (below).

## Arriving

The troop's point becomes the destination's. When the destination's
byte `0x0A` lacks bit 1, its kind is from `0x28` and its byte `0x0A` has
bit 3 (guarded), a troop attacking or fighting (occupation 5 or 6) has
its occupation set to its byte 3 with bits 0 and 1 cleared, and it
settles (below); nothing more happens.

Otherwise it settles, then:

- attacking (5): a hidden place (byte `0x0A` bit 7) is shown; that is all;
- otherwise, alone at the place (the first of its chain): it takes the
  place (see [places_taken.md](places_taken.md));
- otherwise, each of Paul's troops there (byte 3 below `0x80`, without
  bit 5) gets ready to fight: a prospector gets bit 4 of byte 3, any other
  not fighting yet has its occupation set to 6.

Settling: the troop joins the place's chain and loses bit 6 of byte 3.
When it is not a Harkonnen troop and its turn there is above 8, it leaves
the chain again, the first troop of the chain that is not a Harkonnen
one and has bit 5 of byte 3 is sent away (word 4 `0xFBC`, byte 3 `0xA0`,
byte `0x1A` 0, out of the chain), and the troop joins again. The
equipment it holds is counted one more at the place. When byte 3 is
below `0x80` its occupation is set again.

## The marks of troops on the move

A troop on the move is marked at its own point as the map shows it,
while that falls within −16 to `0x148` across and −16 to `0xA0` down
(both ends left out). The mark's frames are the word at `0x18BF` plus
twice the way: with the destination's longitude less the troop's, its
high byte taken as a signed number, across, and the latitudes'
difference down, the way is 1 (right) or 3 (left) when the first is at
least as long as the second, 2 (down) or 0 (up) otherwise, plus four
times the occupation's kind (bits 2 and 3 of byte 3).

After a troop's steps: a troop near and marked has its mark moved to its
new point, the picture as it was; one near and not marked gets a mark at
the end of the marks; a marked one that is no longer near loses its
mark.

## The small map

When the small map of the regions is open for a troop to be shown where
to go (see [spice_map.md](spice_map.md)), the troop's way is drawn in it.

## Not built yet

Fighting troops leaving their fight, the second destination of a troop
whose byte 3 has bits 0 and 1, the troop shown three sietches, and what the original does when a troop arrives where Paul is.
