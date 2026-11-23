# Spice for the Emperor

When the Emperor asks for spice (see [days.md](days.md#the-emperors-demand-hour-3)),
Duncan (character 3) talks it over with Paul, and the spice goes from the
palace's message room. The lines are Duncan's list 2; what they do is in
their actions (see [talking.md](talking.md)).

## What Duncan proposes (action 8)

State byte `0x9F` becomes 3 when there is spice in stock (word `0xA0`),
else 0; word `0x20` and byte `0x1A` become 0. Then four amounts go into the
words from `0xB4`, by the stock S and the demand D (word `0xBC`), and byte
`0xBF` loses bits 1 and 2:

| Stock | `0xB4` | `0xB6` | `0xB8` | `0xBA` | `0xBF` gets |
| --- | --- | --- | --- | --- | --- |
| S below D | S | S/4 + S/2 | S/2 | (S/4 + S/2) − S/2 | |
| below 1½ D | D | S | S/4 + S/2 | S/2 | bit 1 |
| below 2 D | D | S | S/4 + S/2 | 1½ D | bit 2 |
| 2 D or more | D | S | 1½ D | 2 D | bits 1 and 2 |

Halves and quarters are rounded down, 1½ D is D/2 + D, and everything is
kept to 16 bits.

Then a smuggler's bill: of the smugglers' records (from `0x10D8`, `0x11`
bytes each, until a first byte `0xFF`) with a bill (word `0x0E` not 0) and
neither bit 5 nor bit 6 of byte 2, the one whose byte `0x10` is furthest
behind the day (clock / 16, as a byte; a record the same day counts for
nothing). When there is none, the records are gone through from the one
after that named by word `0x113F`, round to the first, for the next with a
bill, which word `0x113F` then names; back at the one it named, there is no
bill. A bill found becomes the one the state speaks of (as when Paul meets
the smuggler, see [people.md](people.md): words `0x10B4`, `0x20` and bytes
`0x1C`, `0x1D`, `0x1F`), and word `0x11F7` takes the record's first byte.

## Asking (actions 4 and 5)

The lines proposing an amount ask a question, with ACCEPT, REFUSE and
ARGUE (see [talking.md](talking.md#questions)); each shows the amount it
proposes. Byte `0x1A` counts the answers, so the lines' conditions know
which amount came last. The line about the bill asks with action 5.

## The answer (action 9)

By byte `0x9F`:

- 1 (accepted): for an amount, word `0xC0` takes the amount the last
  answer was about, the word at `0xB4` + 2 × ((`0x1A` − 1) and 3), and word
  `0x1158` becomes `0xFFFF`: the shipment waits. For the bill, it is paid:
  the record's word `0x0E` goes from the stock to the spice sent today
  (word `0x1172`), the word becomes 0 and byte `0x22` goes down by one.
- 2 (refused): a bill gets bit 6 of its byte 2 (losing bit 5).
- 3 (argued): a bill gets bit 5 (losing bit 6).

## Sending it

When Paul comes into room 8 (the message room), Duncan is there and words
`0xC0` and `0x1158` have bits in common, the amount A of word `0xC0` is
sent:

1. When Duncan goes with Paul and his flags have bit 1, he stops going
   with him: he loses bit 6, the clock goes to his record's word `0x0A`
   when it is 2 or more past its word 8, his bit leaves word `0x10`, and
   he leaves the panel's slots (`0x1152`, `0x1153`), the other moving up.
2. The stock goes down by A and the spice sent today up by A.
3. The mood: A × 256 / D, at most `0x1FF`, halved, and 1 when that is 0;
   so `0x80` is what was asked for.
4. By the mood: from `0xC0`, 7 days and bit 6; above `0x80`, 6 days and
   bit 6; `0x80`, 5 days; below, 4 days and bit 3, and when byte `0xBF`
   had bit 3 already (short twice running) the mood becomes 0. Byte `0xBF`
   becomes that bit with bit 7 (the demand no longer waits), byte `0xBE`
   the mood.
5. The day of the next demand (word `0x118D`) moves on by those days and
   by a number from the first generator masked with (byte `0xC3` / 2) and
   3; byte `0xCF` takes how many days that is from today.
6. Word `0x1158` becomes 0.

The original then shows the shipment leaving, in the message room's
device; Mentat does not show that yet.

## The Emperor's answer (action 15)

Before story step `0x10`, Duncan's line only sets bit 4 of byte `0xFF7`.
From then on the talk ends after it, word `0xC0` becomes 0, byte `0xBF`
gets bit 0, and news comes from the Emperor (see
[days.md](days.md#news)) about his mood: its high byte is 7 plus how many
of 1, `0x40`, `0x80`, `0x90` and `0xFF` the mood is below. When that is
`0x0C`, byte `0x11BB` goes up by one (and the game ends at the next demand,
see days.md).
