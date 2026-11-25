# CD images

Mentat can copy its two game files, `DUNE.DAT` and `DNCDPRG.EXE`, out of an
image of the player's CD. The reader is in `src/core/src/data/cd_image.cpp`;
it follows the public ISO 9660 standard and reads only what that needs.

## Sectors

Every sector holds 2048 bytes of data. A plain image (`.iso`) stores just
those bytes, one sector after another. A raw image (`.bin`) stores 2352 bytes
a sector: twelve sync bytes (`00`, ten times `FF`, `00`), three address
bytes, a mode byte, then the data. In mode 1 the data starts at byte 16; in
mode 2 (form 1) at byte 24, after an eight-byte subheader. The rest is error
correction, which Mentat does not need.

Mentat tells the two apart by looking for the sync bytes at the start of
sector 16 in the raw layout.

## Volume descriptors

From sector 16 on, each sector is a descriptor: a type byte, then `CD001`.
Type 1 is the primary descriptor, type 255 closes the list. In the primary
one, the record of the top folder starts at byte 156.

## Folder records

A folder is a run of records. Each one starts with its length; a length of
zero means the rest of that sector is empty, as records never cross from one
sector into the next. In a record (numbers little-endian):

| Byte | Size | Meaning |
|------|------|---------|
| 0    | 1    | length of the record |
| 2    | 4    | first sector of the file |
| 10   | 4    | size of the file in bytes |
| 25   | 1    | flags: bit 1 set for a folder |
| 32   | 1    | length of the name |
| 33   | n    | the name |

Names end in a version, `;1`, which Mentat drops, as it does a trailing dot.
The records for the folder itself and the one above have the one-byte names
0 and 1. A file's bytes take whole sectors, one after the other, from its
first sector.

## Checks

The reader refuses an image without `CD001` descriptors, a top folder over
1 MB, and a file that would go past the end of the image. A copy that fails
halfway is removed, so it never looks like a whole game file.
