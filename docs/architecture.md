# How Mentat is built

## A one-to-one copy first

Without add-ons, Mentat plays exactly like the 1992 CD version 3.7 of the
original: the same story, dialogue, rules, screens, timing, music and voices.
Anything new, such as gamepad controls or sharp scaling to modern screens,
is added on top without changing how the game plays or looks.

Unlike some strategy games of its time, the original keeps most of its
rules inside its program rather than in data files. Its texts, dialogue
tables, pictures, scenes, music and voices are data in `DUNE.DAT`, and
Mentat reads them from there. How the game reacts (the time of day, the
spice harvest, troops, the Harkonnen, the story's conditions) is observed
in the original and written down in `docs/rules/` before it is built.

The numbers and tables inside the original's program (its starting state,
rule tables, which video is which) are read from the player's own
`DNCDPRG.EXE` when Mentat starts, the same way pictures are read from
`DUNE.DAT`, so none of them are copied into Mentat. See
`docs/formats/program.md`.

## Running the game

The original is one long program that waits for time to pass in many
places, in the middle of a video, a fade or a conversation. Mentat keeps that
shape: the game runs on a thread of its own and hands control back to the
app whenever it waits for the next frame. The app resumes it once per frame
with the clock and the input, then shows its screen and plays the sound it
queued. Only one of the two runs at any moment, so a run with the same input
always does the same thing.

## Measuring the original

The original runs only as a measuring instrument, in an emulator with a
debugger (DOSBox, or a small one of the developer's own), never as part of
Mentat. A savegame is made there, time passes,
and the result is kept as a capture. Mentat loads the same savegame, lets
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
