# The program file, DNCDPRG.EXE

The original keeps its rules, its starting state and many tables inside its
program. So that none of these have to be copied into Mentat, Mentat reads
them from the player's own `DNCDPRG.EXE` when it starts. It never runs the
program.

Mentat knows one version: 78700 bytes, CRC-32 `B628E0FF` (the CD version
3.7). Any other file is refused with a message.

## Layout

The file is a DOS program: a 512-byte header (`MZ`), then the program as it
is loaded. In the loaded program:

- the code starts at offset 0;
- the data part starts at offset `0xF4B0` and the file holds its first
  `0x3CBC` bytes; the rest of the data part (up to 64 KB) starts as zeros
  when the original runs.

Below, a "data offset" counts from the start of the data part, a "code
offset" from the start of the code.

## Tables

### Videos (data offset `0x33A3`)

37 16-bit data offsets, one per video slot. Each points to:

| Offset | Size | What |
| --- | --- | --- |
| 0 | 1 | Flags |
| 1 | 1 | Timer ticks between frames when the frame carries no sound |
| 2 | … | File name in `DUNE.DAT`, ending with a zero byte (empty for slot 0) |

Flags, as far as they are known:

| Bit | What |
| --- | --- |
| 0 | Loops back to the start |
| 2 | A room background whose first 16 bytes of each frame offset are skipped |
| 3 | Has a loop record just before the entry |
| 4, 5 | Drawn by the scene renderer instead of as a plain picture |
| 6 | Unpacked into a buffer of its own instead of the back buffer |
| 7 | Read ahead from the disc while playing |

Slots 25 and up (Irulan's introduction and the story sequences) are the
full-motion videos, drawn with each pixel in every second column and row.

| Slot | File |
| --- | --- |
| 1 | `DFL2.HNM` |
| 2–5 | `MNT1.HNM` – `MNT4.HNM` |
| 6 | `SIET.HNM` |
| 7, 8 | `PALACE.HNM` |
| 9, 10 | `FORT.HNM` |
| 11, 12, 13 | `DEAD3.HNM`, `DEAD.HNM`, `DEAD2.HNM` |
| 14 | `VER.HNM` |
| 15 | `TITLE.HNM` |
| 16–18 | `MTG1.HNM` – `MTG3.HNM` |
| 19 | `PLANT.HNM` |
| 20 | `CREDITS.HNM` |
| 21–24 | `VIRGIN.HNM`, `CRYO.HNM`, `CRYO2.HNM`, `PRESENT.HNM` |
| 25 | `IRULAN.HNM` |
| 26–36 | `SEQA`, `SEQL`, `SEQM`, `SEQP`, `SEQG`, `SEQJ`, `SEQK`, `SEQI`, `SEQD`, `SEQN`, `SEQR` |

### Scene drawing (data offsets `0x1470` and `0x2774`)

At `0x1470`, four signed 16-bit numbers: the rectangle that lines in a
scene are clipped to, as left, top, right and bottom, where right and
bottom are one past the last column and row. In version 3.7 it is 0, 0,
320, 152: the part of the screen above the control panel.

At `0x2774`, eight 16-bit divisors in 8.8 fixed point for shrinking
sprites in a scene; entry 0 is `0x100` (full size) and the others grow
from there. See [scene.md](scene.md).
