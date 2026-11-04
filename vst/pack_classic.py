"""The synths everyone has heard: brass, strings, big poly pads, fat mono leads, FM keys. In the spirit of them, not copies."""
from preset_helpers import *
from preset_fx import echo, plate, spring, chorus, phaser, both

PACK = "Classic 1"

def C(name, note, type_, styles, characters, *parts, **kw):
    return name, note, P(name, note, *parts, **kw)[2], type_, styles, characters

STUDIO = plate(14)
WIDE = chorus("I", 90)
BRASS = dict(attack=22, decay=52, sustain=70, release=30, cutoff=40, reso=8, f_mod=42, m_attack=20, m_decay=50, m_sustain=25, vel_filt=30, vel_amp=25)

ENTRIES = [
    C("Poly Brass", "Eight voices of saw through a filter that opens as you hit it", "Keys", "Classic", "Bright Warm", SAW(1), SAW(2, fine=8), both(WIDE, STUDIO), **BRASS),
    C("Brass Stab", "The same section, short and hard", "Keys", "Classic House", "Bright", SAW(1), SAW(2, fine=-7), STUDIO, dict(BRASS, attack=8, decay=46, sustain=0, m_attack=4, m_decay=42, m_sustain=0, release=18)),
    C("Soft Brass", "Slow swell, velocity for how much it bites", "Pad", "Classic Cinematic", "Warm Soft", SAW(1), SAW(2, fine=6, oct=1, level=35), both(chorus("II", 80), plate(18)), dict(BRASS, attack=44, m_attack=46, cutoff=34, f_mod=30)),
    C("Sync Brass", "A harder edge from the wave's own filter opening", "Lead", "Classic", "Bright Aggressive", SAW(1, timbre=55, timbre_mod=40, cv_src="ENV"), SAW(2, fine=7, timbre=55, timbre_mod=40, cv_src="ENV"), echo("1/8 D", 30, 12), dict(BRASS, voice_mode="MONO", voices=1, m_decay=52)),
    C("Big Eighties", "Two octaves of detuned saws, chorus wide open", "Pad", "Classic Cinematic", "Warm Bright", SAW(1), SAW(2, fine=9), SAW(3, oct=-1, fine=-6, level=60), both(chorus("I+II", 70), plate(22)), attack=38, release=62, cutoff=58, reso=5, f_mod=12, m_attack=50, m_decay=70),
    C("Silk Poly", "Soft pulse pad with a slow opening", "Pad", "Classic Ambient", "Warm Soft", SQUARE(1), SAW(2, fine=7, level=55), both(chorus("II", 100), plate(20)), attack=58, release=70, cutoff=36, reso=6, f_mod=24, m_attack=70, m_decay=72, m_sustain=50, m_release=70, vel_amp=15),
    C("Machine Strings", "A divide-down string box into a phaser, as it always was", "Pad", "Classic Cinematic", "Warm", o(1, "String Machine", harm=30, timbre=55, morph=50), both(phaser(45, rate=18, depth=75, feedback=25), plate(16)), attack=58, release=70, cutoff=72, vel_amp=10),
    C("Ensemble Strings", "Bright bowed ensemble with slow attack", "Pad", "Classic Cinematic", "Bright Warm", o(1, "String Machine", harm=65, timbre=70, morph=60), both(chorus("II", 90), plate(24, decay=62)), attack=66, release=74, cutoff=80, vel_amp=15),
    C("Fat Mono", "Three oscillators into a ladder, one note at a time", "Lead", "Classic", "Warm Aggressive", SAW(1), SAW(2, fine=8), SAW(3, oct=-1, fine=-5, level=70), VIB, **LEGATO, glide=8, cutoff=48, reso=30, f_mod=34, m_decay=56, m_sustain=25, drive=35, release=24),
    C("Portamento Lead", "Slides everywhere. Play slowly and bend", "Lead", "Classic", "Warm Soft", SAW(1), SOFT(2, oct=1, level=30), echo("1/4 D", 35, 16), VIB, **LEGATO, glide=38, cutoff=56, reso=18, f_mod=14, m_decay=56, release=30, bend_range=12),
    C("Sine Lead", "A pure whistle with a little breath", "Lead", "Classic Ambient", "Soft", SOFT(1), echo("1/8 D", 38, 18), VIB, **LEGATO, glide=12, cutoff=70, reso=0, noise=4, attack=8, release=30),
    C("Square Flute", "Hollow and breathy, pressure pushes it", "Lead", "Classic", "Soft Warm", SQUARE(1), plate(14), VIB, **LEGATO, glide=6, noise=10, cutoff=50, reso=5, attack=18, release=26, at_cutoff=30, vel_amp=25),
    C("Taurus Pedal", "Those huge pedal bass notes", "Bass", "Classic Cinematic", "Warm Dark", SAW(1, oct=-1), SAW(2, oct=-1, fine=7), SOFT(3, oct=-2, level=60), **MONO, cutoff=36, reso=16, f_mod=26, m_decay=52, m_sustain=20, drive=30, release=30, vel_amp=20),
    C("Saw Bass", "Plain, solid and useful", "Bass", "Classic House", "Warm Bright", SAW(1, oct=-1), SAW(2, oct=-1, fine=5, level=75), **MONO, cutoff=40, reso=14, f_mod=30, m_decay=44, m_sustain=10, drive=22, release=20),
    C("Square Bass", "Round and hollow, a groove box favourite", "Bass", "Classic Techno", "Warm", SQUARE(1, oct=-1), SOFT(2, oct=-2, level=50), **MONO, cutoff=42, reso=18, f_mod=26, m_decay=42, m_sustain=0, decay=52, sustain=50, drive=18, release=18),
    C("Funk Slap", "Snapping filter, for popping octaves", "Bass", "Classic House", "Bright", SAW(1, oct=-1), **MONO, cutoff=30, reso=42, f_mod=60, m_decay=36, decay=44, sustain=25, drive=25, vel_filt=45, vel_amp=45, release=18),
    C("Eighty One", "A very familiar sine-saw electronic bass", "Bass", "Classic Techno", "Warm", SOFT(1, oct=-1), SAW(2, oct=-1, level=35), **MONO, cutoff=34, reso=10, f_mod=20, m_decay=40, decay=46, sustain=20, release=18, drive=15),
    C("EP Bell", "The ballad electric piano of every eighties record", "Keys", "Classic", "Bright Soft", o(1, "FM 2-Op", harm=33, timbre=38, morph=8), o(2, "FM 2-Op", oct=1, harm=75, timbre=25, morph=0, level=28), both(chorus("I", 60), plate(12)), decay=76, sustain=0, release=44, cutoff=88, reso=0, keytrack=0, vel_amp=70, vel_filt=0),
    C("FM Marimba", "Wood from two operators", "Pluck", "Classic", "Warm", o(1, "FM 2-Op", harm=58, timbre=22, morph=0), plate(10), decay=50, sustain=0, release=28, cutoff=90, reso=0, keytrack=0, vel_amp=70, vel_filt=0),
    C("Six Op Bank", "Another sixty-four voices: HARMONICS walks through them", "Keys", "Classic", "Bright Metallic", o(1, "6-Op FM B", harm=30, timbre=50, morph=50), chorus("I", 45), cutoff=95, reso=0, keytrack=0, vel_amp=60, vel_filt=0, release=38),
    C("Six Op Tines", "An operator bank set for keys", "Keys", "Classic House", "Bright", o(1, "6-Op FM C", harm=22, timbre=50, morph=50), plate(12), cutoff=95, reso=0, keytrack=0, vel_amp=65, vel_filt=0, release=36),
    C("House Organ", "Bright drawbars with a click, for offbeat stabs", "Keys", "House Classic", "Bright", o(1, "Additive", harm=48, timbre=55, morph=45), STUDIO, decay=40, sustain=30, release=14, cutoff=88, reso=0, keytrack=0, vel_amp=25, vel_filt=0),
    C("Ninety Piano", "A hard digital piano, for house chords", "Keys", "House Rave", "Bright", o(1, "FM 2-Op", harm=30, timbre=48, morph=14), o(2, "FM 2-Op", oct=1, harm=45, timbre=30, morph=8, level=40), both(chorus("I", 40), plate(16)), decay=68, sustain=0, release=32, cutoff=92, reso=0, keytrack=0, vel_amp=55, vel_filt=0),
    C("Vibes", "Soft mallets on metal bars", "Pluck", "Classic Ambient", "Soft Metallic", o(1, "Modal", harm=52, timbre=30, morph=48), both(phaser(20, rate=40, depth=30, feedback=0), plate(14)), vca_mode="ON", cutoff=86, reso=0, keytrack=0, release=46, vel_amp=65),
    C("Music Box", "Very small, very high", "Pluck", "Cinematic Ambient", "Bright Soft", o(1, "Modal", oct=1, harm=70, timbre=55, morph=45), plate(26, decay=60), vca_mode="ON", cutoff=100, reso=0, keytrack=0, release=48, vel_amp=55),
    C("Celeste", "A warm chime", "Keys", "Cinematic Classic", "Bright Soft", o(1, "FM 2-Op", oct=1, harm=66, timbre=20, morph=0), plate(20, decay=60), decay=70, sustain=0, release=48, cutoff=92, reso=0, keytrack=0, vel_amp=60, vel_filt=0),
    C("Chip Arp", "A sequencer's idea of a chord", "Sequence", "Classic Experimental", "Bright", o(1, "Chiptune", harm=45, timbre=55, morph=50), echo("1/8", 30, 14), cutoff=92, reso=0, keytrack=0, release=18, vel_amp=20, vel_filt=0),
    C("Vocal Bot", "A speaking chip with a slow tongue", "Lead", "Classic Experimental", "Metallic", o(1, "Speech", harm=20, timbre=55, morph=40, morph_mod=45), chorus("I", 50), **MONO, lfo_rate=46, cutoff=84, reso=0, keytrack=0, release=26),
    C("Whole Choir", "Vowels in unison, eighties reverb", "Pad", "Classic Cinematic", "Warm Soft", o(1, "Grain Formant", harm=45, timbre=35, morph=25), o(2, "Grain Formant", harm=45, timbre=42, morph=25, fine=8, level=85), both(chorus("II", 70), plate(28, decay=70)), vibrato=6, lfo_rate=56, attack=62, release=70, cutoff=78, vel_amp=10),
    C("Tape Chord", "A chord, printed on a slightly tired machine", "Keys", "Classic Dub", "Warm", o(1, "Chords", harm=20, timbre=35, morph=40), both(echo("1/4", 20, 10, wow=55, high=52), chorus("I", 30)), attack=6, decay=62, sustain=40, release=34, cutoff=56, voices=4, vel_amp=30),
]
assert len(ENTRIES) == 30, len(ENTRIES)
