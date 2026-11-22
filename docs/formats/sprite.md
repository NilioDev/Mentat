# Picture files and sprites

Most `.HSQ` files in `DUNE.DAT` are picture files: room backgrounds,
character portraits with their eyes and mouths, icons, the map's pieces.
After unpacking (see [hsq.md](hsq.md)) a picture file looks like this:

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 | Offset of the sprite list |
| 2 | … | Palette blocks (see [palette.md](palette.md)), ending with `0xFFFF`; may be absent |
| list | 2 × n | Sprite list: n offsets, counted from the start of the list |

The first offset in the list is also the size of the list, so the number of
sprites is that first value divided by two. A file whose list starts at
offset 2 carries no palette; it is drawn with whatever palette is loaded.

## A sprite

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 | Width in bits 0–8; bit 13: drawn upside down; bit 14: drawn mirrored; bit 15: rows are packed |
| 2 | 1 | Height |
| 3 | 1 | Mode |
| 4 | … | Pixel rows, top to bottom |

The mode decides how a pixel is stored:

- **Below `0xFE`: four bits per pixel.** Each byte holds two pixels, the
  low nibble first. A row holds the width rounded up to a multiple of four
  pixels, so `(width + 3) / 4 × 2` bytes. A nibble of 0 is see-through;
  any other value has the mode added to it to give the palette index.
  The rounded-up pixels at the end of a row are drawn as well.
- **`0xFE`: one byte per pixel, solid.** Every pixel is drawn.
- **`0xFF`: one byte per pixel, colour 0 is see-through.**

### Packed rows

When bit 15 of the first word is set, each row is packed on its own with a
run-length scheme. A control byte *n* comes first:

- *n* below `0x80`: the next *n* + 1 bytes are copied.
- *n* of `0x80` or more: the next byte is repeated 257 − *n* times.

Runs fill the row's stored bytes (as above) and never cross into the next
row.

### Mirroring

A mirrored sprite starts each row at x + width − 1 (the width from the
header, not the rounded-up width) and goes left. One drawn upside down
starts at its bottom row. The game can also ask for mirroring when it draws;
that flips the sprite's own setting.

## Reading it safely

Offsets in the sprite list must point inside the file, and unpacking stops
with an error when a run crosses the end of a row or the data ends early.
