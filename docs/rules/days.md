# Hours and days

What happens each time the clock moves on, around the troops' work (see
[work.md](work.md)). The clock word `0x02` counts hours in its low four
bits and days above them.

## Each hour

1. The state's word `0x1174` takes the clock; when the day (bits 4 to 7)
   changed, a new day has come (below).
2. Unless the story's byte `0xC2` is 7 or more:
   1. the end of a gathering is looked at (below);
   2. the words `0x11CE` and `0x11F7` are kept;
   3. the troops work (see [work.md](work.md));
   4. on a new day, the wind-traps' water and the green fading (below);
   5. by the hour of the day (the clock's low four bits): hour 3, the
      Emperor's demand; hour 4, a Harkonnen attack; hour 8, a reminder;
      hour 15, the Harkonnens grow (below);
   6. the place word `0x11CE` named is learned again (see
      [people.md](people.md)), and word `0x11F7` comes back as it was;
   7. Paul's place at war (below).

## A new day

1. When the story's step (byte `0x2A`) is not the one kept at `0xFE`,
   the count at `0xFF` starts again from 0; `0xFE` takes the step, and
   `0xFF` grows by one: the days at this step.
2. The twelve records at `0xFD8` (16 bytes each) whose word 2 names a
   place (`0x80` in its low byte, the place's number from 1 in the high
   one, not `0xFF`): when the high byte of the record's word 0 is not the
   place's kind, or its low byte is more than the program's code byte at
   `0x1D35` plus the kind, word 0 becomes the kind in the high byte and 1
   in the low one.
3. A gathering may be called (below).
4. The byte at `0xD5` grows by one unless it is 0 (or would come round
   to 0).
5. The spice of the day: the words `0x1172` (which becomes 0) and `0xA0`
   (the stock) summed, less word `0x1170` (which takes the stock), not
   below 0, is word `0xA6`, the spice made; word `0xAE` keeps it, and the
   change from what `0xAE` held goes to word `0xB0` when it grew and to
   `0xB2` when it fell (the other becoming 0).
6. The Harkonnen forts are counted (see
   [places_taken.md](places_taken.md#then-for-both)).
7. The accounts (below), then the results kept (below).
8. The smugglers' wares (below).

### The accounts

Of the map points with any of bits 4 and 5 set, those marked `11` are
the Harkonnens' and the others Paul's. With S the size of the map less
`0x187`, the share of each is its count times 65536 divided by S, times
100, divided by 65536, rounded up from a half: word `0xA4` takes the
Harkonnens' share and word `0xA2` Paul's plus 1.

The stock plus word `0x1172` less word `0x1170` (not below 0) is word
`0xA6` when that is not less. Word `0xAA` is the men of Paul's troops
(byte 3 below `0x80`), word `0xAC` those of the Harkonnen troops, but
for troops whose byte 3 has bit 5; the troop records are looked at up to
the first whose byte 0 is 0.

### The results kept

The words `0xA4`, `0xA2`, `0xA8`, `0xA6`, `0xAC` and `0xAA` are kept in
turn at `0x115E` onwards. For each, the byte at `0x116A` onwards
becomes 1 when it grew from what was kept, 2 when it fell, and 3 when it
stayed, unless word `0x115C` is the day (the clock without its hours).

### The smugglers' wares

A number is drawn from the second generator. For each smuggler record
(`0x11` bytes from `0x10D8`, while their byte 0 is below `0x14`) whose
byte 2 has bit 3: for each of its five wares from the last, whose count
(byte 4 on) is 0 and whose byte 9 on has bit 7, the number is turned left
twice and the count takes its low two bits.

### A gathering

At step `0x5C`, from the day at word `0x1156`, unless Paul is at place
`0x7C8`: of the sietches (kind below `0x28`, not hidden, not place
`0x2C0`), the one with the most of Paul's troops whose byte 3 is below 8
(the first of those with as many). With one, word `0x11DB` names it,
byte `0xF8` grows by one, each of Paul's troops there gets bit 10 of
word `0x12` and bit 4 of byte 3, and message `0x0F08` is left for it (see
[work.md](work.md#the-chiefs-messages)). Paul looks at the news in the
message room (see [news.md](news.md)).

Each hour, unless the state's word `0x12` has bit 7: at step `0x5D`, when
byte `0x1056` is 7 and the record at `0x1048` names a place, its byte 0
becomes 2. When a troop at that place has bit 10 of word `0x12`, byte
`0xF9` grows by 8; when that comes round to 0 the gathering there ends:
every troop there loses bit 10 of word `0x12` and gets bit 11, message
`0x0709` is left for the place, byte `0xF8` goes down by one, and word
`0x11DB` names the first place where troops still gather. With none,
the story goes to step `0x60` (its days from 0), the record at `0x1048`
taking the Harkonnens' palace (its kind and 2, then its number and
`0x80`).

## The wind-traps' water and the green fading

On a new day, each place with a wind-trap (byte `0x0A` bit 5) whose byte
`0x1B` is below 250: of the six map bytes from the one before its point
(word 6), those marked green (`01` in bits 4 and 5) are counted; half
of that plus 1 is added to byte `0x1B`, up to 250.

Then the green fades: a 16-bit number (1 when the program starts, kept
from day to day) is shifted right `0x46` times; each time a 1 falls out
it is mixed with `0x402` (exclusive or). Each time, the map points from
that number on, `0x7FF` apart, below `0xC5F9`, that are green become
looked over (`10`). When any did and the map is in its mode with bit 7,
it is drawn again.

## The Emperor's demand (hour 3)

Only while byte `0xBF` has bit 7. With D the days since word `0x118D`:

- When byte `0xC2` is not 0: unless D is 0, byte `0xCF` takes −D.
- When byte `0xBF` has bit 4, a demand waits: nothing on the day itself;
  after four days or more the game ends (reason 7, see
  [game_over.md](game_over.md)). Before that, by the Emperor's mood (byte
  `0xBE`): the level is how many of 1, `0x40`, `0x80`, `0x90` and `0xFF` it
  is below, at most 2; the program's code byte at `0x2161` plus 4 D plus the
  level is a warning, sent as news (below) with `0x0B` in the low byte;
  with none, the game ends.
- When byte `0x11BB` is not 0 the game ends.
- Otherwise, unless D is 0 (then byte `0xCF` takes −D), a new demand:
  with n the count at byte `0xC3` (which grows by one), `0x64` plus `0x96`
  n, times (`0xE0` plus a number from the first generator with mask
  `0x3F`), shifted right by 8; while the mood lacks bit 7, times `0x100`
  plus the low byte of the mood shifted left by one and inverted, shifted
  right by 8. It is kept to `0xFFFF` at most and goes to word `0xBC`;
  byte `0xCF` becomes 0, byte `0xBF` gets bits 4 and 7, and news `0x020B`
  (the mood with bit 7) or `0x030B` is sent.

### News

The news waits at `0x1179`: up to ten words, their count at byte `0xC8`.
News already waiting is not sent again. With ten waiting the oldest goes
(the others move up, the last word becomes 0). Byte `0xC9` counts all
news sent. From story step `0x38`, unless byte `0x0B` is 8, message
`0x0201` is left (for no place, see
[work.md](work.md#the-chiefs-messages)).

## A Harkonnen attack (hour 4)

Before story step `0x3C`, not until `0x70` hours after word `0x1154`.
Only on even days (bit 4 of the clock clear), when byte `0x11BC` (a
fight in the last round, which becomes 0) was 0, and when the bit that
comes round as the state's word 0 turns left is set.

The target: of the places from the first while their byte 0 is below 8,
the sietches (kind below `0x20`) neither known (byte `0x0A` bit 1) nor
hidden, where no troop gathers, each further north than the last chosen
(latitude below the last, from `0x64`). Each is learned (see
[people.md](people.md)); when bytes `0x60` and `0x63` are not the same,
the fort it would come from is word `0xE4`, or word `0xDE` when word
`0xE2` is `0x1E` or more (and none when word `0xDC` is too). Not the
Harkonnens' palace; there must be Harkonnen troops not fled there and
none of Paul's fighting. The last such is the target.

Then byte `0xC4` grows by one, and twice the first Harkonnen troop of
the fort (looked for again from the first each time) takes byte 3
`0x8D`, sets off for the target (see [moves.md](moves.md)), loses bit 4
of byte `0x10` and arrives at once. The target gets bit 1 of byte
`0x0A`; Paul's troops there (byte 3 below `0x80`, without bit 5) that
prospect get bit 4 of byte 3, the others not fighting are set to fight
(6). The first nine records at `0xFD8` that name the place get its kind
in the high byte of word 0 and 1 in the low one. Message `0x0F0C`
(`0x0F0D` when troop 3 is there) is left for the place. When word 6
names it, word 4 is set the same way and bytes `0x0B` and `0x2B` become
1, and byte `0xFD` takes how the fight there leans (see above) with bit
0 set; otherwise the map is drawn again.

## Paul's place at war

While byte `0x2B` is set, each hour the place Paul is at (word `0x114E`)
is looked at: bytes `0xFD` and `0x2B` become 0 first. The fight there
goes on when the place's byte `0x0A` has bit 1 (the Harkonnens are known
to be inside); otherwise not at a fort of Paul's (kind from `0x28` with
bit 3 of byte `0x0A`), and elsewhere only while any troop of Paul's fights
there (byte 3 is 6, without bit 5); when none does but Harkonnen troops
(not fled) are there, Paul is caught (game over reason 4).

While it goes on, byte `0x2B` is 1, byte `0xFD` is how the fight leans
(below) with bit 0 set, and byte `0x11DD` (the first sprite of the battle's horizon, see
[attack.md](attack.md)) names the skyline: `0x2F` for a
sietch (kind below `0x20`), `0x33` for a fort of kind from `0x28` but
`0x30`, `0x30` for the others. When the game is to end, for any reason,
the reason becomes 6.

How a fight leans is the value of the map's battle gauge before it is
cut to a gauge (see [map.md](map.md#the-battles-gauge)).

## A reminder (hour 8)

When byte `0xBF` has bit 4 (a demand waits), word `0x10` lacks bit 3,
byte `0x0B` is not 8 and byte `0xC2` is 0, message `0x030B` is left for
no place.

## The Harkonnens grow (hour 15)

When the bit that comes round as the state's word 0 turns left is set,
each Harkonnen troop (byte `0x10` bit 7, records below `0xFA0`) with
from 1 to 199 men gets one more.

## Not built yet

The music of the battle.
