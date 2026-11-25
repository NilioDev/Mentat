# AdLib music

The ten pieces of music are packed files (see [hsq.md](hsq.md)) for the
AdLib and Sound Blaster FM chip. The original's music players for this chip
(`DNADL.HSQ`, `DNADP.HSQ`) read them; how they play them is in
[../rules/music.md](../rules/music.md). The `.AGD` files with the same
names are the same music for the AdLib Gold, in the same layout with the
differences in [AdLib Gold files](#adlib-gold-files); its player
(`DNADG.HSQ`) is in [../rules/music-gold.md](../rules/music-gold.md). The
`.M32` files are for the Roland MT-32 (see [MT-32 files](#mt-32-files));
their player (`DNMID.HSQ`) is in
[../rules/music-mt32.md](../rules/music-mt32.md).

| Number | File |
| --- | --- |
| 1 | `SEKENCE.HSQ` |
| 2 | `WATER.HSQ` |
| 3 | `WORMSUIT.HSQ` |
| 4 | `WORMINTR.HSQ` |
| 5 | `WARSONG.HSQ` |
| 6 | `MORNING.HSQ` |
| 7 | `SIETCHM.HSQ` |
| 8 | `BAGDAD.HSQ` |
| 9 | `ARRAKIS.HSQ` |
| 10 | `CRYOMUS.HSQ` |

All numbers are little-endian.

## Layout

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 | Offset of the instruments, from the start of the file |
| 2 | 2 × 9 | Offset of each of the 9 voices' event lists, counted from offset 2; 0 for an unused voice |
| 20 | 24 | Zeros |
| 44 | 2 | Measure where the repeated part starts |
| 46 | 2 | Measure where the repeated part ends |
| 48 | 2 | How often the repeated part plays |
| 50 | 2 | Tempo |
| 52 | … | The event lists |
| … | 40 × n | The instruments, to the end of the file |

## Event lists

Each list starts with a wait, then holds events, each followed by a wait.
A wait is a variable-length number as in MIDI files: bytes with the top bit
set carry 7 more bits and say another byte follows; the last byte has the top
bit clear. The number counts music steps (see the rules).

An event starts with a byte whose top nibble says what it is (as in MIDI)
and whose low nibble is ignored:

| Top nibble | Bytes after it | What |
| --- | --- | --- |
| `8` | key, velocity | Note off |
| `9` | key, velocity | Note on |
| `A`, `B` | one byte | Nothing |
| `C` | instrument | Instrument change |
| `D` | value | Pressure |
| `E` | value | Pitch bend, 64 is the middle |
| `F` | one byte | End of the list |

## Instruments

40 bytes each. Bytes 2 to 14 describe the first operator of the voice and 15
to 27 the second, each in the usual AdLib order: key scale level, frequency
multiplier, feedback, attack, sustain level, sustaining sound, decay,
release, output level, tremolo, vibrato, key scale rate, and the way the two
operators connect. Feedback and connection are used from the first
operator only.

| Byte | What |
| --- | --- |
| 0, 1 | Unused by the player |
| 2–14 | First operator |
| 15–27 | Second operator; its byte 12 (offset 27) is reused, see below |
| 28, 29 | Wave form of the first and the second operator |
| 30, 31 | How much the note's velocity lowers the first and the second operator's level |
| 32 | How much the velocity raises the feedback |
| 33 | Pitch bend range: 0 for 2 semitones, otherwise 12 |
| 34 | Added to every key |
| 35 | Steps of automatic pitch slide after each note on |
| 36 | Pitch bend added each of those steps |
| 37 | Unused by the player |
| 38, 39 | How much pressure lowers the first and the second operator's level |
| 27 | How much pressure raises the feedback |

## AdLib Gold files

The `.AGD` files are packed too and laid out the same way, with these
differences:

- The header has room for 18 voices: offsets 2 to 37 hold 18 event list
  offsets, still counted from offset 2. Offsets 38 to 43 are zeros.
- Offsets 52 to 83 hold the piece's settings for the card: byte 52 is the
  stereo processor's switch value (its stereo mode), and bytes 53 to 83 are
  the 31 registers of the surround module, in order. The event lists start
  at 84. `CRYOMUS.AGD` has no such block: its event lists start at 52 as in
  the other files, and the player reads its first event bytes as settings.
- Instrument byte 0 is 4 for a four-operator sound: the instrument right
  after it in the file gives operators 3 and 4, in the same layout as
  operators 1 and 2.
- Instrument byte 17 says which sides play the sound: bit 0 left, bit 1
  right.

## MT-32 files

The `.M32` files are packed too (the archive's directory still calls them
`.M32`) and use the same header, with these differences:

- Only the first voice has an event list. It carries the whole piece, for
  every MIDI channel at once: the low nibble of each event's first byte is
  the channel, and the byte goes to the synthesizer as it is.
- There are no instruments: the offset at 0 is the file's size. The pieces
  use the MT-32's own sounds, picked with program changes.
- Events are MIDI messages with their usual lengths: note off and on,
  controller and pitch bend take two bytes after the first, program change
  and channel pressure one. Key pressure (`A`) also takes one, and `F` ends
  the list as in the other files.
