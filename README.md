# Millstone

An 8-voice hybrid desktop synth (VST3, AU and standalone on macOS). See `desktop/README.md` to build and install it.
The engine was first written for Akai MPC OS devices on the [Open MPC port kit](https://github.com/open-mpc/mpc-vst-plugins);
that build (`vst/`) is on hold and untested until a working loader exists, and modules added since are desktop-only.
Each voice has **three Mutable Instruments Plaits macro-oscillators** (all 24 models), a **24 dB ladder filter**
(low-pass or high-pass), **VCA and modulation ADSR envelopes** and a **VCA** with ENV / LPG / ON modes; one global LFO. The
panel follows a semi-modular hybrid desktop synth: VC MIX, OSC MOD (vibrato), glide, noise, a VCF mod depth
with an ENV/LFO source switch. Sequence it from the MPC: there is no sequencer or arpeggiator in the plugin.

Status: **offline-tested only** (x86 test harness, ASan/UBSan). Not yet run on a device.

## Signal flow

```
                 ┌─ OSC 1 (Plaits) ─┐
 MIDI note ──────┼─ OSC 2 (Plaits) ─┼─ mix (+ noise) ─ drive ─ LADDER VCF (LP/HP) ─ VCA ─┐
                 └─ OSC 3 (Plaits) ─┘                                ▲              ▲   │
                                                       ENV or LFO ───┘    ENV/LPG/ON┘   ├─ x8 voices ─ volume ─ out
 LFO (tri/sqr/saw/ramp/S&H, 0.01–350 Hz or tempo-synced) ──► vibrato, VCF, TIMBRE CV, MORPH CV
 VCA ADSR ──► VCA (ENV mode)
 MOD ADSR ──► VCF (MOD SOURCE = ENV), FM (pitch per oscillator), TIMBRE/MORPH CV (CV SOURCE = ENV)
```

## Pages

| Tab | Controls |
|---|---|
| OSC 1 / 2 / 3 | ON, MODEL (24 Plaits models, popup + prev/next), HARMONICS, TIMBRE, MORPH, FM (envelope → pitch, or Plaits' own decay sweep in TRIG mode), TIMBRE CV / MORPH CV (LFO amount into Plaits' modulation inputs), OCTAVE, SEMI, FINE, LEVEL, VC MIX (OUT ↔ AUX output blend) |
| FILTER / ENV | CUTOFF, RESONANCE, MODE (LP/HP), DRIVE, MOD DEPTH (±), MOD SOURCE (ENV/LFO), KEY TRACK; ATTACK, DECAY, SUSTAIN (decay time doubles as release); VCA MODE (ENV / LPG / ON), VOLUME, VEL > AMP, VEL > VCF |
| LFO / VOICE | VOICES (1–8), MODE (POLY / MONO / LEGATO), GLIDE, OSC MOD (vibrato), NOISE; AT > CUTOFF, AT > MORPH, LPG DECAY, LPG COLOUR, BEND RANGE; LFO SHAPE, RATE or DIVISION (SYNC), RETRIG, SYNC, WHEEL > LFO, AT > LFO |

VCA modes: **ENV** shapes every note with the VCA ADSR. **LPG** patches TRIG into Plaits' own low-pass gate
(LPG DECAY / COLOUR), like a hardware trigger input: pings, plucks, drums. **ON** opens the VCA for as long as the
key is held (self-enveloped models such as drums, plucks and 6-op FM ring out). In every mode the self-enveloped
models (6-op FM, string, modal, bass drum, snare, hi-hat) receive a gate from the keys.

MIDI: notes with velocity, mod wheel (CC 1, scaled by WHEEL > LFO), sustain pedal (CC 64), channel and polyphonic
aftertouch, pitch bend (BEND RANGE), all notes off. Q-Links per page, CC 20–35 drive the first page's Q-Links and
NRPN n sets parameter n (port kit defaults).

## Install on the MPC

Download `Millstone-<version>-mpc-aarch64.zip` (Gen2) or `-mpc-armv7.zip` (Gen1, Force). Then either:

- **Plugin Manager** (no SSH; the route for MPC OS 3.10): copy the zip into the `local-plugins/` folder of your
  Plugin Manager USB stick, open the manager, pick it from the **Local** tab, install, return to MPC.
- **SSH**: unzip, `scp -r Millstone-<version> root@<ip>:/tmp/`, then
  `ssh root@<ip> sh /tmp/Millstone-<version>/install.sh -t '<your Synths folder>'` (`/storage/Synths` on an MPC
  One G2 / Key G2, `/nvme/Synths` on a Live III, `/sdcard/Synths` on Gen1 and Force). The installer stops and
  restarts MPC: save your project first.

Then add **Millstone** (manufacturer `glaser`) to a plugin track.

## Build

Needs a checkout of [open-mpc/mpc-vst-plugins](https://github.com/open-mpc/mpc-vst-plugins) next to this
folder (or `MPC_VST=<path>`), Python 3 with Pillow, and a C/C++ compiler.

```
python3 $MPC_VST/tools/gen_vst.py vst/vst.json      # params.h, skin, pluginlist-entry.xml (needs vst/build/shadow_art: see vst/build.sh)
bash $MPC_VST/tools/test_port.sh vst/vst.json        # x86 host test under ASan/UBSan (wrapper level)
tests/run.sh fast build/demo.wav                     # engine tests + a demo recording; tests/run.sh asan for sanitizers
vst/build_zig.sh aarch64                             # Gen2 .so with Zig (pip install ziglang), no Docker needed
$MPC_VST/tools/build_port.sh vst/vst.json all        # the canonical Docker builds (armv7 + aarch64)
python3 $MPC_VST/tools/release.py --so vst/build/aarch64/millstone.so --skin "vst/build/skin/glaser - VST - Millstone" \
    --entry vst/build/pluginlist-entry.xml --version 0.1.0 --id millstone --repo <owner/repo> --license MIT -o dist
```

`vst/params.json` is the single source of truth for parameters (order is the VST index; never reorder a released
list, `vst/params.lock` guards it). The engine reads the generated `params.h`, so a new parameter is one line in
`params.json` plus its use in `src/millstone.cpp`.

## Known notes

- The port kit's host test presses a prev/next button twice within milliseconds; the wrapper's tap debounce
  (250 ms, needed on the device because MPC delivers one finger tap as two presses) ignores the second press, so
  that one check reports FAIL offline. Everything else passes. The engine-level tests (`tests/`) all pass.
- Worst case CPU (8 voices × 3 oscillators, 6-op FM + wave terrain + swarm) is about 340 µs per 128-frame block on
  an x86 core; the Gen2 budget per block is 2.9 ms. Silent voices and oscillators that are OFF cost nothing.

## Credits and licence

Millstone is MIT licensed. Plaits and stmlib are by Emilie Gillet (Mutable Instruments), MIT, vendored in
`src/third_party/eurorack` (`src/VENDORED.md`). The VST2 wrapper, skin tools and installer come from the Open MPC
project (MIT). Not affiliated with Akai, inMusic, Behringer or Mutable Instruments.
