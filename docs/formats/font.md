# Fonts

`DNCHAR.BIN` and `DNCHAR2.BIN` are bitmap fonts, stored unpacked, 2 304
bytes each.

`DNCHAR.BIN`, the font of the game's text:

| Offset | Size | What |
| --- | --- | --- |
| 0 | 256 | Width of each character in pixels, which is how far the next character starts |
| 256 | 9 × n | Nine rows per character, from character 0 on, one byte per row, the leftmost pixel in the highest bit |

The 2 048 bytes after the widths hold the rows of characters 0 to 226 and
part of 227; the characters after them are not in the file.

Only set bits are drawn; the rest stays as it was.

`DNCHAR2.BIN` holds a second, smaller font whose layout is not described
yet.
