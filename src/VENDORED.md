# Vendored third-party source

## Mutable Instruments Plaits + stmlib (MIT)

`src/third_party/eurorack/{plaits,stmlib}` come from https://github.com/pichenettes/eurorack,
commit `08460a69a7e1f7a81c5a2abcc7189c9a6b7208d4`, trimmed to the DSP parts a desktop/ARM Linux
build needs (no STM32 drivers, bootloader, UI, hardware design or resource generators). The copy was
taken from poloq-instruments/mpc-vst-plaits, whose `src/VENDORED.md` records that it is byte-identical
to that upstream commit apart from one change, which this port keeps:

- `plaits/dsp/dsp.h`: `kSampleRate` and `kCorrectedSampleRate` are 44100.0f (MPC OS runs plugins at
  44.1 kHz; Plaits' hardware rate is 48 kHz). Both constants are plain runtime floats used for every
  phase increment and time constant, so pitch and timing stay consistent.

Known upstream bug, handled outside the vendored code: `ChiptuneEngine::Init` initialises `kChordNumNotes` oscillators out
of `kChordNumVoices`, leaving the last one uninitialised. If its memory reads as NaN, `SuperSquareOscillator::Render` loops
forever. `millstone.cpp` zero-initialises its voices for this reason; keep the `()` in those `new` expressions.

`src/third_party/eurorack/tides2` (the slope generator only) and `src/third_party/eurorack/elements` (`dsp/` and `resources`, no
firmware) are copied unchanged from the same repository at commit `f4cfa76`, MIT, and are used by the desktop build only
(`MILLSTONE_DESKTOP`). `Part::Init` leaves `panic_` and `easter_egg_` unset, so `ElementsVoice` is zero-initialised too.

Build flags needed (no source change): `-DTEST` (portable `Sqrt`/`Clip16` instead of ARMv7 inline asm
in `stmlib/dsp/dsp.h`) and `-include cstdio` (`plaits/user_data.h`'s mock flash functions call
`printf` without including it).

License: MIT, Emilie Gillet (`src/third_party/eurorack/LICENSE.txt`).
