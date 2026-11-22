# Timing

## The clock

The original sets the PC timer to 1193182 / 5957 ≈ 200.3 ticks a second.
Everything that waits counts these ticks.

## Videos

A video shows its first frame at once. After that:

- **A frame without sound** comes when at least the video's delay (from the
  video table, see [../formats/program.md](../formats/program.md)) has
  passed since the last frame was shown. The wait counts from when the last
  frame actually appeared, so a late frame delays all later ones.
- **A frame with sound** is paced by the sound. The original plays sound
  from two buffers. When the first frame with sound arrives, it fills one
  buffer with silence as long as that frame's sound, and the other with the
  sound itself, and starts playing the silence. Each later frame with sound
  waits until the older buffer has finished, then takes its place. So frame
  n appears when the sound of frame n − 1 starts to play.

A key press or a click skips a video in the opening.
