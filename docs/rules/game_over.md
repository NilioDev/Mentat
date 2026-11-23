# The end of a game

A game ends when Paul is caught or the story ends badly. The state's
byte at `0x46D9` holds the reason. Built so far:

- 2, Paul caught by the Harkonnens in their zone (see
  [flight.md](flight.md));
- 6, the place Paul is at lost to the Harkonnens (see
  [fights.md](fights.md));
- 7, the Emperor's spice not sent in time (see [days.md](days.md)).

The screen below is the same for every reason, with the face and words
of that reason.

## The flashes

The sky flashes three times, then fades out:

1. The colours to fade to are the sky of the hour (see
   [game.md](game.md)): those of a place's rooms while inside or
   flying, otherwise those of the outside.
2. Three times: the sky's colours are set at once to sky `0x28`, then
   fade towards the target with divisors `0x20` down to `0x10`, one step
   every 3 ticks.
3. Then the fade goes on with divisors `0x0F` down to 1.

Only the sky's colours change; the panel keeps its own.

## The screen

1. The clock is set to `0x16C5`, so the panel shows day 365, and it no
   longer runs.
2. Video slot 12 plays to its end. Its last frame stays as the scene.
3. The frame described at data offset `0x2254`: its rectangle (left,
   top, right, bottom) is filled with the colour in the byte at `+9`,
   then its edge, one pixel wide, is drawn in the colour at `+8`.
4. The face of the one who tells of the end: the byte at data offset
   `0x225D` + reason is a character's number, or none when it is
   negative. The last-but-two and last-but-one sprites of that
   character's portrait sheet are drawn at one pixel in from the frame's
   top left.
5. The words: phrase `0xBA` + reason of the commands' text (counting
   from 1), in the large font, in the box from (88, 3) to (288, 64),
   colour `0xF0`, laid out as in a talk's text box (see
   [text.md](text.md)).
6. The clock is drawn again, and the face's character says phrase
   `0xFFF` (see [talking.md](talking.md)); the two recordings of it are
   played in turn each time it is said.
7. The commands at data offset `0x20A2`.

## The commands

- "RESTART GAME" (action `0x0E47`): video slot 13 plays, then a new
  game starts as the program does after the story pages.
- "LOAD GAME" (action `0xB29E`): the list of saved games in the panel,
  as on the globe (see [globe_screen.md](globe_screen.md)). Loading one
  goes on in the room it was saved in; "Cancel" goes back to this list.
- "EXIT GAME" (action `0x0E3E`): first asks, with the commands at data
  offset `0x20B4`. "YES" (`0x003A`) ends the program; "NO" (`0xD2E2`)
  goes back to the list above.
- " WHAT ? ": the words are said again.
