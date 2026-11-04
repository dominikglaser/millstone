# Millstone desktop (VST3 / AU / Standalone)

The same Millstone engine as the MPC build (`../src/millstone.cpp`, `mpc_engine()`), wrapped with JUCE so it
runs on a computer as a normal plugin and as a standalone app. The engine file is byte-identical to the MPC
version; only the wrapper differs. Converting back to the MPC plugin is a rebuild with the Open MPC wrapper,
not a rewrite.

## What you get

- `Millstone.vst3` — drop into your VST3 folder
- `Millstone.component` — Audio Unit (macOS), for Logic/GarageBand and AU hosts
- `Millstone.app` — standalone, pick an audio output and a MIDI input (your MPC Key 37 over USB) and play

A universal macOS binary (Apple Silicon + Intel) when built on macOS.

## Build (macOS)

Prerequisites: Xcode command-line tools (`xcode-select --install`), CMake 3.22+, Python 3. JUCE 8.0.4 and the
Open MPC kit (for `wrapper/engine.h` and the params generator) are fetched automatically on first configure.

```
cd desktop
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Outputs land in `desktop/build/Millstone_artefacts/Release/{VST3,AU,Standalone}/`.

## Install

```
# VST3 (per user)
cp -R build/Millstone_artefacts/Release/VST3/Millstone.vst3 ~/Library/Audio/Plug-Ins/VST3/
# Audio Unit
cp -R build/Millstone_artefacts/Release/AU/Millstone.component ~/Library/Audio/Plug-Ins/Components/
# Standalone: just run it
open build/Millstone_artefacts/Release/Standalone/Millstone.app
```

The build is not notarized. If you ever move the artifacts to another Mac (download, AirDrop), macOS quarantines
them; clear it with `xattr -dr com.apple.quarantine <path>`. Built locally, there's nothing to clear.

After a rebuild, quit Ableton completely (Cmd-Q) and reopen it: a running Live keeps the copy it already loaded.

In Ableton: rescan plugins (Preferences → Plug-Ins), add Millstone to a MIDI track, set the track's MIDI input
to the MPC Key 37, play. Turn OSC 2 and OSC 3 on for the full three-oscillator sound; one oscillator (Virtual
Analog) is on by default.

## How it fits together

- `src/engine_host.h` — JUCE-free. Drives `mpc_engine()` and resamples its fixed 44.1 kHz output to the host
  rate (Catmull-Rom). Validated at 44.1 / 48 / 96 kHz. Shared by the plugin and the test.
- `src/PluginProcessor.{h,cpp}` — the JUCE shell: parameters (one per engine parameter, from the generated
  `params.h`), MIDI, tempo for the synced LFO, state save/load.
- `src/PluginEditor.{h,cpp}` — the panel. Draws the MPC skin's pages from `../vst/layout.conf` (embedded at build
  time), so both UIs share one layout and theme. Double-click a knob to reset it, double-click a value box to type.
- `tools/` — optional checks, built with `-DMILLSTONE_UI_SNAPSHOT=ON`. `ui_snapshot` renders each page to a PNG and
  tests the controls. `behaviour_test` drives the whole plugin and measures pitch, filter, envelope, VCA, LFO, glide,
  voice handling, MIDI and state against what each section should do.
- `CMakeLists.txt` — fetches JUCE + the kit, generates `params.h` from `../vst/params.json` (one source of
  truth), builds the engine as an isolated GNU-C++14 static library (Plaits needs that), links the plugin.

Parameters, their ranges and defaults all come from `../vst/params.json`, the same file the MPC skin is built
from, so the two builds never drift.
