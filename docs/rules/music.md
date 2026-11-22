# Music

How the original's FM music player (`DNADP.HSQ`, the one for the Sound
Blaster Pro) plays the music files described in
[../formats/music.md](../formats/music.md). Mentat reads the player's
frequency and operator tables from the player's own `DNADP.HSQ` (see
"Tables" below) and feeds the same register writes to an emulated chip.

## Steps and measures

The player is called on every timer tick (about 200.3 a second, see
[timing.md](timing.md)). It keeps a 16-bit counter. On each tick it lowers
the counter's high byte by one; when that byte goes below zero it does a
music step and adds the tempo to the whole counter. So a step comes every
tempo / 256 ticks, with the fraction carried over.

Starting a piece sets the counter to 0 and does the first step at once
(adding the tempo), so the second step comes one tick later than the rest.

The player counts steps in measures of 96. The measure number starts at 1;
the step countdown in the measure starts at 96 and goes back to 96 when it
reaches 0, and then the measure number goes up by one. The game reads both
numbers and turns them into a position: measure × 16 + (96 − countdown) / 6,
the last part taken modulo 16. The opening waits for given positions, so it
follows the music.

## A step

1. **Repeated part.** When the player is not repeating, the measure number
   equals the repeat start and the countdown is 96, it remembers every
   voice's wait and place in its list and sets the repeat counter to the
   file's count less one. When it is repeating and the measure number equals
   the repeat end, it lowers the repeat counter, goes back to the remembered
   waits and places and sets the measure number to the repeat start (the
   countdown is left as it is).
2. **Each voice**, in order 0 to 8: the wait goes down by one. When it
   reaches 0 the voice's events are carried out until one leaves a wait
   that is not 0. Otherwise, if the voice still has automatic pitch slide
   steps, it uses one: the bend goes up by the instrument's slide amount and
   the note's pitch is set again.
3. The countdown goes down by one (see above).

At the start of a piece each voice's wait is its first wait plus one, its
instrument is "none" and its slide count 0. A voice without a list never
plays.

## Events

- **Note off:** when the key plus the instrument's key shift is the note
  playing, the note stops (the frequency is written again without the key
  bit).
- **Note on:** first the velocity sets the two operators' levels (below).
  If a note is playing, the voice's frequency registers are set to 0. The
  note becomes the key plus the instrument's key shift, the slide count
  is reset to the instrument's, the bend to the middle (64), and the note
  starts: note − 24 gives the octave (block) as a twelfth and the semitone as
  the rest, the frequency number comes from the player's table of 12, and the
  key bit is set. A note below 24 or from 120 up plays as frequency 0 in
  block 0.
- **Instrument change:** nothing happens when it is the voice's current
  instrument. Otherwise the instrument's registers are written for both
  operators (`0x20`, `0x40`, `0x60`, `0x80`, `0xE0`, and `0xC0` for the
  voice) and its extra settings are kept for the voice.
- **Pressure:** lowers the operators' levels and raises the feedback from
  the levels set by the last note on, the same way as velocity but with the
  instrument's pressure amounts. Nothing is kept.
- **Pitch bend:** sets the bend and, when a note plays, its pitch.
- **End of list:** the voice waits forever on this event. For voice 0 it
  also ends a play of the piece: the play counter goes down; when it reaches
  0 the music stops (every voice's note stops). A counter of 0 at the start
  means "repeat forever". Otherwise every voice starts again from the start
  of its list with the measure number 1 and the countdown 96, the repeated
  part is checked as in step 1, and voice 0's wait goes down by one at once.

## Velocity and pressure

For a velocity `v`, each operator has an amount `a` from its instrument
byte: when `a` is 0 nothing changes; when `a` is positive the change is
(128 − v) >> (4 − a); when it is negative (as a signed byte) the change is
v >> (4 + a). The change is added to the operator's output level (higher is
quieter), at most 63, and the key scale bits are kept.

The feedback works the same with 6 in place of 4, the change rounded down to
an even number and added to the voice's `0xC0` value; above 15 the value
becomes (value & 15) | 14.

Pressure uses the instrument's pressure amounts, swaps the two cases (a
positive amount scales the pressure value, a negative one 128 − value) and
subtracts the change from the levels (at least 0) instead.

## Pitch bend

When the instrument's bend range byte is 0, the bend moves the pitch by up
to 2 semitones in 32 steps each: below 64 by (64 − bend) steps, from 64 up
by (bend − 64 + 1) steps. The steps / 32 give whole semitones, which
move the note (changing octave as needed; below octave 0 it becomes
semitone 0 of octave 0), and a rest r of 0 to 31. Bending down, the
frequency number becomes that of the new semitone s less distance(s) × r / 32,
where distance(s) is from the semitone below; bending up, it becomes that of
s plus distance(s + 1) × r / 32. Both are rounded down, and the octave is the
block.

Otherwise the range is 12 semitones in steps of 5: the whole part moves the
note, and the rest (0 to 4) adds or takes a small amount from one of two
tables of 5, one for semitones 0 to 5 and one for 6 to 11.

## Volume and fades

The Sound Blaster Pro player sets the FM volume in the card's mixer
(register `0x26`, a left and a right level from 0 to 15). A fade moves both
levels one step toward the target: on every tick for a fade shorter than 96
ticks, every 2nd tick below 192, every 4th below 384, every 8th below 768, and
every 16th otherwise. When both levels reach 0 the music stops.

The game asks for volumes as a byte 0 to 255 and a balance (0x78 is the
middle); the player turns them into the two levels (for the middle,
level = volume / 16). Setting the volume does not change the levels at
once: they move to it one step a tick, and the next piece starts at it.

## Starting and stopping

Starting a piece first sets every operator to the fastest release, so a
note left from the last piece fades at once. Stopping takes the key bit off
every voice. The player tells the game whether music plays (`0x80`), and
whether a fade runs (`0x40`).

## Tables

The player keeps its tables at fixed places in `DNADP.HSQ` (unpacked):

| Offset | Size | What |
| --- | --- | --- |
| `0x47` | 2 × 12 | Frequency numbers of the 12 semitones |
| `0x71` | 9 | Register offset of the first operator of each voice |
| `0x7A` | 9 | Register offset of the second operator of each voice |
| `0x83` | 13 | Entry i: the distance in frequency numbers from semitone i − 1 up to semitone i (entry 0 from the last semitone of the octave below, entry 12 from semitone 11 to the next octave) |
| `0x90` | 5 | Fine bend amounts for semitones 0 to 5 |
| `0x95` | 5 | Fine bend amounts for semitones 6 to 11 |
