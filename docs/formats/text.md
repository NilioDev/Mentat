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
- Bytes below `0x20` (other than `0x0D`) and from `0x80` up are letters of
  the original's own font, such as accented letters, and some stand for words
  the game fills in. Which is which is described where the text is drawn.
