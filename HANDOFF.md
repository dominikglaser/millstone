# Handoff: building Millstone on macOS

Context for a Claude Code session running in VS Code on the user's Mac. The project was scaffolded and
Linux-validated in a cloud session; the remaining work is the native macOS build, which needs a real Mac
toolchain (that's why it's being handed to you).

## What Millstone is

An 8-voice hybrid synthesizer. Each voice has three Mutable Instruments Plaits macro-oscillators (all 24
models), a 24 dB ladder filter (LP/HP), VCA and modulation ADSR envelopes, a VCA with ENV/LPG/ON modes, one global LFO, glide,
noise, vibrato, velocity and aftertouch routing. Modeled on the Behringer Grind's feature set, extended to 3
oscillators and 8-voice polyphony. No sequencer (the user sequences from the MPC).

## Architecture — read this before changing anything

The synth DSP is one engine, `src/millstone.cpp`, exposing `mpc_engine()` (contract in the Open MPC kit's
`wrapper/engine.h`: 44.1 kHz, interleaved int16 stereo, 1..128-frame blocks). Plaits + stmlib are vendored
under `src/third_party/eurorack` (`src/VENDORED.md`, MIT). **This engine file is shared, unmodified, between
two builds:**

- **MPC plugin** (`vst/`): the engine + the kit's VST2 wrapper → a native Akai MPC OS plugin. Built; the
  aarch64 zip exists. Delivery to the user's MPC Key 37 is currently blocked (see below).
- **Desktop plugin** (`desktop/`): the engine + a JUCE wrapper → VST3 / AU / Standalone. This is your job.

`vst/params.json` is the single source of truth for parameters (keys, ranges, defaults, order). Both builds
generate `params.h` from it, so they never drift. Never reorder `params.json` (the VST index is bound by saved
projects); `vst/params.lock` guards it.

## Your task

Build `desktop/` on the Mac and get the user playing Millstone in Ableton (and the standalone) with the MPC
Key 37 as the MIDI keyboard.

```
cd desktop
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Then install per `desktop/README.md` (VST3 → `~/Library/Audio/Plug-Ins/VST3`, AU → `.../Components`, run the
`.app`). Prerequisites: Xcode CLT, CMake 3.22+, Python 3. JUCE 8.0.4 and the kit are fetched by CMake; first
configure downloads them and builds `juceaide`, so it takes a few minutes.

## Gotchas already solved (don't re-debug these)

- The engine is built as its **own static library** (`millstone_engine` in `desktop/CMakeLists.txt`) with
  `CXX_EXTENSIONS ON` (GNU C++14), `-DTEST` and `-include cstdio`. This is deliberate: Plaits' `resources.cc`
  uses a zero-size array (a GNU extension) that fails under JUCE's strict C++17, and `-DTEST` selects portable
  math over ARM inline asm. Keep the engine isolated from JUCE's flags.
- `params.h` is generated at configure time from `vst/params.json` via the kit's `tools/gen_vst.py --params-h`.
  It lands in `vst/build/` (gitignored). If you change parameters, edit `vst/params.json`, not `params.h`.
- Verified on Linux: VST3 + Standalone compile and link; the resampler (`src/engine_host.h`) produces correct
  pitch at 44.1/48/96 kHz with no NaNs. macOS adds the AU target.

## Known limitations to be aware of (not bugs to chase)

- MIDI is applied at block start, not sample-accurate. Fine for playing; tighten later if needed.
- Not code-signed / notarized. Local builds run fine; only downloaded copies need
  `xattr -dr com.apple.quarantine`.

## The MPC side (for later, so you have the full picture)

The MPC plugin is built (`vst/`, aarch64 zip via the Open MPC `release.py`). It can't be loaded on the user's
**MPC Key 37 G2 on stock MPC OS 3.10** yet: the only no-SSH route is the community "Plugin Manager USB" helper,
and its controller profile isn't activating on the Key 37. Root cause established: the Gen2 firmware is one
binary that reconfigures per device from device-tree flags (`inmusic,nvme-fitted`, `...loopback-content`,
`panel-rotation`) and has keyboard-specific control-surface code (`ACVKKeyboardButtonsResponder`), so the
Live III (where the helper works) and the Key 37 differ in storage layout and button handling despite "the
same firmware." That's a report for the helper's maintainer, not something to fix here. The desktop build is
the way to actually use the synth now; the MPC conversion stays a rebuild for when the device route opens up.

## Repo

`github.com/dominikglaser/millstone` — MIT. Engine and Plaits in `src/`, MPC build in `vst/`, desktop build in
`desktop/`, engine tests in `tests/` (`tests/run.sh fast` renders a demo WAV and checks every model).
