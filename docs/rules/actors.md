# Moving sprites

Some scenes move sprites over a still background (the Harkonnen attack in
the opening, for one). Each sprite is kept as a rectangle with its sprite
number and two words its owner uses for its motion.

- A new sprite is placed with its centre at the given point (width taken
  from the low 12 bits of the sprite's width word) and is not drawn yet.
- Moving a sprite draws again the rectangle covering where it was and
  where it is.
- Taking one away draws again where it was.
- Changing its sprite keeps its top-left corner and draws nothing until it
  next moves.

Drawing a rectangle again clips it to the scene's clip, copies the saved
background into the back picture, draws every sprite that touches it in
list order (clipped to it), and copies it to the screen.
