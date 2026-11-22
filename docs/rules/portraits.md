# Portraits

A character's face: its idle animation, and its voice with a moving
mouth. The data is described in [../formats/portrait.md](../formats/portrait.md).

## Showing a face

Character c uses picture file 0x02 + c. Its area comes from the animation
data; when the area starts left of the given column the whole area moves
right by that many columns (stopping at 320).

The face is drawn over the saved background in the back picture. When
the back picture is not the screen, the colours are sent and the scene
rows copied to the screen.

## Idle animation

A timer every 16 ticks shows the next frame.

- The first sequence is random: `random(6) / 2`, or for a portrait's
  alternative set (variant v) sequence (v − 1) × 2 with its first 32
  frames skipped.
- The mood sets how many frames it runs: mood × 4.
- When those frames run out the face is "restless": it plays sequence 4
  (v − 1 for a variant) only now and then. Each tick a random number is
  taken with mask `0x0F38` (`0x0F18` for a variant); when its high byte is
  not 0 nothing happens, otherwise its low byte of frames is skipped and 8
  frames play.

Only what changes is drawn again: the bounds of the pieces in the old
frame but not in the new one, and the other way round. The background is
copied from the saved picture into that rectangle, every piece of the new
frame is drawn clipped to it, and the rectangle is copied to the screen.
The first frame draws the whole area. While the face speaks, the rectangle
stops at the top of the mouth's area.

When a face is taken away, its animation first runs to the end of a group
of 8 frames.

## Speaking

The voice of phrase n is `P<c>\P<c><nnn><I|O>.VOC` in the archive, with
`<c>` the letter `A` + c, `<nnn>` the phrase in three hex digits and `I`
indoors or `O` outdoors; when the one for the current place is missing the
other is used.

The voice's text block holds a 2-byte number, then one mouth shape per
step, ending with `0xFF`. The steps are spread over the voice: one step
lasts the length of the frequency recording (resource 0xB9) in ticks,
divided by 32, about 8 ticks. Shape k shows once the ticks since the
start reach start + 0.5 + k × step, rounded down.

The mouth is drawn in its own area, kept in the program per character
(8 bytes each: left, top, right, bottom), from the saved background up.
At the start of speaking the face becomes restless at once.

## Tables

| File | Offset | Size | What |
| --- | --- | --- | --- |
| `DNCDPRG.EXE` data | `0x27FA` | 17 × 8 | The mouth areas |
| `DNCDPRG.EXE` data | `0x27B6` | 4 per character | The zoom centres (x, y) |
| `DNCDPRG.EXE` data | `0x2792` | bytes | The opening's zoom levels, `0xFF` a wait |
