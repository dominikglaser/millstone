#pragma once

#include <memory>

// How long the reverb keeps ringing after its input stops, to 60 dB down. Its
// loop gain runs from 0.3 to kLongestDecay, so the ring is the loop length
// times ln(0.001)/ln(gain); the loop length is measured. The prediction bounds
// every measured tail from above -- 1.43 s against 1.42 measured at decay 0,
// 3.35 against 3.16 at 0.5, 20.7 against 15.5 at 1 -- which is the safe
// direction for a figure a bounce depends on.
float reverbTailSeconds(float decay);

// What to multiply the tank's output by so that held notes come out at the same level at any DECAY. A longer loop stores more
// energy, as 1 / (1 - gain^2), and without this a long tail is many times louder than a short one.
float reverbLevelForDecay(float decay);

// Reverb, wrapping Mutable Instruments' plate from Rings.
//
// A pimpl for the same reason PlaitsVoice is one: it keeps the MI headers in a
// single translation unit. Theirs rather than ours because it is a complete
// Griesinger topology -- four input diffusers into a loop of two allpass pairs
// and two long modulated delays -- already proven in shipping hardware, with
// exactly the controls worth having.
//
// One thing to know about it: its delay lengths are compile-time constants in
// samples. The decay is corrected for the host's sample rate, so the tail lasts
// as long, but its density and brightness still shift a little with the rate.
class Reverb
{
public:
    Reverb();
    ~Reverb();

    void prepare(double sampleRate);
    void reset();

    // decay, diffusion and damping all run 0 to 1. Mix is a dry/wet crossfade,
    // and at zero the reverb is skipped entirely.
    void setParameters(float decay, float diffusion, float damping, float mix);

    void process(float* left, float* right, int numFrames);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    float mix = 0.0f;
    float trips = 1.0f;
};
