# Saved games

The original saves a game as `DUNE37S1.SAV` or `DUNE37S2.SAV` (the logs of
the globe's lists), and on its own as `DUNE37S0.SAV` (when the program
starts); the list for loading also offers `DUNE37S3.SAV` and
`DUNE37S4.SAV` ("last entering into a place", "last entering new sietch"). Mentat reads and writes the same files, so that a
game saved by either can be loaded by the other.

Words are little-endian.

## The file

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 | The clock (state offset 2), for the lists of saved games |
| 2 | 1 | The marker byte that starts a run: `0xF7` |
| 3 | 1 | The count of the last run written (of no use) |
| 4 | 2 | n: the length from offset 2 to the end of the file |
| 6 | n − 4 | The contents, packed |

The packed bytes are read in turn: a byte other than the marker is itself;
the marker is followed by a count and a byte, which stands for that many of
that byte. The original packs every run of three or more equal bytes (at
most 255 to a run), and any run of the marker byte, that way; runs of one
or two other bytes stay as they are.

Mentat refuses a file whose length goes past its end, whose last run is cut
short, or whose contents unpack to anything but `0x567A` bytes.

## The contents

| Offset | Size | What |
| --- | --- | --- |
| `0x0000` | `0x317F` | Who holds the map's areas: bits `0x30` of each of the first `0xC5FC` map bytes, four to a byte, the first in the top two bits |
| `0x317F` | `0xA2` | Bytes `0xAA` to `0x14B` of the program's code part (below) |
| `0x3221` | `0x11F8` | The dialogue lists with what has been said (see [dialogue.md](dialogue.md)) |
| `0x4419` | `0x1261` | The state (see [state.md](state.md)) |

- **The map**: the map is `0xC5F9` bytes, so the last packed byte also
  holds three bytes of whatever follows the map in the original's memory.
  Mentat writes 0 for them and leaves them out when loading.
- **The program's bytes**: the original keeps these bytes of its own
  code part, which hold code it runs only when it starts and four bytes it
  writes then; on loading it puts them back. Mentat writes them as the
  player's `DNCDPRG.EXE` holds them and leaves them out when loading.
- **The dialogue lists**: as the original keeps them in memory at data
  offset `0xAA76`: the words of the table at their start (as many as the
  first word, as in the file, counts in bytes) have `0xAA76` added. The
  file is shorter than `0x11F8` bytes; the rest is 0.
