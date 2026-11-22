# Contributing

## Clean-room rules

Mentat is an independent, from-scratch implementation.

1. **Write your own code.** Do not copy or adapt code from other
   reimplementations, remakes or tools for the original game, whatever their
   license. They may be read only as documentation of a file format; describe
   what you learned in your own words in `docs/formats/` and implement from
   that description.
2. **No decompiled code.** A disassembler or debugger may be used to observe
   how the original program behaves. Record the behaviour as a specification,
   never paste decompiler output into the source tree.
3. **No game data.** Never commit original data files, executables, or
   anything extracted from them (sprites, pictures, fonts, palettes, sounds,
   text, rule tables). Tests that need game data must skip when the files are
   not present.
4. **No trademarks.** Do not use the original game's title, its publisher's
   names or logos in the program, the packages or the store pages. Saying
   which game Mentat plays, in plain text in the README, is fine.

## Layout

| Path | What |
| --- | --- |
| `src/core` | Engine core. Platform independent, no SDL, fully unit-testable. |
| `src/app` | Platform layer and game executable (SDL3). |
| `tests` | Unit tests for the core. |
| `tools` | Developer tools, such as `mentat-dump`, which writes the pictures and video frames of your own `DUNE.DAT` as PNG files to check the readers, and `mentat-music`, which plays a piece of music (a `.HSQ` name for the Sound Blaster Pro player, `.AGD` for the AdLib Gold) into a WAV file or prints its chip register writes to compare with the original, and `mentat-scene`, which draws one scene of a `.SAL` file as a PNG file or as raw palette indices, and `mentat-run`, which runs the game without a window, presses keys, moves the mouse and clicks at set times, and writes the screen as PNG files (`--subtitles 0` shows lines as subtitles instead of voices, `--seed N` fixes the dice of a new game, to compare with the original, `--saves FOLDER` keeps saved games there). What they write stays on your machine. |
| `third_party` | Code by others, copied unchanged with its license: Nuked OPL3, the FM chip emulator, and the YM7128B and TDA8425 emulators for the AdLib Gold's surround and stereo chips. |
| `docs` | Our own documentation: the file formats in `docs/formats`, the original's behaviour in `docs/rules`, and how the engine is built in `docs/architecture.md`. |

## Code style

C++20. Warnings are errors in CI (`-DMTT_WARNINGS_AS_ERRORS=ON`). Every source
file starts with `// SPDX-License-Identifier: GPL-3.0-or-later`.

## Building

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

SDL3 is used from the system when found, otherwise the pinned release is
downloaded and built (`-DMTT_USE_SYSTEM_SDL3=OFF` forces that).
