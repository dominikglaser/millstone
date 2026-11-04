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

Build flags needed (no source change): `-DTEST` (portable `Sqrt`/`Clip16` instead of ARMv7 inline asm
in `stmlib/dsp/dsp.h`) and `-include cstdio` (`plaits/user_data.h`'s mock flash functions call
`printf` without including it).

License: MIT, Emilie Gillet (`src/third_party/eurorack/LICENSE.txt`).
