"""A sprinkle of the effects on the first library. Sparing on purpose: most of these sit under the sound rather than on top of it, and
basses and most drums stay dry, because that is where a mix wants them. Anything not listed has every effect off."""

def echo(div, fb, mix, low=30, high=65, drive=25, wow=15, duck=0, pong=False, width=100, heads="1"):
    return dict(fx_dl_sync="SYNC", fx_dl_div=div, fx_dl_fb=fb, fx_dl_mix=mix, fx_dl_low=low, fx_dl_high=high, fx_dl_drive=drive, fx_dl_wow=wow,
                fx_dl_duck=duck, fx_dl_pp="ON" if pong else "OFF", fx_dl_width=width, fx_dl_heads=heads)

def plate(mix, decay=50, diffusion=65, damping=35):
    return dict(fx_rv_type="NORMAL", fx_rv_mix=mix, fx_rv_decay=decay, fx_rv_diff=diffusion, fx_rv_damp=damping)

def spring(mix, decay=50, drip=70, damping=30):
    return dict(fx_rv_type="SPRING", fx_rv_mix=mix, fx_rv_decay=decay, fx_rv_diff=drip, fx_rv_damp=damping)

def chorus(mode, mix=100):
    return dict(fx_ch_mode=mode, fx_ch_mix=mix)

def phaser(mix, rate=30, depth=70, feedback=30):
    return dict(fx_ph_mix=mix, fx_ph_rate=rate, fx_ph_depth=depth, fx_ph_fb=feedback)

def both(*parts):
    out = {}
    for part in parts: out.update(part)
    return out

FX = {
    # Dub and reggae: tape and spring.
    "Dub Stabs": both(echo("1/8 D", 55, 28, low=35, high=60, drive=30, wow=20), spring(18)),
    "Reed Organ": spring(14),
    "Soft Organ": both(chorus("I", 60), spring(10)),
    "Drift Organ": spring(12),
    "LPG Pluck": both(echo("1/8 D", 50, 22, low=35, high=60, wow=20), spring(12)),
    "Snare Crack": spring(18, decay=40),
    "Rubber Drum": echo("1/8 D", 40, 18, low=40, high=60, wow=20),
    "Laser": both(echo("1/8 D", 70, 30, low=35, high=60, drive=35, wow=25), spring(15)),
    "Whistle": echo("1/4 T", 55, 25, low=35, high=58, wow=25),
    "Alarm": spring(15),
    "Sonar Ping": both(echo("1/4 D", 60, 28, pong=True), plate(25, decay=65)),
    "Downer": echo("1/8 D", 50, 20, low=35, high=55),
    "Broken Beat": spring(12, decay=40),
    "Kalimba": echo("1/8 D", 40, 15, low=35),

    # Keys: the chorus they would have had.
    "Tine Piano": chorus("I", 70),
    "Mellow Poly": both(chorus("II", 80), plate(15)),
    "DX Keys": both(chorus("I", 50), plate(12)),
    "Chord Memory": both(echo("1/8 D", 40, 18, low=45, duck=30), plate(15)),
    "Funk Clav": phaser(35, rate=35),
    "Felt Piano": plate(14, decay=40, damping=55),
    "Ring Mod": spring(10),

    # Pads: width and a room.
    "Warm Strings": both(chorus("II", 100), plate(20)),
    "Analog Pad": both(chorus("I", 100), plate(22)),
    "Swarm Pad": plate(20),
    "Glass Pad": plate(28, decay=70),
    "Formant Choir": plate(25, decay=70),
    "Chord Cloud": both(echo("1/4 D", 40, 15), plate(22)),
    "Slow Tide Pad": both(chorus("I", 60), plate(18)),
    "Polymeter Pad": plate(20),
    "Gated Strings": chorus("II", 70),
    "Pluck Over Pad": plate(18),
    "Low Cello": plate(16, decay=55, damping=50),
    "Glass Harmonica": plate(22, decay=65),

    # Leads: a repeat behind them.
    "Classic Lead": echo("1/8 D", 35, 16),
    "Square Hero": echo("1/4", 30, 15),
    "Glass FM": both(echo("1/8", 30, 12), plate(18)),
    "Wave Solo": echo("1/8 D", 35, 16, pong=True, width=70),
    "Night Flute": both(echo("1/4 D", 35, 14), plate(24, decay=60)),
    "Formant Cry": plate(20),

    # Plucks and bells.
    "Harp String": plate(15),
    "Modal Bell": both(echo("1/8 D", 35, 15), plate(20)),
    "Synth Pluck": echo("1/8 D", 45, 20, pong=True, width=70),
    "FM Pluck": echo("1/8", 35, 15),
    "LPG Marimba": plate(12),
    "Harp Cloud": plate(25, decay=65),
    "Glass Marimba": plate(12),
    "Struck Bell": plate(18),

    # Sequences.
    "Random Melody": both(echo("1/8 D", 45, 22), plate(15)),
    "Euclid Trio": echo("1/8 D", 30, 12),
    "Trance Gate": both(echo("1/8 D", 35, 15, pong=True, width=80), plate(12)),
    "Random Bleeps": echo("1/16 D", 35, 18, pong=True),
    "Metal Perc": echo("1/16 D", 35, 15, pong=True),

    # Textures and drones.
    "Riser": both(echo("1/8 D", 40, 15), plate(25, decay=65)),
    "Noise Wash": plate(20),
    "Hollow Sweep": phaser(40, rate=20, depth=80),
    "Swarm Motion": phaser(30, rate=25),
    "Granular Air": plate(25, decay=65),
    "Dark Drone": plate(20, decay=70, damping=55),
    "Temple Bowl": plate(18, decay=65),
}
