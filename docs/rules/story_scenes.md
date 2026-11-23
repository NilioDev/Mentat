# The story's scenes

Some steps of the story, and a line's action 13, begin a scene: a short
play in a room of the palace where chosen people stand and speak in turn
while the player clicks on. The scene is a script of bytes in the
program's code.

## Which scene

| Begun by | Script |
| --- | --- |
| Step `0x0C` (see [story_steps.md](story_steps.md)) | `0x1321`, the gathering in the communication room |
| Step `0x48` | `0x1313` |
| Step `0x58` | `0x12FB` |
| Action 13, before step `0x14` | `0x12F8` |
| Action 13, before step `0x18` | `0x134F` |
| Action 13, before step `0x30` | `0x1370` |
| Action 13, from step `0x30` | `0x12DB` |

## Playing it

Once begun, the scene waits for the next click (or key), even one made
while a talk still shows: the talk ends without a word and the scene takes
over. While it plays, the first line of the list of commands lights up and
goes dark in turn every `0x64` ticks, whatever the mouse is over.

A click or a key plays the script on from where it stopped, step by step,
until a step that waits. A click on " WHAT ? " says the line again
instead.

The steps (one byte, then what it takes):

| Byte | Takes | What it does |
| --- | --- | --- |
| 0 | room r, count n, n bytes | Paul is in room r of the place (the low byte of the room word `0x04`); the n bytes say who stands at each of the room's spots, from the first (`0xFF` nobody), instead of who is there. The commands become list `0x1FBA` ("Continue..."), the arrows show none (zones `0x1D1E`), and the room is drawn. Goes on. |
| 2 | character c | The commands become list `0x1FAE` ("Continue...", " WHAT ? "). When c is not the one who spoke last, the room is drawn again first. Character c, zoomed in on where the room shows it (as when Paul clicks a person), says the first line of its list 7 that may be said (one said before is passed over unless it may come again). Waits. |
| 4 | | The commands become list `0x1FBA`; the room is drawn again. Goes on. |
| 6 | character c | The commands become list `0x1FBA`; character c is shown in front of the room, saying nothing. Goes on. |
| 8 | | Waits. |
| `0x0A` | | The commands become list `0x1FBA`. While the part of the day (the clock's low four bits) is below `0x0D`, an hour passes as it would (see [days.md](days.md)); the sky's colours then reach those of the hour at once. The room is drawn, kept as the background, and sprite 0 of resource `0x27` is drawn over it at (78, 33) (the list at data offset `0x2290`). Waits. |
| `0x0C` | | The commands become list `0x1FBA`. The room is drawn, and the part from (0, 50) is enlarged at zoom level 3 (see [display.md](display.md#zoom)), without the panel's ornament, which is drawn over it again; then sprite 1 of resource `0x27` at (26, 4) (the list at `0x2298`). Waits. |
| `0xFF` | | The end: the room's low byte comes back to what it was, the people are those who are there again, and the room is drawn with its own commands. |

Before step `0x14` the program's lines in list 7 for each character
follow the story with their conditions (see [talking.md](talking.md)), so
the same script can say different things.

## Not built yet

The other steps: `0x0E` and `0x10` (Paul's vision of the desert), and
those of the ending (`0x12` on). They are passed over, so the scene at
`0x12F8` shows little.
Nor the scene's music.
