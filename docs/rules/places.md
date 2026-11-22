# Places and rooms

## The places table

The program keeps a table of places at data offset 0x100, 0x1C bytes each,
ending with a byte `0xFF`. Byte 8 is the kind of place, which also picks
its rooms and picture file:

| Kind | Place | Rooms (picture file) |
| --- | --- | --- |
| below 0x20 | a sietch | 0xA1 |
| 0x20 | the palace | 0xA2 |
| below 0x28 | a village | 0xA3 |
| others | the Harkonnen fortress | 0xA4 |

A place is named by a word whose high byte is its record number from 1.

## Rooms

The room lists are at data offset 0x13C4, one word per kind giving the
place of the list; each room has 5 bytes:

| Byte | What |
| --- | --- |
| 0 | The room's code: its scene and picture file |
| 1 to 4 | The ways out: up, right, down and left |

A way out is another room of the same kind (1 to `0x7F`), 0 for none, a
value from `0xFB` to `0xFF` for a way outside, and `0x80` to `0xFA` for one
that is closed. In the palace, the throne room (10) leads up to Paul's room
(9), right to room 5 and down to room 4.

Entering a room first clears the line clip, then draws the room (see
[scenes.md](scenes.md)). Two kinds of rooms have more behind the scene,
drawn after the clearing:

- Room 5 of the palace (`0x2005`) shows the sky through its windows: the
  sky tiles, sprites 0 to 3 of `SKY.HSQ` (resource `0x28`), sprite *r*
  along row *r* at x = 0, 40, … 280 and y = 20*r*.
- A place's first room, its entrance, looks outside. Behind the scene is
  sprite 0, at (0, 0), of a picture file chosen by the kind of place, from
  the five bytes at data offset `0x1972` (a sietch, the palace, a village,
  the fortress, the rest, as in the table above), unless the matching
  count in the five bytes after them is 0. For the villages (picture file
  `0x7F`), half the first word of the village's name less 5 is added.
  After the scene, the ornithopters kept at the place (byte `0x15` of its
  record) stand in front of it, from `ORNY.HSQ` (resource `0x33`), each
  sprite clipped to the scene's 320 × 152: from (`0x95`, `0x39`) in a
  sietch or (`0xCA`, `0x49`) elsewhere, sprite 0 there, sprite 1 at (+6,
  +30), sprite 2 at (+4, +50) and sprite 8, the folded wings, at (−`0x51`,
  −3); each further ornithopter `0x46` to the right and `0x0A` lower. The
  sky's colours for the hour are then set again, over those of the
  pictures.

## Persons

Which characters are in a room is a mask of 16 bits: those present, less
those away. Each character has a spot: (character + the turn number)
modulo the number of spots, or else the first free one. The person in
scene slot s stands at spot entry spots − 1 − s and is drawn with sprites
2c and 2c + 1 of the persons sheet (resource 0x26), with the size and
mirror bits and the colour offset the scene gives the slot.

In a game, a character is in the player's room when the room and place in
its record (see [../formats/state.md](../formats/state.md)) are the
player's.
