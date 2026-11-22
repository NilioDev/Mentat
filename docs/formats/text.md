# Text files

The spoken lines, the menu words and the place names are in packed files
(see [hsq.md](hsq.md)), one pair or one file per language:

| File | What |
| --- | --- |
| `PHRASExy.HSQ` | What the characters say. `x` is the language, `y` is part 1 or 2. |
| `COMMANDx.HSQ` | Menu words, place names, troop jobs and other short texts. |

The language number is 1 for English, then French, German, Spanish,
Italian, Swedish and another, in the order of the original's language
choice.

## Layout

All numbers are little-endian.

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 × n | Offsets of the n strings, counted from the start of the file |
| … | … | The strings |

The first offset points just past the table, so it is also twice the
number of strings.

Each string ends with the byte `0xFF`. Inside a string:

- `0xFE` starts a new page: the text so far is shown, and the rest comes
  after a click or when the voice reaches it.
- `0x0D` starts a new line.
- Bytes from `0x80` up are codes for what the game fills in, described
  below for the phrase files. Bytes 1 to 8 take no room when the text is
  laid out; 6 and 8 switch fonts.

## The phrase files

In `PHRASExy.HSQ` the last entry of the offset table is not a phrase but a
list of words and pieces of text that the phrases share. Inside a phrase a
byte from `0xE0` to `0xFD` names one of those pieces, with the word that
follows it: the piece starts at (word & `0x3FFF`) from the start of the
list, and its length is the byte's low 4 bits plus the word's bits 14 and
15 as bits 4 and 5. A space follows the piece unless the phrase ends
right after it. The original makes a phrase at most 399 bytes long this
way.

Then these codes are filled in:

| Code | Then | What goes in |
| --- | --- | --- |
| `0x80` | a word, high byte first | another phrase (see below) |
| `0x81` to `0x8F` | | another phrase, named by the state's word at `0x11EB` + 2 × (code − `0x80`) |
| `0x90` to `0x9F` | an offset (1 byte) | the state's byte there (its word for `0x92`), in decimal without leading zeros |
| `0xA0` to `0xCF` | | kept; it ends the text when it is laid out |
| `0xD0` | 2 bytes | kept with its bytes |
| `0xD1` | 4 bytes | kept with its bytes |
| `0xD2` to `0xEF` | 1 byte | kept with its byte |
| `0xFE` | | a new page |
| `0xFF` | | the end |

A phrase named by number n is string n − 1 of the phrase file the line came
from when bit 11 of n − 1 is set (the low 11 bits being the string), and of
the command file otherwise. It is put in as the file has it, without its
pieces filled in, up to its first byte from `0xF0` on. Its own codes are
filled in too.

Inside the text, byte `0x06` switches to the small font and `0x08` back to
the large one; the small font marks what a character thinks rather than
says.
