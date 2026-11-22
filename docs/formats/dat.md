# DUNE.DAT: the CD version's archive

The CD version keeps almost all of its files in one big archive, `DUNE.DAT`
(about 398 MB in the English CD version 3.7). It holds the pictures, scenes,
texts, dialogue tables, music, the drivers and the spoken voices.

All numbers are little-endian.

## Layout

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 | Number of slots in the directory |
| 2 | 25 × slots | The directory, one 25-byte entry per slot |
| … | | Up to 65 536: the rest of the directory area |
| 65 536 | | The files, one after the other |

The directory area is always the first 64 KiB of the archive. The slot count
in the header is larger than the number of files in use (2 621 slots in
version 3.7, of which 2 549 are used). The list ends at the first entry whose
name is empty; every slot after it is unused.

## A directory entry

| Offset | Size | What |
| --- | --- | --- |
| 0 | 16 | File name, ASCII, padded with zero bytes |
| 16 | 4 | Size of the file in bytes |
| 20 | 4 | Offset of the file from the start of the archive |
| 24 | 1 | Flag; 0 for every file in version 3.7 |

Names are upper case, in DOS 8.3 form. Some carry a folder in front, written
with a backslash (`PA\PA190I.VOC`): the voices are grouped per speaker that
way. A name is not guaranteed to end in a zero byte when it uses all 16
bytes.

File names in version 3.7 and what they hold:

| Extension | Count | What |
| --- | --- | --- |
| `.HSQ` | 188 | Compressed files: pictures (see [sprite.md](sprite.md)), texts (see [text.md](text.md)), tables, drivers (see [hsq.md](hsq.md)) |
| `.VOC` | 2 288 | Spoken lines and sound effects (see [voc.md](voc.md)) |
| `.HNM` | 37 | Animated scenes and video (see [hnm.md](hnm.md)) |
| `.AGD` | 10 | Music for AdLib Gold |
| `.M32` | 10 | Music for the Roland MT-32 |
| `.BIN` | 5 | Uncompressed tables and the fonts (see [font.md](font.md)) |
| `.LOP` | 6 | Looping scene backgrounds |
| `.SAL` | 4 | Scene layouts |

## Reading it safely

The engine checks each entry before using it: the offset must not lie inside
the directory area, and offset plus size must not run past the end of the
archive. Names are cleaned of padding and must not hold `..`, a drive letter
or a leading slash. An archive that breaks any of these rules is refused.
