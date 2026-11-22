# Talking

Clicking a person in the room, or the person's name in the list of
commands, starts a talk. The data is described in
[../formats/dialogue.md](../formats/dialogue.md) and
[../formats/text.md](../formats/text.md); the face in
[portraits.md](portraits.md).

## The picture

1. The mouse pointer is hidden.
2. The room is drawn again without its people, and the part of it where
   the person stood is enlarged four times (zoom level 6 of
   [display.md](display.md)) into the clean background. The part starts at
   the person's top-left corner as the room drew it, but never right of
   column `0xF0` or below row `0x71`. When the person was not drawn, the
   background stays as it is.
3. The face is shown over it (see [portraits.md](portraits.md)). How long
   its first idle sequence runs goes by the words of the last text laid
   out in a box before (the last story page, at first).
4. What lies behind the ornament at the top of the panel (columns 150 to
   170, rows 137 to 147) is kept, for closing the ornament later.
5. Each time the back buffer is shown, the ornament is drawn on it first,
   as it is at that moment, over anything else.

The character now counts as met: its bit is set in the words at `0x0E` and
`0x14` of the state.

## The menu

| Line | Phrase | What it does |
| --- | --- | --- |
| TALK TO ME | `0x9F` | The next line |
| COME WITH ME | `0x91` | Asks the character to go with Paul; greyed when the character's flags have bit 7. When the character goes with Paul already (bit 6), this line is STOP FOLLOWING ME (`0x92`) instead |
| WHAT ? | `0x95` | The voice says the last line again |
| STOP TALKING | `0x94` | Ends the talk |

## Which line comes next

Each character has 8 lists: 0 to 3 are what it says when Paul talks, 4
are things it says on its own, 5 answers "come with me", 6 "stop
following me", 7 is for when everyone gathers.

The talk goes through lists 0 to 3 in order, starting at list 0. In the
list it is at, it looks from where it stopped for the first line that

- has not been said, or has been said but may be said again (flag bit 6);
- has a condition that holds.

Before each search the state gets the character's flags at `0x18` and, at
`0x16`, the clock less the clock when the character last joined Paul (when
it goes with him) or last left him.

When the list has no such line, the talk goes on with the next list;
after list 3 there is nothing more to say and the talk ends. A list the
table leaves out ends it at once. Character 13 instead starts over at
its list 0, now also passing over the lines it has said that do not have
flag bit 5, and only then ends.

"Come with me" and "stop following me" take the first line of list 5 or 6
that may be said, passing over said lines with flag bit 5. They do not
move the place where TALK TO ME goes on.

## Saying a line

When subtitles are shown below the picture, the ornament first closes, a
step every 8 ticks (see [panel.md](panel.md)). The line's phrase is made
into text, page by page (`0xFE` starts a page). The first page is shown;
each TALK TO ME shows the next, until the last page.

When the last page is shown, the line:

1. does what its flags say (below);
2. is noted down when its byte 2 has bit 2 or 3 and it had not been said;
3. is marked said;
4. lets TALK TO ME go on after it, unless it ended the talk.

Then the face starts a new idle sequence, its calm part as long as the
words of the line, and the voice speaks the line, the pointer hidden while
it starts.

### What lines do

| Action | What |
| --- | --- |
| 1 | The answer is yes |
| 2 | The answer is no |
| 6 | The talk ends after this line: TALK TO ME has nothing more |
| 7 | The answer is "maybe" (`0x80`) |
| 11 | When said the first time: the story moves on a step (state `0x2A`); at step 1 byte `0x100B` is set |
| 12 | When said the first time: the story moves on to the next multiple of 4 |
| 14 | When said the first time: one more at `0xC2` |
| 15 | For Jessica: one more at `0xF5` |

The others (3 to 5, 8 to 10, 13) open messages, choices and events that
Mentat does not have yet.

When the story moves on, `0xFF` is cleared and the story's own list (list
135) is looked through once: its first line that may be said is marked
said and does what it does, but nothing is shown or spoken. A new game
looks through it twice.

### Going with Paul

"Come with me" sets `0x23` to 2 when the spice at `0xAC` is 1000 or more
and a quarter of (100 less `0x29`) is more than `0x36`, else to 0, while
the answer is chosen; then it is 0 again. One more is counted at `0x1B`.
When the answer is yes, the character gets flag bit 6, its bit in the word
at `0x10`, and, when at least 2 parts of the day have passed since it last
left Paul, the clock as when it joined him. "Stop following me" undoes
this, keeping the clock as when it left.

When the talk ends and the character goes with Paul, it takes a slot of
the panel (`0x1152`): the first free one, or else the second, the first
character leaving Paul.

## Subtitles

The text is laid out as in [text.md](text.md) in a width of 288 pixels in
the large font (byte 6 switches to the small font, byte 8 back), the lines
stretched, 10 rows apart. It is drawn in colour `0x0F` from column 16 and
row 1 of a clear picture; then every clear pixel left, right, above or
below a pixel of the text becomes `0xF0`. The rows used (1 + 10 per line)
are copied onto the back buffer so that they end at row `0x92`, clear
pixels letting the picture through. When the face's animation changes a
part of the picture that reaches below the subtitle's top, the subtitle is
drawn again over that part before it goes to the screen.

The next page or line first puts the clean background back over the
scene, draws the face again whole and shows it.

## Voices

The original speaks the lines when it has digital sound; with English or
the first language it then shows no subtitles by default. A line with no
recording (byte 2 bit 4) is shown as text even then.

The recording of phrase n of character c (character 14 and up use 14) is
numbered n less one less than the phrase of c's first line (the first
line of its first list that has lines), so each character counts from 1.
Page k of a line (from 0) uses the recording with the letter `A` + k after
it, as does a line with flag bit 4: it has several recordings and picks
one by what it names (a place, a number) or else at random among the
first four. The recording name is as in [portraits.md](portraits.md); in
the palace's entrance hall the outside one is used.

## Ending

The character's flags get bit 5 and lose bit 2. The face stops at once,
the room is drawn again with its people and shown, and the ornament opens.

## Dice

A new game starts both random number generators from the PC's clock: the
ticks since midnight, 18.2 a second.
