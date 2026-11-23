# The chiefs' messages

How the messages left for Paul (see
[work.md](work.md#the-chiefs-messages)) reach him: in a room, as a
vision.

## When one comes

Only while Paul is inside a room with nothing else going on: no talk, no
map, globe, cockpit, flight or order box open, and no notice shown. The
story's step (byte `0x2A`) must be past `0x14` and a message must be
waiting (the count at `0x1190` not 0).

The time is counted in ticks from when the room was last shown (on coming
in, and again after a vision). Before `0x32` ticks nothing happens. Then,
unless Paul's place is at war (byte `0x2B`, see
[days.md](days.md#pauls-place-at-war)), the room blurs twice as the
vision nears: up to `0x96` ticks it is drawn again through the fine
mosaic (transition `0x28`), from `0xFA` to `0x15E` through the short one
(transition `0x26`, see [display.md](display.md)). From `0x1C2` ticks,
and at once at war, the first message waiting is shown.

## Showing a message

The message word's low byte says what it is about, its high byte who
speaks. A place word that does not name a place record (from `0x100`,
less than `0x7AA` on) is cleared in the queue and taken as no place.
Byte `0xEA` takes the low byte (and byte `0xEB` becomes 1 when that is
1). A speaker of `0x10` or more, or the Fremen (14) with no place, cannot
speak: the message is dropped and nothing is shown.

With a place, it is learned and named for the lines (see
[people.md](people.md)). For a speaker of 14 or more, the troop is 3 when
the message is about 14, otherwise the place's byte 9; with a troop, the
Fremen speaks with the troop's look.

The ornament closes, the list of commands empties, and the vision is
drawn on the back buffer:

- the clouds, sprite 0 of the vision's picture file (resource `0x23`);
- when the Fremen speaks for a place that is not Paul's own (word
  `0x114E`): the frame, sprite 7 of resource `0x21`, at (`0xA8`, `0x17`),
  and in the window (`0xB0`, `0x20`) to (`0x120`, `0x58`) the small map
  around the place (as in [flight.md](flight.md)) with the places on it
  and the place's mark (sprite `0x36` of the panel's file) standing on
  its point.

While the vision shows, every 6 ticks the colours `0x80` to `0xBF` turn
down by one (the first going to the end) and the picture ripples: the
first row of the last run goes back as the back buffer has it, then 116
rows from the next starting row (one further each time, from the top of
the area (8, 0) to (`0x138`, `0x94`), going back to the top when no rows
are left) are copied from the back buffer pushed sideways by the ripple
table (1 to 5 pixels and back, then −1 to −5 and back), carrying on from
the top once the area's bottom is reached.

The speaker's face comes up over the clouds through the mosaic
(transition 6) and says the first line of list 4 of character 16 that
may be said, as a remark (see [talking.md](talking.md)); the subtitle's
edge is colour 8 while byte `0xEA` is above 0. The message leaves the
queue. After `0xBB8` ticks (or a key or click) the shimmer stops, the
talk ends, byte `0xEA` becomes `0xFF` and byte `0xE8` `0x0A`, and the
room comes back through the mosaic with the ornament open.

## Not built yet

When the speaker is with Paul, the original lets that companion say the
message in the room instead, the talk going on afterwards, and drops that
companion's other messages; here every message comes as a vision.
