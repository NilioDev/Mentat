# The mixer panel

"Mixer Panel", the last command of a room before its people (see
[game.md](game.md)), opens a window over the scene with the sound's
volumes, a voice test, the language and how lines are shown. The window
is drawn straight on the screen; the clock goes on.

## The window

The pictures are those of picture file `0x55` (`MIXR.HSQ`). The window's
rectangle is at data offset `0x2886` ((40, 1) to (250, 144)); sprite 0
fills it from its top-left corner. All other places below are counted
from that corner.

### Volumes

Three sliders, from data offset `0x288E`, 8 bytes each: the volume (0 to
255), a byte, then the column. In order: the voices, the music, and the
music while a voice speaks.

A slider is drawn as sprite 1 (its track) at row `0x22`, then sprite 2 (its
knob) at row `0x22 + (255 − volume) / 4` (rounded down): the loudest at the
top, 64 rows in all.

### Balances

Three round knobs under the sliders, from data offset `0x28A6`, 8 bytes
each: the balance (0 to `0xF0`, `0x78` the middle), a byte, the column and
the row; the same order. A knob is sprite 3 + balance / 10.

### The lists

The rectangle at `0x28C7` ((134, 39) to (200, 130)) holds two lists, a row
every 7: the languages, then how lines are shown. The 13 bytes at `0x28CF`
give what each row is: a language 0 to 6 (American, French, German,
English, Italian, Spanish, Fremen; the rows show American, English,
French, German, Italian, Spanish, Fremen), 7 for the rows between the
lists, and 8 + n for the second list (0 subtitles, 1 balloons, 2 no
subtitles). The 13 bytes at `0x28DC` give the row of each of those values.

The choices in use are marked: sprite `0x1C` + 2 × value at the list's
left edge and the value's row, once for the language and once for 8 + the
way lines are shown.

### Starting values

The program's data holds them: voices 255 at balance `0x64`, music `0xE6`
at `0x78`, music while a voice speaks `0xB4` at `0x8C`. They last as long
as the program runs.

## Clicks

The original tests points as strictly inside a rectangle (not on its
edges).

- **Outside the window**: as "Done" below; in a list the panel opened, back
  to the panel's own commands.
- **VOICE TEST** (the rectangle at `0x28BF`, (31, 122) to (90, 132)):
  character 5 says phrase 4 (the same recording as anywhere else), and the
  game waits until it has been heard.
- **A row of the lists**: a language other than the one in use becomes it:
  "no subtitles" turns into "subtitles" (balloons stay), the files of
  text are read again (`COMMAND`*n*`.HSQ` and the two `PHRASE`*n*`.HSQ`, n
  the language + 1), the font is `DNCHAR2.BIN` for Fremen and `DNCHAR.BIN`
  otherwise, and the window and the commands are drawn again in it. A row
  of the second list chooses how lines are shown and the window is drawn
  again. The rows between do nothing.
- **A knob**: grabbed when the point is on a slider's knob (from its
  column, 22 wide, from its row, 5 high) or on a balance (13 by 11). While
  it is held the pointer is the hand. Each move of the mouse with the
  button down finds the control again at the pointer's column and its
  previous row, so a dragged knob stays held and one left behind is let
  go:
  - a slider's knob moves by the rows the mouse moved, as long as it stays
    on its 64 rows; the volume is 255 − 4 × its row;
  - a balance turns by 10, one way or the other from the sum of the moves
    across and down, with the move down counted backwards on the knob's
    left side (its first 6 columns) and the move across counted backwards
    on its lower side (from its sixth row). A balance does not go below 0
    or above `0xF0`.

The voices' volume and balance and the music's take effect at once; the
third pair when a voice next speaks.

## Commands

The panel's list is at data offset `0x201A` (a level byte, then pairs).
Without music its first three lines are greyed out; otherwise the way the
music goes stays lit: the first line when it is off, the second for the
game's music, the third for the CD style.

| Command | What it does |
| --- | --- |
| MUSIC OFF | The panel closes and the music stops; no track plays until it is turned on again. |
| MUSIC ON (GAME RELATIVE) | The panel closes and the music goes with the game again, chosen as usual for where Paul is. |
| MUSIC ON (CD-STYLE) | Another list (`0x206A`): STANDARD ORDER, SHUFFLE (the one in use lit) and Cancel, which closes the panel. |
| EXIT GAME | The question of [game_over.md](game_over.md) (`0x20B6`); no comes back to the panel's list. |
| Done | The panel closes. |

When the panel closes, the scene under the window comes back from the back
buffer, the commands the panel replaced come back, and lines are shown as
chosen on it.

## The music as a CD

The nine tracks of the data at `0x3804` (9, 6, 8, 1, 4, 3, 7, 5, 2) play
one after the other, each once; the order goes round. STANDARD ORDER takes
that order afresh; SHUFFLE keeps the order as it is and shuffles it (and
again each time it goes round): eighteen times two places, each drawn from
0 to 8 with the third generator, swap. Either stops the music, closes the
panel and starts the first track.

When a track has ended, the next one starts after 200 ticks of silence,
unless a voice is speaking or Paul's place is at war. The game's own
choices of music for a room are not used meanwhile. (The original also
mixes the clock into the generator between the two draws of a swap;
Mentat does not.)

## The music while a voice speaks

When a character's voice starts, the music moves to the third volume and
balance over `0x64` ticks; when the voice has ended, back to the music's
own over `0x190` ticks.

## Mentat

The voices' volume becomes a level 0 to 15 for each side as the music's
does (see [music.md](music.md)), the high four bits the left; Mentat
plays a level as about 2 dB, as for the music.
