# HNM videos and animated scenes

`.HNM` files hold the intro, the company logos, the full-motion video
sequences, and the animated backgrounds of some rooms. They are stored
unpacked in `DUNE.DAT`; their pictures are packed frame by frame.

All numbers are little-endian.

## Header

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 | Size of the header in bytes |
| 2 | … | Palette blocks (see [palette.md](palette.md)) ending with `0xFFFF`, possibly none |
| … | … | `0xFF` padding |
| … | 4 × n | A table of 32-bit offsets of frames, counted from the start of the file, used to jump into a video |

Frames follow the header one after the other until the end of the file.

## A frame

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 | Size of the frame in bytes, these two included |
| 2 | … | Chunks, then the picture |

A chunk starts with a two-letter tag and a 16-bit size that counts the tag
and the size too:

- `sd`: sound for the frame (the voice and music of the video sequences).
- `pl`: palette blocks, applied before the picture is shown.

The picture comes after the last chunk and begins with two words:

| Word | Bits | What |
| --- | --- | --- |
| first | 0–8 | Width |
| | 9 | The rest of the picture is HSQ packed |
| | 10 | The whole picture is HSQ packed and fills the width from the top-left corner |
| | 13–15 | As in a sprite: upside down, mirrored, rows packed |
| second | 0–7 | Height |
| | 8–15 | Mode, as in a sprite |

- **Bit 10 set:** an HSQ stream follows (see [hsq.md](hsq.md)) that unpacks
  to width × height bytes, one per pixel, drawn from the top-left corner of
  the picture area.
- **Otherwise:** the rest is x (16 bits), y (16 bits) and the sprite's pixel
  rows (see [sprite.md](sprite.md)), HSQ packed as a whole when bit 9 is
  set. The two words above act as the sprite's header once bits 9 and 10 are
  cleared, and the sprite is drawn at (x, y).
- A frame whose picture is only the two words, or whose first word is 0,
  draws nothing: the previous picture stays.

## Full-motion video

The video sequences (Irulan's introduction and the story sequences) are 160
pixels wide. The original draws each of their pixels in every second column
and every second row, so on screen they cover twice the size with black
pixels in between.
