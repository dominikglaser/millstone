# Millstone desktop (VST3 / AU / Standalone)

Millstone as a normal plugin and a standalone app: the engine in `../src/millstone.cpp`, wrapped with JUCE.

## What you get

- `Millstone.vst3` — drop into your VST3 folder
- `Millstone.component` — Audio Unit (macOS), for Logic/GarageBand and AU hosts
- `Millstone.app` — standalone, pick an audio output and a MIDI input and play

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

The simplest way is the installer, `Millstone-<version>.pkg`. It is signed and notarized by Apple, so it opens without a
security warning, and it lets you choose any of the VST3, the Audio Unit and the standalone app. It puts them in
`/Library/Audio/Plug-Ins/VST3`, `/Library/Audio/Plug-Ins/Components` and `/Applications`, and asks for your Mac password.

To install your own build instead, remove the installer's copies first, so that a host doesn't find two Millstones:

```
# VST3 (per user)
cp -R build/Millstone_artefacts/Release/VST3/Millstone.vst3 ~/Library/Audio/Plug-Ins/VST3/
# Audio Unit
cp -R build/Millstone_artefacts/Release/AU/Millstone.component ~/Library/Audio/Plug-Ins/Components/
# Standalone: just run it
open build/Millstone_artefacts/Release/Standalone/Millstone.app
```

A build made this way is not notarized. If you ever move it to another Mac (download, AirDrop), macOS quarantines it;
clear it with `xattr -dr com.apple.quarantine <path>`. Built locally, there's nothing to clear.

After a rebuild, quit Ableton completely (Cmd-Q) and reopen it: a running Live keeps the copy it already loaded.

### Making the installer

`desktop/tools/make_installer.sh` signs a finished Release build, packages it, sends it to Apple for notarization and
staples the approval to `desktop/build/installer/Millstone-<version>.pkg`. Set the version in `CMakeLists.txt`, rebuild
and run the tests first. Add `--no-notarize` to stop before the upload. It needs, once per Mac:

- the "Developer ID Application" and "Developer ID Installer" certificates in the login keychain, each with its private key
- notarization credentials, which ask for an app-specific password from account.apple.com:
  `xcrun notarytool store-credentials "millstone-notary" --apple-id <email> --team-id <team>`

Keep a backup of the private keys, and keep them and the password out of the repository. Signed and timestamped builds keep
working after a certificate expires; a new certificate is only needed to sign new builds.

In Ableton: rescan plugins (Preferences → Plug-Ins), add Millstone to a MIDI track, set the track's MIDI input
to your keyboard, play. OSC 1 and OSC 2 (Virtual Analog, slightly detuned) are on by default; turn OSC 3 on for the
full three-oscillator sound.

## How it fits together

- `src/engine_host.h` — JUCE-free. Drives `mpc_engine()` and resamples its fixed 44.1 kHz output to the host
  rate (Catmull-Rom). Validated at 44.1 / 48 / 96 kHz. Shared by the plugin and the test.
- `src/PluginProcessor.{h,cpp}` — the JUCE shell: parameters (one per engine parameter, from the generated
  `params.h`), MIDI, tempo for the synced LFO, state save/load.
- `src/PluginEditor.{h,cpp}` and `panel.conf` — the one-screen panel: worn metal and wood, all drawn in code, placed by
  `panel.conf`. Point at or move a control to see it on the display; double-click
  a control to reset it, or the value on the display to type one.
- The patch bay drawer (PATCH button) holds 12 cords between 26 outputs and 46 inputs. Drag from an output to an input to
  patch, pull a plug out of an input to move or remove it, double-click an input to clear it. Oscillators and noise reach the
  filter and VCA every sample; into an oscillator they step every 12 samples, like CV into the real module. Set an
  oscillator's LEVEL to 0 to use it only as a modulator. Cords are ordinary parameters (`cN_src`, `cN_dst`, `cN_amt`).
  Jacks are grouped by module, and each label takes its module's colour, the same on both sides of the bay: purple for the LFOs
  and RANDOM, orange for the envelopes, cream for what you play, blue for the oscillators, green for the filter and voice, teal
  for Tides, amber for the clock and pink for Elements.
- RANDOM is a new value for each note, held for that note. DRIVE pushes the filter's drive. LFO RATE and L2 RATE speed an LFO up
  or slow it down, and LFO RST and L2 RST restart one. The LFOs are shared between voices, so with several notes held the voice
  patching a rate hardest sets the speed.
- Tides 2 (desktop only) runs per voice, patched through the drawer as TIDES 1 to 4. TRIGGER picks NOTE (restarts on each
  note), FREE (keeps running) or CLOCK (locked to the song, with DIV for bars, notes and triplets), or PATCH (fired only by what is patched into T TRIG, such
  as one of the clock outputs). OUTPUT follows the module:
  in GATES mode TIDES 3 and 4 carry the gates and TIDES 1 is silent with SHIFT centred; in AMP mode SHIFT pans the wave across
  the four jacks, and all are silent with it centred. PHASE, the default, is live on all four.
- The clock (desktop only, `../src/workout_clock.h`) is one shared 8-output clock, an original design in the spirit of
  Pamela's Workout. Each output has a rate, shape, width, swing, probability, phase, level, offset and a Euclidean pattern, and
  patches through the drawer as CLOCK 1 to 8. Trigger inputs fire as a cord rises past a quarter: T TRIG, LFO RST, L2 RST, ENV TRIG,
  MOD TRIG, and O1 to O3 TRIG, which strike percussive models while a note is held. With SOURCE on SONG it follows Live. It is edited on an LCD: the buttons pick the
  output, SELECT scrolls through the settings (or click one on the screen), and VALUE changes the one selected. Dragging a setting's name on the screen, or the big reading, changes it directly.
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

Parameters, their ranges and defaults all come from `../vst/params.json`. Only ever add to the end of that list, and to the end
of a parameter's options: saved sets store a parameter by name and a choice by its number. A switch can still draw its options in
another order, with `order=` in `panel.conf`.

## The LFOs

The switch at the top of the LFO section flips its controls and its screen between LFO 1 and LFO 2. Each has its own shape, rate,
retrigger, sync, division and phase.

LFO 1 is wired to OSC MOD, and is scaled by WHEEL > LFO and AT > LFO, which go away on LFO 2's page. Both reach MOD SOURCE on the
filter and CV SOURCE on each oscillator, so each oscillator can follow a different one, and both have a jack on the patch bay.

## The panel's screens

The filter, both envelopes and the LFO each have a small screen showing the real curve and the real numbers, using the engine's own formulas: cutoff in Hz, envelope times in ms and s, LFO rate in Hz or the division it is locked to. Envelope time is drawn compressed, so a 2 ms attack and a 10 s release both fit.

## Presets

**A preset plays the note you press:** MIDI note 60 sounds at 261.6 Hz, and for a bass you transpose down. (Live labels that note C3.) A bass has its lowest oscillator you can hear on the note, and anything else has the pitch you hear most on it. Drums, risers and textures are left alone, as they are not meant to hold a pitch. A few bells and FM keys have strong partials off the note, as those sounds do. New presets simply start on the note; `vst/preset_pitch.py` moved the older ones, and moves CUTOFF against KEYTRACK so that each still sounds the same at the same pitch.

Click the preset name in the display, or in the rack's top unit, to open the browser. Search looks through names, notes and tags. Within a group, several choices widen the list; between groups they narrow it, and a filter that would leave nothing is dimmed. Click a row to hear it, double-click to load and close. < > step through whatever the filter left.

Three packs lean hardest on the patch bay and the effects, using both LFOs, RANDOM, Tides, the clock and oscillators as modulators. **Modular Classics** holds patches a modular player would know, **Cross Patch** playable sounds with something moving inside them, and **The Deep End** the ones that get away from you.

Each preset has one Type, several Styles and Characters, and a Pack. The first library's are in `vst/preset_tags.py`; each later pack is one file, `vst/pack_*.py`, with its tags beside its sounds. Uses and Plays With are worked out from its values by `make_presets.py`, so they cannot be wrong. `vst/preset_fx.py` holds the effects on the first library, and `vst/preset_output.json` the make-up gain. On Plaits' Virtual Analog model MORPH 100 is not a saw: use `SAW()` in `vst/preset_helpers.py`, which uses VA VCF, and `millstone_preset_probe` to see what a setting really produces. New names can only be added, because a Live set finds its preset by name.

The star marks a favourite. FLAG asks for a note. Both are plain text files beside your user presets, in `~/Library/Application Support/Glaser/Millstone/`: `favourites.txt` and `preset-flags.txt`, which OPEN FLAG FILE shows in the Finder, ready to hand back.

`preset_test` listens for duplicates: it compares every pair over time in third-octave bands and fails anything under 1 dB apart.

## The rack

RACK, beside the preset buttons, swaps the panel for a second page: a road-case rack of black units, each with its own colour. It always fills
a tall window of its own, with no patch bay below it. PANEL goes back, with the patch bay open or closed as you left it.

- **Top unit:** the synth preset and the control last touched, the FX preset and FX LOCK, and OUTPUT: up to 18 dB of gain after all the effects, for a sound that full VOLUME leaves too quiet. Presets use it too. At 0 dB it is not in the path at all.
- **CK-8 Clock:** the eight-output clock, with its LCD.
- **TD-2 Tidal Modulator:** Tides. Its screen has presets, draws one cycle of the slope, and holds SMOOTH and SHIFT on VALUE.
- **DE-1 Tape Echo:** a tape delay for dub. See below.
- **RV-2 Digital Reverb:** NORMAL (the plate from Mutable Instruments Rings) and SPRING (three dispersive springs, whose chirp is DRIP). KICK bangs the tank. DECAY changes how long the tail lasts, not how loud held notes are.
- **DI-4 Ensemble:** the chorus, three settings.
- **PH-6 Phaser:** six stages.

Each effect's screen has its own presets, on its < > keys. A preset leaves your MIX alone unless the unit is off, when it brings it up to be heard. A `*` means you have changed it since.

Values on the screens work like knobs: drag one up or down, double-click it for its default, or roll the wheel over it. A plain click changes nothing. That covers the echo's settings and time, the reverb's tail reading (its DECAY) and type, and Tides' settings, mode, output and trigger.

**FX presets.** One FX preset sets the echo, reverb, chorus and phaser together, and nothing else: your voice, Tides, the clock and OUTPUT stay as they are. The strip at the top of the rack steps through them with its < > keys, and clicking the FX PRESET screen opens the browser. Search, filter by style and by the effects a preset uses, and click a row to hear it. Dry turns every effect off. SAVE keeps the effects as they are now, with a name and a style; your own are in `~/Library/Application Support/Glaser/Millstone/FX Presets/`, and DELETE only reaches those. A `*` means the rack has moved since the preset loaded. The factory ones are written in `vst/make_fx_presets.py`.

**FX LOCK** keeps your effects while you browse synth presets, for trying voices through one setup. It is off by default, so a synth preset loads with the effects it was written with. An FX preset never touches the voice, so you can always load a voice and then pick other effects for it, lock or no lock. Each set remembers its own lock, and sets saved earlier keep the effects they were saved with.

The effects run after the voices, in this plugin and not the shared engine, in the order phaser, chorus, delay, reverb, and they cannot exceed -0.5 dB. With all of them off the signal is not touched. The code is in `src/fx/`; the chorus, phaser and plate wrapper come from the `plaits-synth` project. The page is in `src/RackPage.inc` and the `page=rack` items of `panel.conf`.

**The echo.** TIME follows the song or is free in ms, and moving it glides the tape, bending the pitch of what is on it. REPEATS passes 100%: from there it builds on its own until the tape saturates. HEADS plays any of three heads, at 1x, 2x and 3x TIME. LOW CUT and HIGH CUT are 24 dB per octave filters in the loop, so each repeat is thinner and darker. PING PONG bounces left and right. On the screen, pick DRIVE (tape saturation), WOW (wow and flutter), DUCK (holds the echoes down while you play) or WIDTH (narrows the ping-pong), and turn VALUE. The screen also draws the pattern on the tape.

THROW lets in what you play while you hold it. Close ECHO INPUT and only that reaches the tape. THROW and KICK are parameters, so Live can map them to pads.
