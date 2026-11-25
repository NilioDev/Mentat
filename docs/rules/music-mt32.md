# The MT-32 music player

How the original's MIDI music player (`DNMID.HSQ`) plays the `.M32` music
(see [../formats/music.md](../formats/music.md#mt-32-files)) on a Roland
MT-32. Mentat's version is `src/core/src/audio/midi_driver.cpp`; the
synthesizer is Munt's emulator, with the player's own MT-32 ROMs.

The timing is the FM player's (see [music.md](music.md)): the same counter
that the tempo fills and every timer tick empties by 256, 96 steps to a
measure, the same repeated part, the same count of plays, and the same
positions the game waits for. Only the first voice's list is read (the
player runs as many voices as have lists, from the first).

## Starting and stopping

- When the game loads it, the player resets the MIDI interface and puts
  it in its plain mode, then silences the synthesizer.
- **Silence:** on each of the channels 1 to 10, controller 123 (every
  note off) with 0, and the pitch bend back to the middle (0, 64).
- **Start:** silence, then the volumes of channels 1 to 9 it keeps are set
  to 96 (the tenth is not touched: it starts at 77, a byte the program
  happens to hold there). The volume of the moment becomes the game's
  volume byte, the piece's first step runs, and the piece plays.
- **Stop:** silence; the piece no longer plays.

## Events

Each event goes to the synthesizer as it is, except:

- **Controller 7 (volume):** the value is kept as the channel's volume,
  then sent scaled by the volume of the moment: value × volume / 256,
  rounded down. Channels above 10 are not kept.
- **Key pressure:** not sent.
- **End of the list:** as in the FM player: with plays left the piece
  starts again from its beginning; after the last play it stops.

## Volume and fades

The game gives a volume byte (the balance is not used). Setting it makes
it the target, reached one step a tick. A fade sets the target and the
pace, from the ticks it should take, as the FM player does: under 0x60
every tick, under 0xC0 every second tick, under 0x180 every fourth, under
0x300 every eighth, otherwise every sixteenth.

Each fade step moves the volume of the moment by at most 3 toward the
target and sends controller 7 on channels 1 to 10 with each kept volume
scaled again. A fade that reaches 0 stops the piece without silencing it.
Fades only move while a piece plays.
