#pragma once

#include "DelayLine.h"

// The three settings a Juno has: two switches, either or both.
enum class ChorusMode
{
    off,
    one,
    two,
    oneAndTwo
};

// A Juno-style chorus, and the stage that makes the instrument stereo.
//
// The character of this effect is its width, which comes from modulating two
// short delays in antiphase and sending one to each side. The rates and depths
// below are chosen to sit where the three Juno settings sit -- slow and wide,
// a little faster, then fast and shallow -- rather than being measured from
// hardware, so treat them as in the spirit of it rather than a model of it.
class Chorus
{
public:
    void prepare(double sampleRate);
    void reset();

    void setMode(ChorusMode newMode);
    void setMix(float newMix);

    // Mono in, stereo out. Writes rather than adds.
    void process(const float* input, float* left, float* right, int numFrames);

private:
    double sampleRate = 48000.0;
    ChorusMode mode = ChorusMode::off;
    float mix = 1.0f;

    float rateHz = 0.5f;
    float depthSamples = 0.0f;
    float centreSamples = 0.0f;

    double phase = 0.0;
    DelayLine lines[2];
};
