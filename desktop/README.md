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

Outputs land in `desktop/build/Millstone_artefacts/Release/{VST3,AU,Standalone}/`, as universal binaries (Apple
Silicon + Intel, macOS 11 or later) with a verified ad-hoc signature. To sign with a real identity, add
`-DMILLSTONE_SIGN_IDENTITY="Developer ID Application: ..."` when configuring; distribution also needs notarization.

If an older `build` folder predates these settings, reconfigure it once with `cmake --fresh -B build`.

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
to the MPC Key 37, play. OSC 1 and OSC 2 (Virtual Analog, slightly detuned) are on by default; turn OSC 3 on for the
full three-oscillator sound.

## How it fits together

- `src/engine_host.h` — JUCE-free. Drives `mpc_engine()` and resamples its fixed 44.1 kHz output to the host
  rate (Catmull-Rom). Validated at 44.1 / 48 / 96 kHz. Shared by the plugin and the test.
- `src/PluginProcessor.{h,cpp}` — the JUCE shell: parameters (one per engine parameter, from the generated
  `params.h`), MIDI, tempo for the synced LFO, state save/load.
- `src/PluginEditor.{h,cpp}` and `panel.conf` — the one-screen panel: worn metal and wood, all drawn in code, placed by
  `panel.conf`. The MPC skin stays in `../vst/layout.conf`. Point at or move a control to see it on the display; double-click
  a control to reset it, or the value on the display to type one.
- The patch bay drawer (PATCH button) holds 12 cords between 12 outputs and 20 inputs. Drag from an output to an input to
  patch, pull a plug out of an input to move or remove it, double-click an input to clear it. Oscillators and noise reach the
  filter and VCA every sample; into an oscillator they step every 12 samples, like CV into the real module. Set an
  oscillator's LEVEL to 0 to use it only as a modulator. Cords are ordinary parameters (`cN_src`, `cN_dst`, `cN_amt`).
- Tides 2 (desktop only) runs per voice, patched through the drawer as TIDES 1 to 4. TRIGGER picks NOTE (restarts on each
  note), FREE (keeps running) or CLOCK (locked to the song, with DIV for bars, notes and triplets), or PATCH (fired only by what is patched into T TRIG, such
  as one of the clock outputs). OUTPUT follows the module:
  in GATES mode TIDES 3 and 4 carry the gates and TIDES 1 is silent with SHIFT centred; in AMP mode SHIFT pans the wave across
  the four jacks, and all are silent with it centred. PHASE, the default, is live on all four.
- The clock (desktop only, `../src/workout_clock.h`) is one shared 8-output clock, an original design in the spirit of
  Pamela's Workout. Each output has a rate, shape, width, swing, probability, phase, level, offset and a Euclidean pattern, and
  patches through the drawer as CLOCK 1 to 8. Trigger inputs fire as a cord rises past a quarter: T TRIG, LFO RST, ENV TRIG,
  MOD TRIG, and O1 to O3 TRIG, which strike percussive models while a note is held. With SOURCE on SONG it follows Live. It is edited on an LCD: the buttons pick the
  output, SELECT scrolls through the settings (or click one on the screen), and VALUE changes the one selected.
- Elements (desktop only) replaces Plaits on OSC 1 when its type switch says so, one instance per voice, run at its own 32 kHz
  and resampled. Its exciter takes audio from the patch bay through E STK IN and E BLW IN, and O1 TRIG strikes it again.
- Each oscillator has an amp switch. AUTO follows VCA MODE. ENV is a plain oscillator with its trigger unpatched, shaped by the VCA
  envelope. LPG patches its trigger, so Plaits' internal gate shapes it, with LPG DECAY and LPG COLOUR. With any oscillator set, the
  envelope moves in front of the mix and applies only to what is routed to it, so an LPG oscillator rings past the key.
- `tools/run_all_tests.sh [--sanitize]` runs every check below in one go, and CI runs it too. `stress_test` proves every parameter
  changes the sound, instances stay separate, and nothing breaks while presets change under running audio. `engine_fuzz` plays
  random patches and MIDI at the engine; `sanitized_fuzz.sh` does that under AddressSanitizer and UBSan.
- `tools/` — optional checks, built with `-DMILLSTONE_UI_SNAPSHOT=ON`. `ui_snapshot` renders each page to a PNG and
  tests the controls. `behaviour_test` drives the whole plugin and measures pitch, filter, envelope, VCA, LFO, glide,
  voice handling, MIDI and state against what each section should do.
- `CMakeLists.txt` — fetches JUCE + the kit, generates `params.h` from `../vst/params.json` (one source of
  truth), builds the engine as an isolated GNU-C++14 static library (Plaits needs that), links the plugin.

Parameters, their ranges and defaults all come from `../vst/params.json`, the same file the MPC skin is built
from, so the two builds never drift.

## The rack

RACK, beside the preset buttons, swaps the panel for a second page: a road-case rack of black units, each with its own colour. It always fills
a tall window of its own, with no patch bay below it. PANEL goes back, with the patch bay open or closed as you left it.

- **CK-8 Clock:** the eight-output clock, with its LCD.
- **TD-2 Tidal Modulator:** Tides. Its screen has presets, draws one cycle of the slope, and holds SMOOTH and SHIFT on VALUE.
- **DE-1 Tape Echo:** a tape delay for dub. See below.
- **RV-2 Digital Reverb:** NORMAL (the plate from Mutable Instruments Rings) and SPRING (three dispersive springs, whose chirp is DRIP). KICK bangs the tank. DECAY changes how long the tail lasts, not how loud held notes are.
- **DI-4 Ensemble:** the chorus, three settings.
- **PH-6 Phaser:** six stages.

Each effect's screen has its own presets, on its < > keys. A preset leaves your MIX alone unless the unit is off, when it brings it up to be heard. A `*` means you have changed it since.

The effects run after the voices, in this plugin and not the shared engine, in the order phaser, chorus, delay, reverb, and they cannot exceed -0.5 dB. With all of them off the signal is not touched. The code is in `src/fx/`; the chorus, phaser and plate wrapper come from the `plaits-synth` project. The page is in `src/RackPage.inc` and the `page=rack` items of `panel.conf`.

**The echo.** TIME follows the song or is free in ms, and moving it glides the tape, bending the pitch of what is on it. REPEATS passes 100%: from there it builds on its own until the tape saturates. HEADS plays any of three heads, at 1x, 2x and 3x TIME. LOW CUT and HIGH CUT are 24 dB per octave filters in the loop, so each repeat is thinner and darker. PING PONG bounces left and right. On the screen, pick DRIVE (tape saturation), WOW (wow and flutter), DUCK (holds the echoes down while you play) or WIDTH (narrows the ping-pong), and turn VALUE. The screen also draws the pattern on the tape.

THROW lets in what you play while you hold it. Close ECHO INPUT and only that reaches the tape. THROW and KICK are parameters, so Live can map them to pads.
