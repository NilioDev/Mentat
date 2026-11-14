# How Melange is built

## A one-to-one copy first

Without add-ons, Melange plays exactly like the 1992 CD version 3.7 of the
original: the same story, dialogue, rules, screens, timing, music and voices.
Anything new, such as gamepad controls or sharp scaling to modern screens,
is added on top without changing how the game plays or looks.

Unlike some strategy games of its time, the original keeps most of its
rules inside its program rather than in data files. Its texts, dialogue
tables, pictures, scenes, music and voices are data in `DUNE.DAT`, and
Melange reads them from there. How the game reacts (the time of day, the
spice harvest, troops, the Harkonnen, the story's conditions) is observed
in the original and written down in `docs/rules/` before it is built.

## Measuring the original

The original runs only as a measuring instrument, in DOSBox with its
debugger, never as part of Melange. A savegame is made there, time passes,
and the result is kept as a capture. Melange loads the same savegame, lets
the same time pass and must come to the same result. Screens are compared
with captures of the original pixel for pixel. Captures stay on the
developer's machine, like the game files; only the facts learned from them
go into `docs/rules/` and the tests.

## Layers

- `src/core`: reading the original files, the game state and rules, and
  drawing the 320×200 picture with 256 colours. No SDL; fully unit-tested.
- `src/app`: the window, sharp scaling, sound output, and turning mouse,
  keyboard, gamepad and touch into the same commands.

## Screen and input

The original draws a 320×200 picture with 256 colours, mostly by pointing
and clicking. The core draws that same picture; the app scales it sharply to
any window. The gamepad moves a cursor that snaps to buttons, characters,
places on the map and list rows, so every screen can be played without a
mouse.
