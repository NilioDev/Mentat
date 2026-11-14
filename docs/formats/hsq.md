# HSQ compression

Files whose names end in `.HSQ` are packed with a small LZ77-style scheme.
A packed file is a 6-byte header followed by the packed stream.

## Header

| Offset | Size | What |
| --- | --- | --- |
| 0 | 3 | Unpacked size in bytes (24-bit) |
| 3 | 2 | Packed size in bytes, header included |
| 5 | 1 | Check byte |

The check byte is chosen so that the six header bytes added together, modulo
256, give `0xAB`. This is how a packed file is recognised. In version 3.7
the packed size always equals the size of the archive entry, and the top byte
of the unpacked size is always zero.

## The stream

The stream mixes *control bits* with data bytes. Control bits come from a
16-bit little-endian word that is read from the stream whenever the previous
word has been used up; its bits are taken from the lowest bit upwards. A word
is read only at the moment a bit is needed, so it can sit between data bytes.

Each step of the decoder reads one control bit:

- **1**: copy the next stream byte to the output.
- **0**, then **0**: a short copy. Two more control bits give a number
  *n* (first bit is the high bit), and the length is *n* + 2 (2 to 5 bytes).
  The next stream byte *b* gives the distance back into the output:
  256 − *b* (1 to 256 bytes).
- **0**, then **1**: a long copy. The next 16-bit word *w* holds the distance
  in its upper 13 bits: 8192 − (*w* >> 3). Its lowest 3 bits give the length
  minus 2. When those 3 bits are zero, the length minus 2 comes from the
  next stream byte instead; when that byte is zero as well, the stream ends.

A copy reads from the output it is writing, so it may overlap itself (a
distance of 1 repeats the last byte). After the end marker, the output must
be exactly the unpacked size from the header.

## Reading it safely

The decoder refuses a stream that reads past its end, copies from before the
start of the output, or makes more output than the header says.
