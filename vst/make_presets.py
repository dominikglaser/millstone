#!/usr/bin/env python3
"""Writes presets.json and PRESETS.md from the definitions below. Run it after editing them: python3 vst/make_presets.py

Each preset states only what differs from BASE, but the file lists every parameter: the MPC's PRESET menu sets only the
values a preset lists, so anything left out would leak in from the preset before it. It also means a later change to a
parameter's default cannot alter a saved sound. preset_trim.json holds each preset's measured VOLUME, set by
desktop/tools/preset_test --trim so that a library played one after the other stays even."""
import json, os

HERE = os.path.dirname(os.path.abspath(__file__))
MODELS = ["Virtual Analog", "Waveshaper", "FM 2-Op", "Grain Formant", "Additive", "Wavetable", "Chords", "Speech", "Swarm",
          "Noise", "Particle", "String", "Modal", "Bass Drum", "Snare Drum", "Hi-Hat", "VA VCF", "Phase Dist", "6-Op FM A",
          "6-Op FM B", "6-Op FM C", "Wave Terrain", "String Machine", "Chiptune"]

BASE = {
    "voices": 8, "voice_mode": "POLY", "glide": 0, "vibrato": 0, "noise": 0, "lpg_decay": 50, "lpg_colour": 50,
    "cutoff": 70, "reso": 10, "f_mode": "LP", "f_mod": 0, "f_src": "ENV", "keytrack": 50, "drive": 0,
    "attack": 1, "decay": 50, "sustain": 100, "release": 30, "m_attack": 0, "m_decay": 50, "m_sustain": 0, "m_release": 40,
    "vca_mode": "ENV", "volume": 75, "vel_amp": 40, "vel_filt": 15,
    "lfo_shape": "TRI", "lfo_rate": 45, "lfo_retrig": "OFF", "lfo_sync": "OFF", "lfo_div": "1/4", "lfo_lock": "FREE",
    "wheel_lfo": 0, "at_cutoff": 0, "at_morph": 0, "at_lfo": 0, "bend_range": 2,
}
for n in (1, 2, 3):
    BASE.update({"o%d_%s" % (n, k): v for k, v in dict(
        on="ON" if n == 1 else "OFF", engine="Virtual Analog", harm=50, timbre=50, morph=50, fm=0, timbre_mod=0, morph_mod=0,
        cv_src="LFO", oct=0, semi=0, fine=0, level=100, mix=0).items()})

for n in range(1, 13):   # the patch bay: nothing plugged in unless a preset says so
    BASE.update({"c%d_src" % n: "OFF", "c%d_dst" % n: "OFF", "c%d_amt" % n: 50})

def o(n, model=None, **kw):
    out = {"o%d_on" % n: "ON"}
    if model: out["o%d_engine" % n] = model
    out.update({"o%d_%s" % (n, k): v for k, v in kw.items()})
    return out

def P(name, note, *oscs, **kw):
    d = {}
    for x in oscs: d.update(x)
    d.update(kw)
    return name, note, d

MONO = dict(voice_mode="MONO", voices=1)
LEGATO = dict(voice_mode="LEGATO", voices=1)
DRUM = dict(vca_mode="ON", cutoff=100, reso=0, keytrack=0, vel_amp=70, vel_filt=0, release=40)
VIB = dict(lfo_rate=62, wheel_lfo=100)   # about 6.5 Hz, on the mod wheel

LIBRARY = {
"Bass": [
    P("Round Sub", "Clean low end, slides between held notes", o(1, "Virtual Analog", oct=-1, timbre=30, morph=0), o(2, "Virtual Analog", oct=-2, morph=0, level=45), **LEGATO, glide=8, cutoff=38, f_mod=25, m_decay=42, m_sustain=10, release=25, vel_filt=20),
    P("Acid Line", "Squelchy resonant line. Velocity opens it", o(1, "Virtual Analog", timbre=60, morph=100), **LEGATO, glide=20, cutoff=26, reso=72, f_mod=64, m_decay=40, drive=20, decay=45, sustain=60, vel_filt=40, vel_amp=30),
    P("FM Slap", "Hard attack, quick decay", o(1, "FM 2-Op", harm=25, timbre=45, morph=15, oct=-1), **MONO, decay=48, sustain=0, cutoff=68, f_mod=20, m_decay=40, vel_filt=30, release=25),
    P("Reese", "Slow beating detuned pair", o(1, "Virtual Analog", morph=100), o(2, "Virtual Analog", morph=100, fine=-14), o(3, "Virtual Analog", morph=0, oct=-1, level=55), **LEGATO, glide=6, cutoff=44, reso=20, f_mod=10, f_src="LFO", lfo_rate=32, drive=25, release=35),
    P("Tempo Growl", "Wavetable moving in eighths with the song", o(1, "Wavetable", oct=-1, harm=30, timbre=60, morph=40, timbre_mod=45, morph_mod=30), **MONO, lfo_sync="ON", lfo_div="1/8", lfo_lock="BAR", cutoff=52, reso=40, f_mod=25, f_src="LFO", drive=25),
    P("Pluck Bass", "Driven and short, sits well in a groove", o(1, "Virtual Analog", timbre=45, morph=80), o(2, "Virtual Analog", oct=-1, morph=0, level=60), **MONO, decay=44, sustain=15, cutoff=24, f_mod=68, m_decay=38, reso=30, drive=50, release=20),
    P("Modal Thump", "Struck body with a triangle underneath", o(1, "Modal", harm=20, timbre=25, morph=35), o(2, "Virtual Analog", oct=-1, morph=0, level=70), **MONO, decay=56, sustain=0, cutoff=72, vel_amp=60, release=25),
    P("Phase Bass", "Bright bite that closes as the envelope falls", o(1, "Phase Dist", oct=-1, harm=20, timbre=35, morph=45, timbre_mod=45, cv_src="ENV"), **LEGATO, glide=5, m_decay=46, m_sustain=10, cutoff=56, f_mod=25, release=25),
],
"Lead": [
    P("Classic Lead", "Fat detuned saws. Mod wheel adds vibrato", o(1, "Virtual Analog", morph=100), o(2, "Virtual Analog", morph=100, fine=6), **LEGATO, **VIB, vibrato=10, glide=12, cutoff=60, reso=25, f_mod=30, m_decay=55, m_sustain=30),
    P("Sync Scream", "Timbre sweeps down from the envelope", o(1, "Virtual Analog", timbre=70, morph=100, timbre_mod=35, cv_src="ENV"), o(2, "Virtual Analog", fine=-5, timbre=70, morph=100, timbre_mod=35, cv_src="ENV"), **MONO, **VIB, vibrato=10, m_decay=60, m_sustain=15, cutoff=70, reso=30),
    P("Square Hero", "Hollow square, pressure brightens it", o(1, "Virtual Analog", timbre=50, morph=0), **LEGATO, **VIB, vibrato=10, glide=8, cutoff=62, reso=15, at_cutoff=35, f_mod=15),
    P("Glass FM", "Clear and bell-like", o(1, "FM 2-Op", harm=60, timbre=38, morph=20), **MONO, **VIB, vibrato=8, at_morph=30, decay=60, sustain=70, cutoff=85),
    P("Formant Cry", "Vocal, with a slow moving formant", o(1, "Grain Formant", timbre=55, morph=40, timbre_mod=25), **LEGATO, vibrato=12, lfo_rate=60, glide=10, cutoff=80),
    P("Chip Lead", "8-bit arpeggio voice", o(1, "Chiptune", harm=20, timbre=50, morph=50), **MONO, cutoff=90, reso=0, keytrack=0, release=15),
    P("Wave Solo", "Wavetable that opens on each note", o(1, "Wavetable", harm=55, timbre=70, morph=30, morph_mod=40, cv_src="ENV"), **LEGATO, **VIB, vibrato=10, glide=6, m_decay=62, m_sustain=30, cutoff=72),
    P("Overdrive", "Hot and resonant. Pressure opens the filter", o(1, "Virtual Analog", morph=100), o(2, "Waveshaper", harm=40, timbre=45, morph=50, level=70), **LEGATO, glide=10, drive=70, reso=55, cutoff=56, f_mod=20, at_cutoff=40),
],
"Keys": [
    P("Tine Piano", "Electric piano. Plays softer and darker gently", o(1, "FM 2-Op", harm=33, timbre=30, morph=5), o(2, "FM 2-Op", harm=58, timbre=18, morph=0, level=50), decay=78, sustain=0, release=48, cutoff=74, vel_filt=40, vel_amp=70),
    P("DX Keys", "A six-op patch, HARMONICS picks another", o(1, "6-Op FM A", harm=15, timbre=50, morph=50), sustain=100, cutoff=90, reso=0, keytrack=0, vel_amp=60, release=35),
    P("Soft Organ", "Steady and rounded", o(1, "Additive", harm=35, timbre=40, morph=60), vibrato=8, lfo_rate=60, release=22, cutoff=82, vel_amp=0, vel_filt=0),
    P("Reed Organ", "Buzzier, with a slower swell", o(1, "Waveshaper", harm=35, timbre=30, morph=30), o(2, "Additive", oct=1, harm=25, timbre=30, morph=50, level=50), attack=12, release=28, cutoff=74, vel_amp=10, vibrato=8, lfo_rate=58),
    P("Funk Clav", "Bright and percussive through the high-pass", o(1, "Virtual Analog", timbre=60, morph=100), o(2, "Virtual Analog", timbre=30, morph=100, fine=4, level=70), f_mode="HP", cutoff=24, keytrack=100, reso=20, decay=58, sustain=0, release=22, vel_amp=70, drive=20),
    P("Harpsi", "Octave-doubled, sharp attack", o(1, "Phase Dist", harm=25, timbre=60, morph=35), o(2, "Phase Dist", oct=1, fine=5, harm=25, timbre=60, morph=35, level=65), decay=66, sustain=0, release=30, cutoff=88),
    P("Mellow Poly", "Warm eighties polysynth", o(1, "Virtual Analog", morph=100), o(2, "Virtual Analog", morph=100, fine=9), attack=8, decay=58, sustain=70, release=48, cutoff=42, f_mod=35, m_decay=60, m_sustain=30),
    P("Chord Memory", "One key plays a whole chord", o(1, "Chords", harm=30, timbre=40, morph=40), decay=62, sustain=0, cutoff=62, f_mod=25, m_decay=50, release=38, voices=4),
],
"Pad": [
    P("Warm Strings", "String machine ensemble", o(1, "String Machine", harm=45, timbre=55, morph=50), attack=74, release=80, cutoff=70, vel_amp=15),
    P("Analog Pad", "Three saws, filter opens slowly", o(1, "Virtual Analog", morph=100), o(2, "Virtual Analog", morph=100, fine=8), o(3, "Virtual Analog", morph=100, fine=-11, oct=-1, level=60), attack=76, release=82, cutoff=38, f_mod=32, m_attack=84, m_decay=78, m_sustain=70, m_release=80, vel_amp=15),
    P("Swarm Pad", "Dense cloud of detuned voices", o(1, "Swarm", harm=60, timbre=50, morph=55), attack=72, release=82, cutoff=66, vel_amp=15),
    P("Glass Pad", "Wavetable shimmer, slow movement", o(1, "Wavetable", harm=60, timbre=62, morph=30, morph_mod=35), o(2, "Wavetable", oct=1, harm=60, timbre=70, morph=60, morph_mod=-30, level=50, fine=6), attack=70, release=84, lfo_rate=25, cutoff=80, vel_amp=15),
    P("Formant Choir", "Breathy choir", o(1, "Grain Formant", harm=50, timbre=45, morph=30), o(2, "Grain Formant", harm=50, timbre=50, morph=30, fine=7, level=80), vibrato=8, lfo_rate=58, attack=74, release=78, cutoff=76, vel_amp=15),
    P("Terrain Drift", "Waveform slowly wandering", o(1, "Wave Terrain", harm=40, timbre=50, morph=50, timbre_mod=40, morph_mod=30), lfo_rate=22, attack=74, release=82, cutoff=72, vel_amp=15),
    P("Chord Cloud", "Each key blooms into a chord", o(1, "Chords", harm=50, timbre=50, morph=60), o(2, "Swarm", harm=55, timbre=50, morph=50, level=45), attack=78, release=84, cutoff=62, voices=4, vel_amp=15),
    P("Dark Drone", "Low and heavy, long release", o(1, "Virtual Analog", oct=-1, morph=100), o(2, "Virtual Analog", oct=-2, morph=100, fine=5), noise=8, attack=84, release=88, cutoff=30, reso=30, f_mod=8, f_src="LFO", lfo_rate=15, vel_amp=0, vel_filt=0),
],
"Pluck": [
    P("Harp String", "Physical string, rings by itself", o(1, "String", harm=40, timbre=50, morph=55), vca_mode="ON", cutoff=92, reso=0, keytrack=0, drive=20, release=50, vel_amp=60),
    P("Modal Bell", "Struck metal", o(1, "Modal", harm=45, timbre=65, morph=60), vca_mode="ON", cutoff=100, reso=0, keytrack=0, release=50, vel_amp=60),
    P("Nylon", "Soft, darker string", o(1, "String", harm=25, timbre=30, morph=45), o(2, "String", harm=25, timbre=30, morph=45, fine=3), vca_mode="ON", cutoff=80, reso=5, keytrack=70, drive=40, release=45, vel_filt=30, vel_amp=60),
    P("Synth Pluck", "The classic, envelope snapping the filter shut", o(1, "Virtual Analog", morph=100), o(2, "Virtual Analog", morph=100, fine=7), decay=52, sustain=0, cutoff=30, reso=20, f_mod=60, m_decay=42, release=36),
    P("FM Pluck", "Bright metallic transient", o(1, "FM 2-Op", harm=40, timbre=20, morph=10, timbre_mod=60, cv_src="ENV"), decay=50, sustain=0, m_decay=38, cutoff=82, release=32),
    P("Kalimba", "Small tuned tines", o(1, "Modal", harm=25, timbre=30, morph=25), vca_mode="ON", cutoff=88, reso=0, keytrack=0, release=40, vel_amp=60),
    P("LPG Marimba", "Woody, through the low-pass gate", o(1, "Virtual Analog", timbre=50, morph=0), vca_mode="LPG", lpg_decay=40, lpg_colour=70, cutoff=86, release=30),
    P("LPG Pluck", "Gate-struck FM with a soft edge", o(1, "FM 2-Op", harm=45, timbre=35, morph=15), vca_mode="LPG", lpg_decay=34, lpg_colour=30, cutoff=90, release=30),
],
"Percussion": [
    P("Kick 808", "Long and low. Try around C1-C2", o(1, "Bass Drum", harm=20, timbre=30, morph=60), **DRUM),
    P("Kick Punch", "Short and tight", o(1, "Bass Drum", harm=70, timbre=50, morph=40), **DRUM),
    P("Snare Analog", "Body and noise balance", o(1, "Snare Drum", harm=40, timbre=50, morph=45), **DRUM),
    P("Snare Crack", "Thin and snappy", o(1, "Snare Drum", harm=80, timbre=70, morph=35), **DRUM),
    P("Closed Hat", "Short and metallic", o(1, "Hi-Hat", harm=30, timbre=60, morph=22), **DRUM),
    P("Open Hat", "Longer ring", o(1, "Hi-Hat", harm=30, timbre=60, morph=65), **DRUM),
    P("FM Tom", "Pitch drops with the envelope", o(1, "FM 2-Op", harm=15, timbre=20, morph=0, fm=30), dict(), decay=50, sustain=0, m_decay=46, cutoff=90, reso=0, keytrack=0, vel_amp=70, vel_filt=0, release=25),
    P("Metal Perc", "Clangy struck object", o(1, "Modal", harm=80, timbre=70, morph=30), **DRUM),
],
"Texture": [
    P("Granular Air", "Sparse grains, hold it", o(1, "Particle", harm=50, timbre=40, morph=60), attack=40, release=70, cutoff=82, vel_amp=0, vel_filt=0),
    P("Noise Wash", "Filter sweeping filtered noise, like surf", o(1, "Noise", harm=50, timbre=50, morph=50), attack=45, release=70, cutoff=52, reso=25, f_mod=25, f_src="LFO", lfo_rate=24, vel_amp=0, vel_filt=0),
    P("Talking Box", "Speech model mumbling on the LFO", o(1, "Speech", harm=50, timbre=50, morph=30, morph_mod=60), lfo_rate=42, cutoff=84, release=40),
    P("Swarm Motion", "Random steps through a swarm", o(1, "Swarm", harm=60, timbre=45, morph=50, timbre_mod=40), lfo_shape="S/H", lfo_rate=42, attack=30, release=60, cutoff=70),
    P("Terrain Walk", "Stepping waveform shapes", o(1, "Wave Terrain", harm=60, timbre=50, morph=50, timbre_mod=45, morph_mod=45), lfo_shape="S/H", lfo_rate=38, attack=20, release=55, cutoff=76),
    P("Breath Tone", "Airy tone with noise on top", o(1, "Virtual Analog", timbre=50, morph=0, level=75), noise=90, attack=38, release=58, cutoff=66, reso=35, keytrack=100, vel_amp=30),
    P("Hollow Sweep", "Resonance tracing a slow LFO", o(1, "Virtual Analog", morph=0), attack=25, release=60, reso=85, cutoff=44, f_mod=28, f_src="LFO", lfo_rate=34, vel_amp=0),
    P("Tempo Motion", "Half-note filter locked to the song", o(1, "Wavetable", harm=50, timbre=60, morph=40), o(2, "Wavetable", harm=50, timbre=60, morph=40, fine=7), lfo_sync="ON", lfo_div="1/2", lfo_lock="BAR", cutoff=46, reso=35, f_mod=40, f_src="LFO", attack=15, release=50),
],
"FX": [
    P("Riser", "Hold it: rises over a few seconds", o(1, "Virtual Analog", morph=100, fm=25), noise=45, cutoff=18, reso=35, f_mod=80, m_attack=100, m_sustain=100, m_release=60, attack=45, release=55, voices=2, vel_amp=0, vel_filt=0),
    P("Downer", "Pitch falls away from the attack", o(1, "Virtual Analog", morph=100, fm=70), m_decay=72, cutoff=70, reso=30, f_mod=20, sustain=0, decay=75, release=40),
    P("Laser", "Fast pitch zap", o(1, "FM 2-Op", harm=50, timbre=50, morph=30, fm=100), m_decay=38, sustain=0, decay=48, reso=40, cutoff=80, release=25),
    P("Random Bleeps", "Stepped pitches locked to the song", o(1, "Virtual Analog", timbre=50, morph=0), lfo_shape="S/H", lfo_sync="ON", lfo_div="1/8", lfo_lock="BAR", vibrato=55, reso=30, cutoff=72, voices=2),
    P("Whistle", "Self-oscillating filter, playable and tuned", o(1, "Virtual Analog", level=0), dict(o1_on="OFF"), noise=4, reso=100, keytrack=100, cutoff=58, attack=15, release=40, vel_amp=0, vel_filt=0),
    P("Alarm", "Two-tone siren", o(1, "Chiptune", harm=20, timbre=50, morph=50), lfo_shape="SQR", lfo_rate=42, vibrato=55, cutoff=90, reso=0, keytrack=0),
    P("Sonar Ping", "Resonant ping with a long tail", o(1, "Modal", harm=35, timbre=40, morph=68), vca_mode="ON", reso=45, cutoff=70, keytrack=100, release=60, voices=4),
    P("Vinyl Dust", "Crackle over a thin noise bed", o(1, "Particle", harm=85, timbre=70, morph=65), noise=50, attack=25, release=50, cutoff=30, f_mode="HP", keytrack=0, vel_amp=0, vel_filt=0),
],
}

def main():
    spec = json.load(open(os.path.join(HERE, "params.json")))
    params = [p for p in spec["params"] if not p.get("momentary")]
    keys = [p["key"] for p in params]
    missing = [k for k in keys if k not in BASE]
    assert not missing, "BASE lacks %s" % missing
    trim_path = os.path.join(HERE, "preset_trim.json")
    trim = json.load(open(trim_path)) if os.path.exists(trim_path) else {}

    def default(p):
        return p["default"]
    out = [{"name": "Init", "category": "Init", "values": {p["key"]: default(p) for p in params}}]
    seen = {"Init"}
    table = []
    for category, entries in LIBRARY.items():
        assert len(entries) == 8, "%s has %d" % (category, len(entries))
        for name, note, delta in entries:
            assert len(name) <= 24 and name not in seen, name
            seen.add(name)
            values = dict(BASE)
            for k, v in delta.items():
                assert k in values, "%s: %s is not a parameter" % (name, k)
                values[k] = v
            if name in trim:
                values["volume"] = trim[name]
            out.append({"name": name, "category": category, "values": {k: values[k] for k in keys}})
            table.append((category, name, note))
    with open(os.path.join(HERE, "presets.json"), "w") as f:
        json.dump({"presets": out}, f, indent=1)
        f.write("\n")
    with open(os.path.join(HERE, "PRESETS.md"), "w") as f:
        f.write("# Millstone presets\n\nGenerated by `make_presets.py`. Init is the default patch; the other %d are below.\n" % len(table))
        last = None
        for category, name, note in table:
            if category != last:
                f.write("\n## %s\n\n| Preset | Character |\n|---|---|\n" % category)
                last = category
            f.write("| %s | %s |\n" % (name, note))
    print("wrote %d presets" % len(out))

if __name__ == "__main__":
    main()
