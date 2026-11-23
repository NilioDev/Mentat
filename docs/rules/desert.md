# Walking in the desert

What happens when Paul leaves a place on foot.

## Ways outside

A room's way out (see [places.md](places.md)) from −1 to −5 leads outside:
−1 up, −2 right, −3 down, −4 left; −5 does not move at all (Paul stands on
the place's own point and so comes straight back in). Going out:

- byte 0xE7 (how often Paul was found unconscious, for what people say)
  goes back to 0;
- Paul leaves the place: word 0x114E becomes 0, bytes 0x08 and 0x09
  become 0xFF;
- he starts from the place's point and takes the first step.

## Where Paul is outside

Outside, the room word (0x04) is the longitude and the place word (0x06)
holds the latitude in its low byte and, in its high byte, how many 256ths
of a latitude he is south of it. Byte 0x0B takes the longitude's low byte,
and byte 0x0D the room number before it. Those who go with Paul (byte 0x0F
bit 6 of their record) are moved along.

A step is tiny: east or west it changes the longitude by 1 (of 65536 to a
turn); north or south it changes the 256ths by 1, passing to the next
latitude when they run over. Paul cannot go further north than −0x61 or
further south than 0x61: the step is then not taken.

When the 256ths are 0 and a place lies exactly at Paul's longitude and
latitude, he comes into it, in its first room, as when landing there.

## Counting the steps

Every step taken outside (not the first one out of a place) counts:

- the steps (at most 0x7F) since Paul last came into a place; coming into
  one sets them back to 0;
- byte 0xF4, Paul's tiredness, goes up by 1 up to 0x14.

Each hour, the tiredness goes down by 1 while it stays 0x10 or more;
otherwise it, and byte 0xF5, become 0.

## The sun

After a step, once there are 0x14 steps or more and their number less
0x14 is a multiple of 16, the sun shows over the view (SUN.HSQ) for
(steps − 0x14) / 16 + 1 ticks. At 0x37 steps or more (so at the fourth
time, at 0x44 steps), Paul faints instead of going on:

1. the video DEAD3.HNM plays;
2. 5 hours pass;
3. he wakes in the place he last came into: in a sietch in its second
   room, in the palace in its tenth, elsewhere in the first;
4. byte 0xE7 goes up by 1, and the people there may say how he was found.

## The view

What Paul sees outside, in front of the sky of the hour:

- while he is at the longitude of the place he last came into and fewer
  256ths south of it than that kind of place has views (5 for a sietch, 4
  for the palace, 5 for a village, 4 for the fortress and 4 for the rest),
  that place's view from further away: its first room's picture (see
  [places.md](places.md)) plus the 256ths;
- otherwise one of the desert's pictures: DN20.HSQ and the 18 after it,
  or, when the land is green (the place he last came into has byte 0x0A
  bit 0, or one of the map's points from the one before his to two after
  it has bits 4 and 5 equal to 1 and 0), VG01.HSQ and the 9 after it.
  Which one: the place word with its two bytes swapped, exclusive-or the
  room word, plus 1, modulo the number of pictures.

Nobody is drawn in the view. All four arrows show, without the middle.

## Commands outside

"See Dune map"; "Call a worm" (greyed before story step 0x4F); "Wait for
evening" before the 11th part of the day, "Wait for morning" from it; the
mixer.

## Waiting

- Wait for evening: before the 12th part of the day, the hours pass until
  that part.
- Wait for morning: from the 11th part, the hours pass until the next
  day's first part.
- Fight for a whole day (in a place's first room while it is at war): an
  hour at a time, while the place is at war, 16 hours at most.

Each hour that passes is a whole hour of the game (see
[days.md](days.md)). At story step 0x14, waiting also counts as a long
quiet (see [story_steps.md](story_steps.md)).
