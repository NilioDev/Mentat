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
> what fits how far the story has come. The map, travelling and the rest
> of the game are not there yet.

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

## Controls

| Action | Mouse | Keyboard | Gamepad |
| --- | --- | --- | --- |
| Skip a video | Click | Any key | Any button |
| Walk to another room | Click an arrow | | |
| Talk to someone | Click the person or their name | | |
| Hear more, ask again, stop talking | Click a line of the talk menu | | |
| Close Mentat | Window's close button | | Back |

## Build from source

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
