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

Then the shipment leaves, shown in the message room's device. Times are
in ticks; each wait counts from when the step before it began.

1. The device lights up as for the news (see [news.md](news.md)): its
   pictures, glow 2, 20 frames of sparks 9 ticks apart, then glow 1 once
   any voice has ended; then the 13 frames of a hologram rising.
2. Dune's hologram: on a copy of the screen, picture `0x1B` of the stars'
   pictures (resource `0x2C`, STARS.HSQ) at (`0x8C`, `0x27`), with sound
   `0x0A`, comes with the sparkle (transition 8). It stays 100 ticks.
3. Space, at step n: the scene black, the three pictures of stars 0, 1
   and 2 at x = −n²/2, −n²/2 + `0x130` and −n²/2 + `0x260` (y 0); then,
   each placed by its middle, the sun (picture `0x24`) at
   (`0x45` − 4 × (n²/2), `0x4E`), Kaitain (picture `0x25` + n/4) at
   (4n + `0xF2`, `0x67`), or (`0x3E2` − n², `0x67`) when n is over `0x14`,
   and Dune (picture 3 + n) at (`0x80` + (n²/2) / 16, `0x4F`). Halves and
   sixteenths are rounded down, everything clipped to the scene.
4. Step `0x18` comes with the mosaic (transition 6); then steps `0x17`
   down to 0, one every 12 ticks, each shown at once: the view leaves
   Dune behind for the sun and Kaitain.
5. The computer (voice character `0x19`) says phrase `0x27`. The part
   of the screen at data `0x15AA` (126, 76 to 320, 152) is kept, 200
   ticks pass and the game waits for the voice to end. Sound 1 plays.
6. The frigate: the list at data `0x15B2`, 6 bytes a frame until a byte
   `0xFF`: a picture (none when negative) and its x and y (its top left).
   Each frame, 12 ticks apart, puts back the kept part, draws the picture
   and shows the part. At the frame at `0x161E` the sound stops. After
   the last, the kept part is put back once more.
7. Steps 1 to `0x18` of space, one every 12 ticks: back to Dune.
8. The room as it is with Dune's hologram over it (and its sound) comes
   with the mosaic, then the room alone with the sparkle.
9. The device goes out as after a hologram: sound 9, frames 4 down to 0
   9 ticks apart, then its pictures with glow 1.

## The Emperor's answer (action 15)

Before story step `0x10`, Duncan's line only sets bit 4 of byte `0xFF7`.
From then on the talk ends after it, word `0xC0` becomes 0, byte `0xBF`
gets bit 0, and news comes from the Emperor (see
[days.md](days.md#news)) about his mood: its high byte is 7 plus how many
of 1, `0x40`, `0x80`, `0x90` and `0xFF` the mood is below. When that is
`0x0C`, byte `0x11BB` goes up by one (and the game ends at the next demand,
see days.md).
