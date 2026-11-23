# Riding a worm

"CALL A WORM" is among the commands out in the desert, and in a place's
entrance while the place is at war (see [game.md](game.md)). It stays
grey until the story has reached step `0x4F`. It is the cockpit's way of choosing a place
(see [cockpit.md](cockpit.md)), but the worm takes Paul there instead of
an ornithopter.

## Choosing the place

The room stays in view. Over it the cockpit draws only:

- the title bar: the rectangle at data `0x14A4`, filled with colour
  `0xF5`;
- the cockpit's map of the area, with four one-pixel rings around it,
  from the inside out in colours `0xFC`, `0xFA`, `0xF8` and `0xF6`;
- the places on the map and the panel's ornament.

The title and the place names are typed as in the cockpit, but at line
`0x22` and on background `0xF5`. A click on a place or the desert sets
the course as for a flight (see [flight.md](flight.md)); "Cancel" brings
the room back.

## The worm comes

The first time, the story moves on to step `0x50`. Sound 8 plays with
the video of the worm coming out of the sand (slot `0x0E`). There is no
take-off, and the place's ornithopters stay as they are.

## The ride

The ride goes by in the desert video of slot 1, in steps like a flight,
with the small map in the corner and "SKIP TO DESTINATION" and "CHANGE
DESTINATION" (which brings the worm's map back over the ride). The music
is the flight's moment plus one. The Harkonnens do not watch for worms:
their zones neither warn nor catch Paul. At the place there is no
approach video: Paul is in its first room at once.

## The worm's frames

Over each video frame the worm is drawn from its animation file
(resource `0xBE`) with the sprites of resource `0x39`:

- words 0 to 3 are the rectangle the worm is drawn in, and clipped to;
- the frames start at 8 + the word at 8, plus the word found there;
- a frame is a list of part bytes up to 0. Byte 1 means the part is
  `0x100` + the next byte. A frame starting with `0xFF` starts the
  frames over;
- part *p* is a list at `0x0A` + the word at `0x0A` + 2 × (*p* − 2): for
  each sprite three bytes, sprite + 1, x and y within the rectangle, up
  to a 0.
