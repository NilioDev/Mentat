# The message room

How Paul looks at the news he has been sent (see
[days.md](days.md#news)): the device in the palace's message room (room 8),
the hologram of whoever sent a piece of news, and what they say.

## The room's commands

In room 8 of the palace, while any news waits (byte `0xC8` above 0), the
room offers "VIEW NEW MESSAGES" (greyed when byte `0xC9`, the news not seen
yet, is 0) and "Messages already seen" (greyed when every piece is new).

A news word's low six bits say who sent it (the character of that number,
`0x0B` the Emperor); its high byte what it is about. Bit 7 of the low byte
marks it seen.

## Opening a list

1. Byte `0xDB` becomes 0 for the new news, `0xFF` for the news seen.
2. The computer's voice says phrase `0x2A` (new) or `0x2B` (seen), as the
   voices of character `0x19`.
3. The device lights up, drawn straight on the screen with the sprites of
   file `0x15`: its three parts from the list at data offset `0x14C8`
   (sprite, x, y each), and its glow, sprite `0x0B` + *n* at (100, 86),
   with *n* = 1; then *n* = 2; then 20 frames 9 ticks apart of sparks, and
   *n* = 1 again.
4. Every message of the chiefs' queue (see [messages.md](messages.md))
   whose word's low byte is 1 (those only saying news waits) goes; the
   others move up, and when any went, the place after the last one left is
   set to 0.
5. Byte `0xE9` becomes 0.
6. The list: a line for each piece of news of the kind asked for, from the
   newest, named by the phrase `0x78` + its sender; then "Cancel" (phrase
   `0xA3`).

A frame of sparks draws, on each of the rows 0x67, 0x6A, 0x6D and 0x70, the
spark sprite `0x17` + *r* at x 163, where *r* is the original's first random
number masked to 4 bits, drawn again while it equals the last row's. The
two side parts of the device (the list's second and third sprites) then
take the sprite `0x13` + (frame & 3), keeping their mirror bits, and are
drawn; they stay so for later.

## Picking a piece of news

The line picked names the piece; then:

1. Byte `0x24` takes its high byte. Picked from the new news, it is marked
   seen and byte `0xC9` goes down by one; when the Emperor sent it and its
   high byte is 2 or 3, byte `0xBF` gets bit 5.
2. Unless the sender's hologram is already shown, any other one goes (see
   below), the computer says phrase `0x1A` + the sender, and the sender's
   hologram comes: 13 frames 9 ticks apart (from the start of each) of the
   glow rising and falling (*n* = 2 + (frame & 7), folded back from 5 on:
   frames 5, 6 and 7 give 3, 2 and 1 before the 2 is added) with sparks;
   the screen becomes the back picture, the hologram (sprite `0x1E` + the
   sender, at the sender's place in the table at data offset `0x225D`,
   a word each, x in its low byte and y in its high) is drawn on it with
   sound `0x0A`, and it is shown with the sparkle (transition `0x08`, see
   [display.md](display.md)).
3. Byte `0xE9` takes the sender.
4. The sender talks, zoomed in on its hologram's place as in any talk (see
   [talking.md](talking.md)): the first line of list 4 of its group that
   may be said (lines said before may be said again unless they have bit
   5). The menu is "Viewed" (phrase `0xD9`) and " WHAT ? ", which says the
   line again.

## Viewed

Byte `0xEB` takes byte `0xC9`. The talk ends: the sender counts as talked
to. The hologram is drawn on the screen over the face with sound `0x0A`,
and goes: the screen becomes the back picture, the room is drawn again on
it and shown with the sparkle; then, with sound 9, 5 frames of the glow
from frame 4 down to 0, and the device with its glow at 1 again. Byte
`0x24` becomes 0 and byte `0x23` 6. The room's commands come back; the
screen keeps the device as it is.

When the news was about `0x0C` (the Emperor's troops come), the game ends
then with reason 7 (see [game_over.md](game_over.md)).

## Cancel

The computer says phrase `0x29`, and the room's commands come back.

## Not built yet

What the original does when another list replaces the talk's menu without
"Viewed", and action 10 of the Emperor's lines (the amount of spice he
asks for).
