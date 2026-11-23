# The story's steps

The story's step is byte `0x2A` of the state. This file says what moves it
on and what each step changes; the lines the characters say about it are in
the dialogue (see [talking.md](talking.md)).

## Moving on

A line's action 11 moves the story one step on: byte `0x2A` grows by one,
byte `0xFF` (the days at this step, see [days.md](days.md)) becomes 0 and
the story's own lines run (list of character 16, as for any step). Nothing
else happens, whatever the new step.

Every other way of moving the story on names the step it goes to; when
that is not past the step reached, nothing happens. Otherwise byte `0x2A`
takes it, byte `0xFF` becomes 0, the story's lines run, and then, for a
step up to `0x6C` that is a multiple of 4, the step's own changes below.

These move the story on so: a line's action 12 (to the next multiple of 4)
and finding sietch `0x0603` (to `0x10`, see "found" in
[flight.md](flight.md)). The others the program has, and Mentat not yet:
`0x30`, `0x3C`, `0x4C` (Paul's forts reaching the count at byte `0x1178`),
`0x50` and `0x64` (a gathering).

## What each step changes

"Character n" is the record at `0xFD8` + 16 n: word 0 its room (low byte
the room, high byte the kind of rooms), word 2 its place. "Places found"
are found as when flying past them: shown from then on, and counted when
sietches.

The palace's rooms are kept in the state from `0x1225`, five bytes each
(the room's picture, then its ways out up, right, down and left); a way
whose byte has bit 7 shows no arrow. "Opening" a way clears that bit; the
arrows are then set again when Paul is in a room. The palace's plan is a
list of (sprite, x, y) words at `0x120B` ending in `0xFFFF`; its last three
sprites are the shut parts, and writing `0xFFFF` over one's word leaves it
and those after it out.

| Step | What changes |
| --- | --- |
| 4 | Room 2's picture byte goes down by one; places `0x218` and `0x2DC` found; character 1's room byte 9 |
| 8 | Room 2's way left opened (to room 12) |
| `0x0C` | Room 8's way right and room 7's way left opened (between them); the plan's last shut part goes; a scene (`0x1321`) begins |
| `0x10` | Character 0's room byte 5, character 1's 9; news `0x010B` |
| `0x14` | Character 0's room byte `0x0A`; places `0x34C`, `0x368`, `0x384` found |
| `0x1C` | Room 7's way right opened; the plan's second shut part goes |
| `0x20` | Byte 3 of character 2 becomes 1 |
| `0x28` | Place `0x800` found |
| `0x2C` | Word `0x1154` takes the clock; character 4 to room `0x2006`, character 2 to `0x2008` in place `0x0180`, character 1 to room byte `0x0A` in place `0x0180`; word `0x1201` becomes `0x109`; charisma up by `0x14`; byte `0x0A` gets bit 4; places `0x5EC`, `0x5D0`, `0x608`, `0x640`, `0x65C` found |
| `0x30` | Message `0x0004` left (for no place); news `0x1409` |
| `0x34` | Unless character 1's room byte is 8, it becomes `0x0A` with place `0x0180`; news `0x280A` |
| `0x38` | Byte 3 of character 0 becomes `0xFF` |
| `0x40` | Byte `0x0F` of character 8 gets bit 1 |
| `0x44` | Place `0x3D8` found |
| `0x48` | Charisma up by `0x0A`; a scene (`0x1313`) begins; byte `0x0F` of character 7 gets bit 4 and loses bit 1; byte `0x1178` becomes byte `0x28` plus 2; places `0x3F4`, `0x410`, `0x3BC`, `0x88C` found |
| `0x4C` | Byte `0x1141` up by one; character 1 to room byte 2, place `0x0180`; message `0x0105` left |
| `0x50` | Byte `0x0A` gets bit 6; charisma up by `0x28`; character 1's room byte 9 |
| `0x54` | Room 11's way right opened; the plan's first shut part goes |
| `0x58` | Byte `0x0A` gets bit 5; a scene (`0x12FB`) begins; places `0x7E4`, `0x790`, `0x7AC`, `0x854`, `0x81C` found |
| `0x5C` | Character 6's room byte 5; byte `0x11D0` up by `0x0C`; word `0x1156` becomes the day (clock / 16) plus 3; byte `0x1141` up by one |
| `0x60` | Character 7 goes to the Harkonnens' palace (place record `0x11C`): word 0 its kind in the high byte and 2, word 2 its number and `0x80` |
| `0x64` | The last battle (below) |

Raising the charisma is as for a place taken (see
[places_taken.md](places_taken.md)); messages and news are as in
[work.md](work.md#the-chiefs-messages) and [days.md](days.md#news).

### The last battle

Of the place records from `0x138` on (while the next record's byte 0 is
below 8), the Harkonnen forts (kind from `0x28`, byte `0x0A` without bit 3)
where none of Paul's troops fight (see [fights.md](fights.md)), the one
with the greatest latitude word (signed, word 4), counting only those
above −100; with none, the record at `0x100`. Character 7 goes there (word
0 the kind in the high byte and 3, word 2 its number and `0x80`), word
`0xF2` takes the place's byte 0 in its high byte and byte 1 in its low
one, and news `0x2B0A` is sent.

## A long quiet at step `0x14`

At step `0x14` exactly, with nothing else going on (as for the chiefs'
messages, see [messages.md](messages.md)), nobody with Paul (word `0x10`
is 0) and Paul outside (byte 8 is `0xFF`), once `0x3E8` ticks have passed
since the room was last shown (the count then moves on by `0x3B6`): the
story goes to `0x15` without the steps' changes: byte `0xFF` becomes 0,
byte 3 of character 0 becomes 1, character 4 goes to room `0x200B` in
place `0x0180`, character 1's room byte becomes `0x0A`, byte `0xD5`
becomes `0xFF`, word `0x118D` takes the day and the Emperor makes his first
demand (see [days.md](days.md#the-emperors-demand-hour-3)), byte `0x0A`
gets bit 0 (messages may now be left), message `0x0001` is left, and the
story's lines run.

## Not built yet

Some of the scenes' steps (see [story_scenes.md](story_scenes.md)), and the other ways of moving the story on listed above. Mentat has no ways out of
a place's rooms yet, so Paul is never outside but when flying.
