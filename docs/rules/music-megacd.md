# The Mega CD music

The Mega CD version of the game has music of its own, played on the Mega
Drive's FM chip (a YM2612) by a sound program that runs on the console's
Z80. The player's main processor, a 68000, loads that program, hands it
each piece, and feeds it more of the piece as it plays. Mentat does the
68000's part itself, as described here. The Z80 program and the pieces
come from the player's own Mega CD disc, and they run on two emulators:
Ivan Kosarev's Z80 and Nuke.YKT's Nuked-OPN2. Mentat's version is
`src/core/src/audio/megacd_music.cpp`.

The notes were made by running the Z80 program in a measuring rig, with the
68000's side done by hand until what came out of the chip matched, and by
reading the 68000's sound routines to see what they write and when.

## Where it is on the disc

Everything is in the disc's `DUNE.DAT`. Its numbers are big-endian, and
its sectors are counted from the start of `DUNE.DAT`, 2048 bytes each.

- **File table:** six bytes for file *n* at 0x19354 + 6*n*: the first
  sector (3 bytes), then the size (3 bytes).
- **The sound program:** in file 1, the main processor's program, from
  byte 0x24AA to the end of the file (0x1F73 bytes). It belongs at the
  start of the Z80's memory. Its first instruction jumps to where it
  starts.
- **The pieces:** a list of nine file numbers, 2 bytes each, at 0x18168:
  the pieces 1 to 9.

Mentat copies these out when the player gives it an image of the disc
(see `importMegaCdMusic`), as `MCDPROG.BIN` and `MCDSONG1.BIN` to
`MCDSONG9.BIN`, kept beside the game files.

## A piece

A piece's numbers are little-endian, as the Z80 reads them.

| Offset | What |
| --- | --- |
| 0 | where a block of 0xF74 bytes starts (the piece's sounds) |
| 2 | 24 words: the offsets of its parts, counted from byte 2; the first nine are the parts played, 0 for none |
| 0x32 | the tempo |
| 0x34 | the first part's notes, and on |

The piece is read from a buffer of 0x80F0 bytes. What lies past the
file's end is zero in Mentat; the console has whatever was there before.

## The Z80's memory

| Address | What |
| --- | --- |
| 0x0003 | the command; the program clears it when it has taken it |
| 0x0004-0x0009 | the command's arguments |
| 0x000A | set to 0xFF when more of the piece has been written |
| 0x000C | nine windows of 0x18 bytes: the part of each piece's part that the program reads now |
| 0x00E4 | a copy of the piece's 24 part offsets |
| 0x0114 | the tempo, as the timer wants it |
| 0x0120 | for parts 1-5, two bytes each: the offset in the part the program wants next, plus 1; 0 when it wants nothing |
| 0x01DE | for parts 1-5: where the program finds the window, as an offset into the part |
| 0x01F2 | as 0x0120, for parts 6-9 |
| 0x02B0 | as 0x01DE, for parts 6-9 |
| 0x02BA | the piece's sounds (0xF74 bytes) |
| 0x125F | bit 7 set while a piece plays |

The program keeps its own time on the FM chip's timer B. It reads the
chip's status to know when the timer has run out.

## Commands

The main processor writes a command's arguments, then the command.

- **1, where to call:** bytes 4-7 hold a main-processor address (big-endian).
  The program writes there through the Z80's window onto the main
  processor's memory: 0xFF when it wants more of the piece, and its state
  in the next byte. Mentat gives it 0xFFFF24A0, the address the game uses.
- **3, play:** the piece must be in place first (below). Byte 8 is the
  piece's number. Byte 9 is how quiet it plays: 0 is the loudest, 0x7F is
  silent. The game sends 0x7F less eight times its music volume (0-15).
- **4, stop.**
- **6, fade:** byte 8 is the quietness to reach. Byte 9 is a pattern of
  bits that the program turns one step each beat, moving the volume one
  step when the bit that comes round is set. 0xFF moves at every beat.

### Putting a piece in place

Before command 3, the main processor:

1. copies the block of sounds (0xF74 bytes from the offset at byte 0) to
   0x02BA;
2. for each of the nine parts that has an offset, copies the part's first
   0x18 bytes to its window;
3. copies the 24 part offsets to 0x00E4;
4. puts the tempo at 0x0114: 256 − tempo × 76 / 1152 (whole numbers,
   rounded down).

### More of the piece

At each frame (60 a second), when the program has set its byte to 0xFF,
the main processor goes over the nine parts in order. For each part whose
word at 0x0120 (or 0x01F2) is not 0, with *v* that word:

- its window's place, less *v* − 1, goes in the next word of 0x01DE.
  After the fifth word read, it goes in 0x02B0 instead. The words are
  written only for the parts that asked;
- 0x18 bytes of the part, from offset *v* − 1, are copied to its window.

Then it writes 0xFF at 0x000A and clears the byte the program set.

Commands and refills happen at frames: Mentat sends one waiting command a
frame, when the program has taken the last one.

## Which piece plays

The Mega CD pieces 1 to 9 stand where the CD version plays its tracks 1
to 9: SEKENCE, WATER, WORMSUIT, WORMINTR, WARSONG, MORNING, SIETCHM,
BAGDAD and ARRAKIS. This was matched by comparing the pitches each piece
uses with those of the CD version's AdLib Gold pieces: the order holds
for most of them. Track 10 (the Cryo logo's music) has no Mega CD piece
and plays nothing.

## The game's volume

The game's music volume byte (0-255) becomes a quietness of
(255 − volume) / 2, at most 0x7F. A change while a piece plays is sent
as a fade with pattern 0xFF. The game's fades use the pattern 0xFF when
they last under 60 ticks, 0x55 under 240, and 0x11 when longer.
