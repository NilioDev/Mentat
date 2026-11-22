# Waiting

How the original waits for time, music and the player.

## Checking for input

While it waits, the original keeps checking for input. Each check also
draws a new random number and runs the timers. A key press, or a mouse
button that was not held at the last check, counts as input. In the opening,
input skips the rest.

## Timers

The game keeps a short list of timers, each with an interval in ticks, a
count and a job. Each check adds the ticks that passed since the last check
to every count. When a count reaches its interval, it keeps only the rest
(count modulo interval) and the job runs once. Starting a step of the
opening sets every count back to 0.

## Waiting for the music

The opening can wait until the music reaches a position (see
[music.md](music.md)): the measure number × 16 plus the sixth of the step in
the measure. It waits while the music plays and its position is lower. When
no music plays it does not wait at all.

## Pauses

A pause of a number of ticks waits in a way that depends on what plays:

- while music plays, it does not wait (the music waits pace the opening);
- while a spoken line plays, it waits until the line ends;
- otherwise it waits the ticks.

Input ends every wait early.
