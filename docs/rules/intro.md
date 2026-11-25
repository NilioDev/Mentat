# The opening

The opening that plays before the title menu is a script in the program
(see [../formats/program.md](../formats/program.md)). Mentat reads the
script from the player's own `DNCDPRG.EXE` and carries out each step the way
the original does.

## The script

The script is a list of steps of six 16-bit words. A first word of
`0xFFFF` ends the list (the original then starts it again).

| Word | What |
| --- | --- |
| 0 | Music position to wait for before the step (see [waiting.md](waiting.md)) |
| 1 | What to draw: the work that prepares the picture |
| 2 | Music position to wait for before the picture shows |
| 3 | The transition that shows it (see [display.md](display.md)); negative for none |
| 4 | What runs once it shows |
| 5 | Ticks to pause after the step (see [waiting.md](waiting.md)) |

Words 1 and 4 name the program's own routines; Mentat keeps a list of the
ones it knows by their place in the program.

## A step

1. The origin is set to 24 rows.
2. Wait for the first music position.
3. The step's timers are stopped and the working palette is kept in the
   spare one.
4. The picture is prepared, with the back picture as the screen, so that
   everything drawn goes into the back picture.
5. Wait for the second music position. The back picture stays the screen
   through this wait: a face that already moves, or a battle that already
   runs, only changes the back picture until the step is shown.
6. The transition runs, and drawing goes to the back picture again.
7. After an input check, the run part runs, then the pause.

When the script ends it starts again from the first step, until the
player presses something.

Input during any wait skips the opening: the screen is cleared, the origin
goes back to 0, an open video is closed and the timers stop.

The music pieces of the opening play once (not looped).

## The steps' work

- **The logos.** Their videos fill the screen from its top row: their
  steps set the origin to 0.
- **The publisher's logo.** Stops the music, starts piece 6 and opens its
  video; the video plays in the back picture, each new frame copied to the
  screen. When the music reaches its 8th measure it stops.
- **The developer's logos.** Piece 10 starts with the first video. Each
  video plays to its end; the second clears the picture first.
- **"Presents".** Origin 0, the picture cleared, and the video plays on the
  screen to its end.
- **Irulan's introduction.** The subtitle sheet for the language is
  selected (resource 0x69 plus the language number: 1 French, 2 German,
  3 English, 4 Italian, 5 Spanish). The video plays on the screen. The
  program keeps a list of frame numbers: at the frame of entry k, an even
  entry draws sprite k / 2 of the sheet at (0, 190) and an odd one clears
  rows 190 to 199. At the end the video closes, the speech stops and
  piece 6 starts again.
- **The title.** The title video plays on the screen up to frame 0x56, and
  later steps play the rest.
- **The sunrise.** The picture is drawn from a sprite list in the program
  (sprite words, x and y, ending with `0xFFFF`) with the sunrise sheet.
  Sprite n of that sheet holds, after 2 bytes, 80 colours for colours 128 to
  207, which go into the spare palette. A timer every 9 ticks moves the
  picture's colours 73 to 223 a fade step toward it, with the divisor
  counting down from 30. When the count reaches 0 the next palette is
  loaded (30 steps, or 10 for palette 13). When the next palette would be
  11 the timer only selects the sheet of the next scene, and from 14 it does
  nothing.

## Part two

The flight over the desert, the palace, the sietch and the Harkonnens.

- **The desert flights and the plant.** The video plays raised (origin 24)
  in the back picture; a timer every tick shows each new frame and copies
  the 152 scene rows to the screen. The third flight first loads sky
  palette 0x12 (see below) and its run part also starts a timer every 16
  ticks that moves colours 73 to 223 a fade step toward the spare palette,
  with the divisor counting down from 0x3F.
- **The sky.** Sprite n of the sky sheet (resource 0x28) holds, after 2
  bytes, 151 colours for colours 73 to 223, which go into the spare palette.
- **Rooms.** A room of a place is drawn the way the game draws it (see
  [places.md](places.md)), with the persons present.
- **Faces.** A character's portrait is shown over the saved background
  with a long first idle animation (mood 0x1E), and the run part speaks
  phrase 0x190 (see [portraits.md](portraits.md)). The run part waits for
  the voice.
- **Chani.** The room shows without a transition. The run part sends the
  colours, zooms in on her face and speaks. The zoom uses the centre in the
  program's table (4 bytes a character) and the list of zoom levels in the
  program: each level shows for at least 6 ticks, during which timers and
  input wait; the entry `0xFF` waits 300 ticks. The zoom ends by showing the
  back picture again. See [display.md](display.md).
- **Kynes, Paul, Feyd and the Baron.** The background is a sprite list from
  the program, drawn and saved, then the face shows, moved right when its
  place lies left of the given column (Feyd 0x3A, the Baron 0x52). Paul
  uses portrait 0x2D with mood 1. Feyd's guards come from a second list,
  drawn clipped to the scene.
- **The Baron leaves.** Sprites 5 and 6 of the backgrounds slide in from
  x = −0x60 by 0x20 at a time, at y = 0x0D.
- **The sunset.** Sunrise picture 0x0B with sprite 7 at (0x13, 0x4A); a
  timer every 16 ticks runs the sunrise fades.
- **The Harkonnen attack.** See [attack.md](attack.md).
- **The pictures.** Each picture step shows one of the pictures
  0x56 to 0x61; some clear the scene first.
- **The verses.** The video plays raised with sound effect 8, which stops
  at the video's eleventh frame.
