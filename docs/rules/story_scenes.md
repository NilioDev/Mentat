# The story's scenes

Some steps of the story, and a line's action 3, begin a scene: a short
play in a room of the palace where chosen people stand and speak in turn
while the player clicks on. The scene is a script of bytes in the
program's code.

## Which scene

| Begun by | Script |
| --- | --- |
| Step `0x0C` (see [story_steps.md](story_steps.md)) | `0x1321`, the gathering in the communication room |
| Step `0x48` | `0x1313` |
| Step `0x58` | `0x12FB` |
| Action 3, before step `0x14` | `0x12F8` |
| Action 3, before step `0x18` | `0x134F` |
| Action 3, before step `0x30` | `0x1370` |
| Action 3, from step `0x30` | `0x12DB` |
| Paul comes into room `0x3002` of a place (the Harkonnens' throne room) while no scene plays | `0x128F`, the end of the story; the story's step becomes `0xC8` |

## Playing it

Once begun, the scene waits for the next click (or key), even one made
while a talk still shows: the talk ends without a word and the scene takes
over. While it plays, the first line of the list of commands lights up and
goes dark in turn every `0x64` ticks, whatever the mouse is over.

A click or a key plays the script on from where it stopped, step by step,
until a step that waits. A click on " WHAT ? " says the line again
instead.

The steps (one byte, then what it takes):

| Byte | Takes | What it does |
| --- | --- | --- |
| 0 | room r, count n, n bytes | Paul is in room r of the place (the low byte of the room word `0x04`); the n bytes say who stands at each of the room's spots, from the first (`0xFF` nobody), instead of who is there. The commands become list `0x1FBA` ("Continue..."), the arrows show none (zones `0x1D1E`), and the room is drawn. Goes on. |
| 2 | character c | The commands become list `0x1FAE` ("Continue...", " WHAT ? "). When c is not the one who spoke last, the room is drawn again first. Character c, zoomed in on where the room shows it (as when Paul clicks a person), says the first line of its list 7 that may be said (one said before is passed over unless it may come again). Waits. |
| 4 | | The commands become list `0x1FBA`; the room is drawn again. Goes on. |
| 6 | character c | The commands become list `0x1FBA`; character c is shown in front of the room, saying nothing. Goes on. |
| 8 | | Waits. |
| `0x0A` | | The commands become list `0x1FBA`. While the part of the day (the clock's low four bits) is below `0x0D`, an hour passes as it would (see [days.md](days.md)); the sky's colours then reach those of the hour at once. The room is drawn, kept as the background, and sprite 0 of resource `0x27` is drawn over it at (78, 33) (the list at data offset `0x2290`). Waits. |
| `0x0C` | | The commands become list `0x1FBA`. The room is drawn, and the part from (0, 50) is enlarged at zoom level 3 (see [display.md](display.md#zoom)), without the panel's ornament, which is drawn over it again; then sprite 1 of resource `0x27` at (26, 4) (the list at `0x2298`). Waits. |
| `0x0E` | | The commands become list `0x1FAE`. The small map of the regions (see [spice_map.md](spice_map.md)) opens at the top left of the screen, (0, 0), over what the scene shows, and the chief of the troops here (character 15) says the first line of his list 7 that may be said, with no face: only its recording is heard. Waits. |
| `0x10` | | The small map closes. The commands become list `0x1FAE`. The chief goes on in the box a troop's chief talks in when given orders (see [orders.md](orders.md)), at the top of the screen, with the face of the troop at this place: the next line of his list 7 that may be said. Waits. |
| `0x12` | room r, count n, n bytes | As step 0, but byte `0xE8` of the state becomes 0 and the panel's slots (word `0x1152`) `0xFFFF`. The room is drawn in the back picture with Paul and Chani's faces over it (below), and shown with a dissolve (transition `0x10`, see [display.md](display.md)). Goes on. |
| `0x14` | | The end picture (below). Waits. |
| `0x16` | | The cast (below); then the whole program starts again from its opening. |
| `0xFF` | | The end: the room's low byte comes back to what it was, the people are those who are there again, and the room is drawn with its own commands. |

## Paul and Chani

Their two faces show side by side, both moving (see
[portraits.md](portraits.md)). The background is kept, Chani's face
(character 7, mood 0) is shown with its area's top 15 rows lower, the
background is kept again with her in it, and Paul's face (portrait `0x2D`,
the alternative set 1, mood 1) is shown moved right by `0x2D` columns.
One timer moves both: each tick Chani's next frame first, then Paul's.

## The end picture

1. The blinking of the commands stops. Colours 128 to 239 fade to black in
   `0x28` steps.
2. Sprites 0, 1 and 2 of resource `0x1E` are drawn at (0, 0) in the back
   picture, with Paul and Chani's faces over them. The palette is kept;
   colours 99 to 174 start black and fade in, in `0x30` steps, while the
   picture shows.
3. The faces stop. The scene is filled with colour `0x8F` and sprite 3 is
   drawn at (52, 0); it comes down from the top (transition `0x22`).
4. After 300 ticks colours 160 to 174 are set black, sprite 4 ("THE END")
   is drawn at (90, 64) and they fade in, in `0x20` steps. The blinking
   starts again.

A fade of n steps moves the colours a 1/n part of the way to the kept
palette, then 1/(n − 1) and so on, waiting 16 ticks between steps; a key
or a click ends the waiting, and the rest of the steps come at once.

## The cast

The music is track 3 and the scene sits 18 rows down the screen. Then
for each page, in turn: the faces stop, the back picture is cleared, the
page is drawn, its name is written at (10, 157) in the large letters
(resource `0x13`: sprite c − `0x20` for letter c, each followed by its
width plus 2 columns; a line break goes 10 rows down; letters from `0x80`
on show as `@`), it is shown with a fade through black (transition
`0x3A`), and it stays for 600 ticks (a key or a click ends that). Page n's
name is entry `0x120` + n of the command text.

| Page | What it shows |
| --- | --- |
| 0 | Sprite 5 of resource `0x1E` at (`0x40`, `0x34`) |
| 1 | The sky of the hour as seen from a room, its tiles (see [places.md](places.md)), sprite 0 of resource `0x2D` at (0, `0x3C`); Paul's face (portrait `0x2D`, set 1, mood 1) |
| 2 to 9, 11 to 13 | A room, with nobody standing in it but its own guards, and a face (mood `0x1E`): Leto (room `0x200A` of place `0x0180`), Jessica (`0x2003`), Gurney (`0x2006`), Duncan (`0x2004`), the Emperor (`0x3002` of `0x0280`), Harah (`0x0703` of `0x1180`), Thufir (`0x2008`), Stilgar (`0x0802` of `0x1080`), Feyd (`0x3002` of `0x0280`, moved right by `0x3A`), Chani (`0x0803` of `0x1080`), Kynes (`0x1005` of `0x3F80`). The windows show the sky of the hour; Kynes' room lies under the open sky, whose colours and tiles it shows. |
| 10 | The Baron as the opening shows him (see [intro.md](intro.md)) |
| 14 | First the credits video (slot 20) plays to its end on a cleared screen; the page is empty |
| 15 | Sprites 0 to 2 of resource `0x1E` at (0, 0), with Paul and Chani's faces |

After the last page the program waits for a key or a click, and starts
again from its opening.

In the original, after step `0x0E` the colours stay those of the small
map, so the room and the panel look odd behind the box of step `0x10`;
Mentat keeps the room's colours.

## Not built yet

The scene's music.
