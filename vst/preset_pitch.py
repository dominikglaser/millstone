"""Makes a preset play the note you press, without changing how it sounds: C4 plays C4, and a bass is transposed down by the player.

MOVE is how many semitones each preset's oscillators go up or down. The filter follows the key, not the oscillators, so the
sound only stays the same if CUTOFF moves against KEYTRACK by the same amount; a preset then renders the same as before, played
that many semitones away from its old key. A bass has its lowest oscillator you can hear on the note (a sub more than 15 dB down is colour, not the note); anything else has
the pitch heard most on the note.

Basses are moved here rather than rewritten in their packs so that this stays one reviewable step. New presets just start on the note."""

MOVE = {
    # basses with a sub: the sub plays the note, the rest sit above it
    **dict.fromkeys(["Round Sub", "Tide Bass", "Growl Bass", "Pressure Sub", "Taurus Pedal", "Audio Rate Growl", "Tide Bass Line",
                     "Plucked Elements Bass", "Three Osc Fifths", "Gated Offbeat Bass", "Rolling Bass", "Offbeat Bass"], 24),
    **dict.fromkeys(["FM Slap", "Tempo Growl", "Pluck Bass", "Modal Thump", "Filter Growl", "Steppers Sub", "Square Bass", "Velocity Bite",
                     "Clock Pump Bass", "Deep Hoover", "Tide Step Bass",
                     "Roots Bass", "Rubber Dub", "Upright Dub", "Dread Growl", "Saw Bass", "Funk Slap", "Eighty One", "Neuro Reese",
                     "Jungle Sub", "Drive Bass", "Terrain Bass", "Struck Bass", "Wheel Wobble", "Ring Sub", "Pitch Drop Bass",
                     "Sample Hold Bass", "Formant Bass", "Wavetable Sweep Bass", "Noise Ripped Bass", "Slow Tide Sub"], 12),
    # drones and low pads
    **dict.fromkeys(["Dark Drone", "Deep Space", "Tide Drone", "Bowed Abyss", "Stone Choir", "Wave Monk", "Organ Pedal Drone"], 24),
    **dict.fromkeys(["Feedback Drone", "Warning Drone", "Feedback Choir", "Power Hum", "Ring Drone", "Clock Monolith", "Tape Drone",
                     "Scorched Earth", "Low Cello", "Submerged", "Solo Cello", "Hoover", "Braam", "DX Keys", "Six Op Tines",
                     "Six Op Wheel", "String Section"], 12),
    # sounding above the key
    **dict.fromkeys(["Celeste", "Shimmer Wall", "Frozen Lake", "Machine Strings", "Chord Cloud", "Wave Sequence", "Chip Arp"], -12),
    "Glass Halo": -36,
    "Rave Stab": -24,
    "Wave Solo": -19,
    "Chip Lead": -7,   # its arpeggio sat on G, F and D: this makes it C, Bb and G
    # struck bars, whose lowest partial is not where the oscillator says: that partial goes on the note
    "Toy Piano": -10, "Music Box": -6,
    "Vibes": 1, "Modal Bell": 1, "Euclid Melody": 1, "Speech Lead": 1,
}

# Cents, measured with the effects off, for models and patches that land a little off the note: feedback cords add an offset,
# the bars above land between semitones, and Elements' strings run slightly sharp.
FINE = {
    "Osc FM": -52, "DX Keys": -18, "Six Op Wheel": -18, "Self Feedback": 16, "Feedback Drone": 10, "Note Envelope": 14,
    "Tide Pluck Harp": 15, "Tidal Pluck": 8, "Tape Stop Pluck": -9, "Fired By Clock": 15, "Tide Step Bass": 13,
    "Chord Strings": -6, "Plucked Harp": -6, "Glass Strum": -6, "String Section": -6, "Nylon": -6,
    "Music Box": -5, "Toy Piano": 22, "Vibes": 46, "Modal Bell": -27,
    "Euclid Melody": -26, "Speech Lead": 44,
}

# These needed their sound changed, not just their octave. Plaits' Additive TIMBRE sets the loudest harmonic, so at 40 an organ
# was almost all 10th harmonic; low TIMBRE keeps the fundamental, and HARMONICS and MORPH set how bright it is above that.
VOICE = {
    "Soft Organ": dict(o1_harm=30, o1_timbre=8, o1_morph=25),
    "Drift Organ": dict(o1_harm=30, o1_timbre=12, o1_morph=25, c1_amt=6, c2_amt=6),   # its Tides cords, scaled to the lower TIMBRE
    "Bubble Organ": dict(o1_harm=15, o1_timbre=4, o1_morph=25),
    "Sound System Organ": dict(o1_harm=30, o1_timbre=8, o1_morph=0),
    "House Organ": dict(o1_harm=15, o1_timbre=8, o1_morph=0),
    "Organ Donor": dict(o1_harm=0, o1_timbre=16, o1_morph=0),
    "Velocity Organ": dict(o1_harm=30, o1_timbre=4, o1_morph=25, c1_amt=10),   # and its velocity cord
    "Hand Organ": dict(o1_harm=45, o1_timbre=12, o1_morph=0),
    "Cathedral": dict(o1_harm=30, o1_timbre=16, o1_morph=0),
    "Velocity Space": dict(o1_harm=45, o1_timbre=4, o1_morph=25, c1_amt=10),
    "Organ Pedal Drone": dict(o1_harm=30, o1_timbre=4, o1_morph=0),
    # Elements' blown resonator only lands on the note at some geometries
    "Night Flute": dict(e_geom=0), "Bamboo Flute": dict(e_geom=0), "Flute And Strings": dict(e_geom=0), "Blown Whistle": dict(e_geom=30),
    # A resonant filter with no oscillator: its pitch is the cutoff
    "Self Oscillating Lead": dict(cutoff=39),
}

# Sounds that are not meant to sit on the key: drums are chosen by ear at a key, and risers and textures move or have no pitch.
NOT_PITCHED = {"Init", "Perc", "Riser/FX", "Texture"}


def true_to_note(name, category, values, keys_on=("o1", "o2", "o3")):
    values.update(VOICE.get(name, {}))
    on = [o for o in keys_on if values[o + "_on"] == "ON"]
    move = MOVE.get(name, 0)
    if move:
        for o in on:
            total = values[o + "_oct"] * 12 + values[o + "_semi"] + move
            values[o + "_oct"], values[o + "_semi"] = _split(total, values[o + "_semi"])
        values["cutoff"] = round(values["cutoff"] + values["keytrack"] / 100 * move / 12 * 10, 1)
        assert 0 <= values["cutoff"] <= 100, "%s: cutoff %s" % (name, values["cutoff"])
    if category in NOT_PITCHED:
        return
    # A detuned pair should sit either side of the note, not above it. The beating between them does not change.
    heard = [o for o in on if values[o + "_level"] > 0]
    if heard:
        loudest = max(heard, key=lambda o: values[o + "_level"])
        pitch = lambda o: values[o + "_oct"] * 12 + values[o + "_semi"]
        group = [o for o in heard if pitch(o) == pitch(loudest)]
        weight = sum(values[o + "_level"] ** 2 for o in group)
        centre = sum(values[o + "_fine"] * values[o + "_level"] ** 2 for o in group) / weight
        if abs(centre) >= 2:
            for o in on:
                values[o + "_fine"] = round(values[o + "_fine"] - centre, 1)
    for o in on:
        values[o + "_fine"] = round(values[o + "_fine"] + FINE.get(name, 0), 1)


def _split(total, semi):
    """Keeps an oscillator's own semitone setting where it can, so a preset's intervals still read as they were written."""
    oct_ = (total - semi) / 12
    if oct_ == int(oct_) and -2 <= oct_ <= 2:
        return int(oct_), semi
    oct_ = max(-2, min(2, round(total / 12)))
    semi = total - oct_ * 12
    assert -12 <= semi <= 12, "out of range: %d semitones" % total
    return oct_, semi
