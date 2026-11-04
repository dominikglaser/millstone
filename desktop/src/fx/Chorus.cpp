#include "Chorus.h"

#include <cstddef>

namespace
{
// Centre delay and sweep for each setting, in milliseconds.
struct ChorusSetting
{
    float rateHz;
    float centreMs;
    float depthMs;
};

constexpr ChorusSetting kSettings[] = {
    { 0.00f, 3.0f, 0.0f },  // off
    { 0.50f, 3.2f, 1.1f },  // I: slow and wide
    { 0.83f, 3.2f, 1.7f },  // II: a little faster and wider
    { 8.50f, 2.6f, 0.35f }  // I+II: fast and shallow, the nervous one
};

constexpr float kMaximumDelaySeconds = 0.02f;
}

void Chorus::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;

    for (auto& line : lines)
        line.prepare(sampleRate, kMaximumDelaySeconds);

    setMode(mode);
    reset();
}

void Chorus::reset()
{
    phase = 0.0;

    for (auto& line : lines)
        line.reset();
}

void Chorus::setMode(ChorusMode newMode)
{
    mode = newMode;

    const auto& setting = kSettings[static_cast<std::size_t>(mode)];
    rateHz = setting.rateHz;
    centreSamples = setting.centreMs * 0.001f * static_cast<float>(sampleRate);
    depthSamples = setting.depthMs * 0.001f * static_cast<float>(sampleRate);
}

void Chorus::setMix(float newMix)
{
    mix = std::clamp(newMix, 0.0f, 1.0f);
}

void Chorus::process(const float* input, float* left, float* right, int numFrames)
{
    if (mode == ChorusMode::off || mix <= 0.0f)
    {
        // Still stereo, just the same on both sides.
        for (int i = 0; i < numFrames; ++i)
            left[i] = right[i] = input[i];

        return;
    }

    const double increment = static_cast<double>(rateHz) / sampleRate;
    const float dry = 1.0f - mix * 0.5f;   // the wet side never fully replaces the dry
    const float wet = mix * 0.5f;

    for (int i = 0; i < numFrames; ++i)
    {
        // A triangle, which is what a bucket-brigade chorus was swept with, and
        // the two sides read it half a cycle apart.
        const float sweep = 1.0f - 4.0f * std::abs(static_cast<float>(phase) - 0.5f);

        const float sample = input[i];
        lines[0].write(sample);
        lines[1].write(sample);

        const float a = lines[0].read(centreSamples + depthSamples * sweep);
        const float b = lines[1].read(centreSamples - depthSamples * sweep);

        left[i] = dry * sample + wet * a;
        right[i] = dry * sample + wet * b;

        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;
    }
}
