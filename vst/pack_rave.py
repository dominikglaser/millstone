"""Early rave and everything after it: hoovers, stabs, supersaws, risers. Hoovers live on portamento: play them legato."""
from preset_helpers import *
from preset_fx import echo, plate, spring, chorus, phaser, both

PACK = "Rave 1"

def R(name, note, type_, styles, characters, *parts, **kw):
    return name, note, P(name, note, *parts, **kw)[2], type_, styles, characters

SCOOP = cords(("MOD ENV", "ALL PITCH", -32))     # starts below the note and climbs into it
HOOVER = dict(voice_mode="LEGATO", voices=1, glide=36, cutoff=70, reso=8, keytrack=30, drive=30, m_decay=52, m_sustain=0, attack=4, release=34, vel_amp=10, vel_filt=0)

ENTRIES = [
    R("Hoover", "The one. Pulse and saw an octave apart, scooping up into every note", "Lead", "Rave", "Aggressive Bright", SAW(1), SQUARE(2, oct=-1, fine=8, level=80), SAW(3, oct=1, fine=-11, level=30), SCOOP, both(chorus("I+II", 100), plate(14)), **HOOVER),
    R("Deep Hoover", "Darker and heavier, for the drop. Play it low", "Bass", "Rave Techno", "Aggressive Dark", SAW(1, oct=-1), SQUARE(2, oct=-1, fine=9, level=85), SOFT(3, oct=-2, level=50), SCOOP, chorus("I+II", 80), dict(HOOVER, cutoff=52, reso=14)),
    R("Hoover Rise", "Hold it and it climbs for bars", "Riser/FX", "Rave Cinematic", "Aggressive Evolving", SAW(1), SQUARE(2, oct=-1, fine=7, level=80), cords(("MOD ENV", "ALL PITCH", 60)), both(chorus("I+II", 100), plate(18)), dict(HOOVER, m_attack=92, m_decay=80, m_sustain=100, m_release=60, glide=0)),
    R("Belgian Mental", "That chopped, droning stab that started it all", "Riser/FX", "Rave Techno", "Aggressive Metallic", SAW(1), SAW(2, semi=7, fine=6, level=80), SQUARE(3, oct=-1, level=55), chorus("I+II", 80), f_mod=30, m_decay=46, decay=52, sustain=20, cutoff=48, reso=30, drive=55, release=20),
    R("Rave Stab", "A big minor chord, one finger", "Keys", "Rave House", "Bright Aggressive", o(1, "Chords", harm=30, timbre=70, morph=60), both(echo("1/8 D", 30, 14), plate(16)), decay=44, sustain=0, release=18, cutoff=72, reso=18, drive=40, voices=4),
    R("Piano Rave", "Hard digital piano, chord memory, all hands up", "Keys", "Rave House", "Bright", o(1, "FM 2-Op", harm=30, timbre=52, morph=14), o(2, "Chords", harm=30, timbre=60, morph=50, level=50), plate(18), decay=62, sustain=0, release=30, cutoff=92, reso=0, keytrack=0, vel_amp=45, vel_filt=0, voices=6),
    R("Organ Donor", "A rave organ, driven", "Keys", "Rave House", "Aggressive Warm", o(1, "Additive", harm=56, timbre=60, morph=40), chorus("I", 60), decay=44, sustain=50, release=16, cutoff=80, drive=45, vel_amp=20, vel_filt=0),
    R("Super Saw", "Seven saws' worth of detune, for the anthem", "Lead", "Rave Cinematic", "Bright", SAW(1), SAW(2, fine=11), SAW(3, fine=-13), both(chorus("I+II", 60), echo("1/8 D", 35, 14, pong=True), plate(18)), attack=4, release=38, cutoff=78, reso=4, f_mod=8, vel_amp=20),
    R("Trance Pluck", "Short and bright, wants a long delay after it", "Pluck", "Rave", "Bright", SAW(1), SAW(2, fine=9), both(echo("1/8 D", 52, 26, pong=True), plate(18)), decay=44, sustain=0, cutoff=34, reso=22, f_mod=62, m_decay=38, release=30, vel_filt=25),
    R("Gate Anthem", "Hold a chord: sixteenths, song tempo, no mercy", "Sequence", "Rave", "Bright", SAW(1), SAW(2, fine=10), cords(("CLOCK 1", "VCA", 100)), clock(1, div="x4", shape="GATE", width=45, level=100), both(echo("1/8 D", 30, 12), plate(14)), vca_mode="ON", cutoff=72, reso=8, release=22, voices=6),
    R("Acid Squelch", "The little silver box, wound right up", "Bass", "Rave Techno", "Aggressive Bright", SAW(1), **LEGATO, glide=14, cutoff=20, reso=82, f_mod=72, m_decay=36, decay=46, sustain=55, drive=45, vel_filt=45, vel_amp=30, release=16),
    R("Acid Square", "The same box, other switch", "Bass", "Rave Techno", "Aggressive", SQUARE(1), **LEGATO, glide=14, cutoff=22, reso=78, f_mod=66, m_decay=36, decay=46, sustain=55, drive=38, vel_filt=45, vel_amp=30, release=16),
    R("Jungle Sub", "Clean, round, and very loud in a room", "Bass", "Rave Dub", "Warm Soft", SOFT(1, oct=-1), **MONO, cutoff=40, reso=0, keytrack=0, drive=18, release=16, vel_amp=20, vel_filt=0),
    R("Neuro Reese", "A Reese with its filter chewing in eighths", "Bass", "Rave Techno", "Aggressive Dark", SAW(1, fine=-11), SAW(2, fine=11), SOFT(3, oct=-1, level=50), both(phaser(30, rate=30, depth=80, feedback=55), chorus("I", 25)), **LEGATO, glide=5, cutoff=40, reso=30, f_src="LFO", f_mod=30, lfo_sync="ON", lfo_div="1/8", lfo_lock="BAR", drive=55, release=24, vel_filt=0),
    R("White Riser", "Noise that opens for as long as you hold it", "Riser/FX", "Rave Techno Cinematic", "Bright Evolving", dict(o1_on="OFF", o2_on="OFF", o3_on="OFF"), both(plate(24, decay=70), echo("1/8 D", 45, 18, pong=True)), noise=100, cutoff=12, reso=25, f_mod=90, m_attack=100, m_decay=90, m_sustain=100, m_release=40, keytrack=0, attack=20, release=42, vel_amp=0, vel_filt=0, voices=1),
    R("Pitch Riser", "Saw climbing several octaves. Hold for the build", "Riser/FX", "Rave Techno", "Bright Evolving", SAW(1), SAW(2, fine=12), cords(("MOD ENV", "ALL PITCH", 70)), plate(20, decay=65), m_attack=96, m_decay=90, m_sustain=100, m_release=30, attack=30, release=32, cutoff=60, reso=20, f_mod=30, vel_amp=0, vel_filt=0, voices=1),
    R("Snare Build", "Hold a key: a snare roll faster than you can count", "Riser/FX", "Rave", "Aggressive Bright", o(1, "Snare Drum", harm=60, timbre=55, morph=25), cords(("CLOCK 1", "O1 TRIG", 100)), clock(1, div="x4", shape="TRIG"), plate(18), vca_mode="ON", cutoff=100, reso=0, keytrack=0, release=24, voices=1, vel_amp=0, vel_filt=0),
    R("Downlifter", "From the top of the room to the floor", "Riser/FX", "Rave Cinematic", "Dark Evolving", dict(o1_on="OFF", o2_on="OFF", o3_on="OFF"), plate(30, decay=70), noise=100, cutoff=92, reso=18, f_mod=-70, m_decay=88, m_sustain=0, keytrack=0, attack=2, decay=86, sustain=0, release=40, vel_amp=0, vel_filt=0, voices=1),
    R("Impact", "A hit for the first beat of the drop", "Riser/FX", "Rave Cinematic", "Aggressive Dark", o(1, "Bass Drum", harm=18, timbre=40, morph=85), plate(36, decay=78, damping=50), noise=25, vca_mode="ON", cutoff=100, reso=30, keytrack=0, vel_filt=0, release=44, vel_amp=30, drive=0),
    R("Whoosh", "Air moving past, there and gone", "Riser/FX", "Rave Cinematic", "Soft Evolving", dict(o1_on="OFF", o2_on="OFF", o3_on="OFF"), both(phaser(40, rate=35, depth=90, feedback=30), plate(22)), noise=90, cutoff=36, reso=60, f_mod=66, m_attack=70, m_decay=70, m_sustain=0, keytrack=0, attack=52, decay=74, sustain=0, release=42, vel_amp=0, vel_filt=0, voices=1, drive=0),
]
assert len(ENTRIES) == 20, len(ENTRIES)
