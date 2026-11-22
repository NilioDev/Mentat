# VOC sound files

The spoken voices (2288 files in folders such as `PA\`, one folder per
character and language) and the sound effects (`SN*.VOC`) are Creative Voice
Files, stored unpacked in `DUNE.DAT`. The sound in the full-motion videos
uses the same blocks inside the video (see [hnm.md](hnm.md)).

All numbers are little-endian.

## Header

| Offset | Size | What |
| --- | --- | --- |
| 0 | 20 | `Creative Voice File` followed by the byte `0x1A` |
| 20 | 2 | Offset of the first block (26) |
| 22 | 2 | Version (`0x010A`) |
| 24 | 2 | Check value |

## Blocks

Each block starts with a type byte and, unless the type is 0, a 24-bit
length of what follows.

| Type | What |
| --- | --- |
| 0 | End of the file |
| 1 | Sound: a rate byte, a packing byte, then the samples |
| 2 | More samples at the rate of the last block |
| 3 | Silence: a 16-bit count less one, then a rate byte |
| 5 | Text |
| 6 | Repeat start: a 16-bit count of extra plays, `0xFFFF` to repeat until stopped |
| 7 | Repeat end |

The rate byte `r` gives 1000000 / (256 − r) samples per second. The voices
use 211 (22222 Hz), the videos 166 (11111 Hz).

Packing 0 means unsigned 8-bit samples, with 128 as silence. Packing 1,
used by two sound effects, stores 4 bits per sample: the first byte is the
first sample, and each following byte holds two steps, high half first.
In a step, bit 3 is the direction (set is down) and bits 0–2 the size `s`.
The move is `s` at level 0, and `(s << level) + (1 << (level − 1))` at
levels 1 to 3. The level starts at 0, goes up by one after a step of size
5 to 7 (at most 3) and down by one after a step of size 0 (at least 0).
Samples stay within 0 to 255.

## The text block in the voices

Most voices carry a text block before the sound. It holds the mouth shapes
of the speaking face: a 16-bit value (`0x0190`) and then one byte per step
of the speech, each picking a mouth picture. How fast the steps go is
described with the faces.
