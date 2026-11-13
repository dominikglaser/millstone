#!/usr/bin/env python3
"""Writes presets.json and PRESETS.md from the definitions below. Run it after editing them: python3 vst/make_presets.py

Each preset states only what differs from BASE, but the file lists every parameter: the MPC's PRESET menu sets only the
values a preset lists, so anything left out would leak in from the preset before it. It also means a later change to a
parameter's default cannot alter a saved sound. preset_trim.json holds each preset's measured VOLUME, set by
desktop/tools/preset_test --trim so that a library played one after the other stays even."""
import json, os
from preset_tags import TAGS, TYPES, STYLES, CHARACTERS
from preset_fx import FX
from preset_helpers import *
from preset_pitch import true_to_note, MOVE
import pack_dub, pack_classic, pack_rave, pack_techno, pack_space, pack_elements, pack_drums, pack_patch, pack_modular, pack_cross, pack_deep, pack_cinema

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
BASE.update(lfo2_shape="TRI", lfo2_rate=45, lfo2_retrig="OFF", lfo2_sync="OFF", lfo2_div="1/4", lfo2_lock="FREE", lfo_page="1")
for n in (1, 2, 3):
    BASE.update({"o%d_%s" % (n, k): v for k, v in dict(
        on="ON" if n == 1 else "OFF", engine="Virtual Analog", harm=50, timbre=50, morph=50, fm=0, timbre_mod=0, morph_mod=0,
        cv_src="LFO", amp="AUTO", oct=0, semi=0, fine=0, level=100, mix=0).items()})

for n in range(1, 13):   # the patch bay: nothing plugged in unless a preset says so
    BASE.update({"c%d_src" % n: "OFF", "c%d_dst" % n: "OFF", "c%d_amt" % n: 50})

BASE.update(t_mode="LOOP", t_out="PHASE", t_trig="NOTE", t_div="1/4", t_rate=45, t_shape=50, t_slope=50, t_smooth=50, t_shift=50)

BASE.update(ck_src="SONG", ck_bpm=120)
for n in range(1, 9):
    BASE.update({"k%d_%s" % (n, k): v for k, v in dict(div="x1", shape="GATE", width=50, swing=0, prob=100, phase=0, level=100, offset=0, steps=0, fill=4, rot=0).items()})

BASE.update(o1_type="PLAITS", e_model="MODAL", e_contour=100, e_bow=0, e_bow_t=50, e_blow=0, e_flow=50, e_blow_t=50, e_strike=80, e_mallet=50, e_strike_t=50, e_geom=20, e_bright=50, e_damp=25, e_pos=30, e_space=30)
# Effects are all off in a preset unless it says otherwise, so a sound is what the voices make.
BASE.update(fx_ph_rate=30, fx_ph_depth=70, fx_ph_fb=30, fx_ph_mix=0, fx_ch_mode="OFF", fx_ch_mix=100,
            fx_dl_sync="SYNC", fx_dl_time=375, fx_dl_div="1/8 D", fx_dl_fb=45, fx_dl_in="OPEN", fx_dl_throw="OFF", fx_dl_heads="1", fx_dl_pp="OFF", fx_dl_width=100,
            fx_dl_low=25, fx_dl_high=70, fx_dl_drive=25, fx_dl_wow=20, fx_dl_duck=0, fx_dl_mix=0,
            fx_rv_type="NORMAL", fx_rv_decay=50, fx_rv_diff=60, fx_rv_damp=40, fx_rv_kick="OFF", fx_rv_mix=0, fx_out=0)

LIBRARY = {
"Bass": [
    P("Round Sub", "Clean low end, slides between held notes", o(1, "Virtual Analog", oct=-1, timbre=30, morph=0), o(2, "Virtual Analog", oct=-2, morph=0, level=45), **LEGATO, glide=8, cutoff=38, f_mod=25, m_decay=42, m_sustain=10, release=25, vel_filt=20),
    P("Acid Line", "Squelchy resonant line. Velocity opens it", SAW(1, timbre=60), **LEGATO, glide=20, cutoff=26, reso=72, f_mod=64, m_decay=40, drive=20, decay=45, sustain=60, vel_filt=40, vel_amp=30),
    P("FM Slap", "Hard attack, quick decay", o(1, "FM 2-Op", harm=25, timbre=45, morph=15, oct=-1), **MONO, decay=48, sustain=0, cutoff=68, f_mod=20, m_decay=40, vel_filt=30, release=25),
    P("Reese", "Two saws a few cents apart, phasing against each other over a clean sub", SAW(1, fine=-9), SAW(2, fine=9), SOFT(3, oct=-1, level=40), **LEGATO, glide=6, cutoff=57, reso=12, keytrack=20, f_mod=8, f_src="LFO", lfo_rate=30, drive=30, attack=2, release=28, vel_amp=15, vel_filt=0),
    P("Tempo Growl", "Wavetable moving in eighths with the song", o(1, "Wavetable", oct=-1, harm=30, timbre=60, morph=40, timbre_mod=45, morph_mod=30), **MONO, lfo_sync="ON", lfo_div="1/8", lfo_lock="BAR", cutoff=52, reso=40, f_mod=25, f_src="LFO", drive=25),
    P("Pluck Bass", "Driven and short, sits well in a groove", SAW(1, timbre=45), o(2, "Virtual Analog", oct=-1, morph=0, level=60), **MONO, decay=44, sustain=15, cutoff=24, f_mod=68, m_decay=38, reso=30, drive=50, release=20),
    P("Modal Thump", "Struck body with a triangle underneath", o(1, "Modal", harm=20, timbre=25, morph=35), o(2, "Virtual Analog", oct=-1, morph=0, level=70), **MONO, decay=56, sustain=0, cutoff=72, vel_amp=60, release=25),
    P("Phase Bass", "Bright bite that closes as the envelope falls", o(1, "Phase Dist", oct=-1, harm=20, timbre=35, morph=45, timbre_mod=45, cv_src="ENV"), **LEGATO, glide=5, m_decay=46, m_sustain=10, cutoff=56, f_mod=25, release=25),
],
"Lead": [
    P("Classic Lead", "Fat detuned saws. Mod wheel adds vibrato", SAW(1), SAW(2, fine=6), **LEGATO, **VIB, vibrato=10, glide=12, cutoff=60, reso=25, f_mod=30, m_decay=55, m_sustain=30),
    P("Sync Scream", "Timbre sweeps down from the envelope", o(1, "Virtual Analog", timbre=70, morph=100, timbre_mod=35, cv_src="ENV"), o(2, "Virtual Analog", fine=-5, timbre=70, morph=100, timbre_mod=35, cv_src="ENV"), **MONO, **VIB, vibrato=10, m_decay=60, m_sustain=15, cutoff=70, reso=30),
    P("Square Hero", "Hollow square, pressure brightens it", o(1, "Virtual Analog", timbre=50, morph=0), **LEGATO, **VIB, vibrato=10, glide=8, cutoff=62, reso=15, at_cutoff=35, f_mod=15),
    P("Glass FM", "Clear and bell-like", o(1, "FM 2-Op", harm=60, timbre=38, morph=20), **MONO, **VIB, vibrato=8, at_morph=30, decay=60, sustain=70, cutoff=85),
    P("Formant Cry", "Vocal, with a slow moving formant", o(1, "Grain Formant", timbre=55, morph=40, timbre_mod=25), **LEGATO, vibrato=12, lfo_rate=60, glide=10, cutoff=80),
    P("Chip Lead", "8-bit arpeggio voice", o(1, "Chiptune", harm=20, timbre=50, morph=50), **MONO, cutoff=90, reso=0, keytrack=0, release=15),
    P("Wave Solo", "Wavetable that opens on each note", o(1, "Wavetable", harm=55, timbre=70, morph=30, morph_mod=40, cv_src="ENV"), **LEGATO, **VIB, vibrato=10, glide=6, m_decay=62, m_sustain=30, cutoff=72),
    P("Overdrive", "Hot and resonant. Pressure opens the filter", SAW(1), o(2, "Waveshaper", harm=40, timbre=45, morph=50, level=70), **LEGATO, glide=10, drive=70, reso=55, cutoff=56, f_mod=20, at_cutoff=40),
],
"Keys": [
    P("Tine Piano", "Electric piano. Plays softer and darker gently", o(1, "FM 2-Op", harm=33, timbre=30, morph=5), o(2, "FM 2-Op", harm=58, timbre=18, morph=0, level=50), decay=78, sustain=0, release=48, cutoff=74, vel_filt=40, vel_amp=70),
    P("DX Keys", "A six-op patch, HARMONICS picks another", o(1, "6-Op FM A", harm=15, timbre=50, morph=50), sustain=100, cutoff=90, reso=0, keytrack=0, vel_amp=60, release=35),
    P("Soft Organ", "Steady and rounded", o(1, "Additive", harm=35, timbre=40, morph=60), vibrato=8, lfo_rate=60, release=22, cutoff=82, vel_amp=0, vel_filt=0),
    P("Reed Organ", "Buzzier, with a slower swell", o(1, "Waveshaper", harm=35, timbre=30, morph=30), o(2, "Additive", oct=1, harm=25, timbre=30, morph=50, level=50), attack=12, release=28, cutoff=74, vel_amp=10, vibrato=8, lfo_rate=58),
    P("Funk Clav", "Bright and percussive through the high-pass", SAW(1, timbre=60), SAW(2, fine=4, level=70, timbre=30), f_mode="HP", cutoff=24, keytrack=100, reso=20, decay=58, sustain=0, release=22, vel_amp=70, drive=20),
    P("Harpsi", "Octave-doubled, sharp attack", o(1, "Phase Dist", harm=25, timbre=60, morph=35), o(2, "Phase Dist", oct=1, fine=5, harm=25, timbre=60, morph=35, level=65), decay=66, sustain=0, release=30, cutoff=88),
    P("Mellow Poly", "Warm eighties polysynth", SAW(1), SAW(2, fine=9), attack=8, decay=58, sustain=70, release=48, cutoff=42, f_mod=35, m_decay=60, m_sustain=30),
    P("Chord Memory", "One key plays a whole chord", o(1, "Chords", harm=30, timbre=40, morph=40), decay=62, sustain=0, cutoff=62, f_mod=25, m_decay=50, release=38, voices=4),
],
"Pad": [
    P("Warm Strings", "String machine ensemble", o(1, "String Machine", harm=45, timbre=55, morph=50), attack=74, release=80, cutoff=70, vel_amp=15),
    P("Analog Pad", "Three saws, filter opens slowly", SAW(1), SAW(2, fine=8), SAW(3, fine=-11, oct=-1, level=60), attack=76, release=82, cutoff=38, f_mod=32, m_attack=84, m_decay=78, m_sustain=70, m_release=80, vel_amp=15),
    P("Swarm Pad", "Dense cloud of detuned voices", o(1, "Swarm", harm=60, timbre=50, morph=55), attack=72, release=82, cutoff=66, vel_amp=15),
    P("Glass Pad", "Wavetable shimmer, slow movement", o(1, "Wavetable", harm=60, timbre=62, morph=30, morph_mod=35), o(2, "Wavetable", oct=1, harm=60, timbre=70, morph=60, morph_mod=-30, level=50, fine=6), attack=70, release=84, lfo_rate=25, cutoff=80, vel_amp=15),
    P("Formant Choir", "Breathy choir", o(1, "Grain Formant", harm=50, timbre=45, morph=30), o(2, "Grain Formant", harm=50, timbre=50, morph=30, fine=7, level=80), vibrato=8, lfo_rate=58, attack=74, release=78, cutoff=76, vel_amp=15),
    P("Terrain Drift", "Waveform slowly wandering", o(1, "Wave Terrain", harm=40, timbre=50, morph=50, timbre_mod=40, morph_mod=30), lfo_rate=22, attack=74, release=82, cutoff=72, vel_amp=15),
    P("Chord Cloud", "Each key blooms into a chord", o(1, "Chords", harm=50, timbre=50, morph=60), o(2, "Swarm", harm=55, timbre=50, morph=50, level=45), attack=78, release=84, cutoff=62, voices=4, vel_amp=15),
    P("Dark Drone", "Low and heavy, long release", SAW(1, oct=-1), SAW(2, oct=-2, fine=5), noise=8, attack=84, release=88, cutoff=30, reso=30, f_mod=8, f_src="LFO", lfo_rate=15, vel_amp=0, vel_filt=0),
],
"Pluck": [
    P("Harp String", "Physical string, rings by itself", o(1, "String", harm=40, timbre=50, morph=55), vca_mode="ON", cutoff=92, reso=0, keytrack=0, drive=20, release=50, vel_amp=60),
    P("Modal Bell", "Struck metal", o(1, "Modal", harm=45, timbre=65, morph=60), vca_mode="ON", cutoff=100, reso=0, keytrack=0, release=50, vel_amp=60),
    P("Nylon", "Soft, darker string", o(1, "String", harm=25, timbre=30, morph=45), o(2, "String", harm=25, timbre=30, morph=45, fine=3), vca_mode="ON", cutoff=80, reso=5, keytrack=70, drive=40, release=45, vel_filt=30, vel_amp=60),
    P("Synth Pluck", "The classic, envelope snapping the filter shut", SAW(1), SAW(2, fine=7), decay=62, sustain=0, cutoff=30, reso=20, f_mod=60, m_decay=54, release=46),
    P("FM Pluck", "Bright metallic transient", o(1, "FM 2-Op", harm=40, timbre=20, morph=10, timbre_mod=60, cv_src="ENV"), decay=62, sustain=0, m_decay=46, cutoff=82, release=46),
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
    P("Riser", "Hold it: rises over a few seconds", SAW(1, fm=25), noise=45, cutoff=18, reso=35, f_mod=80, m_attack=100, m_sustain=100, m_release=60, attack=45, release=55, voices=2, vel_amp=0, vel_filt=0),
    P("Downer", "Pitch falls away from the attack", SAW(1, fm=70), m_decay=72, cutoff=70, reso=30, f_mod=20, sustain=0, decay=75, release=40),
    P("Laser", "Fast pitch zap", o(1, "FM 2-Op", harm=50, timbre=50, morph=30, fm=100), m_decay=38, sustain=0, decay=48, reso=40, cutoff=80, release=25),
    P("Random Bleeps", "Stepped pitches locked to the song", o(1, "Virtual Analog", timbre=50, morph=0), lfo_shape="S/H", lfo_sync="ON", lfo_div="1/8", lfo_lock="BAR", vibrato=55, reso=30, cutoff=72, voices=2),
    P("Whistle", "Self-oscillating filter, playable and tuned", o(1, "Virtual Analog", level=0), dict(o1_on="OFF"), noise=4, reso=100, keytrack=100, cutoff=58, attack=15, release=40, vel_amp=0, vel_filt=0),
    P("Alarm", "Two-tone siren", o(1, "Chiptune", harm=20, timbre=50, morph=50), lfo_shape="SQR", lfo_rate=42, vibrato=55, cutoff=90, reso=0, keytrack=0),
    P("Sonar Ping", "Resonant ping with a long tail", o(1, "Modal", harm=35, timbre=40, morph=68), vca_mode="ON", reso=45, cutoff=70, keytrack=100, release=60, voices=4),
    P("Vinyl Dust", "Crackle over a thin noise bed", o(1, "Particle", harm=85, timbre=70, morph=65), noise=50, attack=25, release=50, cutoff=30, f_mode="HP", keytrack=0, vel_amp=0, vel_filt=0),
],
}

# Trying out Elements, Tides, the clock and the patch bay. To be sorted into the library later. Hold a key to hear them: clocked
# ones run with or without the transport. Name prefixes: E Elements, T Tides, C clock, P patch bay.
TESTING = [
    # Elements
    P("Struck Bell", "Modal, struck: rings and fades. Elements' reverb on", dict(ELEMENTS), e_model="MODAL", e_strike=90, e_mallet=60, e_geom=70, e_bright=65, e_damp=35, e_pos=30, e_space=55),
    P("Bowed String", "Bow only: sustains while held", dict(ELEMENTS), e_model="STRING", e_strike=0, e_bow=80, e_geom=15, e_bright=45, e_damp=70, e_space=40),
    P("Blown Pipe", "Blow only: breathy and sustained", dict(ELEMENTS), e_model="MODAL", e_strike=0, e_blow=80, e_flow=60, e_blow_t=40, e_geom=10, e_bright=50, e_damp=70, e_space=45),
    P("String Pluck", "A single struck string", dict(ELEMENTS), e_model="STRING", e_strike=90, e_mallet=30, e_geom=10, e_bright=55, e_damp=45, e_space=25),
    P("Chord Strings", "STRINGS: each key plays a resonant chord", dict(ELEMENTS), e_model="STRINGS", e_strike=70, e_bow=30, e_geom=30, e_damp=55, e_space=50),
    P("Clock Strikes", "Hold a key: the clock strikes the resonator in a Euclidean pattern", dict(ELEMENTS), cords(("CLOCK 1", "O1 TRIG", 100)), clock(1, div="x2", shape="TRIG", steps=16, fill=7), e_model="MODAL", e_strike=90, e_geom=60, e_damp=40, e_space=40, drive=25),
    P("OSC Resonator", "OSC 2 is heard only through the resonator", dict(ELEMENTS), cords(("OSC 2", "E STK IN", 80)), o(2, "Wavetable", level=0, harm=50, timbre=60), e_model="MODAL", e_strike=0, e_geom=40, e_damp=60, e_bright=60, e_space=30),
    P("Noise Breath", "Noise through the exciter: wind in a resonator", dict(ELEMENTS), cords(("NOISE", "E BLW IN", 70)), e_model="MODAL", e_strike=0, e_geom=25, e_damp=70, e_bright=55, e_space=40),
    P("LFO Geometry", "Bowed, with the LFO moving the resonator's shape", dict(ELEMENTS), cords(("LFO", "E GEOM", 35)), lfo_rate=34, e_model="MODAL", e_strike=0, e_bow=70, e_geom=45, e_damp=70),
    P("Tides Swell", "Tides breathing the bow in and out", dict(ELEMENTS), cords(("TIDES 1", "E BOW", 80)), t_trig="FREE", t_mode="LOOP", t_rate=42, e_model="STRING", e_strike=0, e_bow=20, e_geom=15, e_damp=70),
    # Tides
    P("Note Envelope", "Tides as a one-shot on each note, opening the filter", SAW(1), cords(("TIDES 1", "CUTOFF", 85)), t_trig="NOTE", t_mode="AD", t_rate=54, cutoff=30, reso=25, attack=0, decay=66, sustain=0, release=50),
    P("Free Drift", "Four outputs moving a held chord, each voice drifting apart", SAW(1), o(2, "Wavetable", fine=6, level=80), cords(("TIDES 1", "CUTOFF", 40), ("TIDES 2", "O1 TIMB", 50), ("TIDES 3", "O2 MRPH", 50), ("TIDES 4", "O2 TIMB", 40)), t_trig="FREE", t_mode="LOOP", t_out="PHASE", t_rate=38, t_shift=70, cutoff=48, attack=55, release=65),
    P("Clock Filter", "Tides locked to the song's tempo in eighths", SAW(1), cords(("TIDES 1", "CUTOFF", 70)), t_trig="CLOCK", t_mode="LOOP", t_div="1/8", cutoff=38, reso=40, vca_mode="ON", release=25),
    P("Fired By Clock", "PATCH: only our clock fires Tides, a note does not", SAW(1), cords(("TIDES 1", "CUTOFF", 90), ("CLOCK 1", "T TRIG", 100)), clock(1, div="x2", shape="TRIG", steps=16, fill=9), t_trig="PATCH", t_mode="AD", t_rate=62, cutoff=26, reso=35, vca_mode="ON", release=25),
    P("Gate Chop", "GATES output chopping the note", SAW(1), SAW(2, fine=7), cords(("TIDES 3", "VCA", -100)), t_trig="FREE", t_mode="LOOP", t_out="GATES", t_rate=52, cutoff=62, vca_mode="ON", release=25),
    P("Shape Sweep", "A second Tides output reshaping the first", SAW(1), cords(("TIDES 1", "CUTOFF", 75), ("TIDES 2", "T SLOPE", 70)), t_trig="FREE", t_mode="LOOP", t_rate=46, t_shift=75, cutoff=36, reso=30, vca_mode="ON", release=25),
    # Clock
    P("Euclid Kick", "Hold a key: a Euclidean kick, 5 hits in 16", o(1, "Bass Drum", harm=25, timbre=35, morph=45), cords(("CLOCK 1", "O1 TRIG", 100)), clock(1, div="x2", shape="TRIG", steps=16, fill=5), vca_mode="ON", cutoff=100, reso=0, keytrack=0, vel_filt=0, release=40),
    P("Drum Machine", "Hold a key: kick, snare and hats on three clock outputs", o(1, "Bass Drum", harm=25, timbre=35, morph=45), o(2, "Snare Drum", harm=45, timbre=50, morph=40), o(3, "Hi-Hat", harm=30, timbre=60, morph=20), cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "O2 TRIG", 100), ("CLOCK 3", "O3 TRIG", 100)), clock(1, div="x2", shape="TRIG", steps=16, fill=5), clock(2, div="x1", shape="TRIG", phase=50), clock(3, div="x4", shape="TRIG", prob=80), vca_mode="ON", cutoff=100, reso=0, keytrack=0, vel_filt=0, release=40, voices=2),
    P("Trance Gate", "16th-note gate over saws", SAW(1), SAW(2, fine=8), cords(("CLOCK 1", "VCA", -100)), clock(1, div="x4", shape="GATE", width=45), cutoff=66, vca_mode="ON", release=30),
    P("Random Pitch", "Sample and hold on pitch, a new note each step", o(1, "Virtual Analog", morph=0), cords(("CLOCK 1", "O1 PITCH", 25)), clock(1, div="x2", shape="S/H"), cutoff=75, vca_mode="ON", release=25),
    P("Random Filter", "Sample and hold on the filter", SAW(1), cords(("CLOCK 1", "CUTOFF", 45)), clock(1, div="x4", shape="S/H"), cutoff=46, reso=50, vca_mode="ON", release=25),
    P("Swing Stabs", "The clock restarts the envelope, with swing", SAW(1), SAW(2, fine=8), cords(("CLOCK 1", "ENV TRIG", 100)), clock(1, div="x4", shape="TRIG", swing=70), decay=42, sustain=0, cutoff=55, release=20),
    P("Maybe Hats", "Hats at 60% probability", o(1, "Hi-Hat", harm=30, timbre=65, morph=15), cords(("CLOCK 1", "O1 TRIG", 100)), clock(1, div="x4", shape="TRIG", prob=60), vca_mode="ON", cutoff=100, reso=0, keytrack=0, vel_filt=0, release=30),
    P("Slow Sweeps", "Two slow clock shapes on the filter and the wave", o(1, "Wavetable", harm=50, timbre=55, morph=30), cords(("CLOCK 1", "CUTOFF", 55), ("CLOCK 2", "O1 MRPH", 60)), clock(1, div="/2", shape="SAW"), clock(2, div="/4", shape="TRI"), cutoff=44, reso=30, vca_mode="ON", release=30),
    # Patch bay
    P("Ring Mod", "OSC 2 ring modulating OSC 1, bell-like. Try OSC 2's SEMI", o(1, "FM 2-Op", **SINE), o(2, "FM 2-Op", semi=7, level=0, **SINE), cords(("OSC 2", "VCA", 100)), cutoff=100, reso=0, keytrack=0, vel_filt=0, attack=1, decay=60, sustain=40, release=45),
    P("Osc FM", "OSC 3 modulating OSC 1's pitch: gritty, stepped FM", o(1, "FM 2-Op", **SINE), o(3, "FM 2-Op", semi=6, level=0, **SINE), cords(("OSC 3", "O1 PITCH", 28)), cutoff=100, reso=0, keytrack=0, vel_filt=0, decay=60, sustain=50, release=40),
    P("Filter Growl", "OSC 2 at audio rate on the cutoff", SAW(1), o(2, "FM 2-Op", semi=-12, level=0, **SINE), cords(("OSC 2", "CUTOFF", 70)), cutoff=44, reso=50, vca_mode="ON", release=30),
    P("Noise Grit", "Noise roughing up the level and the wave", SAW(1, timbre=55), cords(("NOISE", "VCA", 55), ("NOISE", "O1 TIMB", 30)), cutoff=70, vca_mode="ON", release=30),
    P("Self Feedback", "OSC 1 into its own pitch and the filter", o(1, "FM 2-Op", timbre=20), cords(("OSC 1", "O1 PITCH", 18), ("OSC 1", "CUTOFF", 40)), cutoff=52, reso=25, vca_mode="ON", release=30),
    P("Pressure Play", "Velocity, wheel and pressure patched to the filter and wave", SAW(1, timbre=50), cords(("VEL", "CUTOFF", 60), ("WHEEL", "O1 TIMB", 80), ("AT", "CUTOFF", 40)), cutoff=30, reso=30, release=30),
]

# Built around what the newer modules do best. Anything clocked plays while a key is held, and follows the song when it runs.
SONG = {
"Elements": [
    P("Glass Marimba", "Bright struck bars, short and woody", dict(ELEMENTS), e_model="MODAL", e_strike=90, e_mallet=70, e_strike_t=60, e_geom=55, e_bright=70, e_damp=45, e_pos=40, e_space=25, vel_amp=60),
    P("Felt Piano", "A soft-hammered string in a warm room", dict(ELEMENTS), e_model="STRING", e_strike=85, e_mallet=20, e_strike_t=30, e_geom=12, e_bright=35, e_damp=55, e_pos=25, e_space=45, vel_amp=70),
    P("Low Cello", "Bowed low string. Play it slowly", dict(ELEMENTS), o(1, oct=-1), e_model="STRING", e_strike=0, e_bow=85, e_bow_t=40, e_geom=15, e_bright=35, e_damp=75, e_pos=20, e_space=40, e_contour=60),
    P("Temple Bowl", "A huge struck bowl, long ring", dict(ELEMENTS), e_model="MODAL", e_strike=80, e_mallet=30, e_geom=85, e_bright=45, e_damp=20, e_pos=50, e_space=75, release=60),
    P("Night Flute", "Airy blown tube", dict(ELEMENTS), e_model="MODAL", e_strike=0, e_blow=85, e_flow=70, e_blow_t=45, e_geom=8, e_bright=55, e_damp=70, e_space=50),
    P("Harp Cloud", "Each key rings a chord of strings", dict(ELEMENTS), e_model="STRINGS", e_strike=75, e_mallet=35, e_geom=40, e_bright=50, e_damp=55, e_space=65, voices=4),
    P("Rubber Drum", "Tuned, dead and bouncy", dict(ELEMENTS), e_model="MODAL", e_strike=95, e_mallet=15, e_strike_t=30, e_geom=5, e_bright=30, e_damp=35, e_pos=30, e_space=15, drive=38),
    P("Glass Harmonica", "Bowed glass. Pressure brings it forward", dict(ELEMENTS), cords(("AT", "E BOW", 40)), e_model="MODAL", e_strike=0, e_bow=55, e_bow_t=65, e_geom=65, e_bright=70, e_damp=65, e_space=60),
],
"Tides": [
    P("Slow Tide Pad", "Saws washing in and out, each voice on its own", SAW(1), SAW(2, fine=9), cords(("TIDES 1", "CUTOFF", 45), ("TIDES 2", "O2 LEVEL", -30)), t_trig="FREE", t_mode="LOOP", t_rate=30, t_shift=70, cutoff=46, reso=20, attack=70, release=78, vel_amp=15),
    P("Four Winds", "Four related outputs moving four things", o(1, "Wavetable", harm=55, timbre=50, morph=40), o(2, "Wave Terrain", harm=45, fine=6, level=80), cords(("TIDES 1", "O1 TIMB", 40), ("TIDES 2", "O1 MRPH", 40), ("TIDES 3", "O2 TIMB", 40), ("TIDES 4", "CUTOFF", 35)), t_trig="FREE", t_mode="LOOP", t_out="PHASE", t_rate=36, t_shift=80, cutoff=56, attack=62, release=74, vel_amp=15),
    P("Tide Bass", "Each note fires a curved filter sweep", SAW(1, oct=-1), o(2, "Virtual Analog", oct=-2, morph=0, level=60), cords(("TIDES 1", "CUTOFF", 80)), ON, voice_mode="LEGATO", voices=1, glide=6, t_trig="NOTE", t_mode="AD", t_rate=58, t_shape=30, t_slope=30, cutoff=22, reso=35, release=22),
    P("Breathing Swarm", "A swarm that swells on the tide", o(1, "Swarm", harm=60, timbre=50, morph=50), cords(("TIDES 1", "O1 LEVEL", 35), ("TIDES 2", "O1 TIMB", 40)), t_trig="FREE", t_mode="LOOP", t_rate=34, t_shift=70, o1_level=60, cutoff=68, attack=55, release=74, vel_amp=15),
    P("Folded Lead", "Tides pushing the waveshaper", o(1, "Waveshaper", harm=30, timbre=40, morph=50), cords(("TIDES 1", "O1 TIMB", 55)), ON, voice_mode="LEGATO", voices=1, glide=8, t_trig="FREE", t_mode="LOOP", t_rate=54, t_smooth=70, cutoff=78, reso=15, release=30),
    P("Drift Organ", "An organ whose drawbars never quite settle", o(1, "Additive", harm=35, timbre=40, morph=60), cords(("TIDES 1", "O1 TIMB", 22), ("TIDES 2", "O1 HARM", 18)), t_trig="FREE", t_mode="LOOP", t_rate=32, t_shift=65, cutoff=80, release=25, vel_amp=0, vel_filt=0),
    P("Tidal Pluck", "Tides shaping the strike of each note", SAW(1), SAW(2, fine=7), cords(("TIDES 1", "CUTOFF", 75), ("TIDES 1", "VCA", 70)), t_trig="NOTE", t_mode="AD", t_rate=54, t_shape=25, cutoff=30, reso=25, attack=0, decay=68, sustain=0, release=50),
    P("Eighths Motion", "Filter and wave moving in time with the song", o(1, "Wavetable", harm=45, timbre=60, morph=35), SAW(2, oct=-1, level=55), cords(("TIDES 1", "CUTOFF", 55), ("TIDES 2", "O1 MRPH", 35)), ON, t_trig="CLOCK", t_mode="LOOP", t_div="1/8", t_shift=60, cutoff=40, reso=35, release=30),
],
"Clocked": [
    P("Four On The Floor", "Hold a key: kick and offbeat hats", o(1, "Bass Drum", harm=25, timbre=35, morph=40), o(3, "Hi-Hat", harm=30, timbre=60, morph=18), cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "O3 TRIG", 100)), clock(1, div="x1", shape="TRIG"), clock(2, div="x1", shape="TRIG", phase=50), OPEN, voices=2),
    P("Euclid Trio", "Hold a key: three interlocking patterns", o(1, "Bass Drum", harm=25, timbre=35, morph=45), o(2, "Snare Drum", harm=45, timbre=50, morph=40), o(3, "Hi-Hat", harm=30, timbre=60, morph=20), cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "O2 TRIG", 100), ("CLOCK 3", "O3 TRIG", 100)), clock(1, div="x2", shape="TRIG", steps=16, fill=5), clock(2, div="x2", shape="TRIG", steps=16, fill=3, rot=4), clock(3, div="x4", shape="TRIG", steps=16, fill=11, prob=90), OPEN, voices=2),
    P("Broken Beat", "Hold a key: swung, with hits that come and go", o(1, "Bass Drum", harm=30, timbre=40, morph=35), o(2, "Snare Drum", harm=60, timbre=60, morph=35), cords(("CLOCK 1", "O1 TRIG", 100), ("CLOCK 2", "O2 TRIG", 100)), clock(1, div="x4", shape="TRIG", steps=16, fill=6, swing=60, prob=85), clock(2, div="x2", shape="TRIG", steps=8, fill=3, rot=2, swing=60), OPEN, voices=2),
    P("Dub Stabs", "Off-kilter chords re-struck by the clock", o(1, "Chords", harm=30, timbre=40, morph=40), cords(("CLOCK 1", "ENV TRIG", 100)), clock(1, div="x2", shape="TRIG", steps=8, fill=3), decay=46, sustain=0, cutoff=55, reso=25, release=25, voices=4),
    P("Random Melody", "Sample and hold wanders over a plucked tone", o(1, "Virtual Analog", morph=0, timbre=40), cords(("CLOCK 1", "O1 PITCH", 20), ("CLOCK 2", "ENV TRIG", 100)), clock(1, div="x2", shape="S/H"), clock(2, div="x2", shape="TRIG"), decay=48, sustain=10, cutoff=70, release=25, voices=1),
    P("Gated Strings", "A soft 16th chop over strings", o(1, "String Machine", harm=45, timbre=55, morph=50), cords(("CLOCK 1", "VCA", -90)), clock(1, div="x4", shape="GATE", width=55, swing=30), ON, cutoff=72, release=40),
    P("Sidechain Pump", "Every beat ducks a pad, like a compressor", SAW(1), SAW(2, fine=9), cords(("CLOCK 1", "VCA", 90)), clock(1, div="x1", shape="RAMP", level=100, offset=-50), ON, cutoff=62, release=45, drive=25),
    P("Polymeter Pad", "Thirds, fourths and fifths drifting against each other", o(1, "Wavetable", harm=55, timbre=55, morph=40), o(2, "Wave Terrain", harm=45, fine=6, level=80), cords(("CLOCK 1", "O1 TIMB", 35), ("CLOCK 2", "O1 MRPH", 35), ("CLOCK 3", "CUTOFF", 30)), clock(1, div="/3", shape="TRI"), clock(2, div="/4", shape="SINE"), clock(3, div="/5", shape="TRI"), cutoff=52, reso=20, attack=62, release=74, vel_amp=15),
],
"Patched": [
    P("Bell Ring", "Two sines ring modulated: bright metal", o(1, "FM 2-Op", **SINE), o(2, "FM 2-Op", semi=7, fine=10, level=0, **SINE), cords(("OSC 2", "VCA", 100)), cutoff=100, reso=0, keytrack=0, vel_filt=0, decay=72, sustain=0, release=60),
    P("Metal FM", "Stepped FM between oscillators, with an envelope on it", o(1, "FM 2-Op", **SINE), o(3, "FM 2-Op", semi=-5, level=0, **SINE), cords(("OSC 3", "O1 PITCH", 30), ("MOD ENV", "O1 PITCH", 8)), m_decay=52, cutoff=100, reso=0, keytrack=0, vel_filt=0, decay=62, sustain=30, release=40),
    P("Growl Bass", "An oscillator gnawing at the filter", SAW(1, oct=-1), o(2, "FM 2-Op", oct=-2, level=0, **SINE), cords(("OSC 2", "CUTOFF", 45), ("MOD ENV", "CUTOFF", 35)), ON, voice_mode="LEGATO", voices=1, glide=5, m_decay=50, cutoff=26, reso=45, drive=30, release=22),
    P("Noise Snarl", "Noise tearing at the level and the filter", SAW(1), SAW(2, fine=8), cords(("NOISE", "VCA", 40), ("NOISE", "CUTOFF", 20)), ON, cutoff=58, reso=30, release=30),
    P("Feedback Drone", "An oscillator into its own pitch. Move the knobs", o(1, "FM 2-Op", timbre=25, oct=-1), cords(("OSC 1", "O1 PITCH", 14), ("LFO", "O1 TIMB", 20)), ON, lfo_rate=24, cutoff=60, reso=20, attack=45, release=50),
    P("Touch Morph", "Velocity picks the tone, pressure and the wheel move it", o(1, "Wave Terrain", harm=50, timbre=35, morph=30), cords(("VEL", "O1 TIMB", 45), ("AT", "O1 MRPH", 50), ("WHEEL", "CUTOFF", 40)), cutoff=46, reso=25, release=32),
    P("Pluck Over Pad", "OSC 1 struck through its LPG, OSC 2 a soft pad", o(1, "FM 2-Op", harm=40, timbre=30, morph=10, amp="LPG"), o(2, "Virtual Analog", morph=100, level=55, amp="ENV"), lpg_decay=46, lpg_colour=40, attack=62, release=70, cutoff=72, vel_amp=30),
    P("Struck By Osc", "Elements excited by a wavetable, ringing with the note", dict(ELEMENTS), cords(("OSC 2", "E STK IN", 70), ("LFO", "E GEOM", 20)), o(2, "Wavetable", level=0, harm=60, timbre=50, morph_mod=30), lfo_rate=30, e_model="STRING", e_strike=0, e_geom=20, e_damp=65, e_bright=55, e_space=40),
],
}

OFF_VALUES = ("OFF", 0)

OUTPUT = json.load(open(os.path.join(HERE, "preset_output.json")))   # make-up gain, dB, for sounds that full VOLUME cannot bring up to level
PACKS = [pack_dub, pack_classic, pack_rave, pack_techno, pack_space, pack_elements, pack_drums, pack_patch, pack_modular, pack_cross, pack_deep, pack_cinema]   # later packs, one file each, in the order they arrived
PACK_OF = {}
UNMATCHED = {"Upright Dub", "Bowed Metal", "Bowed Glass", "Bowed Lead"}   # bowed and bouncing: their level swings by several dB with the shared noise generator
for pack in PACKS:
    UNMATCHED |= getattr(pack, "UNMATCHED", set())
    for name, note, delta, type_, styles, characters in pack.ENTRIES:
        TAGS[name] = (type_, styles.split(), characters.split())
        PACK_OF[name] = pack.PACK

def analyse(values):
    """What a preset uses and plays with, read from its values so that it cannot be wrong or forgotten."""
    cords = [(values["c%d_src" % n], values["c%d_dst" % n]) for n in range(1, 13) if values["c%d_src" % n] != "OFF" and values["c%d_dst" % n] != "OFF"]
    sources = [src for src, _ in cords]
    oscillators = [n for n in (1, 2, 3) if values["o%d_on" % n] == "ON"]
    elements = values["o1_type"] == "ELEMENTS"
    tides = any(src.startswith("TIDES") for src in sources)
    clocked = any(src.startswith("CLOCK") for src in sources)
    uses = []
    if [n for n in oscillators if not (n == 1 and elements)]: uses.append("Plaits")
    if elements: uses.append("Elements")
    if tides: uses.append("Tides")
    if clocked: uses.append("Clock")
    if cords: uses.append("Patch Bay")
    if values["vca_mode"] == "LPG" or any(values["o%d_amp" % n] == "LPG" for n in oscillators): uses.append("LPG")
    if values["fx_dl_mix"] > 0: uses.append("Delay")
    if values["fx_rv_mix"] > 0: uses.append("Reverb")
    if values["fx_ph_mix"] > 0 or values["fx_ch_mode"] != "OFF": uses.append("Colour FX")
    plays = []
    if clocked: plays.append("Hold key")
    if values["lfo_sync"] == "ON" or clocked or (tides and values["t_trig"] == "CLOCK"): plays.append("Song tempo")
    if values["wheel_lfo"] > 0 or "WHEEL" in sources: plays.append("Wheel")
    if values["at_cutoff"] > 0 or values["at_morph"] > 0 or values["at_lfo"] > 0 or "AFTERTOUCH" in sources: plays.append("Pressure")
    return uses, plays

def filed(name, values, note, unmatched=False):
    assert name in TAGS, "%s has no tags in preset_tags.py" % name
    type_, styles, characters = TAGS[name]
    assert type_ in TYPES, "%s: type %s" % (name, type_)
    assert styles and all(x in STYLES for x in styles), "%s: styles %s" % (name, styles)
    assert characters and all(x in CHARACTERS for x in characters), "%s: character %s" % (name, characters)
    uses, plays = analyse(values)
    entry = {"name": name, "category": type_, "type": type_, "styles": styles, "character": characters, "uses": uses, "plays": plays, "pack": PACK_OF.get(name, "Factory 1"), "note": note}
    if unmatched: entry["unmatched"] = True   # not level-matched: some depend on the shared random generator
    return entry

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
    out = [{"name": "Init", "category": "Init", "type": "Init", "styles": [], "character": [], "uses": ["Plaits"], "plays": [], "pack": "Factory 1", "note": "The starting point", "values": {p["key"]: default(p) for p in params}}]
    seen = {"init"}
    table = []
    for category, entries in LIBRARY.items():
        assert len(entries) == 8, "%s has %d" % (category, len(entries))
        for name, note, delta in entries:
            assert len(name) <= 24 and name.lower() not in seen, name
            seen.add(name.lower())
            values = dict(BASE)
            for k, v in delta.items():
                assert k in values, "%s: %s is not a parameter" % (name, k)
                values[k] = v
            values.update(FX.get(name, {}))
            values["fx_out"] = OUTPUT.get(name, 0)
            if name in trim:
                values["volume"] = trim[name]
            true_to_note(name, TAGS[name][0], values)
            out.append(dict(filed(name, values, note), moved=MOVE.get(name, 0), values={k: values[k] for k in keys}))
            table.append((TAGS[name][0], name, note))
    options = {p["key"]: p.get("options") for p in params}
    ranges = {p["key"]: (p.get("min"), p.get("max")) for p in params}
    aliases = {"VEL": "VELOCITY", "AT": "AFTERTOUCH"}   # the jack labels, for readability above
    checked = [(category, entry) for category, entries in SONG.items() for entry in entries] + [("Testing", entry) for entry in TESTING] + [("Pack", entry[:3]) for pack in PACKS for entry in pack.ENTRIES]
    for category, (name, note, delta) in checked:
        assert len(name) <= 24 and name.lower() not in seen, name
        seen.add(name.lower())
        values = dict(BASE)
        for k, v in delta.items():
            assert k in values, "%s: %s is not a parameter" % (name, k)
            if options[k]:
                v = aliases.get(v, v)
                if isinstance(v, str): v = v.replace(" TIMB", " TIMBRE").replace(" MRPH", " MORPH") if v.endswith((" TIMB", " MRPH")) else v
                assert v in options[k], "%s: %s=%r is not one of %s" % (name, k, v, options[k])
            else:
                assert ranges[k][0] <= v <= ranges[k][1], "%s: %s=%r is out of range" % (name, k, v)
            values[k] = v
        values.update(FX.get(name, {}))
        values["fx_out"] = OUTPUT.get(name, 0)
        if name in trim:
            values["volume"] = trim[name]
        true_to_note(name, TAGS[name][0], values)
        out.append(dict(filed(name, values, note, unmatched=category == "Testing" or name in UNMATCHED), moved=MOVE.get(name, 0), values={k: values[k] for k in keys}))
        table.append((TAGS[name][0], name, note))
    with open(os.path.join(HERE, "presets.json"), "w") as f:
        json.dump({"presets": out}, f, indent=1)
        f.write("\n")
    with open(os.path.join(HERE, "PRESETS.md"), "w") as f:
        f.write("# Millstone presets\n\nGenerated by `make_presets.py`. Init is the default patch; the other %d are below.\n" % len(table))
        for type_ in TYPES:
            f.write("\n## %s\n\n| Preset | Style | Character | Notes |\n|---|---|---|---|\n" % type_)
            for category, name, note in sorted((x for x in table if x[0] == type_), key=lambda x: x[1].lower()):
                f.write("| %s | %s | %s | %s |\n" % (name, ", ".join(TAGS[name][1]), ", ".join(TAGS[name][2]), note))
    for name, delta in FX.items():
        assert name in TAGS, "preset_fx.py names %s, which does not exist" % name
        for k, v in delta.items():
            assert k in options and (v in options[k] if options[k] else ranges[k][0] <= v <= ranges[k][1]), "%s: %s=%r" % (name, k, v)
    unused = set(TAGS) - {e["name"] for e in out}
    assert not unused, "tags for presets that do not exist: %s" % sorted(unused)
    print("wrote %d presets" % len(out))

if __name__ == "__main__":
    main()
