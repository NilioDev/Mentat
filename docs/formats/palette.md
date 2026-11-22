# Palette blocks

The original draws with the 256-colour VGA palette. Picture files and
videos carry *palette blocks* that change part of it. Each colour is three
bytes, red, green and blue, from 0 to 63 (the VGA's 6 bits per channel).

## Layout

A list of blocks, each starting with a 16-bit little-endian word:

| Word | Meaning |
| --- | --- |
| `0xFFFF` | End of the list. |
| `0x0100` | A block for colour 0 alone. Its 3 colour bytes follow, but the original never applies them: colour 0 keeps its value. |
| anything else | Low byte: first colour. High byte: number of colours (0 means 256). That many 3-byte colours follow. |

To show a 6-bit value on a modern screen it is scaled to 8 bits by
`(v << 2) | (v >> 4)`, which maps 63 to 255 the way a VGA card does.

## Reading it safely

A block that would run past colour 255 or past the end of the data is
refused.
