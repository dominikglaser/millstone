"""Dub and reggae. Heavy on the echo and the spring, because that is the point: these are the sounds the tape delay was built for.
Bass stays dry, as it would on a mixing desk, and everything else is written to sit around it."""
from preset_helpers import *
from preset_fx import echo, plate, spring, chorus, phaser, both

PACK = "Dub 1"
STRUCK = dict(vca_mode="ON", reso=0, keytrack=0)

def D(name, note, type_, styles, characters, *parts, **kw):
    return name, note, P(name, note, *parts, **kw)[2], type_, styles, characters

ENTRIES = [
    # ---- bass: round, dry, and in the middle of the room ---------------------------------------------------------------
    D("Steppers Sub", "Nearly a sine. Holds the floor and leaves room for the kick", "Bass", "Dub House", "Warm Soft",
      o(1, "Virtual Analog", oct=-1, timbre=15, morph=0), **MONO, cutoff=34, reso=5, decay=60, sustain=75, release=18, vel_amp=25, vel_filt=5, drive=12),
    D("Roots Bass", "A warm round note with a little thumb at the front", "Bass", "Dub Classic", "Warm",
      o(1, "Virtual Analog", timbre=25, morph=20), o(2, "Virtual Analog", oct=-1, morph=0, level=60), **MONO, cutoff=27, reso=12, f_mod=28, m_decay=44, decay=56, sustain=45, release=20, drive=25, vel_amp=30),
    D("Rubber Dub", "Bouncy and short, slides between notes when you play legato", "Bass", "Dub Experimental", "Warm",
      o(1, "Waveshaper", oct=-1, harm=22, timbre=28, morph=30), **LEGATO, glide=8, cutoff=38, reso=18, f_mod=30, m_decay=40, decay=50, sustain=25, release=22, drive=20),
    D("Upright Dub", "A plucked double bass in a small room", "Bass", "Dub Classic", "Warm Soft",
      o(1, "String", oct=-1, harm=28, timbre=30, morph=58), **MONO, **STRUCK, cutoff=55, drive=35, release=32, vel_amp=55),
    D("Pressure Sub", "Push the keys: pressure opens it up from a hum to a growl", "Bass", "Dub Techno", "Dark Warm",
      o(1, "Virtual Analog", oct=-1, timbre=35, morph=45), o(2, "Virtual Analog", oct=-2, morph=0, level=55), **MONO, cutoff=22, reso=20, at_cutoff=45, decay=60, sustain=90, release=22, drive=30, vel_amp=15),
    D("Dread Growl", "Filter moving in eighths with the song", "Bass", "Dub Techno", "Aggressive Dark",
      SAW(1), SAW(2, oct=-1, level=60), **MONO, cutoff=26, reso=38, f_src="LFO", f_mod=40, lfo_sync="ON", lfo_div="1/8", lfo_lock="BAR", drive=40, release=22),

    # ---- the offbeat: skanks, organs and keys ---------------------------------------------------------------------------
    D("Offbeat Skank", "The guitar on the two and four. Short, bright, and gone", "Keys", "Dub Classic", "Bright",
      SAW(1, timbre=55), SAW(2, fine=5, level=70, timbre=35), both(echo("1/8 D", 35, 18, low=45, high=68, duck=45), spring(14, decay=40)), f_mode="HP", cutoff=30, reso=15, keytrack=100, decay=38, sustain=0, release=14, vel_amp=55, drive=0),
    D("Bubble Organ", "The organ shuffle: percussive drawbars for pushing eighths", "Keys", "Dub Classic", "Warm",
      o(1, "Additive", harm=30, timbre=38, morph=55), spring(12, decay=40), decay=36, sustain=8, release=14, cutoff=78, vel_amp=30, vel_filt=10, drive=0),
    D("Sound System Organ", "Full and steady, with a slow speaker turning behind it", "Keys", "Dub Classic", "Warm Evolving",
      o(1, "Additive", harm=38, timbre=42, morph=62), o(2, "Waveshaper", oct=1, harm=20, timbre=25, morph=20, level=35), both(phaser(32, rate=42, depth=55, feedback=15), spring(15)), attack=6, release=24, cutoff=70, vel_amp=10, vel_filt=0),
    D("Dub Tines", "Electric piano through tape, with a spring on the end", "Keys", "Dub Classic", "Warm Soft",
      o(1, "FM 2-Op", harm=33, timbre=28, morph=5), o(2, "FM 2-Op", harm=58, timbre=16, morph=0, level=45), both(chorus("I", 55), echo("1/8 D", 40, 16, low=40, high=58, wow=25), spring(12)), decay=74, sustain=0, release=45, cutoff=68, vel_amp=65, vel_filt=35),
    D("Clav Answer", "A funky clav that bounces back across the room", "Keys", "Dub House", "Bright",
      SAW(1, timbre=60), echo("1/8", 45, 22, low=50, high=70, pong=True, width=85), f_mode="HP", cutoff=26, keytrack=100, reso=20, decay=46, sustain=0, release=18, vel_amp=70, drive=18),
    D("Melodica Cry", "Breathy reed, on the wheel for vibrato. Leave space for the echo", "Lead", "Dub Classic", "Soft",
      o(1, "Waveshaper", harm=42, timbre=30, morph=25), both(echo("1/4 D", 45, 24, low=40, high=56, wow=30), spring(18)), VIB, **LEGATO, glide=5, attack=18, release=30, cutoff=62, reso=8, noise=6, at_cutoff=20, vel_amp=30),
    D("Horn Section", "Two brassy saws for the riff at the end of the bar", "Lead", "Dub Classic", "Bright Warm",
      SAW(1), SAW(2, fine=7, oct=1, level=45), both(echo("1/4", 35, 18, low=40, high=60), spring(20)), attack=16, decay=52, sustain=55, release=26, cutoff=46, f_mod=38, m_attack=14, m_decay=48, reso=8, vel_filt=30),

    # ---- dub techno: one chord and a very long echo ------------------------------------------------------------------------
    D("Deep Chord", "The classic: a filtered minor chord dissolving into its own echoes", "Keys", "Dub Techno", "Dark Evolving",
      o(1, "Chords", harm=30, timbre=40, morph=35), both(echo("1/8 D", 70, 34, low=45, high=52, duck=25, pong=True, width=70), plate(22, decay=70, damping=55)), decay=46, sustain=0, release=24, cutoff=52, reso=24, f_mod=18, m_decay=40, voices=4, drive=0),
    D("Echo Chamber Chord", "Two tape heads feeding back into a spring", "Keys", "Dub Techno", "Dark Evolving",
      o(1, "Chords", harm=55, timbre=35, morph=30), both(echo("1/8", 76, 36, low=48, high=50, wow=30, drive=40, heads="1+3"), spring(24, decay=60)), decay=42, sustain=0, release=22, cutoff=54, reso=20, voices=4, drive=0),
    D("Basic Stab", "Thin and clipped. All the body comes from the delay", "Keys", "Dub Techno", "Metallic",
      o(1, "Chords", harm=30, timbre=60, morph=45), both(echo("1/4 D", 62, 30, low=55, high=58, duck=20), plate(16, damping=50)), f_mode="HP", cutoff=36, decay=30, sustain=0, release=12, reso=15, voices=4, drive=15),
    D("Tape Haze", "Slow air with the wow turned up. The tape is doing the moving", "Pad", "Dub Ambient", "Soft Evolving",
      o(1, "Swarm", harm=55, timbre=42, morph=50), both(echo("1/4", 55, 22, low=40, high=50, wow=60), plate(26, decay=75, damping=55)), attack=72, release=78, cutoff=48, reso=8, noise=5, vel_amp=10, vel_filt=0),
    D("Submerged", "A low pad under water, turning slowly", "Pad", "Dub Ambient", "Dark Evolving",
      SAW(1), SAW(2, fine=8, oct=-1, level=70), both(phaser(45, rate=14, depth=85, feedback=45), plate(24, decay=72, damping=65)), attack=68, release=74, cutoff=30, reso=18, f_src="LFO", f_mod=10, lfo_rate=14, vel_amp=10, vel_filt=0),

    # ---- hold a key ----------------------------------------------------------------------------------------------------------
    D("Steppers Pulse", "Hold a chord: it re-strikes on the offbeat with the song", "Sequence", "Dub Techno", "Warm",
      o(1, "Chords", harm=30, timbre=40, morph=35), cords(("CLOCK 1", "ENV TRIG", 100)), clock(1, div="x1", shape="TRIG", phase=50), both(echo("1/8 D", 55, 26, low=45, high=56, duck=20), spring(14)), decay=40, sustain=0, release=20, cutoff=50, reso=18, voices=4, drive=0),
    D("Riddim Box", "Hold a key: one drop kick on the three, rim on the offbeats, hats between", "Sequence", "Dub Classic", "Warm",
      o(1, "Bass Drum", harm=20, timbre=30, morph=50), o(2, "Snare Drum", harm=70, timbre=70, morph=25, level=70), o(3, "Hi-Hat", harm=30, timbre=60, morph=18, level=60), cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "O2 TRIG", 100), ("CLOCK 3", "O3 TRIG", 100)), clock(1, div="/2", shape="TRIG", phase=50), clock(2, div="x1", shape="TRIG", phase=50), clock(3, div="x2", shape="TRIG", phase=50, prob=80), spring(16, decay=40), vca_mode="ON", cutoff=100, reso=0, keytrack=0, vel_filt=0, release=40, voices=2),

    # ---- the engineer's toys --------------------------------------------------------------------------------------------------
    D("Dub Siren", "The two-tone siren, into the tape", "Riser/FX", "Dub", "Bright Aggressive",
      SAW(1, timbre=60), both(echo("1/8 D", 68, 36, low=38, high=56, wow=25, drive=35), spring(20)), **MONO, lfo_shape="SQR", lfo_rate=44, vibrato=48, cutoff=80, reso=8, keytrack=0, vel_amp=0, vel_filt=0, release=25),
    D("Rising Siren", "Hold it and it climbs. Let go and the echoes carry it off", "Riser/FX", "Dub Rave", "Bright Evolving",
      SAW(1, timbre=70), cords(("MOD ENV", "ALL PITCH", 70)), both(echo("1/8 D", 72, 34, low=40, high=58, wow=20), spring(18)), **MONO, m_attack=88, m_sustain=100, m_release=70, lfo_shape="TRI", lfo_rate=58, vibrato=22, cutoff=75, keytrack=0, vel_amp=0, vel_filt=0, release=30),
    D("Zap Drop", "A falling laser that echoes off into the left and right", "Riser/FX", "Dub", "Bright Aggressive",
      o(1, "FM 2-Op", harm=50, timbre=55, morph=30, fm=100), echo("1/8 D", 72, 38, low=40, high=62, drive=30, wow=20, pong=True), m_decay=36, sustain=0, decay=44, reso=30, cutoff=80, release=20, vel_filt=0),
    D("Tape Runaway", "One hit and the machine nearly takes over. Turn REPEATS past 100 for the rest", "Riser/FX", "Dub Experimental", "Aggressive Evolving",
      o(1, "Modal", harm=40, timbre=50, morph=40), echo("1/16 D", 86, 42, low=35, high=55, drive=60, wow=35, heads="1+3"), **STRUCK, cutoff=70, release=25, vel_filt=0),
    D("Spring Crash", "The tank kicked over. Hit it hard", "Riser/FX", "Dub", "Aggressive Dark",
      dict(o1_on="OFF", o2_on="OFF", o3_on="OFF"), spring(70, decay=70, drip=85, damping=20), noise=100, decay=28, sustain=0, release=10, cutoff=100, reso=0, keytrack=0, vel_filt=0, vel_amp=70, drive=0),
    D("Thunder Drop", "A low hit that falls away and keeps rolling", "Riser/FX", "Dub Cinematic", "Dark",
      o(1, "Bass Drum", harm=12, timbre=25, morph=75), both(echo("1/4 D", 50, 22, low=25, high=38, drive=45), plate(30, decay=82, damping=70)), **DRUM),
    D("Echo Drip", "Water in a cave, from one ear to the other", "Riser/FX", "Dub Ambient", "Metallic Soft",
      o(1, "Modal", harm=62, timbre=48, morph=38), both(echo("1/8 D", 66, 34, low=50, high=66, pong=True), plate(20, decay=60)), **STRUCK, cutoff=85, release=30),

    # ---- drums ------------------------------------------------------------------------------------------------------------------
    D("Dub Rim", "Dry crack, wet tail. A rimshot sent straight to the echo", "Perc", "Dub Classic", "Bright",
      o(1, "Snare Drum", harm=88, timbre=78, morph=18), echo("1/8 D", 55, 30, low=50, high=62, wow=20), **DRUM),
    D("Steppers Kick", "Short and round, for four on the floor", "Perc", "Dub House", "Warm",
      o(1, "Bass Drum", harm=20, timbre=25, morph=32), **DRUM),
    D("Snare In The Tank", "A snare with the spring wide open", "Perc", "Dub Classic", "Warm",
      o(1, "Snare Drum", harm=62, timbre=52, morph=38), spring(42, decay=60, drip=80), **DRUM),
]
assert len(ENTRIES) == 30, len(ENTRIES)
