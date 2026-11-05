"""What preset definitions are written in, shared by make_presets.py and the packs."""

def o(n, model=None, **kw):
    out = {"o%d_on" % n: "ON"}
    if model: out["o%d_engine" % n] = model
    out.update({"o%d_%s" % (n, k): v for k, v in kw.items()})
    return out

# Virtual Analog's morph=100 is a thin notched wave beside a square-ish voice, not a saw: measured, it has no even harmonics to speak of.
# VA VCF has real waveforms, and its own filter sits wide open at timbre=100.
def SAW(n, timbre=100, **kw):
    return o(n, "VA VCF", harm=50, timbre=timbre, morph=25, **kw)

def SQUARE(n, timbre=100, **kw):
    return o(n, "VA VCF", harm=50, timbre=timbre, morph=50, **kw)

def SOFT(n, timbre=0, **kw):   # close to a triangle: for subs and flutes
    return o(n, "Virtual Analog", timbre=timbre, morph=0, **kw)

def P(name, note, *oscs, **kw):
    d = {}
    for x in oscs: d.update(x)
    d.update(kw)
    return name, note, d

MONO = dict(voice_mode="MONO", voices=1)
LEGATO = dict(voice_mode="LEGATO", voices=1)
DRUM = dict(vca_mode="ON", cutoff=100, reso=0, keytrack=0, vel_amp=70, vel_filt=0, release=40)
VIB = dict(lfo_rate=62, wheel_lfo=100)   # about 6.5 Hz, on the mod wheel

def cords(*patches):
    d = {}
    for i, (source, dest, amount) in enumerate(patches):
        d["c%d_src" % (i + 1)] = source
        d["c%d_dst" % (i + 1)] = dest
        d["c%d_amt" % (i + 1)] = amount
    return d

def clock(channel, **kw):
    return {"k%d_%s" % (channel, k): v for k, v in kw.items()}

ELEMENTS = dict(o1_type="ELEMENTS", vca_mode="ON", cutoff=100, reso=0, keytrack=0, vel_filt=0, release=45)
SINE = dict(timbre=0)

ON = dict(vca_mode="ON", release=40)
OPEN = dict(ON, cutoff=100, reso=0, keytrack=0, vel_filt=0)

# For the packs built around the patch bay: every one has a cord and an effect, or it is in the wrong pack.
def Lab(name, note, type_, styles, characters, *parts, **kw):
    delta = P(name, note, *parts, **kw)[2]
    assert any(k.endswith("_src") for k in delta), name + " has no cord"
    assert any(k in delta for k in ("fx_dl_mix", "fx_rv_mix", "fx_ph_mix", "fx_ch_mode")), name + " has no effect"
    return name, note, delta, type_, styles, characters

def L1(rate=45, shape="TRI", **kw):
    return dict(lfo_rate=rate, lfo_shape=shape, **{"lfo_" + k: v for k, v in kw.items()})

def L2(rate=45, shape="TRI", **kw):
    return dict(lfo2_rate=rate, lfo2_shape=shape, **{"lfo2_" + k: v for k, v in kw.items()})

def Tid(rate, mode="LOOP", out="PHASE", trig="FREE", **kw):
    return dict(t_trig=trig, t_mode=mode, t_out=out, t_rate=rate, **{"t_" + k: v for k, v in kw.items()})

HIT = dict(attack=0, sustain=0, release=20)
NOISE_ONLY = dict(o1_on="OFF", o2_on="OFF", o3_on="OFF")
