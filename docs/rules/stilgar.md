# Stilgar's questions

Stilgar (character 5) asks two questions that change the game (see
[talking.md](talking.md#questions) for how a question is asked and
answered).

## The water of life

His line about the water of life asks whether Paul will drink it. The
lines that answer it have action 8, which for Stilgar means: once the
line is said and its voice has started, the following is done.

1. Bit 3 of state byte `0x0A` is set: the water has been offered.
2. Unless Paul accepted (byte `0x9F` is 1), nothing more.
3. The game waits until the voice has said the line, or 600 ticks when
   there is no voice.
4. With charisma (byte `0x29`) below 100, Paul dies of it: the game ends
   with reason 3 (see [game_over.md](game_over.md)).
5. Otherwise bit 1 of byte `0x0A` is set and byte `0xD5` becomes `0xFF`.
   The picture fades to black (transition `0x38`, see
   [display.md](display.md)), stays black for 1000 ticks (no click
   shortens it), and the talk is over; the room is drawn and fades in
   from black (transition `0x36`).
6. Three hours pass, one by one, each as the clock's own hours do.
7. Byte `0x23` becomes `0x11`, and what coming into a room brings follows
   (those there may speak up, see [talking.md](talking.md)).

## The last attack

His line "the troops are now converging on the palace" has action 9, which
for Stilgar launches the attack: byte `0xC2` goes up by one, and Paul's
troops in the army (byte 3 is 4) with bit 2 of byte `0x19`, the first
seven of them in the order of their records, set off for the Harkonnens'
palace (place record `0x11C`) as troops given orders do (see
[moves.md](moves.md)); each still on its way then makes 4 more steps (8
when its byte `0x19` has bit 6).
