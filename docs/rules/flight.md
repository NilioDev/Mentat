# Flying the ornithopter

From the cockpit (see [cockpit.md](cockpit.md)) a click on a place sets
off for it: the ornithopter takes off from the place's entrance, the
desert goes by in the videos with a small map in the top right corner,
and at the place the ornithopter comes down in front of its entrance.
This page covers flying to a place. Flying off in a direction, what
happens on the way (places seen ahead, the Harkonnens' zones) and flying
from the desert are not covered yet.

## Choosing the place

A click on the cockpit's map looks at what the mouse points at there:

- nothing (off the map): the click does nothing;
- a place: the place Paul is in does nothing, unless he is flying
  already; any other is the destination;
- the desert: a flight in a direction (not covered yet).

The cockpit's voice (character `0x19`) then says the place's name:
phrase `0x2BC` + ((first word − 1) × 16 + second word, kept to 8 bits).
Nine times the name stays 20 ticks, then the line is cleared as when the
mouse points at nothing (the title starts being typed again) for 10
ticks, and the name is written again. Then the game waits for the voice
to end.

## Setting course

The heading is a byte, 256 to a turn: 0 north, then clockwise. Toward a
point from where Paul is:

1. *dy* = its latitude − his, *dx* = its longitude − his (16 bits, so
   the shorter way round). When *dy* is outside −128 … 127 both are
   halved (rounded down).
2. *dy* is scaled: its low byte becomes the high byte of a 16-bit signed
   number, the low byte 0.
3. When |*dx*| ≥ |*dy*|: 0x20 × *dy* / *dx* (rounded towards 0), plus
   `0x40`, or plus `0xC0` when *dx* is below 0. Otherwise 0x20 × *dx* /
   *dy*, less `0x80` when *dy* is 0 or more, then negated. When both are 0
   the heading stays as it was.

Setting course for a place takes this heading toward the place's point,
and sets the latitude's left-over (below) to `0x80`.

## Taking off

When Paul is not flying yet:

1. The music changes to the flight's (moment 6 of the table in
   [game.md](game.md)), waiting for the flight's first picture.
2. The room is drawn again without its ornithopters and kept.
3. Effect 6 starts. For steps 0 to 32, every 20 ticks: the kept room,
   then the ornithopters from `ORNYTK.HSQ` (resource `0x2A`): the one
   leaving at the first place (see [places.md](places.md)) at that step,
   the others parked; the scene is then shown. From step 15 the one
   leaving rises: with *n* = step − 14, *x* − 5*n*, *y* − ⌊*n*² / 2⌋. Its
   shadow is sprite 2 + (step − 15, kept within 0 … 5) and its wings
   sprite 8 + (step, at most 14). At steps 13 and 26, before drawing,
   the effect's repeating part ends and its next part follows.
4. The first step (below) is taken at once; the flight's state begins:
   the place's kind byte (state offset 8) is `0xFF`, the room and place
   words hold the longitude and latitude, and the place the player is in
   (offset `0x114E`) is 0. The place left (offset `0x1150`) keeps one
   ornithopter fewer.

The clock does not run by itself while flying: only the steps move it.

## The flight's screen

- The commands: "SKIP TO DESTINATION" (data offset `0x21FC`) and "CHANGE
  DESTINATION" (`0x21F8`); the arrows from data offset `0x1D1E` (none).
- The corner: in the third buffer, around Paul's place,
  - the small map (see [cockpit.md](cockpit.md)) in the rectangle at data
    offset `0x148A`, (`0xCC`, 4) to (`0x13C`, `0x3C`);
  - its frame: four rings around it, from the inside colours `0xFC`,
    `0xFA`, `0xF8`, `0xF6`; ring *k* (from 1) runs from (left − *k*, top −
    *k*) to (right + *k* − 1, bottom + *k* − 1), both ends drawn;
  - the places as on the cockpit's map;
  - sprite `0x2E` of the panel's picture file at the destination's point,
    one pixel up and left;
  - the trail (below).
- The video: slot 2 (`MNT1.HNM`) from its start. Each frame is drawn on
  the back buffer; the corner's part of the third buffer, the rectangle
  at data offset `0x1492`, (`0xC8`, 0) to (`0x140`, `0x40`), is copied
  over it, and the scene is copied to the screen.
- The sky's colours stay as they were when the flight began, also when
  the hour changes.

## Steps

Every 768 ticks (the first at once):

1. One more step is counted; every 16 steps the clock moves on a part of
   the day and its count of ticks starts again.
2. Paul moves (below). The characters who go with him (byte `0x0F` bit 6)
   and stood where he was move with him.
3. While the corner is shown, sprite `0x2F` is drawn at the last point of
   the trail (one pixel up and left, clipped to the map), and Paul's new
   place is added to the trail. The trail keeps the last 276 places.
4. When Paul's map point (see [map.md](map.md)) is the destination's
   (record offset 6), the place is reached (below).
5. While the corner is shown: when it was marked to be drawn again, it is
   drawn again around Paul with the whole trail (each point whose sprite
   corner falls within (`0xCC`, 4) to (`0x13A`, `0x3A`)). Then, when
   Paul's place is on the map: outside (`0xD6`, `0x0A`) to (`0x132`,
   `0x36`) the corner is marked to be drawn again at the next step;
   inside it sprite `0x30` marks him, one pixel up and left.
6. The next video is chosen by what lies ahead (below).

### Moving

1. The heading is set toward the destination from where Paul is.
2. The heading's direction is (*across*, *down*): with *t* = heading +
   `0x20`, when *t*'s low seven bits are `0x40` or more: *across* = 32
   (−32 when *t*'s bit 7 is set) and *down* = heading − `0x40` (negated
   after taking `0x80` off when bit 7 is set), as a signed byte;
   otherwise *down* = −32 (32 when bit 7 is set) and *across* = the
   heading (negated after taking `0x80` off when bit 7 is set).
3. With *s* the longitude units a map point covers at Paul's latitude:
   along = *s* × *across* / 32 and up = *down* × *s* / 32 (rounded towards
   0).
4. *total* = |up| + the latitude's left-over. When *total* is `0x200` or
   more, along becomes along × 256 / *total* and *total* `0x100`. The
   left-over keeps *total*'s low byte; the latitude moves by its high
   byte, the way of up's sign; the longitude moves by along.
5. When the latitude is then below −`0x60` or `0x60` or more, the heading
   turns half round and the longitude moves by `0x8000`.

### The videos

Slots 2 and 5 show dunes, 3 and 4 rocks. After a step, the place 6 steps
ahead and the place 7 steps ahead are looked at (moving as above, the
heading kept as it then is, the latitude's left-over put back); their
map bytes are added, kept to 8 bits and halved. When that byte's low four
bits are 8 or more the ground ahead is rocky. The next video is then:

- rocky: 3 after slot 2 or 5, otherwise 4;
- dunes: 2 after slot 2, 5 after 3 or 4, otherwise 2.

When a video ends, the next one starts; when it is the same one, it
starts again.

## Commands

- "SKIP TO DESTINATION": the video stops and up to 200 steps are taken at
  once (with the clock), stopping at the destination; after 200 the place
  is reached anyway. There is no approach.
- "CHANGE DESTINATION": the video stops and the cockpit opens around
  Paul's place; the flight waits meanwhile. "Cancel" goes back to the
  flight, and a place chosen there sets course for it without taking off.
  Either way the flight's screen is shown again from its first video.

## Reaching the place

1. Unless the place was reached flying, the video stops.
2. The state's byte `0xC5` takes a number from the second generator (its
   low byte).
3. The place keeps one more ornithopter, and Paul enters it: its first
   room, the place he is in and the place last entered are this place.
   A place that was hidden is found (bit 7 of byte `0x0A` cleared, byte
   `0x0B` cleared, and for a sietch the count of sietches found, state
   offset `0x27`, goes up); other kinds than sietches are marked visited
   (byte `0x0A` bit 4). The characters who go with Paul come too.
4. When the place was reached flying, by the place's kind (byte 8):
   - a sietch (below `0x20`) or the palace (`0x20`): the corner goes, and
     the flight's videos go on over dunes until slot 2's frame `0x3C` (a
     sietch) or `0x16` (the palace); then the place's own video plays
     through: slot 6 (`SIET.HNM`) or 7 (`PALACE.HNM`);
   - a fortress (`0x28` to `0x2F`): slot 9 (`FORT.HNM`) plays through;
   - otherwise the ornithopter lands: as taking off, with the room drawn
     without its ornithopters, effect 7, from step 31 down to 0, coming
     from the right (*x* + 5*n*); the effect's repeating part ends at step
     13.
5. The room is drawn and dissolves in.
