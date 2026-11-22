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
[scenes.md](scenes.md)).

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
