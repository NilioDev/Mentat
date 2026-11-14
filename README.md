# Melange

**Play the 1992 adventure-strategy classic *Dune* (CD version) on today's
computers, phones and tablets, without an emulator.**

Melange is a free, open-source, from-scratch engine for the original game.
Bring your own copy of the CD version: Melange reads its `DUNE.DAT` and plays
the game exactly as the original does, with the same story, screens, music
and voices, scaled sharply to any screen and with gamepad, mouse, keyboard
and touch controls.

> [!NOTE]
> **Status: just started.** Melange finds and opens your `DUNE.DAT`, reads
> its list of files and unpacks the compressed ones. It opens a window, but
> shows nothing of the game yet.

Melange is not affiliated with or endorsed by the makers or publishers of the
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

Copy `DUNE.DAT` from the root of your CD into a folder named `gamedata` next
to the program, or start it with the folder: `./Melange path/to/folder`.
Without it, Melange says what is missing.

## Controls

| Action | Mouse | Keyboard | Gamepad |
| --- | --- | --- | --- |
| Close Melange | Window's close button | Esc | B or Back |

## Build from source

See [CONTRIBUTING.md](CONTRIBUTING.md).

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
