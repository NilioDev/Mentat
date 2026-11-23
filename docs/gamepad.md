# Playing with a gamepad

The original is played with the mouse. With a gamepad, Mentat moves the
same pointer, so everything works the same way.

- The left stick moves the pointer freely. The further it is pushed, the
  faster the pointer goes, and it moves finely near the middle (full speed
  is 220 screen pixels a second).
- The direction pad makes the pointer jump to the next thing that can be
  clicked in that direction. That can be a zone of the panel in use (the
  lines of the list that hold a command, the arrows, the buttons), a
  person standing in the room, on the map a place or a troop's mark, a
  place on the small maps (the cockpit's, and the small map of the regions
  for the spice and for a troop's orders, which then hides what lies under
  it), Paul's place and each place known where it shows on the turning
  globe, or the knobs, the voice test and the rows of the lists on the
  mixer panel. Each frame the game publishes these targets in
  `Platform::pointerTargets`.
- Holding the bottom button while moving the stick drags, as the mouse
  does: the mixer panel's sliders and knobs turn that way.
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

## Choosing the buttons

A text file `gamepad.txt` in Mentat's own folder (beside the `saves`
folder) can say what each button does, one `button = action` a line.
Case, spaces, dashes and underscores do not matter, and `#` starts a
comment. Buttons not named keep what they do; lines that cannot be read
are left out (Mentat logs how many).

- Buttons: `a`/`south`/`cross`, `b`/`east`/`circle`, `x`/`west`/`square`,
  `y`/`north`/`triangle`, `back`/`select`, `guide`, `start`, `left stick`,
  `right stick`, `left shoulder`/`lb`, `right shoulder`/`rb`, `dp up`/`up`,
  `dp down`/`down`, `dp left`/`left`, `dp right`/`right`.
- Actions: `click`, `right click`, `key` (or `skip`), `jump up`,
  `jump down`, `jump left`, `jump right`, `quit`, `none`.

For example, to click with B and keep Back from closing Mentat:

```
b = click
a = right click
back = none
```

The pieces are in `src/core/.../pad_buttons.hpp`.
