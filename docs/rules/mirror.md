# The mirror

In Paul's room in the palace (room 9) "LOOK AT MIRROR" is among the
commands. It shows Paul's face in the mirror, and offers the commands of
the game itself: start again, load, save, leave the game.

## Looking in

1. The clock stops (it stays stopped until Paul looks away or the game
   goes on from another game).
2. The ornament closes.
3. On the back picture, from the mirror's pictures (resource `0x3B`):
   picture 0 (the room) and picture 1 (its colours over it), both at
   (0, 0).
4. The faces:
   - normally Paul's alone (portrait character `0x2D`), where it stands,
     with mood 1;
   - when bit `0x80` of the state's byte `0x10` is set, Chani's
     (character 7) 15 rows down with mood 0, then Paul's moved right by
     `0x2D` columns with mood 1, both moving, as at the end of the game.
5. Picture 2 (the frame) at (0, 0).
6. No arrows (the zones at `0x1D1E`), and the commands at `0x20C2`.
7. The picture comes with transition `0x04`, the squash (see
   [display.md](display.md)).

## Paul's face

Paul's portrait has a set of faces for how far the game has gone. With
the clock (state word 2, in parts of a day, 16 to a day):

- age = clock / 64 (rounded down), at most 8: one more every 4 days;
- the set is age × 2 + 1, plus 1 when Paul is tired (state byte `0xF4`
  at `0x10` or more).

## The commands

| Command | What it does |
| --- | --- |
| RESTART GAME | As at the end of the game: the video, then a new game. |
| LOAD GAME | The list of saved games to load; "Cancel" goes back. A game loaded goes on where it was saved. |
| SAVE GAME | The list of saved games to save over, as on the globe; once saved, or with "Cancel", the mirror's commands come back. |
| EXIT GAME | Asks first (the list at `0x20B4`); no goes back to the mirror's commands. |
| Look away from the mirror | The faces go, the clock runs again, and the room comes back with the squash, then the ornament opens. |
