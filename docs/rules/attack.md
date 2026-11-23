# The Harkonnen attack, from far away

A step of the opening: ornithopters cross a green night sky, shots fall
and the horizon flashes.

## Starting

The attack's picture file is resource 0x2B. Sprite 2 is repeated over the
sky's area and sprite 3 over the ground's (each area four words in the
program: left, top, right, bottom). The horizon is a sprite list (sprite,
x and y words, ending with `0xFFFF`). The background is saved, moving
sprites are clipped to the scene, a timer runs every 3 ticks and sound
effect 3 starts; it repeats until stopped.

Stopping the attack stops the sound and the timer and forgets the sprites.

## A tick

A noise word is taken from the second random generator (seed × 0xCBD1 +
1, the middle two bytes of the product) at every tick; "a noise bit"
below takes its top bit and rotates the word left.

- **Flashes.** A counter goes down by one each tick (wrapping at 256).
  From 16 down: at 16 the colours of sprite 0x37 (after 2 bytes, 28
  colours) go to colours 128 to 155 and are sent, at 10 those of sprite
  0x36; each time the spare palette then gets sprite 0x35's colours. At
  other values the colours take a fade step toward the spare palette.
- **Shots on the horizon.** After a pause, a number of shots: a random
  word gives y in its low 7 bits and x from bit 7 and the high byte. Only
  y from 0x30 to 0x5F and x below 320 make a blast, starting at sprite
  0x1C + (y & 7). When the shots run out a new random word gives the next
  count (low 7 bits) and pause (high byte).
- **Flyers.** Every 8 ticks a flyer starts at one of 4 places in the
  program, chosen by bits 2 and 3 of its kind. A new kind is drawn when the
  wave runs out (a wave of random(7) flyers; from 4 the kind turns).
  When a wave starts, the flash counter may be set to 0x0B, or 0x11.
  - Kinds without bit 4 are ornithopters on a straight line: speed from
    the program's table of 4, sprites 4, 8, 12 or 16 with their wings
    beating through four sprites at random.
  - Kinds with bit 4 are shots that may turn by 10 at a time, moving in
    1/32 pixel steps in the direction of their heading, with sprites
    0x14 to 0x1B at random.
- **Blasts** step through their sprites up to 0x2D and then go.

Sprites leave when they leave the screen.

## Tables

| File | Offset | What |
| --- | --- | --- |
| `DNCDPRG.EXE` data | `0x1582` | The sky's area |
| `DNCDPRG.EXE` data | `0x158A` | The ground's area |
| `DNCDPRG.EXE` data | `0x11DD` | The horizon's sprite list (6 bytes each) |
| `DNCDPRG.EXE` data | `0x1592` | The flyers' 4 starting places (x, y words) |
| `DNCDPRG.EXE` data | `0x15A2` | The ornithopters' 4 speeds (x, y bytes) |

## At war

In the game, the same battle is the view of Paul's place at war. Its
horizon is the sprite list kept in the state from `0x11DD` (the
skyline's sprite there is set by the hourly look at the place, see
[days.md](days.md#pauls-place-at-war)). While something else is shown in
front (a talk, the map, the globe), the flashes go on but nothing moves.
The battle stops when the place is at peace again. The view at war
chooses no music: the battle's sound plays over whatever music there was.
