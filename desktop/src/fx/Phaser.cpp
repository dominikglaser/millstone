#include "Phaser.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPi = 3.14159265358979323846;

// The band the notches sweep across. Low enough to be felt, high enough to
// still be a phaser rather than a filter.
constexpr float kLowestHz = 180.0f;
constexpr float kHighestHz = 2200.0f;
}

void Phaser::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void Phaser::reset()
{
    phase = 0.0;
    feedbackSample = 0.0f;

    for (int i = 0; i < kStages; ++i)
        state[i] = previousInput[i] = 0.0f;
}

void Phaser::setParameters(float newRateHz, float newDepth, float newFeedback, float newMix)
{
    rateHz = std::clamp(newRateHz, 0.0f, 20.0f);
    depth = std::clamp(newDepth, 0.0f, 1.0f);

    // Short of 1, or the feedback path would not settle.
    feedback = std::clamp(newFeedback, 0.0f, 0.95f);
    mix = std::clamp(newMix, 0.0f, 1.0f);
}

void Phaser::process(float* samples, int numFrames)
{
    if (mix <= 0.0f)
        return;

    const double increment = static_cast<double>(rateHz) / sampleRate;

    for (int i = 0; i < numFrames; ++i)
    {
        // A sine sweep, in octaves so the movement sounds even across the band.
        const float sweep = 0.5f + 0.5f * static_cast<float>(std::sin(2.0 * kPi * phase)) * depth;
        const float breakHz = kLowestHz * std::pow(kHighestHz / kLowestHz, sweep);

        const float tangent = static_cast<float>(std::tan(kPi * breakHz / sampleRate));
        const float g = (tangent - 1.0f) / (tangent + 1.0f);

        const float dry = samples[i];
        float x = dry + feedback * feedbackSample;

        for (int stage = 0; stage < kStages; ++stage)
        {
            // First-order all-pass: H(z) = (g + z^-1) / (1 + g z^-1)
            const float y = g * x + previousInput[stage] - g * state[stage];
            previousInput[stage] = x;
            state[stage] = y;
            x = y;
        }

        feedbackSample = x;
        samples[i] = dry * (1.0f - mix * 0.5f) + x * mix * 0.5f;

        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;
    }
}
