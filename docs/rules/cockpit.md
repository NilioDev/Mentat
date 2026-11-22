# The ornithopter's cockpit

In a palace's entrance, "TAKE AN ORNITHOPTER" (phrase `0xA7`, action
`0x42E9`, greyed while no ornithopter is kept there) puts Paul in an
ornithopter's cockpit, where he chooses where to fly on a small map.
This page covers the cockpit as it opens, what the mouse shows on it and
"Cancel". Flying off is not covered yet.

## Opening the cockpit

Nothing fades: the cockpit replaces the room at once.

1. `ORNYPAN.HSQ` (resource `0x24`) is chosen, with its colours.
2. The panel: the cockpit's commands (data offset `0x212E`: two bytes,
   then (phrase, action) pairs up to a phrase of 0; only "Cancel",
   action `0xD2E2`), and the map's six arrow zones (data offset
   `0x1CCA`, see [map.md](map.md)). Zones 0 to 2 stay as they are.
3. The map's middle is Paul's place.
4. On the back buffer:
   - the sky's colours for the hour are set, as on entering a room (see
     [game.md](game.md));
   - the sky: sprites 0 to 3 of `SKY.HSQ` (resource `0x28`, whatever the
     room), sprite *r* along row *r*, at x = 0, 40, … 280 and y = 20*r*;
   - the `ORNYPAN.HSQ` sprites listed at data offset `0x14B4`, as
     (sprite, x, y) up to `0xFFFF`: the instruments, then the map's frame;
   - the small map (below) in the rectangle at data offset `0x149C`,
     (81, 45) to (241, 134);
   - the places on it (below);
   - the `ORNYPAN.HSQ` sprites listed at data offset `0x14C0`: the map's
     frame again, its green lines over the map;
   - the ornament, as it is.
5. The back buffer is shown and the changed colours are sent.
6. Paul's mark starts blinking, the title starts being typed (both
   below), and the cockpit's voice asks for a destination: character
   `0x19`'s phrase `0x2BC` (the sound file `PZ\PZ2BCI.VOC`; the one for
   outside is used when there is none for inside, see
   [talking.md](talking.md)).

## The small map

The map is drawn a screen pixel to a map point, around the map's middle
(longitude *λ*, latitude *φ*). For a rectangle *w* pixels wide and *h*
high (160 × 89):

- *c* = (*h* − 1) / 2 rows show above the middle row and as many below.
  The middle row's latitude is first kept within ±(86 − *c*), ±42 here,
  so that no row goes past latitude 86.
- The row of latitude *φ'* takes the ring of that latitude (2 × its
  width in `TABLAT.BIN`, see [map.md](map.md)) from where the latitude
  starts in `MAP.HSQ`. Its middle point is ⌊ring × *λ* / 65536⌋; the row
  starts *w* / 2 points before it (wrapping round the ring) and takes *w*
  points. A ring narrower than *w* is taken whole, starting ring / 2
  points before the middle, and centred in the rectangle.
- Each point is drawn in colour 16 + its low four bits.
- The rows are drawn from the bottom one up. From latitude 70 (north or
  south), the planet's rim closes in from both sides by *s* pixels, *s*
  from a table of 18 bytes for latitudes 70 to 87 in the player's video
  driver (`DNVGA.HSQ` unpacked, file offset `0x2284`). Such a row is:
  - when *s* is 4 or more: *s* − 4 black pixels, then colours `0x1C`,
    `0x19`, `0x18`, `0x17`; when *s* is less, `0x18` and `0x17` only;
  - *w* − 2*s* points, starting *s* points into the row;
  - the rim the other way round: `0x17`, `0x18`, and when *s* is 4 or
    more `0x19`, `0x1C` and *s* − 4 black pixels.

  When *w* − 2*s* is below 0 the whole row is black.

Map point (*λ'*, *φ'*) is on the screen at
x = ⌊(*λ'* − *λ*, as a signed 16-bit number) × ring(*φ'*) / 65536⌋ + left
+ *w* / 2, y = *φ'* − *φ* + top + *c*, and is on the map when it falls
inside the rectangle.

## The places

The places of the state not hidden (byte `0x0A` bit 7 clear) whose point
is on the map are kept, with where they are, in the order of the state.
Each is drawn centred on its point and clipped to the map, with the
panel's sprite (resource 0) `0x3A` and up chosen by its kind as on the map
screen, a sietch further than Paul can see taking the sprite 5 further on
(see [map.md](map.md)).

## Paul's mark

Sprite `0x4C` of the panel's picture file marks Paul's place when it is on
the map, its bottom row at the place and its left 13 pixels left of it. It
is drawn on the screen only: first at once, then a timer every 300 ticks
copies the mark's rectangle (within the map) from the back buffer and
draws the mark again every other time, so that it blinks.

## The line above the map

A line of the small font (see [panel.md](panel.md)) at (`0x55`, `0x26`),
on the screen only, with background colour `0x20`.

### The title

Phrase `0x57` ("SELECT DESTINATION ON MAP") in colour `0x61`, typed by a
timer every 24 ticks: a letter each time, and after a space the next
letter at once. The timer stops when the line is used for a name, and
typing starts again from the first letter when the mouse points at
nothing.

### What the mouse points at

While the cockpit is shown, the mouse's place is looked at all the time:

1. Off the map's rectangle it points at nothing.
2. On it, the nearest place kept (the distance across plus the distance
   down; the first of those as near) is pointed at when that distance is
   below 9.
3. Otherwise it points at the desert. While Paul's mark is on the map,
   the desert has a direction from his place: with *dx* the mouse's x
   less (the place's x − 2) and *dy* the mouse's y less the place's y, an
   angle byte (256 to a turn) is found:
   - when |*dx*| ≥ |*dy*|: 0 when *dx* is 0, otherwise 32 × *dy* / *dx*
     (rounded towards 0) plus `0x40`, or plus `0xC0` when *dx* is below 0;
   - otherwise 32 × *dx* / *dy* (rounded towards 0), less `0x80` when *dy*
     is 0 or more, then negated.

   With 3 added (keeping 8 bits), when its low five bits are below 6 its
   top three bits are the direction: 0 north, then clockwise in eighths.

Only when what is pointed at changes is the line written again, from its
start, in colour `0xFE` (except the title):

- nothing: the title starts being typed (when it is not being typed
  already);
- the desert: phrase `0xA4` ("DESERT"), then with a direction a space
  and phrase `0xDA` + the direction ("northwards" …);
- a place: by its kind (byte 8), phrase `0x44` ("Sietch: ") below `0x20`,
  `0x45` below `0x21`, `0x46` below `0x28`, `0x47` below `0x30` and
  `0x46` from there; then the phrase of its first word (byte 0), a hyphen
  when its second word (byte 1) is 3 or more and a space otherwise, and
  phrase `0x0C` + the second word.

The rest of the line is then filled with spaces until x is past `0xED`.

The mouse pointer shows the hand over the map and the arrows near its
sides, as over the map screen (see [panel.md](panel.md)).

## Cancel

"Cancel" leaves the cockpit: the timers stop, and the room is drawn again
and shown at once, with its commands and arrows.
