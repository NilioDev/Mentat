# Dialogue and conditions

What the characters can say lives in two packed files (see
[hsq.md](hsq.md)): `DIALOGUE.HSQ` holds the lists of lines, `CONDIT.HSQ`
the conditions that decide when a line may be said. The words themselves
are in the phrase files (see [text.md](text.md)). How the game uses all
this is in [../rules/talking.md](../rules/talking.md).

All numbers are little-endian unless said otherwise.

## DIALOGUE.HSQ

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 × n | Where each list starts, counted from the start of the file; `0xFFFF` for no list |
| … | … | The lists |

There are 136 lists: 8 for each of 17 characters (list 8c + k is
character c's list k), the last one being the story's own list (see the
rules). A list is a run of 4-byte lines ending with the word `0xFFFF`.

| Byte | What |
| --- | --- |
| 0 | Flags: bit 7 said, bit 6 may be said again, bit 5 may be said again when the character runs out of lines, bit 4 one of several recordings; bits 0 to 3 what saying the line does (0 for nothing) |
| 1 | The condition number, low 8 bits |
| 2 | Bits 6 and 7: the condition number's bits 8 and 9. Bit 4: there is no recording of the line. Bits 2 and 3: the line is noted down. Bits 0 and 1: the phrase number's bits 8 and 9 |
| 3 | The phrase number, low 8 bits |

The phrase number counts from 1. Characters 0 to 5 take it from the first
phrase file of the language, characters 6 and on (all lines from the start
of list 48) from the second.

The game keeps this file in memory as part of a game's state, its first
table turned into memory addresses (the file's offsets plus where it is
loaded), and only bit 7 of byte 0 ever changes. A saved game holds the
whole of it.

## CONDIT.HSQ

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 × n | Where condition n + 1 starts, counted from the start of the file |
| … | … | The conditions |

A condition is a small formula over the game's state (see
[state.md](state.md)), worked out to a word; it holds when the word is
not 0. It is a value, then pairs of an operator and a value, ending with
the byte `0xFF`.

A value is one of:

| First byte | Then | Value |
| --- | --- | --- |
| 1 | an offset (1 byte) | the state's byte there |
| 0 or 2 to `0x7F` | an offset (1 byte) | the state's word there |
| `0x80` | 1 byte | that byte |
| `0x81` to `0xFE` | 2 bytes | that word |

An operator below `0x80` combines the value so far with the next one
(true is `0xFFFF`, false 0):

| Operator | Result |
| --- | --- |
| 0 | equal |
| 2 | less than, unsigned |
| 4 | greater than, unsigned |
| 6 | not equal |
| 8 | less than or equal, signed |
| `0x0A` | greater than or equal, signed |
| `0x0C` | sum |
| `0x0E` | difference |
| `0x10` | bitwise and |
| `0x12` | bitwise or |
| others | false |

An operator with bit 7 set closes a part: the value so far is kept with
the operator and the next value starts a new part. At the end the parts
are joined from the left, each kept operator (its low 5 bits, as in the
table) joining the result so far with the next part. At most 12 parts.
