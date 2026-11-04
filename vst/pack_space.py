"""Ambient, cinematic and strange. Mostly slow, mostly long, a few that are just interesting to hold down."""
from preset_helpers import *
from preset_fx import echo, plate, spring, chorus, phaser, both

PACK = "Space 1"

def S(name, note, type_, styles, characters, *parts, **kw):
    return name, note, P(name, note, *parts, **kw)[2], type_, styles, characters

SLOW = dict(attack=72, release=78, vel_amp=8, vel_filt=0)

ENTRIES = [
    S("Slow Bloom", "Takes its time, and opens as it goes", "Pad", "Ambient", "Warm Evolving", SAW(1), SOFT(2, oct=1, level=30), both(chorus("II", 60), plate(30, decay=72)), dict(SLOW, cutoff=30, reso=10, f_mod=34, m_attack=88, m_decay=84, m_sustain=80, m_release=80)),
    S("Cathedral", "An organ a long way away", "Pad", "Cinematic Ambient", "Warm Soft", o(1, "Additive", harm=30, timbre=30, morph=62), plate(42, decay=84, damping=45), dict(SLOW, attack=48, cutoff=72)),
    S("Frozen Lake", "Bright and glassy, hardly moving", "Pad", "Ambient Cinematic", "Bright Soft", o(1, "Wavetable", oct=1, harm=70, timbre=55, morph=35, morph_mod=20), both(chorus("I", 50), plate(34, decay=80, damping=20)), dict(SLOW, lfo_rate=14, cutoff=84)),
    S("Deep Space", "Very low, very wide, almost not there", "Drone", "Ambient Cinematic", "Dark Evolving", SAW(1, oct=-1), SOFT(2, oct=-2, level=70), both(phaser(30, rate=8, depth=90, feedback=35), plate(36, decay=84, damping=70)), dict(SLOW, cutoff=20, reso=25, f_src="LFO", f_mod=10, lfo_rate=8)),
    S("Ghost Choir", "Voices through a wall", "Pad", "Cinematic", "Dark Soft", o(1, "Grain Formant", harm=30, timbre=30, morph=25, timbre_mod=12), plate(40, decay=82, damping=70), dict(SLOW, vibrato=5, lfo_rate=48, cutoff=46), cutoff=30, drive=0),
    S("Tension Strings", "Close harmony, a shade sharp. Something is about to happen", "Pad", "Cinematic", "Dark Evolving", o(1, "String Machine", harm=25, timbre=45, morph=45), SAW(2, semi=1, fine=12, level=22), plate(26, decay=66, damping=60), dict(SLOW, attack=60, cutoff=50, reso=15)),
    S("Braam", "The film trailer. One big low brass hit", "Riser/FX", "Cinematic", "Aggressive Dark", SAW(1, oct=-1), SAW(2, oct=-1, fine=12), SQUARE(3, oct=-2, level=60), plate(34, decay=78, damping=55), cutoff=62, reso=20, f_mod=46, m_attack=36, m_decay=66, m_sustain=30, drive=0, attack=12, decay=78, sustain=25, release=56, vel_amp=10),
    S("Sub Boom", "A low swell and a long fall", "Riser/FX", "Cinematic Techno", "Dark", SOFT(1, oct=-2), o(2, "Bass Drum", harm=10, timbre=20, morph=85, level=70), plate(24, decay=70, damping=85), attack=4, decay=78, sustain=0, release=48, cutoff=34, reso=0, keytrack=0, vel_filt=0, vel_amp=20),
    S("Glass Rain", "Hold a chord: small bright drops at random", "Sequence", "Ambient", "Bright Soft", o(1, "Modal", oct=1, harm=70, timbre=70, morph=35), cords(("CLOCK 1", "O1 TRIG", 100)), clock(1, div="x4", shape="TRIG", prob=35), both(echo("1/8 D", 50, 24, pong=True), plate(30, decay=74, damping=20)), vca_mode="ON", cutoff=100, reso=0, keytrack=0, release=46, voices=6),
    S("Whale Song", "A filter singing to itself across a long distance", "Texture", "Ambient Experimental", "Soft Evolving", SOFT(1, level=0), dict(o1_on="OFF"), both(echo("1/4 D", 60, 30, wow=40, high=48), plate(30, decay=75)), noise=3, reso=100, keytrack=100, cutoff=46, f_src="LFO", f_mod=14, lfo_rate=12, attack=40, release=60, vel_amp=0, vel_filt=0, voices=1),
    S("Radio Static", "Between stations, with something almost in there", "Texture", "Experimental Cinematic", "Soft Evolving", o(1, "Speech", harm=70, timbre=70, morph=50, morph_mod=30, timbre_mod=20), both(chorus("I+II", 25), phaser(20, rate=25)), noise=25, lfo_shape="S/H", lfo_rate=40, cutoff=60, f_mode="HP", reso=20, keytrack=0, attack=20, release=40, vel_amp=0, vel_filt=0),
    S("Particle Storm", "A cloud of grains that thickens with pressure", "Texture", "Ambient Experimental", "Bright Evolving", o(1, "Particle", harm=35, timbre=75, morph=40), plate(34, decay=74, damping=30), at_morph=40, attack=30, release=62, cutoff=40, vel_amp=0, vel_filt=0, drive=0),
    S("Terrain Bass", "A wave terrain, down low and growling", "Bass", "Experimental Techno", "Aggressive Evolving", o(1, "Wave Terrain", oct=-1, harm=40, timbre=45, morph=40, timbre_mod=18), **MONO, lfo_rate=30, cutoff=52, reso=20, drive=45, release=20),
    S("Harmonic Sweep", "Additive partials walking past, one at a time", "Texture", "Ambient Experimental", "Bright Evolving", o(1, "Additive", harm=70, timbre=50, morph=30, timbre_mod=45), plate(20), lfo_rate=20, attack=35, release=60, cutoff=88, vel_amp=0, vel_filt=0),
    S("Inharmonic", "FM at an unfriendly ratio. Bell, gong, or trouble", "Keys", "Experimental Cinematic", "Metallic Dark", o(1, "FM 2-Op", harm=82, timbre=45, morph=25), plate(28, decay=70), decay=78, sustain=0, release=58, cutoff=88, reso=0, keytrack=0, vel_amp=60, vel_filt=0),
    S("Bent Tape", "A chord on a machine that is dying slowly", "Pad", "Experimental Dub", "Warm Evolving", o(1, "Chords", harm=40, timbre=40, morph=45), both(echo("1/4 D", 62, 34, wow=100, drive=50, high=46), plate(16)), attack=28, release=42, cutoff=52, reso=12, voices=4, vel_amp=20),
    S("Tidal Drift", "Four outputs each moving something else. Never the same twice", "Texture", "Ambient Experimental", "Evolving", o(1, "Wavetable", harm=50, timbre=50, morph=40), o(2, "Swarm", harm=55, timbre=45, morph=50, level=50), cords(("TIDES 1", "O1 TIMBRE", 30), ("TIDES 2", "O1 MORPH", 30), ("TIDES 3", "CUTOFF", 18), ("TIDES 4", "O2 LEVEL", 30)), plate(26, decay=70), t_mode="LOOP", t_trig="FREE", t_out="PHASE", t_rate=24, t_shift=70, attack=48, release=60, cutoff=62, vel_amp=0, vel_filt=0),
    S("Static Choir", "Formants and noise, breathing together", "Pad", "Cinematic Experimental", "Dark Evolving", o(1, "Grain Formant", harm=60, timbre=25, morph=60), cords(("LFO", "CUTOFF", 22)), plate(30, decay=76, damping=60), dict(SLOW, attack=56, lfo_rate=18, cutoff=56)),
    S("Tuned Noise", "Rushing air at the pitch you play", "Texture", "Ambient Cinematic", "Soft", dict(o1_on="OFF", o2_on="OFF", o3_on="OFF"), plate(24, decay=66), noise=14, keytrack=100, cutoff=52, reso=88, attack=30, release=52, vel_amp=10, vel_filt=0),
    S("Last Light", "A single warm note, going down slowly", "Lead", "Cinematic Ambient", "Warm Soft", SOFT(1), SAW(2, oct=-1, level=25), both(echo("1/4 D", 55, 28, wow=25, high=52), plate(36, decay=80, damping=50)), VIB, **LEGATO, glide=18, attack=30, release=52, cutoff=50, reso=4, vel_amp=20, vel_filt=0, drive=0),
]
assert len(ENTRIES) == 20, len(ENTRIES)
