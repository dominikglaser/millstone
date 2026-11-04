"""Elements on OSC 1: things that are bowed, blown and struck. Some alone, some with the other oscillators beside them or driving them."""
from preset_helpers import *
from preset_fx import echo, plate, spring, chorus, phaser, both

PACK = "Elements 1"

BASE_E = dict(o1_type="ELEMENTS", vca_mode="ON", cutoff=100, reso=0, keytrack=0, vel_filt=0, release=45)
QUIET = dict(e_strike=0)

def X(name, note, type_, styles, characters, *parts, **kw):
    return name, note, P(name, note, dict(BASE_E), *parts, **kw)[2], type_, styles, characters

ENTRIES = [
    # ---- alone: bowed ---------------------------------------------------------------------------------------------------
    X("Solo Cello", "Up close, on the string. Play slowly", "Lead", "Cinematic Classic", "Warm", o(1, oct=-1), QUIET, plate(14), e_model="STRING", e_bow=80, e_bow_t=45, e_geom=14, e_bright=38, e_damp=74, e_pos=18, e_space=25),
    X("Solo Violin", "Brighter and nearer the bridge", "Lead", "Cinematic Classic", "Bright", QUIET, plate(16), VIB, e_model="STRING", e_bow=78, e_bow_t=60, e_geom=12, e_bright=62, e_damp=72, e_pos=12, e_space=30),
    X("Bowed Metal", "A saw blade played with a bow", "Texture", "Cinematic Experimental", "Metallic Evolving", QUIET, plate(24, decay=70), e_model="MODAL", e_bow=70, e_bow_t=70, e_geom=78, e_bright=62, e_damp=72, e_pos=35, e_space=40),
    X("Bowed Glass", "Wet finger on a wine glass", "Pad", "Ambient Cinematic", "Bright Soft", QUIET, plate(26, decay=72), e_model="MODAL", e_bow=62, e_bow_t=55, e_geom=62, e_bright=70, e_damp=78, e_pos=45, e_space=50, attack=20),
    X("String Section", "Strings, the model that plays a chord of them", "Pad", "Cinematic Classic", "Warm", QUIET, chorus("I", 35), e_model="STRINGS", e_bow=75, e_bow_t=45, e_geom=20, e_bright=45, e_damp=74, e_space=55, voices=4),
    # ---- alone: blown ------------------------------------------------------------------------------------------------------
    X("Bamboo Flute", "Soft air in a wooden tube", "Lead", "Ambient Cinematic", "Soft", QUIET, plate(18), VIB, e_model="MODAL", e_blow=85, e_flow=72, e_blow_t=38, e_geom=8, e_bright=48, e_damp=72, e_space=35),
    X("Pan Pipes", "More air than note", "Lead", "Ambient", "Soft Warm", QUIET, plate(22, decay=60), e_model="MODAL", e_blow=90, e_flow=45, e_blow_t=55, e_geom=6, e_bright=40, e_damp=74, e_pos=45, e_space=40),
    X("Bottle Blow", "Across the top of a big bottle", "Texture", "Experimental Ambient", "Warm Soft", QUIET, e_model="MODAL", e_blow=80, e_flow=35, e_blow_t=30, e_geom=3, e_bright=25, e_damp=78, e_pos=50, e_space=20),
    X("Wind Harp", "Air across strings, in the open", "Pad", "Ambient Experimental", "Soft Evolving", QUIET, plate(26, decay=70), e_model="STRINGS", e_blow=78, e_flow=30, e_blow_t=70, e_geom=30, e_bright=55, e_damp=78, e_space=70, voices=4),
    # ---- alone: struck ----------------------------------------------------------------------------------------------------
    X("Steel Pan", "Sunny and a little out", "Pluck", "Dub Classic", "Bright Metallic", spring(10), e_model="MODAL", e_strike=88, e_mallet=30, e_strike_t=50, e_geom=46, e_bright=62, e_damp=45, e_pos=30, e_space=25, vel_amp=55),
    X("Wood Block", "Hollow, dry and short", "Perc", "Classic Dub", "Warm", e_model="MODAL", e_strike=95, e_mallet=70, e_strike_t=35, e_geom=4, e_bright=48, e_damp=20, e_pos=25, e_space=5, vel_amp=55),
    X("Gamelan", "Bronze keys, with beating in the ring", "Pluck", "Experimental Ambient", "Metallic Bright", plate(16, decay=60), e_model="MODAL", e_strike=85, e_mallet=45, e_strike_t=65, e_geom=64, e_bright=60, e_damp=55, e_pos=45, e_space=45, vel_amp=55),
    X("Tubular Bell", "Long metal tubes, from a height", "Pluck", "Cinematic", "Metallic Bright", plate(22, decay=72), e_model="MODAL", e_strike=100, e_mallet=55, e_strike_t=55, e_geom=74, e_bright=66, e_damp=64, e_pos=20, e_space=55, vel_amp=55, release=55, drive=0),
    X("Soft Hammer", "An upright with the practice pedal on", "Keys", "Cinematic Ambient", "Warm Soft", e_model="STRING", e_strike=78, e_mallet=12, e_strike_t=22, e_geom=10, e_bright=30, e_damp=58, e_pos=22, e_space=30, vel_amp=70),
    X("Plucked Harp", "A strummed set of strings", "Pluck", "Cinematic Classic", "Soft", plate(14), e_model="STRINGS", e_strike=80, e_mallet=45, e_strike_t=45, e_geom=25, e_bright=58, e_damp=52, e_pos=35, e_space=45, vel_amp=55, voices=4),
    X("Mallet Tom", "A tuned drum, felt-headed stick", "Perc", "Cinematic Dub", "Warm", e_model="MODAL", e_strike=90, e_mallet=18, e_strike_t=30, e_geom=18, e_bright=28, e_damp=40, e_pos=25, e_space=20, vel_amp=60),
    # ---- with another oscillator beside it ----------------------------------------------------------------------------------
    X("Bowed Saw Layer", "A bow on top of a soft saw pad, the two rub together", "Pad", "Cinematic Ambient", "Warm Evolving", SAW(2, level=40, fine=5), QUIET, both(chorus("I", 40), plate(22, decay=68)), e_model="STRING", e_bow=72, e_bow_t=45, e_geom=14, e_bright=42, e_damp=74, e_space=40, attack=30, cutoff=60),
    X("Piano And Sub", "A felt piano with a sine an octave down", "Keys", "Ambient House", "Warm Soft", SOFT(2, oct=-1, level=45), e_model="STRING", e_strike=85, e_mallet=20, e_strike_t=30, e_geom=12, e_bright=36, e_damp=55, e_pos=25, e_space=35, vel_amp=65),
    X("Choir Over Bell", "A struck bell with voices coming up behind it", "Pad", "Cinematic", "Bright Warm", o(2, "Grain Formant", harm=45, timbre=35, morph=25, level=45), plate(26, decay=70), e_model="MODAL", e_strike=80, e_mallet=50, e_geom=66, e_bright=60, e_damp=50, e_space=50, vel_amp=30),
    X("Flute And Strings", "Breath in front, machine strings behind", "Pad", "Cinematic Classic", "Warm Soft", o(2, "String Machine", harm=45, timbre=55, morph=50, level=50), QUIET, plate(20), VIB, e_model="MODAL", e_blow=85, e_flow=65, e_blow_t=40, e_geom=8, e_bright=48, e_damp=72, e_space=35),
    X("Struck Bass", "A plucked string with a square underneath, for weight", "Bass", "Dub Classic", "Warm", o(1, oct=-1), SQUARE(2, oct=-1, level=45, timbre=50), **MONO, e_model="STRING", e_strike=90, e_mallet=30, e_strike_t=40, e_geom=10, e_bright=38, e_damp=48, e_pos=20, e_space=8, vel_amp=45),
    X("Marimba Chord", "Bars over a soft chord, for gentle house", "Keys", "House Ambient", "Warm Soft", o(2, "Chords", harm=30, timbre=30, morph=35, level=35), echo("1/8 D", 35, 14), e_model="MODAL", e_strike=88, e_mallet=62, e_strike_t=55, e_geom=50, e_bright=60, e_damp=42, e_pos=40, e_space=20, vel_amp=55, voices=4),
    # ---- with another oscillator driving it ------------------------------------------------------------------------------------
    X("Swarm Resonator", "A swarm heard only through a ringing body", "Texture", "Experimental Ambient", "Evolving Metallic", o(2, "Swarm", harm=55, timbre=50, morph=50, level=0), cords(("OSC 2", "E STK IN", 70)), QUIET, plate(22), e_model="MODAL", e_geom=55, e_bright=60, e_damp=70, e_pos=40, e_space=40, attack=22),
    X("Talking Strings", "Speech driving a set of strings", "Texture", "Experimental", "Metallic Evolving", o(2, "Speech", harm=50, timbre=50, morph=40, morph_mod=40, level=0), cords(("OSC 2", "E STK IN", 75)), QUIET, plate(16), lfo_rate=40, e_model="STRINGS", e_geom=30, e_bright=55, e_damp=68, e_space=40, voices=4),
    X("Noise Bowl", "Wind going round a metal bowl", "Drone", "Ambient Cinematic", "Metallic Soft", cords(("NOISE", "E BLW IN", 70), ("LFO", "E POS", 20)), QUIET, plate(24, decay=74), lfo_rate=14, e_model="MODAL", e_geom=72, e_bright=52, e_damp=78, e_pos=50, e_space=60, attack=24, release=60),
    X("Bass Drum Body", "A kick striking a big resonator, in tune", "Perc", "Experimental Techno", "Dark Metallic", o(2, "Bass Drum", harm=25, timbre=35, morph=45, level=0), cords(("OSC 2", "E STK IN", 90)), QUIET, e_model="MODAL", e_geom=22, e_bright=38, e_damp=46, e_pos=30, e_space=15, vel_amp=55),
    # ---- moved by the patch bay ----------------------------------------------------------------------------------------------------
    X("Clock Marimba", "Hold a chord: the clock plays it in a rolling pattern", "Sequence", "Techno Ambient", "Warm", cords(("CLOCK 1", "O1 TRIG", 100)), clock(1, div="x4", shape="TRIG", steps=16, fill=7), echo("1/8 D", 40, 18, pong=True, width=80), e_model="MODAL", e_strike=88, e_mallet=60, e_geom=48, e_bright=58, e_damp=38, e_pos=35, e_space=20, voices=4),
    X("Tide Bow", "Tides pulling the bow in and out, on its own", "Pad", "Ambient Experimental", "Evolving", cords(("TIDES 1", "E BOW", 70), ("TIDES 2", "E GEOM", 18)), QUIET, plate(26, decay=70), t_trig="FREE", t_mode="LOOP", t_rate=30, t_out="PHASE", e_model="STRING", e_bow=25, e_bow_t=50, e_geom=18, e_bright=45, e_damp=74, e_space=50),
    X("Pressure Bow", "Your fingers are the bow: press harder for more", "Lead", "Cinematic Experimental", "Warm Evolving", cords(("AFTERTOUCH", "E BOW", 80), ("WHEEL", "E GEOM", 30)), QUIET, plate(16), e_model="STRING", e_bow=20, e_bow_t=45, e_geom=14, e_bright=48, e_damp=76, e_space=30),
    X("Random Geometry", "A struck thing that changes shape on every hit", "Sequence", "Experimental", "Metallic Evolving", cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "E GEOM", 45)), clock(1, div="x2", shape="TRIG", prob=80), clock(2, div="x2", shape="S/H", offset=-50), both(echo("1/8 D", 40, 18, pong=True), plate(14)), e_model="MODAL", e_strike=90, e_mallet=45, e_geom=45, e_bright=58, e_damp=40, e_pos=35, e_space=25, voices=2),
]
assert len(ENTRIES) == 30, len(ENTRIES)
