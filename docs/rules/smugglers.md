# Buying from a smuggler

In a village the smuggler stands in the room (see [people.md](people.md)),
and his record (from `0x10D8`, `0x11` bytes each) is the one the state
speaks of (word `0x10B4`). His lines are character 13's; what they do is in
their actions (see [talking.md](talking.md)).

The record, by byte:

| Bytes | What |
| --- | --- |
| 0 | The village's first name |
| 1 | His patience: how long he gives way when Paul argues |
| 2 | Flags: bit 3 once met, bit 5 his bill argued over, bit 6 refused |
| 3 | The day he last made an offer |
| 4 to 8 | How many of each kind of equipment he has |
| 9 to `0x0D` | The price of each kind, in tens of kilos of spice, halved (bit 7 not counted) |
| `0x0E` | The word of his bill |
| `0x10` | The day of his last sale |

## His offer (action 8)

The story moves on to step `0x3C` at least (as with action 12). Byte `0x9E`,
which his lines look at, takes a number from the first generator masked
with 3; byte 3 of his record takes the day (clock / 16) and byte `0x1A`
becomes 0.

Then the kind he offers moves on from the one word `0x11F1` names
(`0xE8` + the kind): the next kind he has any of, the kinds counted by
state byte `0x1141` and gone round at most twice. When there is none,
nothing else changes. Otherwise word `0x11F1` names it and byte `0x9D`,
the price, becomes twice his price for it.

## Asking

The line naming the price asks a question (action 4, see
[talking.md](talking.md#questions)), and Paul answers ACCEPT, REFUSE or
ARGUE. With the smuggler the answer does more before byte `0x9F` takes it:

- ACCEPT: bits 5 and 6 of his byte 2 go. The price goes on word `0x20` and
  on his bill, and byte `0x9D` becomes 0; when the bill was 0 before, one
  more bill is counted at `0x22`. His byte `0x10` takes the day, he has one
  fewer of the kind, and the place Paul is in (word `0x114E`) one more, at
  its byte `0x14` + the kind. The answer is 1.
- REFUSE: a number from the first generator masked with 7. When it is 0,
  byte `0x9E` gets bit 4 and the answer is 3, as if Paul had argued.
  Otherwise the price becomes 0 and the answer is 2.
- ARGUE: a number r from the first generator masked with 3. When it is 0,
  byte `0x9E` gets bit 4. Otherwise `0x9E` becomes (`0x9E` + 1) and 3, and
  when (r and 1) + byte `0x1A` (as a byte) is below his patience the
  price goes down by an eighth of itself (rounded down); else it becomes 0.
  The answer is 3 either way.

Then, as for any question, `0x1A` counts one more, the talk menu comes
back and the talk goes on as TALK TO ME does.

The original's voice says the price; it is not written. Mentat writes it
beside the face where the label table puts it, as the price × 10 and
" kgs" (see [talking.md](talking.md#a-few-words-beside-the-face)).

Action 9 does nothing for the smuggler. Paying his bill is Duncan's (see
[shipments.md](shipments.md)).
