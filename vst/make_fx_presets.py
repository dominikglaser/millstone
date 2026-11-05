#!/usr/bin/env python3
"""Writes fx_presets.json: whole-rack presets for the effects, separate from the synth presets. Run it after editing: python3 vst/make_fx_presets.py

A preset sets the echo, reverb, chorus and phaser together, and nothing else: no voice, no Tides, no clock, no OUTPUT. Anything a
preset does not list goes to its default, which for every mix means off, so loading one replaces the whole rack."""
import json, os
from preset_fx import echo, plate, spring, chorus, phaser, both

HERE = os.path.dirname(os.path.abspath(__file__))

def free(ms, fb, mix, low=25, high=72, drive=20, wow=10):
    return dict(fx_dl_sync="FREE", fx_dl_time=ms, fx_dl_fb=fb, fx_dl_mix=mix, fx_dl_low=low, fx_dl_high=high, fx_dl_drive=drive, fx_dl_wow=wow)

def F(name, note, styles, *parts):
    return name, note, styles.split(), both(*parts)

FX_PRESETS = [
    F("Dry", "Everything off: the voice on its own", "Classic Techno Dub House Rave Ambient Cinematic Experimental"),

    F("Echo Chamber", "Dark tape into a wet spring. The sound of the mixing desk as an instrument", "Dub",
      echo("1/8 D", 62, 30, low=35, high=56, drive=35, wow=25), spring(26, decay=60)),
    F("Steppers Tape", "Dotted eighths, tight and rolled off, for skanks and melodica", "Dub",
      echo("1/8 D", 52, 24, low=38, high=62, drive=28, wow=18), spring(12, decay=35)),
    F("Roots Spring", "Mostly spring: the drip behind a guitar or an organ", "Dub",
      spring(38, decay=55, drip=80, damping=25), echo("1/4", 30, 8, low=40, high=55)),
    F("Long Tape Throw", "Long repeats that darken as they go. Ride the mix", "Dub",
      echo("1/4 D", 78, 34, low=40, high=50, drive=45, wow=30, heads="1+3")),

    F("Swamp Phaser", "A slow, resonant phaser into worn tape, for drums and everything else on the desk", "Dub",
      phaser(55, rate=26, depth=80, feedback=58), echo("1/8 D", 50, 20, low=38, high=54, drive=35, wow=28), spring(14, decay=45)),

    F("Dub Techno Chord", "Wide dotted repeats over a dark room, for one chord held forever", "Techno Dub",
      echo("1/4 D", 70, 32, low=45, high=58, drive=30, wow=12, pong=True, width=60), plate(26, decay=72, damping=62), chorus("I", 40)),
    F("Warehouse", "Short, hard and dark: a concrete room with one bounce", "Techno",
      echo("1/8", 35, 14, low=48, high=60, drive=30, wow=6), plate(20, decay=38, diffusion=50, damping=68)),
    F("Hypnotic Phase", "A slow sweep with repeats behind it, for a loop that must not stay still", "Techno",
      phaser(55, rate=22, depth=75, feedback=55), echo("1/8 D", 42, 16, low=45, high=62, wow=8)),
    F("Dry Room", "Barely there: a little air around a stab", "Techno House",
      plate(10, decay=22, diffusion=50, damping=55)),

    F("House Plate", "A bright, short plate that sits under chords and keys", "House Classic",
      plate(20, decay=42, diffusion=65, damping=25)),
    F("Deep House Keys", "Soft chorus, a ducked echo and a warm room", "House",
      chorus("II", 65), echo("1/8 D", 38, 16, low=42, high=64, duck=35), plate(14, decay=45, damping=50)),
    F("Disco Phaser", "A quick shimmer, for a clav or a string line", "House Classic",
      phaser(45, rate=48, depth=60, feedback=30), plate(10, decay=30)),

    F("Rave Hall", "Big, bright and obvious", "Rave",
      plate(34, decay=70, diffusion=75, damping=20), chorus("I", 50)),
    F("Hoover Space", "Chorus at full width and a long hall, for anything that scoops", "Rave Cinematic",
      chorus("I+II", 100), plate(30, decay=68, diffusion=75, damping=35)),
    F("Jungle Echo", "Fast bright repeats that bounce across the room", "Rave Dub",
      echo("1/8", 48, 22, low=30, high=74, drive=20, wow=8, pong=True, width=85), spring(12, decay=30)),

    F("Long Cloud", "A very long, soft tail with no edges", "Ambient",
      plate(48, decay=100, diffusion=85, damping=45)),
    F("Slow Shimmer", "A slow phase moving inside a long hall", "Ambient Cinematic",
      phaser(40, rate=12, depth=70, feedback=35), plate(38, decay=88, diffusion=80, damping=30), chorus("I", 45)),
    F("Tape Fog", "Worn repeats disappearing into the hall", "Ambient Experimental",
      echo("1/2", 64, 28, low=45, high=48, drive=40, wow=45), plate(32, decay=82, damping=60)),

    F("Distant Hall", "Pushed well back: for a line that belongs behind the mix", "Cinematic",
      plate(50, decay=80, diffusion=80, damping=55), echo("1/4", 34, 14, low=48, high=56)),
    F("Trailer Room", "An enormous space with a hard top end", "Cinematic Rave",
      plate(42, decay=92, diffusion=70, damping=15), chorus("II", 45)),

    F("Studio Plate", "Clean and even, the plate on everything", "Classic",
      plate(22, decay=48, diffusion=65, damping=35)),
    F("Polysynth Width", "The chorus an eighties polysynth came with, and a small room", "Classic House",
      chorus("II", 100), plate(12, decay=30, damping=45)),
    F("Slapback", "One quick repeat", "Classic Dub",
      free(110, 10, 22)),
    F("Stone Phaser", "A resonant seventies phaser, nothing else", "Classic",
      phaser(60, rate=40, depth=70, feedback=60)),

    F("Runaway Tape", "Repeats that feed themselves. Keep a hand on the mix", "Experimental Dub",
      echo("1/4 T", 108, 30, low=30, high=55, drive=55, wow=30)),
    F("Broken Spring", "A spring kicked too hard, with a nervous chorus on it", "Experimental",
      spring(45, decay=85, drip=100, damping=10), chorus("I+II", 70)),
    F("Metal Phase", "Tight resonant sweeps feeding short repeats", "Experimental Techno",
      phaser(75, rate=64, depth=90, feedback=88), echo("1/16", 40, 18, low=40, high=72, wow=0)),
]

FX_PRESETS += [
    F("Melted Tape", "Slow repeats sagging in pitch, for pads and anything held", "Ambient Experimental",
      echo("1/2", 42, 24, low=40, high=50, wow=70), plate(28, decay=60)),
    F("Water Tank", "A spring thrown wide open: the tank is the sound", "Dub Experimental",
      spring(55, decay=85, drip=90, damping=15)),
    F("Hall Of Pings", "Bouncing echoes crossing a large, even room", "Cinematic Ambient",
      echo("1/4 D", 60, 30, high=55, pong=True), plate(30, decay=70)),
    F("Bent Room", "Phaser, swung repeats and a long room, all moving at once", "Experimental Cinematic",
      phaser(50, rate=18, feedback=80), echo("1/4 T", 60, 30, wow=60, pong=True), plate(34, decay=78)),
    F("Insect Scatter", "Very short ping-pong for sharp hits and small sounds", "Experimental Techno",
      echo("1/16 D", 50, 24, pong=True), plate(24, decay=58)),
    F("Lab Bench", "A dry room and a small spring: lets a patch speak for itself", "Experimental Classic",
      spring(16, decay=35), plate(10, decay=30)),
    F("Valve Delay", "Overdriven repeats that break up as they stack", "Rave Experimental",
      echo("1/8 D", 40, 20, low=30, high=60, drive=70, wow=15)),
    F("Sea Cave", "A slow phaser inside a dark, deep space", "Ambient Dub",
      phaser(45, rate=10, depth=80, feedback=70), plate(34, decay=78, damping=60)),
    F("Chorus Cloud", "Everything wide: full chorus into a soft hall", "Ambient Classic",
      chorus("I+II", 100), plate(26, decay=62, damping=45)),
    F("Sixteenth Machine", "Tight synced repeats that lock a sequence to the song", "Techno House",
      echo("1/16", 40, 18, low=45, high=64, wow=4)),
    F("Dying Transmitter", "Dark tape, spring and a narrow slow phase", "Experimental Dub",
      echo("1/4 D", 55, 24, low=50, high=44, drive=40, wow=40), spring(20, decay=45), phaser(30, rate=24, feedback=50)),
    F("Afterimage", "A tail with nothing up front: pushes a pluck far away", "Cinematic Ambient",
      echo("1/8 D", 45, 22, low=35, high=58, pong=True, width=70), plate(42, decay=82, damping=35)),
]

def uses(values):
    used = []
    if values.get("fx_dl_mix", 0) > 0: used.append("Delay")
    if values.get("fx_rv_mix", 0) > 0: used.append("Reverb")
    if values.get("fx_ch_mode", "OFF") != "OFF": used.append("Chorus")
    if values.get("fx_ph_mix", 0) > 0: used.append("Phaser")
    return used

def main():
    params = {p["key"]: p for p in json.load(open(os.path.join(HERE, "params.json")))["params"]}
    styles = {"Classic", "Techno", "Dub", "House", "Rave", "Ambient", "Cinematic", "Experimental"}
    out, seen = [], set()
    for name, note, style, values in FX_PRESETS:
        assert name.lower() not in seen and len(name) <= 24, name
        seen.add(name.lower())
        assert style and set(style) <= styles, name
        for k, v in values.items():
            assert k.startswith("fx_") and k != "fx_out", "%s: %s is not an effect setting" % (name, k)
            p = params[k]
            assert v in p["options"] if p.get("options") else p["min"] <= v <= p["max"], "%s: %s=%r" % (name, k, v)
        out.append(dict(name=name, note=note, styles=style, uses=uses(values), values=values))
    with open(os.path.join(HERE, "fx_presets.json"), "w") as f:
        json.dump({"presets": out}, f, indent=1)
        f.write("\n")
    print("wrote %d FX presets" % len(out))

if __name__ == "__main__":
    main()
