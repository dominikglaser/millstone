#pragma once

// A phaser: a cascade of first-order all-pass sections whose break frequency is
// swept, mixed back with the dry signal. The all-passes leave the magnitude
// alone and only move phase, so the sweeping notches come entirely from the
// interference between wet and dry -- which is why the mix control is not
// optional here the way it is on most effects.
class Phaser
{
public:
    void prepare(double sampleRate);
    void reset();

    void setParameters(float rateHz, float depth, float feedback, float mix);

    // In place, mono, ahead of the chorus.
    void process(float* samples, int numFrames);

private:
    static constexpr int kStages = 6;

    double sampleRate = 48000.0;
    float rateHz = 0.4f;
    float depth = 0.7f;
    float feedback = 0.3f;
    float mix = 0.0f;

    double phase = 0.0;
    float state[kStages] {};
    float previousInput[kStages] {};
    float feedbackSample = 0.0f;
};
