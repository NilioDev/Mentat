# Fonts

`DNCHAR.BIN` holds the game's two bitmap fonts, stored unpacked, 2 304
bytes:

| Offset | Size | What |
| --- | --- | --- |
| 0 | 128 | Large font: width of characters 0 to 127 in pixels, which is how far the next character starts |
| 128 | 128 | Small font: the same for its characters |
| 256 | 9 × 128 | Large font: nine rows per character, from character 0 on |
| 1 408 | 7 × 128 | Small font: seven rows per character |

A row is one byte, the leftmost pixel in the highest bit. A character is
drawn as many columns wide as its width; columns past the eighth have no
bits. Characters from `0x80` on are drawn as `@`.

The large font is the font of the game's text (the story, dialogue). The
small font is the font of the control panel: the commands and the day.

Text is drawn in one of two ways:

- only the set bits, in the text colour; the rest stays as it was;
- with a background colour: every pixel of the character's cell (its width
  × the font's height) is drawn, the clear bits in the background colour.

`DNCHAR2.BIN` has the same size and layout; which language uses it is not
known yet.
