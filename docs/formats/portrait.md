# Portrait animation data

The last entry of a character's picture file (resources 0x02 to 0x0F, and
the others the game shows as faces) is not a picture but the animation
data of the portrait. Offsets below count from the start of the entry's
data, after its 4-byte sprite header.

| Offset | Size | What |
| --- | --- | --- |
| 0 | 4 × 2 | The area the portrait covers: left, top, right, bottom |
| 8 | 2 | w0: where the sequence table starts, less 8 |
| 10 | 2 × n | The parts table: for part number p (from 2), the word at 10 + 2 × (p − 2) is the part's place, counted from offset 10 |

## Parts

A part is a list of sprites ending with a 0 byte. Each sprite is three
bytes: the sprite number from 1 (so sprite s − 1 of the file), then x and y
from the area's top-left corner.

## Sequences

The sequence table at 8 + w0 is a list of words, each the place of a
sequence counted from the table. The last entry is the mouth table.

A sequence is a list of frames ending with `0xFF`. A frame is a list of
part numbers ending with 0; a byte 1 means the next byte is a part number
from 256 up (256 + that byte).

## Mouths

The mouth table holds pairs of bytes: a part number, then 0. Pair k is
mouth shape k.
