# Fonts

`DNCHAR.BIN` and `DNCHAR2.BIN` are fonts of 256 characters, 8 pixels high.
They are stored unpacked, 2 304 bytes each:

| Offset | Size | What |
| --- | --- | --- |
| 0 | 256 | Width of each character in pixels, which is how far the next character starts |
| 256 | 256 × 8 | Eight rows per character, one byte per row, the leftmost pixel in the highest bit |

Only set bits are drawn; the rest stays as it was.
