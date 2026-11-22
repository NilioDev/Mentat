# TDA8425 emulator

An emulator of the Philips TDA8425 stereo audio processor (volume, bass,
treble and stereo effects), by Andrea Zoppi. The AdLib Gold sends its
output through this chip; Mentat uses it for the AdLib Gold music. It is
not game code.

- Upstream: https://github.com/TexZK/TDA8425_emu
- Version: 0.2.0, commit `d786946ff8c639733c692ace7e3c7d3bae0ff400`
- License: BSD 2-Clause (see `LICENSE`), which is compatible with Mentat's
  GPL 3.0 or later.

`TDA8425_emu.c` and `TDA8425_emu.h` are copied unchanged. Mentat builds
them with `TDA8425_USE_EXTENSIONS` set to 0, so only the real chip's
registers exist.
