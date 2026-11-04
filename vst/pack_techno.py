"""Warehouse and modular techno: rumble, drive, repetition. Many of these play themselves while a key is held, with the song."""
from preset_helpers import *
from preset_fx import echo, plate, spring, chorus, phaser, both

PACK = "Techno 1"

def T(name, note, type_, styles, characters, *parts, **kw):
    return name, note, P(name, note, *parts, **kw)[2], type_, styles, characters

ENTRIES = [
    T("Rumble Kick", "A kick with its own reverb tail filtered down to a rumble", "Perc", "Techno", "Dark Aggressive", o(1, "Bass Drum", harm=22, timbre=45, morph=48), both(plate(26, decay=62, damping=88)), **DRUM, drive=40),
    T("Peak Kick", "Short, hard, clipped", "Perc", "Techno", "Aggressive", o(1, "Bass Drum", harm=62, timbre=60, morph=30), **DRUM, drive=70),
    T("Ride Bell", "A dry metal ride", "Perc", "Techno House", "Metallic Bright", o(1, "Hi-Hat", harm=62, timbre=42, morph=55), **DRUM),
    T("Noise Hat", "Filtered noise, hard-edged", "Perc", "Techno", "Bright", dict(o1_on="OFF", o2_on="OFF", o3_on="OFF"), noise=100, vca_mode="ENV", attack=0, decay=30, sustain=0, release=8, cutoff=86, f_mode="HP", reso=10, keytrack=0, vel_amp=70, vel_filt=0),
    T("Clap Room", "Burst of noise in a concrete room", "Perc", "Techno House", "Bright", o(1, "Snare Drum", harm=30, timbre=85, morph=30), plate(26, decay=45, damping=40), **DRUM, drive=0),
    T("Rim Click", "As short as it gets", "Perc", "Techno Experimental", "Metallic", o(1, "Modal", oct=1, harm=70, timbre=70, morph=8), **DRUM),
    T("Warehouse Stab", "A single dirty chord, into the room", "Keys", "Techno", "Dark Aggressive", o(1, "Chords", harm=20, timbre=55, morph=45), both(echo("1/8 D", 50, 22, low=50, high=55), plate(26, decay=60, damping=55)), decay=40, sustain=0, release=18, cutoff=46, reso=24, drive=55, voices=4),
    T("Detroit Chord", "Warm, hopeful, slightly out of tune", "Keys", "Techno House", "Warm", o(1, "Chords", harm=70, timbre=35, morph=30), both(chorus("I", 70), echo("1/8 D", 35, 14, high=58), plate(14)), attack=4, decay=60, sustain=25, release=34, cutoff=52, reso=10, f_mod=14, voices=4),
    T("Detroit Strings", "That string line over the top", "Pad", "Techno Classic", "Warm Bright", o(1, "String Machine", harm=50, timbre=62, morph=55), both(chorus("II", 80), plate(20)), attack=44, release=58, cutoff=70, vel_amp=10, drive=0),
    T("Metal Hit", "A struck girder, delay off the side wall", "Riser/FX", "Techno Experimental", "Metallic Aggressive", o(1, "Modal", harm=90, timbre=85, morph=42), both(echo("1/16 D", 40, 20, pong=True, high=62), plate(20, decay=55)), vca_mode="ON", cutoff=100, reso=0, keytrack=0, release=30, drive=40),
    T("Drive Bass", "Saw through an overdriven ladder. Filter it by hand", "Bass", "Techno", "Aggressive Dark", SAW(1, oct=-1), SQUARE(2, oct=-1, fine=4, level=60), **MONO, cutoff=30, reso=38, f_mod=28, m_decay=38, decay=44, sustain=35, drive=80, release=16, vel_filt=25),
    T("Rolling Bass", "Hold a key: sixteenths rolling under everything", "Sequence", "Techno", "Dark", SAW(1, oct=-1), SOFT(2, oct=-2, level=60), cords(("CLOCK 1", "ENV TRIG", 100), ("CLOCK 1", "MOD TRIG", 100)), clock(1, div="x4", shape="TRIG"), **MONO, decay=36, sustain=0, cutoff=26, reso=30, f_mod=44, m_decay=32, drive=45, release=12, vel_filt=0),
    T("Offbeat Bass", "Hold a key: the bass answers the kick", "Sequence", "Techno House", "Warm", SAW(1, oct=-1), SOFT(2, oct=-2, level=55), cords(("CLOCK 1", "ENV TRIG", 100), ("CLOCK 1", "MOD TRIG", 100)), clock(1, div="x1", shape="TRIG", phase=50), **MONO, decay=40, sustain=0, cutoff=32, reso=22, f_mod=36, m_decay=36, drive=30, release=14),
    T("Hypnotic Loop", "Hold a key: seven in sixteen, the filter drifting slowly over it", "Sequence", "Techno Experimental", "Evolving Dark", SAW(1), cords(("CLOCK 1", "ENV TRIG", 100), ("CLOCK 1", "MOD TRIG", 100), ("CLOCK 2", "CUTOFF", 20)), clock(1, div="x4", shape="TRIG", steps=16, fill=7), clock(2, div="/8", shape="TRI", offset=-50), echo("1/8 D", 45, 20, low=50, high=55, pong=True, width=70), **MONO, decay=36, sustain=0, cutoff=32, reso=45, f_mod=40, m_decay=34, drive=25, release=14),
    T("Berlin Sequence", "Hold a key: eighths through a closing filter, tape delay on it", "Sequence", "Classic Ambient", "Warm Evolving", SAW(1), SQUARE(2, oct=-1, level=50), cords(("CLOCK 1", "ENV TRIG", 100), ("CLOCK 1", "MOD TRIG", 100), ("CLOCK 2", "CUTOFF", 18)), clock(1, div="x2", shape="TRIG"), clock(2, div="/16", shape="SINE", offset=-50), echo("1/8 D", 50, 22, wow=25), **MONO, decay=38, sustain=0, cutoff=30, reso=30, f_mod=38, m_decay=34, release=12),
    T("Modular Bleep", "Hold a key: random pitches on a clock, S&H style", "Sequence", "Techno Experimental", "Bright", SOFT(1), cords(("CLOCK 1", "ENV TRIG", 100), ("CLOCK 2", "ALL PITCH", 40)), clock(1, div="x2", shape="TRIG", prob=75), clock(2, div="x2", shape="S/H"), both(echo("1/16 D", 45, 22, pong=True), plate(12)), **MONO, decay=32, sustain=0, cutoff=70, reso=10, release=14, drive=0),
    T("Perc Machine", "Hold a key: a tom and a rim in Euclidean patterns", "Sequence", "Techno Experimental", "Metallic", o(1, "Modal", harm=20, timbre=45, morph=20), o(2, "Hi-Hat", harm=70, timbre=40, morph=15, level=60), cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "O2 TRIG", 100)), clock(1, div="x4", shape="TRIG", steps=16, fill=5), clock(2, div="x4", shape="TRIG", steps=13, fill=4), echo("1/8 D", 30, 12, pong=True), vca_mode="ON", cutoff=100, reso=0, keytrack=0, release=24, voices=2),
    T("Tunnel Pad", "Dark, hissing and very wide", "Pad", "Techno Ambient", "Dark Evolving", SAW(1), SAW(2, fine=11), noise=14, cutoff=34, reso=30, f_src="LFO", f_mod=14, lfo_rate=12, attack=68, release=70, vel_amp=0, vel_filt=0),
    T("Warning Drone", "A low alarm for the middle of the night", "Drone", "Techno Experimental", "Dark Aggressive", SQUARE(1, oct=-1), SAW(2, oct=-1, fine=14), SAW(3, semi=7, fine=-8, level=40), plate(20, damping=70), attack=60, release=66, cutoff=26, reso=45, drive=60, f_src="LFO", f_mod=8, lfo_rate=8, vel_amp=0, vel_filt=0, voices=2),
    T("Scream Lead", "A resonant filter being pushed too far", "Lead", "Techno Rave", "Aggressive Bright", SAW(1), SAW(2, fine=7), echo("1/8 D", 40, 16), VIB, **LEGATO, glide=10, cutoff=62, reso=80, f_mod=18, m_decay=52, drive=60, at_cutoff=25, release=22),
]
assert len(ENTRIES) == 20, len(ENTRIES)
