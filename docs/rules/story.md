# The credits and the story

What the original shows when the player presses a key during the opening
(see [intro.md](intro.md)).

## The credits

The credits video (slot 20, `CREDITS.HNM`) is opened in the cleared back
picture and shown with the fade through black (`0x3A`, see
[display.md](display.md)); music 3 starts if it is not playing. Each new
frame of the video is copied to the screen. The video's end or a key ends
the credits, and the story follows either way.

## The pages

Music 3 keeps playing. Eight pages follow, each drawn in the cleared back
picture and shown with the fade through black:

| Page | Picture |
| --- | --- |
| 1 | Space, scrolled by 0 (below) |
| 2 | Space, scrolled by `0x20`, with the planet turning |
| 3 | The desert (below) |
| 4 | Paul, as in the opening |
| 5 | The Baron, as in the opening |
| 6 | As page 2 |
| 7 | As page 4 |
| 8 | Opening picture `0x61` (`INT15.HSQ`) |

Under the picture, entry `0x116` + page of the language's `COMMAND` text
(`COMMAND1.HSQ` for English) is drawn in colour 6 in the box from (0,
`0x99`), 320 wide and `0x2F` high, laid out as in [text.md](text.md).

Once a page shows, the music fades over `0x64` ticks to volume `0x8C`
with balance `0x8C`, and the narrator reads the page: the recording is
named like a character's (see [portraits.md](portraits.md)) as character
`0x19` with the page as the phrase, `PZ\PZ00nI.VOC`. The page stays
`0xFA0` ticks. Then the voice stops and the music fades back over `0x190`
ticks to volume `0xE6`, balance `0x78`. A key during a page ends the story
at once.

### Space

Sprites of `STARS.HSQ` (resource `0x2C`), for a scroll value c, with
s = −(c × c / 2):

- sprites 0, 1 and 2, the stars, with their corner at (s, 0),
  (s + `0x130`, 0) and (s + `0x260`, 0);
- sprite `0x24` centred on (4s + `0x45`, `0x4E`);
- sprite c / 4 + `0x25` centred on (4c + `0xF2`, `0x67`) when c is at
  most `0x14`, otherwise on (2s + `0x3E2`, `0x67`);
- sprite c + 3 centred on (−s / 16 + `0x80`, `0x4F`).

"Centred" takes half the sprite's width and height off the position.

### The desert

The sky's picture file seen from a room (resource `0x29`) is chosen, and
its sprite for the hour sets colours 73 to 223 at once. The hour is the
game's clock, 2 at this point; the sprite is entry (clock & 15) of the
table at data `0x2280` in the program, plus (clock / 4) & `0x1C`. Then the
sky picture file (resource `0x28`) gives four rows of tiles: sprite r at
x = 0, `0x28`, … below 320, y = r × `0x14`. Last, sprite 0 of
`INTDS.HSQ` (resource `0x2D`) at (0, `0x3C`).

## The palace at dawn

After the eighth page: `XPLAIN9.HSQ` (resource `0x1B`), sprite 0 at
(0, 0), dissolves in (`0x10`). After `0xC8` ticks the colours 73 to 223
start to move toward sky sprite `0x0C` of the sky seen from a room: every
16 ticks a step, 64 steps, the same fade as the opening's sky. `0x4B0`
ticks later the fade stops where it is and the cleared picture dissolves
in. The game starts then.
