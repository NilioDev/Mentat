# The game

What happens once the story has been told. This page grows with the game.

## Starting a new game

1. The music stops and the screen goes black.
2. The player is put in the throne room (room `0x200A`, place `0x0180`),
   the palace is given one ornithopter, and the clock is set to 2.
3. On the back buffer: the panel (see [panel.md](panel.md)) with the
   walking layout, the clock and the slots, then the view of the room
   (below).
4. The whole screen dissolves in (transition `0x34`, see
   [display.md](display.md)), and the ornament opens.
5. The music chosen for the room starts, from now on repeating; the clock
   starts; the mouse pointer shows.

## The view of a room

Entering a room works out who is there (see [places.md](places.md)), sets
the arrows from the room's ways out, makes the list of commands, sets the
sky's colours, chooses the music, and draws the room with its people and
the ornament.

### Commands inside

In this order:

1. "SEE DUNE MAP".
2. In a place's first room: during the war "MASSIVE ATTACK", "FIGHT FOR A
   WHOLE DAY" and "CALL A WORM" (greyed until the story has come to
   `0x4F`); otherwise "TAKE AN ORNITHOPTER", greyed when the place has none
   (it opens the cockpit, see [cockpit.md](cockpit.md)).
3. In the palace: in room 8, when messages have come, "VIEW NEW MESSAGES"
   (greyed when none is new) and "Messages already seen" (greyed when every
   message is new); in room 9, Paul's room, "LOOK AT MIRROR".
4. "Mixer Panel".
5. The people in the room, in the order of the table of characters, those
   with flag bit 6 last.

The commands are (phrase, action) pairs the program keeps from data offset
`0x21DC`.

## Walking

A click on a usable arrow takes the player through that way out to another
room of the same place: the room is set, its view drawn on the back buffer
and copied to the screen at once.

## The clock

The clock counts parts of a day, 16 to a day; the part is its low four
bits. It moves on by one every 12000 ticks (data offset `0x146E`), about a
minute, the first time on the first tick after it starts. The day, from 1,
is (clock + 3) / 16 modulo 365, plus 1, so a day begins three parts before
the count of 16 comes round.

When the part changes, the clock on the panel is drawn again and the sky
moves to the new part's colours.

## The sky

Each part of the day has its sky: the byte at data offset `0x2280` + the
part, plus (clock / 4) & `0x1C`. The sky's colours are a sprite of picture
file `0x29` (seen from a room) or `0x28` (outside). Its data, after two
bytes, holds the colours for 73 to 223 (from a room) or 128 to 207
(outside), followed by 16 colours for 240 to 255, the panel's.

- **Entering a view** sets the sky of the hour at once. When a fade is
  running towards another sky, it goes on towards this one for `0x30`
  steps instead.
- **A new part of the day**, when its sky is not the one shown, starts a
  fade: every 16 ticks each colour moves 1/n of the way, with n from `0x40`
  down to 1.

## Music

The music follows the kind of moment, the index into the table at data
offset `0x375C`:

| Moment | When |
| --- | --- |
| 1 | the globe is shown (see [globe_screen.md](globe_screen.md)) |
| 2 | the map is shown (see [map.md](map.md)) |
| 5 | outside, or in a place's first room |
| 8, 9 | in a sietch (8 for the first seven kinds of sietch) |
| `0x0A` | in room 3 of the palace |
| `0x0B` | elsewhere in the palace |
| `0x0C` | in a village or the fortress |

The table's byte is the track (see [music.md](music.md)); with bit 7 set
the track waits until the next picture is shown, while other music fades
out over `0x12C` ticks. In the palace the track is 9, `ARRAKIS`.
