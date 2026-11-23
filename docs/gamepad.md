# Playing with a gamepad

The original is played with the mouse. With a gamepad, Mentat moves the
same pointer, so everything works the same way.

- The left stick moves the pointer freely. The further it is pushed, the
  faster the pointer goes, and it moves finely near the middle (full speed
  is 220 screen pixels a second).
- The direction pad makes the pointer jump to the next thing that can be
  clicked in that direction. That can be a zone of the panel in use (the
  lines of the list that hold a command, the arrows, the buttons), a
  person standing in the room, or on the map a place or a troop's mark. Each frame the game publishes these targets
  in `Platform::pointerTargets`.
- A target counts as nearer the closer its nearest point is in the pushed
  direction. Each pixel that its middle lies to the side counts three
  times as much. A target the pointer is already on is passed over, and
  when nothing lies that way the pointer stays where it is
  (`jumpTarget`, `src/core/.../pointer_jump.hpp`).
- The bottom button (A on most pads) is the left mouse button, and the
  right button (B) is the right mouse button; both are held for as long as
  they are pressed. Back closes Mentat. Every other button acts as a key
  press, which skips a video or a pause.
- The gamepad takes over the pointer when it moves it or clicks, starting
  from where the mouse pointer was. The mouse takes it back as soon as it
  moves.

Not yet: jumping between the places on the small maps (orders, flights,
spice) and on the globe, and changing the buttons.
