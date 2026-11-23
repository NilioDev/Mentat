# Mentat

**Play the 1992 adventure-strategy classic *Dune* (CD version) on today's
computers, phones and tablets, without an emulator.**

Mentat is a free, open-source, from-scratch engine for the original game.
Bring your own copy of the CD version: Mentat reads its `DUNE.DAT` and
`DNCDPRG.EXE` and plays the game exactly as the original does, with the same story, screens, music
and voices, scaled sharply to any screen and with gamepad, mouse, keyboard
and touch controls.

> [!NOTE]
> **Status: early work.** Mentat plays the whole opening as the original
> does, in time with its music as the AdLib Gold card played it, in stereo
> with the card's surround sound: the logos, Irulan's spoken
> introduction with subtitles, the title, the flights over the desert, the
> palace and the sietch with the characters speaking with moving lips, the
> Harkonnens and their attack, with the original's fades, dissolves and
> zoom. Like the original it starts over until you press a key; then come
> the credits and the pages that tell the story, read by the narrator,
> and the palace at dawn. The game itself has begun: you start in the
> throne room with Leto, with the control panel, the clock that runs day
> and night, the palace's music and the mouse pointer, you can walk
> through the palace with the arrows, and you can talk with the people you
> meet: their face over the room and their own recorded voice, saying
> what fits how far the story has come. People also speak up by
> themselves when you come in or head for a door, and may stop you
> going, and those who follow you walk with you from room to room. "See Dune map" opens the map of
> the planet around you with the places you know, and you can move it
> with the arrows; a click on a place shows its label, with what it is,
> its name and, for places you know, the battle there or its equipment.
> The small planet beside it opens the globe, which turns with your
> place marked, and a click on it takes you to the map of that area.
> "See results" on the globe shows the day, your charisma,
> the areas you hold, your spice and the men on each side, with the planet
> coloured by who holds it, and "Save game" and "Load game" keep your
> game, in the same files as the original. In the palace's entrance you
> can take an ornithopter and fly to a place you know: it takes off, the
> desert goes by with a small map of your way in the corner, and you land
> at the place, or skip straight there. A click on the desert flies off
> that way, steered with the arrows. A companion on board points out a
> hidden sietch you fly past, and you are warned when you enter the
> Harkonnens' zone; fly on and they catch you, and the game ends with the
> original's flashes, video and words, where you can start again, load a
> game or stop. In a sietch you meet its Fremen, who can agree to work
> for you. Their chief then takes your orders on the map, answering in a
> box with his face: he tells you about his troop, and you can have it
> specialize in spice or in the army, give it equipment the sietch keeps
> or take some back, or send it somewhere: point at a
> place on the small map of the regions, and if the chief agrees the troop
> sets off and walks there hour by hour, its mark crossing the map. A
> troop that reaches a place where nobody else is takes it; taking a
> Harkonnen fort raises your charisma and your troops' spirits. Hour by
> hour your troops work: they mine spice into your stock (and a harvester
> may meet a worm), prospect places, train for the army, build wind-traps
> and make the desert green. Send them to attack a Harkonnen place and
> they fight it out hour by hour: they may win it, flee, or lose a place
> of yours. The Harkonnens grow stronger and attack your sietches, the
> green you planted fades, and each day the spice and land accounts are
> kept. The map shows your troops' marks, by what they do. Once the
> story gets that far, "See spice density" on the map opens that small
> map of the regions you know, coloured by their spice or, with a click on
> its bar, by the work of your troops there; point at a place to see who
> is there, or click it to move the map to it. While you wait in a room,
> news from your troops reaches you: the room blurs and a chief's face
> appears in a shimmering vision, showing the place on a small map. As the
> story moves on, doors in the palace open, people move to other rooms,
> new places become known and news arrives. At moments of the story a
> scene plays: the family gathers in a room and each speaks in turn while
> you click "Continue...". The rest of the game is not there yet.

Mentat is not affiliated with or endorsed by the makers or publishers of the
original game or the owners of the *Dune* name. It contains none of the
original game's files: you need your own copy.

## Planned

- 🎯 A one-to-one copy of the CD version 3.7: same story, dialogue, rules,
  timing, music and spoken voices.
- 🖥️ Windows, macOS, Linux and Android from one codebase.
- 🎮 Full gamepad support: a cursor that snaps to buttons, characters, places
  on the map and menu entries, and every button can be changed.
- 💾 Save whenever you like.

## Add your game

Copy `DUNE.DAT` and `DNCDPRG.EXE` from the root of your CD into a folder
named `gamedata` next to the program, or start it with the folder: `./Mentat path/to/folder`.
Without it, Mentat says what is missing.

## Saved games

Mentat keeps your games where your system keeps settings for programs:

- Windows: `%APPDATA%\NilioDev\Mentat\saves`
- macOS: `~/Library/Application Support/NilioDev/Mentat/saves`
- Linux: `~/.local/share/NilioDev/Mentat/saves`

They are the original's own files, `DUNE37S1.SAV` and `DUNE37S2.SAV`, so
you can copy games saved in the original into that folder and play them on,
and the original can load the games you save in Mentat.

## Controls

| Action | Mouse | Keyboard | Gamepad |
| --- | --- | --- | --- |
| Skip a video | Click | Any key | Any button |
| Walk to another room | Click an arrow | | |
| Talk to someone | Click the person or their name | | |
| Hear more, ask again, stop talking | Click a line of the talk menu | | |
| Look at the map | Click "SEE DUNE MAP"; "EXIT MAPS" goes back | | |
| Move the map | Click or hold an arrow; the middle goes back to you | | |
| See what a place is | On the map, click the place; click it again or elsewhere to close the label | | |
| Send a troop somewhere | In its orders, "MOVE TROOP", then click a place on the small map; "Cancel" or a click outside it goes back | | |
| Equip a troop | In its orders, "MODIFY EQUIPMENT", then click a piece in the chief's box to give it back or in the box below to take it; "Done" or a click elsewhere ends | | |
| See the spice and your troops' work | On the map, "SEE SPICE DENSITY"; click its bar to switch, a place to go there, the box at its top left (or the command again) to close it | | |
| Look at the globe | On the map, click the small planet at the left of the panel; "EXIT GLOBE" or the button beside the list goes back | | |
| Turn the globe | Click or hold an arrow; the middle turns it to you; click on the planet to see the map there | | |
| See how you are doing | On the globe, "SEE RESULTS"; "STANDARD VISION" goes back | | |
| Fly to a place | In the palace's entrance, "TAKE AN ORNITHOPTER", then click a place on the small map; "Cancel" goes back | | |
| Fly over the desert | In the cockpit, click the desert; the arrows turn; "BACK TO STARTING POINT" goes home | | |
| While flying | "SKIP TO DESTINATION" lands at once; "CHANGE DESTINATION" opens the map again | | |
| After the game ends | "RESTART GAME", "LOAD GAME" or "EXIT GAME"; " WHAT ? " says it again | | |
| Save or load a game | On the globe, "SAVE GAME" or "LOAD GAME", then a log; "Cancel" goes back | | |
| Close Mentat | Window's close button | | Back |

## Build from source

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
