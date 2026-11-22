# The state of a game

The original keeps the state of a game at the start of its data part (see
[program.md](program.md)): the first `0x1261` bytes. A new game starts from
the values the program file holds there; Mentat reads them from the
player's own `DNCDPRG.EXE`, keeps them as the same bytes, and names the
fields it knows. A saved game holds these bytes too, and the dialogue
lists with what has been said (see [dialogue.md](dialogue.md)); its other
parts are described when Mentat reads saved games.

Offsets are data offsets; words are little-endian.

| Offset | Size | What |
| --- | --- | --- |
| `0x02` | 2 | The clock: parts of a day, 16 to a day (see [../rules/game.md](../rules/game.md)) |
| `0x04` | 2 | The player's room: high byte the kind of rooms, low byte the room from 1 |
| `0x06` | 2 | The player's place: high byte its number from 1, low byte `0x80` when inside |
| `0x08` | 1 | The kind of rooms of the player's room |
| `0x0E` | 2 | Characters Paul has talked to, a bit each (also kept at `0x14`) |
| `0x10` | 2 | Characters to leave out of the room, a bit each; also those who go with Paul |
| `0x12` | 2 | Characters in the player's room, a bit each, worked out on entering |
| `0x16` | 2 | While talking: the clock since the character last joined or left Paul |
| `0x18` | 1 | While talking: the character's flags |
| `0x19` | 1 | Set when a line has been said |
| `0x1B` | 1 | Counts "come with me" and "stop following me" |
| `0x23` | 1 | While asking a character to come: 2 when Paul can pay, else 0 |
| `0x2A` | 1 | How far the story has come |
| `0x2B` | 1 | Set once the war with the Harkonnen has begun |
| `0xFF` | 1 | Cleared when the story moves on |
| `0xC8` | 1 | Messages received |
| `0xC9` | 1 | Messages not seen yet |
| `0x100` | `0x1C` each | The places of Arrakis (see [../rules/places.md](../rules/places.md)) |
| `0xFD8` | 16 × 16 | The characters |
| `0x1152` | 2 | Which character each of the panel's two slots holds, `0xFF` for nothing |
| `0x11DB` | 2 | A place the characters talk about; its name and kind go to `0x11ED` and `0x11EF` |
| `0x11EB` | 2 × 15 | Phrases that lines name with codes `0x81` to `0x8F` (see [text.md](text.md)) |

A new game starts in room `0x200A` (the throne room) of place `0x0180`
(the palace, inside), at clock 2, with Leto there.

## Places

Of a place's `0x1C` bytes, these are known so far:

| Offset | Size | What |
| --- | --- | --- |
| 8 | 1 | The kind of place, which picks its rooms |
| `0x15` | 1 | Ornithopters kept there; a new game gives the palace one |

## Characters

16 records of 16 bytes:

| Offset | Size | What |
| --- | --- | --- |
| 0 | 2 | The room the character is in |
| 2 | 2 | The place the character is in |
| 4 | 2 | The original's number for talking to the character |
| 8 | 2 | The clock when the character last joined Paul |
| `0x0A` | 2 | The clock when the character last left Paul |
| `0x0E` | 1 | The character's number, which is also its bit in the masks above |
| `0x0F` | 1 | Flags: bit 7 will not go with Paul; bit 6 goes with Paul (and is named after the others in the list of commands); bit 5 Paul has talked to it |

Character 0 is Leto; the list of commands names character n with phrase
`0x78` + n of the command file.
