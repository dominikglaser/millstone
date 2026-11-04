"""Drums, one hit per key, and a few kits that play themselves. Kicks want low notes (C1 to C2); everything else is tuned by the key you play."""
from preset_helpers import *
from preset_fx import echo, plate, spring, chorus, phaser, both

PACK = "Drums 1"
OFF = dict(o1_on="OFF", o2_on="OFF", o3_on="OFF")
HIT = dict(vca_mode="ENV", attack=0, sustain=0, release=8, cutoff=100, reso=0, keytrack=0, vel_amp=70, vel_filt=0, voices=4)
KIT = dict(DRUM, voices=2, vel_amp=0)

def B(name, note, type_, styles, characters, *parts, **kw):
    return name, note, P(name, note, *parts, **kw)[2], type_, styles, characters

ENTRIES = [
    # ---- kicks: HARMONICS is the snap of the attack, TIMBRE the tone, MORPH the length ----------------------------------
    B("Deep 808", "A long sine boom, for trap and dub alike", "Perc", "Classic Dub", "Warm Dark", o(1, "Bass Drum", harm=10, timbre=18, morph=82), **DRUM),
    B("Short 909", "Punchy, with a click on the front", "Perc", "Techno House", "Bright Aggressive", o(1, "Bass Drum", harm=55, timbre=70, morph=38), **DRUM),
    B("House Kick", "Round and steady, for four on the floor", "Perc", "House", "Warm", o(1, "Bass Drum", harm=32, timbre=42, morph=50), **DRUM),
    B("Gabber Kick", "Hit so hard the drum distorts", "Perc", "Rave Techno", "Aggressive", o(1, "Bass Drum", harm=85, timbre=78, morph=58), dict(DRUM, drive=100)),
    B("Tape Kick", "Soft and thick, printed a bit too hot", "Perc", "Dub Classic", "Warm Soft", o(1, "Bass Drum", harm=22, timbre=24, morph=44), dict(DRUM, cutoff=58, drive=45)),
    B("Acoustic Kick", "A real drum with a felt beater, a little room on it", "Perc", "Classic", "Warm", o(1, "Bass Drum", harm=40, timbre=30, morph=28), o(2, "Modal", oct=-1, harm=12, timbre=30, morph=14, level=35), plate(8, decay=25, damping=60), **DRUM),
    # ---- snares and claps ---------------------------------------------------------------------------------------------------
    B("Fat Snare", "Big body, plenty of wire", "Perc", "Classic", "Warm", o(1, "Snare Drum", harm=30, timbre=40, morph=58), plate(12, decay=30), **DRUM),
    B("Piccolo Snare", "High and tight", "Perc", "Classic Dub", "Bright", o(1, "Snare Drum", oct=1, harm=70, timbre=62, morph=30), spring(10, decay=30), **DRUM),
    B("Brush Snare", "Wires only, swept rather than hit", "Perc", "Classic Ambient", "Soft", OFF, plate(10, decay=25, damping=60), HIT, noise=80, decay=44, f_mode="HP", cutoff=66, attack=8, vel_amp=75),
    B("Hand Clap", "Several hands, a dry room", "Perc", "House Classic", "Bright", o(1, "Snare Drum", harm=12, timbre=90, morph=32), plate(14, decay=30, damping=45), dict(DRUM, f_mode="HP", cutoff=52)),
    B("Noise Snare", "Pure synth: a thump and a burst", "Perc", "Techno Rave", "Bright Aggressive", SOFT(1, level=70), cords(("MOD ENV", "O1 PITCH", 30)), HIT, noise=90, decay=38, m_decay=26, drive=25),
    # ---- cymbals ---------------------------------------------------------------------------------------------------------------
    B("Tight Hat", "Shut and clean", "Perc", "Techno House", "Bright Metallic", o(1, "Hi-Hat", harm=25, timbre=72, morph=8), **DRUM),
    B("Sizzle Hat", "Half open, plenty of hiss", "Perc", "House Rave", "Bright Metallic", o(1, "Hi-Hat", harm=55, timbre=78, morph=42), **DRUM),
    B("Pedal Hat", "The foot closing it: dark and short", "Perc", "Classic", "Metallic Soft", o(1, "Hi-Hat", harm=15, timbre=38, morph=14), **DRUM),
    B("Ride Cymbal", "A long even wash with a ping on top", "Perc", "Classic", "Metallic Bright", o(1, "Hi-Hat", oct=-1, harm=40, timbre=62, morph=72), o(2, "Modal", oct=2, harm=88, timbre=70, morph=40, level=30), **DRUM),
    B("Crash", "Hit it, and let it go", "Perc", "Classic Rave", "Metallic Aggressive", o(1, "Hi-Hat", oct=-1, harm=78, timbre=70, morph=94), plate(14, decay=50), **DRUM),
    # ---- hands, sticks and wood: MODAL is a struck body, tuned by the key ----------------------------------------------------------
    B("Bongo High", "The small drum, a finger slap", "Perc", "Classic Dub", "Warm Bright", o(1, "Modal", oct=1, harm=22, timbre=58, morph=20), **DRUM),
    B("Bongo Low", "The big one, open tone", "Perc", "Classic Dub", "Warm", o(1, "Modal", harm=22, timbre=48, morph=24), **DRUM),
    B("Conga", "A long open note with a soft palm", "Perc", "Classic Dub", "Warm", o(1, "Modal", harm=18, timbre=38, morph=36), cords(("MOD ENV", "O1 PITCH", 4)), dict(DRUM, m_decay=30)),
    B("Log Drum", "A big hollow piece of wood. Deep House will thank you", "Perc", "House Dub", "Warm", o(1, "Modal", oct=-1, harm=14, timbre=30, morph=34), SOFT(2, oct=-1, level=45), dict(DRUM, decay=42, sustain=0, vca_mode="ENV")),
    B("Claves", "Two sticks of hard wood: one clear tone, gone almost at once", "Perc", "Classic Dub", "Bright", dict(o1_type="ELEMENTS", vca_mode="ENV", attack=0, decay=46, sustain=0, release=8, cutoff=100, reso=0, keytrack=0, vel_filt=0, vel_amp=60), o(1, oct=2, semi=12), spring(6, decay=25), e_model="MODAL", e_strike=100, e_mallet=20, e_strike_t=20, e_geom=30, e_bright=50, e_damp=30, e_pos=40, e_space=0, e_contour=100),
    B("Cowbell", "The drum machine's: two square tones in the right ratio. Exact around C#4", "Perc", "Classic Techno", "Metallic Bright", SQUARE(1, timbre=30, oct=1, semi=-1), SQUARE(2, timbre=30, oct=1, semi=6, fine=-20), HIT, decay=52, f_mode="HP", cutoff=42, reso=20, keytrack=100, vel_amp=55),
    B("Shaker", "Beads in a tin, a soft push on the front", "Perc", "House Classic", "Soft", OFF, HIT, noise=70, attack=12, decay=30, f_mode="HP", cutoff=88),
    B("Tambourine", "Jingles over a dry slap", "Perc", "Classic House", "Bright Metallic", o(1, "Hi-Hat", oct=1, harm=88, timbre=88, morph=30), HIT, noise=20, decay=40, f_mode="HP", cutoff=72),
    B("Tabla", "A pitched drum that bends as the hand presses", "Perc", "Experimental Ambient", "Warm", o(1, "Modal", oct=1, harm=30, timbre=52, morph=28), cords(("MOD ENV", "O1 PITCH", -14)), dict(DRUM, m_attack=20, m_decay=42, m_sustain=0)),
    # ---- kits that play themselves: hold a key, follows the song -------------------------------------------------------------------
    B("Break Loop", "Hold a key: kick, snare on the backbeat, busy hats", "Sequence", "Rave Classic", "Aggressive", o(1, "Bass Drum", harm=40, timbre=50, morph=38), o(2, "Snare Drum", harm=45, timbre=55, morph=36), o(3, "Hi-Hat", harm=30, timbre=70, morph=14, level=60), cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "O2 TRIG", 100), ("CLOCK 3", "O3 TRIG", 100)), clock(1, div="x4", shape="TRIG", steps=16, fill=5, rot=0), clock(2, div="x1", shape="TRIG", phase=50, swing=10), clock(3, div="x4", shape="TRIG", prob=85), **KIT),
    B("Techno Loop", "Hold a key: four kicks, offbeat hats, a clap on the two", "Sequence", "Techno", "Metallic", o(1, "Bass Drum", harm=55, timbre=58, morph=42), o(2, "Hi-Hat", harm=30, timbre=70, morph=24, level=70), o(3, "Snare Drum", harm=12, timbre=88, morph=30, level=70), cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "O2 TRIG", 100), ("CLOCK 3", "O3 TRIG", 100)), clock(1, div="x1", shape="TRIG"), clock(2, div="x1", shape="TRIG", phase=50), clock(3, div="/2", shape="TRIG", phase=25), plate(8, decay=30, damping=55), **KIT),
    B("Bongo Groove", "Hold a key: two bongos talking in a Euclidean pattern", "Sequence", "Dub Classic", "Warm", o(1, "Modal", oct=1, harm=22, timbre=58, morph=18), o(2, "Modal", harm=22, timbre=48, morph=22), cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "O2 TRIG", 100)), clock(1, div="x4", shape="TRIG", steps=16, fill=7), clock(2, div="x4", shape="TRIG", steps=16, fill=5, rot=3), spring(10, decay=30), **KIT),
    B("Conga And Clave", "Hold a key: the three-two, more or less", "Sequence", "Classic House", "Warm Bright", o(1, "Modal", harm=18, timbre=38, morph=30), o(2, "Modal", oct=2, harm=30, timbre=62, morph=10, level=70), o(3, "Hi-Hat", harm=20, timbre=86, morph=12, level=45), cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "O2 TRIG", 100), ("CLOCK 3", "O3 TRIG", 100)), clock(1, div="x4", shape="TRIG", steps=16, fill=6), clock(2, div="x4", shape="TRIG", steps=16, fill=5), clock(3, div="x4", shape="TRIG", prob=70), **KIT),
    B("Electro Box", "Hold a key: deep kick, rim, tight hats, tape on it", "Sequence", "Classic Techno", "Warm Metallic", o(1, "Bass Drum", harm=15, timbre=22, morph=68), o(2, "Modal", oct=2, harm=70, timbre=70, morph=8, level=60), o(3, "Hi-Hat", harm=25, timbre=72, morph=10, level=55), cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "O2 TRIG", 100), ("CLOCK 3", "O3 TRIG", 100)), clock(1, div="x4", shape="TRIG", steps=16, fill=3), clock(2, div="x1", shape="TRIG", phase=50), clock(3, div="x2", shape="TRIG", swing=15), echo("1/16 D", 20, 8, high=58), **KIT),
]
assert len(ENTRIES) == 30, len(ENTRIES)
