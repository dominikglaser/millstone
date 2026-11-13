"""Cinematic: fifty lush, wide sounds for film and ambient work. Slow swells, choirs, bowed strings and drones that move by
themselves: the clocks run as tempo-locked LFOs (SINE / TRI / S/H at /2 to /32 of the song's beat), Tides shapes the VCA and
the filter (VCA MODE and MOD SOURCE = TIDES n), Euclidean clocks scatter bells and particles over held chords, and every
sound sits in long plate reverb and tape echo. Hold the keys and let them breathe; the wheel and pressure open most of them up.
"""
from preset_helpers import *
from preset_fx import echo, plate, spring, chorus, phaser, both

PACK = "Cinematic"

# a tempo-locked LFO from the clock: SINE / TRI / SAW / RAMP / S/H, `div` cycles per beat (/8 = one every two bars)
def CLFO(channel, div, shape="SINE", **kw):
    return clock(channel, div=div, shape=shape, **kw)

# bowed / blown / struck Elements on OSC 1 (strike off unless asked for: BASE strikes at 80)
def BOW(**kw):
    d = dict(ELEMENTS, e_strike=0, e_bow=70, e_bow_t=45, e_blow=0, e_contour=60)
    d.update(kw)
    return d

PAD = [
    Lab("Aurora Strings", "Wide strings breathing in two-bar waves locked to the song", "Pad", "Cinematic Ambient", "Warm Evolving",
        o(1, "String Machine", harm=40, timbre=55, morph=60), SAW(2, fine=7, timbre=60, level=60), CLFO(1, "/8"), L2(22),
        cords(("CLOCK 1", "CUTOFF", 14), ("LFO 2", "O2 PITCH", 1), ("AFTERTOUCH", "CUTOFF", 20)),
        both(chorus("I+II", 80), plate(38, decay=78)), attack=55, release=62, cutoff=58, reso=10),
    Lab("Glacier Choir", "A cold choir whose vowels drift on slow Tides phases", "Pad", "Cinematic Ambient", "Soft Evolving",
        o(1, "Grain Formant", harm=30, timbre=40, morph=35), o(2, "Speech", oct=1, harm=45, timbre=45, morph=40, level=35), Tid(24, shift=70),
        cords(("TIDES 1", "O1 MORPH", 20), ("TIDES 2", "O1 TIMBRE", 14), ("TIDES 3", "O2 MORPH", 22), ("WHEEL", "CUTOFF", 25)),
        both(chorus("II", 70), plate(42, decay=82, damping=45)), attack=60, release=66, cutoff=52),
    Lab("Cathedral Bloom", "Organ pipes swelling in on a Tides AR envelope, no amp envelope at all", "Pad", "Cinematic Classic", "Warm Soft",
        o(1, "Additive", harm=20, timbre=4, morph=10), o(2, "Additive", oct=1, harm=25, timbre=6, morph=10, level=55), SOFT(3, oct=-1, level=45),
        Tid(22, mode="AR", out="PHASE", trig="NOTE", slope=40, smooth=70), L1(30),
        cords(("LFO", "O2 HARM", 8), ("AFTERTOUCH", "O1 TIMBRE", 12)),
        both(phaser(18, rate=10, feedback=40), plate(46, decay=76)), vca_mode="TIDES 1", cutoff=74),
    Lab("Bowed Horizon", "Elements bowing a long string over a soft sub; press harder to dig in", "Pad", "Cinematic Ambient", "Warm Evolving",
        BOW(e_model="STRING", e_geom=35, e_bright=45, e_damp=30, e_pos=25, e_space=55), SOFT(2, oct=-1, level=45), Tid(20, shift=60),
        cords(("AFTERTOUCH", "E BOW", 30), ("TIDES 1", "E POS", 15), ("TIDES 2", "E BRIGHT", 12)),
        plate(40, decay=80), attack=40, release=60),
    Lab("Sunken Cathedral", "Dark chords whose filter rises and falls with a slow Tides loop", "Pad", "Cinematic Experimental", "Dark Evolving",
        o(1, "Chords", harm=55, timbre=30, morph=40), SOFT(2, oct=-1, level=50), Tid(18, shift=55, smooth=80),
        cords(("TIDES 2", "O1 MORPH", 15), ("WHEEL", "RESO", 30)),
        both(echo("1/2", 45, 22, high=45, wow=35), plate(44, decay=84, damping=55)), f_src="TIDES 1", f_mod=30, cutoff=38, reso=25, attack=50, release=64),
    Lab("Silk Nebula", "Wavetable and swarm turning on clocked triangles and sines", "Pad", "Ambient Cinematic", "Soft Evolving",
        o(1, "Wavetable", harm=45, timbre=35, morph=40), o(2, "Swarm", oct=1, harm=30, timbre=30, morph=50, level=50), CLFO(2, "/4", "TRI"), CLFO(3, "/6"),
        cords(("CLOCK 2", "O1 MORPH", 20), ("CLOCK 3", "O2 LEVEL", 25), ("CLOCK 2", "CUTOFF", 8)),
        both(chorus("II", 80), plate(40, decay=80)), attack=50, release=62, cutoff=62),
    Lab("Distant Armada", "Massive slow brass-strings in fifths, the filter tides in", "Pad", "Cinematic Classic", "Warm Aggressive",
        SAW(1, timbre=70), SAW(2, fine=6, timbre=70), SAW(3, semi=7, fine=-5, timbre=70, level=60), L2(18),
        cords(("LFO 2", "CUTOFF", 10), ("VELOCITY", "CUTOFF", 18), ("WHEEL", "DRIVE", 40)),
        both(phaser(20, rate=8, feedback=30), plate(36, decay=76)), attack=62, release=58, cutoff=40, reso=12, f_mod=18, m_attack=60, m_decay=70, m_sustain=40, drive=10),
    Lab("Ember Haze", "Warm terrain pad on worn tape, a little different every time", "Pad", "Cinematic Ambient", "Warm Soft",
        o(1, "Wave Terrain", harm=40, timbre=35, morph=45), SOFT(2, oct=-1, level=40), L1(24),
        cords(("RANDOM", "O1 TIMBRE", 12), ("LFO", "O1 MORPH", 12), ("AFTERTOUCH", "CUTOFF", 18)),
        both(echo("1/4 D", 45, 24, high=50, wow=70), plate(36, decay=76)), attack=45, release=60, cutoff=56),
    Lab("Ocean Of Glass", "A soft pad with glass bells scattered over it by a Euclidean clock", "Pad", "Cinematic Ambient", "Bright Evolving",
        SOFT(1, timbre=20), o(2, "Modal", oct=1, harm=55, timbre=60, morph=50, level=55, amp="LPG"), clock(1, div="x2", shape="TRIG", steps=16, fill=5, prob=80),
        cords(("CLOCK 1", "O2 TRIG", 100), ("RANDOM", "O2 HARM", 10)),
        both(echo("1/4 D", 50, 26, pong=True, high=60), plate(44, decay=82)), attack=40, release=60, cutoff=70),
    Lab("Velvet Twilight", "Dark detuned saws in a thick chorus, the wheel lets the light in", "Pad", "Cinematic Classic", "Warm Soft",
        SAW(1, timbre=55, fine=-6), SAW(2, timbre=55, fine=6), L1(20),
        cords(("WHEEL", "CUTOFF", 40), ("LFO", "O1 PITCH", 1), ("LFO 2", "O2 PITCH", -1)), L2(23),
        both(chorus("I+II", 100), plate(38, decay=78)), attack=48, release=66, cutoff=34, reso=15),
    Lab("Starfield Drift", "Particles glitter over a soft bed, a new density on every beat", "Pad", "Ambient Cinematic", "Bright Soft",
        o(1, "Particle", harm=40, timbre=45, morph=60, level=70), SOFT(2, level=70), CLFO(1, "x1", "S/H"),
        cords(("CLOCK 1", "O1 TIMBRE", 18), ("CLOCK 1", "O1 HARM", 10), ("WHEEL", "O1 LEVEL", 40)),
        both(echo("1/8 D", 55, 26, pong=True), plate(44, decay=84)), attack=40, release=64, cutoff=72),
    Lab("Monolith Strings", "Low string machine under a slow Tides filter, quietly enormous", "Pad", "Cinematic Experimental", "Dark Warm",
        o(1, "String Machine", oct=-1, harm=35, timbre=50, morph=55), SAW(2, oct=-1, fine=5, timbre=50, level=50), Tid(16, shift=45, smooth=75),
        cords(("TIDES 3", "O1 MORPH", 12), ("VELOCITY", "CUTOFF", 12)),
        both(echo("1/2", 40, 18, high=40), plate(40, decay=82, damping=55)), f_src="TIDES 2", f_mod=26, cutoff=40, drive=12, attack=55, release=64),
    Lab("Halo Swell", "Every chord pulses in and out on Tides, slow as breathing", "Pad", "Ambient Cinematic", "Soft Evolving",
        SAW(1, timbre=40), SOFT(2, semi=12, level=50), Tid(27, out="PHASE", trig="NOTE", shift=60),
        cords(("TIDES 2", "CUTOFF", 12), ("WHEEL", "O2 LEVEL", 40)),
        both(chorus("II", 80), plate(46, decay=84)), vca_mode="TIDES 1", cutoff=60, release=60),
    Lab("Last Light Choir", "A wordless choir fading into the sunset", "Pad", "Cinematic Ambient", "Soft Warm",
        o(1, "Speech", harm=35, timbre=40, morph=50), o(2, "Grain Formant", semi=7, harm=25, timbre=35, morph=30, level=45), CLFO(4, "/16"), L2(26),
        cords(("CLOCK 4", "O1 MORPH", 18), ("LFO 2", "O2 TIMBRE", 12), ("AFTERTOUCH", "O2 LEVEL", 30)),
        both(chorus("I", 80), plate(46, decay=86, damping=50)), attack=58, release=68, cutoff=58),
]

DRONE = [
    Lab("Abyssal Engine", "A deep drone turning over in four- and eight-bar clock cycles", "Drone", "Cinematic Experimental", "Dark Evolving",
        SAW(1, oct=-1, timbre=50), o(2, "Wave Terrain", harm=40, timbre=40, morph=40, level=60), CLFO(1, "/32"), CLFO(2, "/16", "TRI"),
        cords(("CLOCK 1", "CUTOFF", 18), ("CLOCK 2", "O2 MORPH", 25), ("CLOCK 2", "O2 TIMBRE", -15)),
        both(phaser(26, rate=8, feedback=50), plate(40, decay=82)), attack=60, release=66, cutoff=36, reso=30),
    Lab("Event Horizon", "Low FM with its harmonics pulled by Tides, ringing at the edge", "Drone", "Cinematic Experimental", "Dark Metallic",
        o(1, "FM 2-Op", oct=-1, harm=40, timbre=30, morph=20), SOFT(2, semi=7, level=0), Tid(14, shift=50, smooth=80),
        cords(("TIDES 1", "O1 HARM", 10), ("TIDES 2", "O1 TIMBRE", 18), ("OSC 2", "VCA", 30)),
        both(echo("1/2 D", 35, 20, high=40, wow=30), plate(44, decay=78, damping=55)), vca_mode="ON", attack=50, release=60, cutoff=60),
    Lab("Desert Monastery", "Bowed strings resonating in a stone hall, the geometry wandering", "Drone", "Cinematic Ambient", "Warm Evolving",
        BOW(e_model="STRINGS", e_geom=45, e_bright=40, e_damp=35, e_pos=30, e_space=65, e_bow=90, e_bow_t=55), Tid(12, smooth=80),
        cords(("TIDES 1", "E GEOM", 12), ("TIDES 2", "E POS", 14), ("AFTERTOUCH", "E BOW", 25)),
        plate(42, decay=84), attack=50, release=64),
    Lab("Iron Sky", "A metallic swarm under heavy clouds, phasing slowly", "Drone", "Cinematic Experimental", "Metallic Dark",
        o(1, "Swarm", oct=-1, harm=40, timbre=35, morph=50), o(2, "Modal", harm=60, timbre=30, morph=60, level=40), CLFO(3, "/8", "TRI"),
        cords(("CLOCK 3", "O1 TIMBRE", 18), ("RANDOM", "O2 HARM", 8), ("WHEEL", "RESO", 35)),
        both(phaser(36, rate=6, feedback=70), plate(40, decay=82)), attack=55, release=64, cutoff=46, reso=30),
    Lab("Tidal Hymn", "Chords rising and fading on a long Tides wave, no envelope", "Drone", "Ambient Cinematic", "Soft Evolving",
        o(1, "Chords", harm=40, timbre=25, morph=30), SOFT(2, oct=-1, level=45), Tid(18, out="PHASE", trig="NOTE", shift=50, smooth=60),
        cords(("TIDES 3", "O1 MORPH", 12), ("WHEEL", "CUTOFF", 30)),
        both(chorus("II", 70), plate(46, decay=86)), vca_mode="TIDES 1", cutoff=56, release=60),
    Lab("Polar Night", "Wind over ice: filtered noise and a low tone, the filter on Tides", "Drone", "Cinematic Ambient", "Dark Soft",
        SOFT(1, oct=-1, level=60), Tid(12, shift=50, smooth=85), cords(("TIDES 3", "NOISE", 15), ("WHEEL", "RESO", 35)),
        both(echo("1/2", 45, 22, high=40, wow=40), plate(44, decay=84)), noise=40, f_src="TIDES 1", f_mod=30, cutoff=40, reso=35, attack=55, release=66),
    Lab("Ancient Signal", "A voice from very far away, its syllables stepping every bar", "Drone", "Cinematic Experimental", "Dark Evolving",
        o(1, "Speech", oct=-1, harm=40, timbre=40, morph=45), SOFT(2, oct=-1, level=40), CLFO(1, "/4", "S/H"),
        cords(("CLOCK 1", "O1 TIMBRE", 18), ("CLOCK 1", "O1 MORPH", 20), ("LFO 2", "CUTOFF", 10)), L2(16),
        both(echo("1/4 D", 55, 26, high=45, wow=40), plate(42, decay=84)), attack=50, release=64, cutoff=48),
]

TEXTURE = [
    Lab("Rain On Titan", "Fine particles falling, now thick, now thin, in endless echo", "Texture", "Cinematic Ambient", "Bright Evolving",
        o(1, "Particle", harm=50, timbre=55, morph=70), CLFO(2, "/2", "S/H"),
        cords(("CLOCK 2", "O1 TIMBRE", 22), ("CLOCK 2", "O1 HARM", 12), ("WHEEL", "CUTOFF", 30)),
        both(echo("1/8 D", 60, 30, pong=True, high=55), plate(44, decay=84)), noise=12, attack=30, release=62, cutoff=70),
    Lab("Whispering Dunes", "Sand and breath shifting, vowels half heard", "Texture", "Cinematic Ambient", "Soft Evolving",
        o(1, "Grain Formant", harm=25, timbre=40, morph=40, level=60), L2(18), Tid(20, shift=70),
        cords(("LFO 2", "CUTOFF", 14), ("TIDES 1", "O1 MORPH", 20), ("TIDES 2", "NOISE", 15)),
        both(echo("1/4 D", 45, 22, wow=50, high=50), plate(42, decay=82)), noise=30, attack=50, release=62, cutoff=50, reso=20),
    Lab("Clockwork Galaxy", "Three Euclidean clocks striking bars in different metres", "Texture", "Cinematic Experimental", "Metallic Evolving",
        o(1, "Modal", harm=45, timbre=50, morph=55, amp="LPG"), o(2, "Modal", semi=7, harm=55, timbre=40, morph=50, level=70, amp="LPG"),
        o(3, "Modal", oct=1, harm=60, timbre=60, morph=45, level=55, amp="LPG"),
        clock(1, div="x2", shape="TRIG", steps=16, fill=5), clock(2, div="x3", shape="TRIG", steps=12, fill=4, rot=2), clock(3, div="x4", shape="TRIG", steps=16, fill=3, prob=70),
        cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "O2 TRIG", 100), ("CLOCK 3", "O3 TRIG", 100), ("RANDOM", "O3 HARM", 10)),
        both(echo("1/4 D", 50, 24, pong=True), plate(42, decay=80)), vca_mode="ON", cutoff=100, reso=0, keytrack=0, release=40),
    Lab("Frozen Wind Harp", "Strings blown by an icy wind, the bow point drifting", "Texture", "Cinematic Ambient", "Bright Soft",
        BOW(e_model="STRINGS", e_bow=0, e_blow=65, e_flow=60, e_blow_t=55, e_geom=55, e_bright=60, e_damp=30, e_pos=35, e_space=70),
        cords(("RANDOM", "E POS", 12), ("LFO 2", "E BLOW", 15), ("AFTERTOUCH", "E BRIGHT", 20)), L2(14),
        plate(44, decay=84), attack=40, release=62),
    Lab("Satellite Choir", "A broken transmission singing, its words reshuffled every beat", "Texture", "Cinematic Experimental", "Evolving Soft",
        o(1, "Speech", harm=40, timbre=45, morph=50), o(2, "Grain Formant", oct=1, harm=30, timbre=40, morph=40, level=40), CLFO(1, "x1", "S/H"),
        cords(("CLOCK 1", "O1 HARM", 20), ("CLOCK 1", "O2 MORPH", 15), ("NOISE", "O1 TIMBRE", 6)),
        both(echo("1/8 D", 55, 28, pong=True, wow=40), plate(40, decay=80)), noise=8, attack=40, release=60, cutoff=64),
    Lab("Shimmer Particles", "A bright mist of particles swaying on a clocked sine", "Texture", "Ambient Cinematic", "Bright Evolving",
        o(1, "Particle", oct=1, harm=55, timbre=50, morph=55), SOFT(2, oct=1, level=40), CLFO(2, "/4"),
        cords(("CLOCK 2", "O1 TIMBRE", 18), ("CLOCK 2", "CUTOFF", 10), ("WHEEL", "O1 MORPH", 30)),
        both(chorus("I+II", 80), plate(46, decay=86)), attack=40, release=64, cutoff=78),
    Lab("Glass Cascade", "Glass bells tumbling down a ping-pong echo", "Texture", "Cinematic Ambient", "Bright Metallic",
        o(1, "Modal", oct=1, harm=60, timbre=65, morph=40, amp="LPG"), clock(1, div="x2", shape="TRIG", prob=65),
        cords(("CLOCK 1", "O1 TRIG", 100), ("RANDOM", "O1 HARM", 15), ("RANDOM", "O1 TIMBRE", 10)),
        both(echo("1/8 D", 65, 34, pong=True, high=65), plate(40, decay=80)), vca_mode="ON", cutoff=100, reso=0, keytrack=0, release=40),
    Lab("Breathing Machine", "Noise and tone inhaling on one Tides output, exhaling through another", "Texture", "Cinematic Experimental", "Dark Evolving",
        SAW(1, timbre=40, level=70), Tid(22, out="PHASE", trig="NOTE", shift=65, smooth=70), cords(("TIDES 3", "NOISE", 20), ("WHEEL", "RESO", 30)),
        both(phaser(26, rate=10, feedback=50), plate(42, decay=82)), noise=30, vca_mode="TIDES 2", f_src="TIDES 1", f_mod=28, cutoff=44, reso=25, release=60),
]

RISER = [
    Lab("Ascension", "A slow rise: pitch, filter and noise climbing for seconds", "Riser/FX", "Cinematic", "Bright Evolving",
        SAW(1, timbre=60), SAW(2, fine=10, timbre=60), cords(("MOD ENV", "ALL PITCH", 12), ("MOD ENV", "CUTOFF", 40), ("MOD ENV", "NOISE", 40)),
        both(echo("1/8 D", 55, 26, pong=True), plate(42, decay=84)), noise=10, m_attack=80, m_decay=80, m_sustain=100, attack=60, release=60, cutoff=30, reso=25),
    Lab("Into The Void", "Everything falls: pitch drops two octaves into a dark reverb", "Riser/FX", "Cinematic", "Dark",
        SAW(1, timbre=60), SOFT(2, oct=-1, level=60), cords(("MOD ENV", "ALL PITCH", -24), ("MOD ENV", "CUTOFF", -30)),
        both(echo("1/4", 50, 24, high=45, wow=40), plate(46, decay=88, damping=55)), m_attack=84, m_decay=50, m_sustain=100, f_mod=0, cutoff=70, attack=5, decay=80, sustain=60, release=66),
    Lab("Pulse Of Dread", "A tempo pulse that grows louder and brighter while held", "Riser/FX", "Cinematic Techno", "Dark Aggressive",
        SAW(1, oct=-1, timbre=60), SQUARE(2, level=50), clock(1, div="x2", shape="SAW"),
        cords(("CLOCK 1", "VCA", 60), ("MOD ENV", "CUTOFF", 45), ("MOD ENV", "DRIVE", 40)),
        both(echo("1/8", 35, 16), plate(36, decay=76)), m_attack=82, m_sustain=100, attack=40, release=50, cutoff=24, reso=30),
    Lab("Braam Horizon", "The big cinematic brass hit, with a long tail", "Riser/FX", "Cinematic", "Aggressive Dark",
        SAW(1, oct=-1, timbre=70), SAW(2, oct=-1, fine=8, timbre=70), SQUARE(3, oct=-2, level=60), cords(("VELOCITY", "DRIVE", 50), ("MOD ENV", "ALL PITCH", -1)),
        both(echo("1/4", 35, 18, high=45), plate(40, decay=84)), **MONO, drive=45, cutoff=30, reso=20, f_mod=45, m_attack=8, m_decay=68, m_sustain=20, attack=3, decay=70, sustain=50, release=62),
    Lab("Time Rift", "A swell that bends time: Tides on pitch, tape echo wobbling", "Riser/FX", "Cinematic Experimental", "Evolving",
        o(1, "Wavetable", harm=50, timbre=45, morph=45), Tid(30, mode="AD", out="PHASE", trig="NOTE", slope=80, smooth=60),
        cords(("TIDES 2", "ALL PITCH", 7), ("TIDES 3", "O1 MORPH", 30), ("TIDES 4", "CUTOFF", 25)),
        both(echo("1/4 D", 65, 34, wow=90, high=50), plate(44, decay=86)), vca_mode="TIDES 1", cutoff=50, release=60),
]

KEYS = [
    Lab("Dream Rhodes", "A soft electric piano in chorus; the wheel brings in a tempo tremolo", "Keys", "Cinematic Classic", "Warm Soft",
        o(1, "6-Op FM A", harm=30, timbre=40, morph=30), L1(60, sync="ON", div="1/8"), cords(("LFO", "VCA", 30), ("VELOCITY", "O1 TIMBRE", 20)),
        both(chorus("II", 70), plate(32, decay=72)), decay=72, sustain=30, release=58, cutoff=72, wheel_lfo=100),
    Lab("Celestial Piano", "A struck, softly ringing piano floating in a big room", "Keys", "Cinematic Ambient", "Bright Soft",
        o(1, "Modal", harm=40, timbre=45, morph=30), SOFT(2, level=40), cords(("VELOCITY", "O1 TIMBRE", 25), ("VELOCITY", "CUTOFF", 15)),
        both(echo("1/4 D", 40, 18, high=55), plate(40, decay=82)), decay=74, sustain=0, release=60, cutoff=74),
    Lab("Crystal Celesta", "High glass keys with an echo that answers back", "Keys", "Cinematic Ambient", "Bright Metallic",
        o(1, "Modal", oct=1, harm=55, timbre=60, morph=35), cords(("VELOCITY", "O1 TIMBRE", 25), ("RANDOM", "O1 HARM", 5)),
        both(echo("1/8 D", 55, 26, pong=True, high=65), plate(38, decay=80)), decay=68, sustain=0, release=56, cutoff=86),
    Lab("Moonlit Organ", "A quiet pipe organ in a slow phaser and a deep hall", "Keys", "Cinematic Classic", "Warm Soft",
        o(1, "Additive", harm=20, timbre=3, morph=15), o(2, "Additive", oct=1, harm=20, timbre=5, morph=15, level=45), L1(56),
        cords(("WHEEL", "O1 PITCH", 1), ("AFTERTOUCH", "O2 LEVEL", 40)),
        both(phaser(26, rate=10, feedback=40), plate(42, decay=82)), attack=8, release=56, cutoff=70, wheel_lfo=40),
    Lab("Felt Lullaby", "Elements striking softly with felt, warm and close", "Keys", "Cinematic Ambient", "Warm Soft",
        dict(ELEMENTS), cords(("VELOCITY", "E STRIKE", 25), ("VELOCITY", "E BRIGHT", 15)),
        both(chorus("I", 50), plate(36, decay=78)), e_model="MODAL", e_strike=55, e_mallet=20, e_strike_t=25, e_geom=30, e_bright=35, e_damp=40, e_pos=30, e_space=50),
]

PLUCK = [
    Lab("Harp Of Stars", "A plucked string thrown into a ping-pong echo and a starry hall", "Pluck", "Cinematic Ambient", "Bright Soft",
        o(1, "String", harm=40, timbre=50, morph=40), cords(("VELOCITY", "O1 TIMBRE", 25), ("RANDOM", "O1 MORPH", 8)),
        both(echo("1/8 D", 55, 28, pong=True, high=60), plate(40, decay=82)), vca_mode="ON", cutoff=86, release=40),
    Lab("Kalimba Dusk", "A soft kalimba echoing in time, chorus around it", "Pluck", "Cinematic Ambient", "Warm Soft",
        o(1, "Modal", harm=30, timbre=40, morph=25), cords(("VELOCITY", "O1 TIMBRE", 20), ("VELOCITY", "CUTOFF", 15)),
        both(chorus("I", 60), echo("1/4 D", 45, 22, high=55), plate(30, decay=74)), decay=56, sustain=0, release=50, cutoff=78),
    Lab("Silver Rain Pluck", "Short bright plucks shimmering off into the distance", "Pluck", "Cinematic Ambient", "Bright",
        SAW(1, timbre=70), SOFT(2, oct=1, level=40), cords(("VELOCITY", "CUTOFF", 25), ("RANDOM", "O2 LEVEL", -25)),
        both(echo("1/8 D", 60, 30, pong=True, high=62), plate(38, decay=80)), decay=46, sustain=0, release=48, cutoff=34, reso=20, f_mod=55, m_decay=40),
    Lab("Koto Mist", "A bright string struck by Elements, drifting into fog", "Pluck", "Cinematic Ambient", "Bright Warm",
        dict(ELEMENTS), cords(("VELOCITY", "E STRIKE", 25), ("RANDOM", "E POS", 8)),
        both(echo("1/4", 45, 22, high=55, wow=30), plate(36, decay=80)), e_model="STRING", e_strike=75, e_mallet=60, e_strike_t=60, e_geom=40, e_bright=60, e_damp=30, e_pos=20, e_space=45),
]

LEAD = [
    Lab("Lonely Horn", "A solo voice for the wide shot: soft brass with a slow vibrato", "Lead", "Cinematic Classic", "Warm Soft",
        SAW(1, timbre=50), SQUARE(2, oct=-1, level=30, timbre=40), L1(52),
        cords(("WHEEL", "CUTOFF", 20), ("AFTERTOUCH", "CUTOFF", 25), ("MOD ENV", "CUTOFF", 15)),
        both(echo("1/4 D", 45, 22, high=50), plate(38, decay=80)), **LEGATO, glide=15, cutoff=42, attack=22, release=50, m_attack=30, m_decay=60, m_sustain=40, vibrato=22, wheel_lfo=60),
    Lab("Ethereal Solo", "A sung vowel on a glide, pressure brings the vibrato", "Lead", "Cinematic Ambient", "Soft Bright",
        o(1, "Grain Formant", harm=35, timbre=40, morph=35), L1(58), cords(("AFTERTOUCH", "ALL PITCH", 1), ("LFO", "O1 MORPH", 10)),
        both(echo("1/4 D", 50, 26, pong=True), plate(42, decay=84)), **LEGATO, glide=25, attack=20, release=56, cutoff=74, at_lfo=60),
    Lab("Theremin Ghost", "A sine on a long glide, haunting the hall", "Lead", "Cinematic Experimental", "Soft",
        SOFT(1, timbre=5), L1(60), cords(("LFO", "ALL PITCH", 1), ("WHEEL", "LFO RATE", 30)),
        both(echo("1/4 D", 55, 28, wow=40), plate(46, decay=86)), **LEGATO, glide=55, attack=25, release=60, cutoff=86, reso=0),
]

BASS = [
    Lab("Titan Sub", "A cinematic sub that grows a slow, dark harmonic shadow", "Bass", "Cinematic", "Dark Warm",
        SOFT(1, oct=-1), SAW(2, oct=-1, timbre=40, level=30), CLFO(1, "/8"),
        cords(("CLOCK 1", "CUTOFF", 8), ("VELOCITY", "O2 LEVEL", 30), ("WHEEL", "DRIVE", 40)),
        plate(14, decay=60), **MONO, cutoff=30, reso=15, attack=10, release=50),
    Lab("Abyss Pulse Bass", "A low drone bass pumping in eighths with the song", "Bass", "Cinematic Techno", "Dark Aggressive",
        SAW(1, oct=-1, timbre=55), SOFT(2, oct=-2, level=50), clock(1, div="x2", shape="SAW"),
        cords(("CLOCK 1", "VCA", 55), ("CLOCK 1", "CUTOFF", 12), ("WHEEL", "RESO", 35)),
        both(echo("1/8 D", 30, 12, high=45), plate(16, decay=60)), **MONO, cutoff=30, reso=25, drive=20, release=40),
]

SEQUENCE = [
    Lab("Ostinato Engine", "Hold a chord: a Euclidean clock replays it as a driving string ostinato", "Sequence", "Cinematic Techno", "Warm Evolving",
        o(1, "String Machine", harm=40, timbre=55, morph=50), SAW(2, timbre=55, level=50), clock(1, div="x4", shape="TRIG", steps=16, fill=7),
        CLFO(2, "/8"), cords(("CLOCK 1", "ENV TRIG", 100), ("CLOCK 1", "MOD TRIG", 100), ("CLOCK 2", "CUTOFF", 12)),
        both(echo("1/8 D", 45, 20, pong=True), plate(32, decay=76)), attack=1, decay=54, sustain=0, release=40, cutoff=44, f_mod=30, m_decay=44),
    Lab("Heartbeat Of Ice", "A frozen pulse: a gated chord with a clocked S/H on the filter", "Sequence", "Cinematic Experimental", "Bright Evolving",
        o(1, "Wavetable", harm=50, timbre=40, morph=40), SOFT(2, oct=1, level=40), clock(1, div="x2", shape="GATE", width=70), CLFO(2, "x1", "S/H"),
        cords(("CLOCK 1", "VCA", -100), ("CLOCK 2", "CUTOFF", 16), ("CLOCK 2", "O1 MORPH", 15)),
        both(echo("1/8 D", 55, 28, pong=True, high=60), plate(38, decay=80)), cutoff=52, reso=25, attack=10, release=50),
]

ENTRIES = PAD + DRONE + TEXTURE + RISER + KEYS + PLUCK + LEAD + BASS + SEQUENCE
assert len(ENTRIES) == 50, len(ENTRIES)
UNMATCHED = set()
