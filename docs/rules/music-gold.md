# Music on the AdLib Gold

How the original's AdLib Gold music player (`DNADG.HSQ`) plays the `.AGD`
files (see [../formats/music.md](../formats/music.md)), and how Mentat turns
the card's output into sound. The player works like the Sound Blaster Pro
player in [music.md](music.md): the same steps, measures, events, velocity,
pressure, bends and fades. This page lists only what is different.

Mentat reads the player's tables from the player's own `DNADG.HSQ` (see
"Tables" below), feeds the same register writes to an emulated OPL3 chip,
and models the card's surround module and stereo processor around it. With
the game files of a CD that has no `DNADG.HSQ`, Mentat falls back to the
Sound Blaster Pro player and the `.HSQ` music.

## The chip

The card has an OPL3: two register sets of 9 channels each (the second set
at registers `0x100` and up), stereo output, and channels that can be
joined in pairs for four-operator sounds. The player has 18 voices, played
in order 0 to 17 in every step.

When the game loads it, the player:

1. writes `0x20` to register `0x01`, 0 to `0xBD` and `0x40` to `0x08`;
2. turns on the OPL3's own mode (`0x105` = 1) and turns off every
   four-operator pair (`0x104` = 0);
3. sets the card: bass `0xFB`, treble `0xF7`, both volumes `0xF7`, both FM
   volumes `0xFF` (see "The card" below);
4. stops, as below.

## Channels for voices

A voice has no fixed channel. Every instrument change, even to the
instrument the voice already has, gives the voice channels again: first
whatever it held is free, then the player searches, in this order:

1. For a four-operator instrument: a free pair of channels c and c + 3, for
   c from 0 to 2, in the first register set and then the second.
2. Otherwise, or when no pair is free: the highest free channel of 8, 7 and
   6 in the first set, then the same in the second.
3. In the second set, then in the first: the lowest pair (c, c + 3, c from
   0 to 2) of which exactly one channel is in use. The voice takes c when it
   is free, otherwise c + 3.
4. In the second set: the highest pair with a free channel, taken the same
   way.
5. When everything is in use: the first set the same way, so two voices
   share a channel.

Only the pair search gives a voice two channels. In every other case it
gets one, and a four-operator instrument plays with its first two operators
only.

A channel's operators are at offset (c / 3) × 8 + c mod 3 and 3 above it;
the paired channel's operators are 8 above those.

A voice gives its channels back:

- at a note off, when the wait after it is at least `0x30` steps and the
  next event is an instrument change or the end of the list;
- at the end of its list, for every voice but voice 0;
- when a piece starts, and when it starts again after a play.

## Instruments

- **Four operators.** An instrument whose byte 0 is 4 takes operators 3 and
  4 from the instrument after it. When the voice got a pair, the player
  writes the next instrument's two operators to the paired channel the way
  it writes the first two, and turns that pair's bit on in register
  `0x104`. The bit is (first operator offset & 3), plus 3 in the second set.
  Any other instrument turns the bit off. Channels 6 to 8 have no such bit
  and are left alone. Velocity and pressure then change all four operators'
  levels, each with its own amount.
- **Sides.** Byte 17 (the second operator's feedback) is not a feedback:
  it gives the channel's `0xC0` bits 4 and 5, so bit 0 plays the sound on
  the left and bit 1 on the right. Velocity and pressure change the
  feedback in the low four bits and keep these.
- **Wave forms** use 3 bits (the OPL3's eight wave forms), not 2.

## Starting and stopping

Starting a piece sets the release of every operator of both register sets
to the fastest, from operator offset `0x15` down to 0, both sets for each
offset. It then sends the piece's settings to the card: byte 52 to the
stereo processor's switches, and bytes 53 to 83 to the surround module's 31
registers. `CRYOMUS.AGD` has no settings, so its first event bytes go to
the card instead; Mentat does the same.

Stopping, and the end of a fade, write every voice's frequency again
without the key bit, from voice 17 down. Before a voice's first note that
frequency is the one from the player's table (`0x157` for every voice).

## Volume

The player sets the card's two FM volumes (control registers 9 and `0x0A`)
in place of the Sound Blaster Pro's mixer: for a level n from 0 to 15, the
value is n × 2 + 1 with bit 7 set. The high level goes to register 9.

## The card

The card's control chip takes register writes for:

| Register | What |
| --- | --- |
| 4, 5 | Volume left, right (stereo processor) |
| 6 | Bass (stereo processor) |
| 7 | Treble (stereo processor) |
| 8 | Switches: the stereo mode and the input (stereo processor) |
| 9, `0x0A` | FM volume left, right: bits 0 to 4 |
| `0x18` | The surround module's serial line |

The stereo processor is a TDA8425 and the surround module a YM7128. The
pieces' switch values are `0xD6` (pseudo stereo), `0xDE` (spatial stereo,
in `MORNING` and `WORMSUIT`), and 0 for `CRYOMUS`.

## Mentat's choices for the sound

The card's analogue side is not measured. Mentat's model:

1. The OPL3's stereo output, at its own rate of 49716 frames a second, with
   full scale as 1.
2. Each side times its FM volume, taken as linear: (value & 31) / 31.
3. The surround module gets the two sides' mean; its left and right output
   is added to the direct sound at full level.
4. The stereo processor applies volume, bass, treble and stereo mode. Its
   pseudo stereo capacitors are not known for the card; Mentat uses 15 nF
   for both. A stereo mode with mode bits 1 and 2 both 0 is not defined for
   the chip; Mentat plays it as plain stereo.
5. The result times 0.75, which brings it near the loudness of the Sound
   Blaster Pro music.

## Tables

The player keeps its tables at fixed places in `DNADG.HSQ` (unpacked):

| Offset | Size | What |
| --- | --- | --- |
| `0x42` | 2 × 12 | Frequency numbers of the 12 semitones |
| `0x5A` | 2 × 18 | Each voice's frequency before its first note |
| `0xC7` | 13 | Distances between semitones, as in [music.md](music.md) |
| `0xD4` | 5 | Fine bend amounts for semitones 0 to 5 |
| `0xD9` | 5 | Fine bend amounts for semitones 6 to 11 |
