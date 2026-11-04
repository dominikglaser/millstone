#include "Reverb.h"

#include "rings/dsp/fx/reverb.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace
{
// The tank. Rings reserves about 21,600 words of this and declares exactly
// this size, so it is the figure to use.
constexpr std::size_t kTankWords = 32768;

// Short of 1, where the loop would stop decaying.
constexpr float kShortestDecay = 0.3f;
constexpr float kLongestDecay = 0.92f;

// The tank's effective loop length, measured from how long it takes to fall
// 60 dB at a known loop gain. Used only to report the tail.
constexpr float kLoopSeconds = 0.25f;

// How much of the voices reach the tank. The wet level is the mix control's
// job, so this only has to be low enough that the diffusers are not driven
// into their own distortion.
constexpr float kInputGain = 0.5f;
}

struct Reverb::Impl
{
    rings::Reverb reverb;
    std::uint16_t tank[kTankWords] {};
};

Reverb::Reverb() : impl(std::make_unique<Impl>()) {}
Reverb::~Reverb() = default;

void Reverb::prepare(double sampleRate)
{
    // The tank's lengths are fixed in samples, so a higher rate goes round the loop more often. Each trip then has to lose
    // proportionally less for the tail to last the same time.
    trips = static_cast<float>(48000.0 / sampleRate);
    std::memset(impl->tank, 0, sizeof(impl->tank));
    impl->reverb.Init(impl->tank);
    impl->reverb.set_input_gain(kInputGain);
}

void Reverb::reset()
{
    // Init clears the tank's read pointers; the memory itself is cleared here
    // so a stopped transport does not leave a tail waiting to reappear.
    std::memset(impl->tank, 0, sizeof(impl->tank));
    impl->reverb.Init(impl->tank);
    impl->reverb.set_input_gain(kInputGain);
}

namespace
{
float reverbLoopGain(float decay)
{
    return kShortestDecay + std::clamp(decay, 0.0f, 1.0f) * (kLongestDecay - kShortestDecay);
}
}

float reverbTailSeconds(float decay)
{
    return kLoopSeconds * std::log(0.001f) / std::log(reverbLoopGain(decay));
}

float reverbLevelForDecay(float decay)
{
    const float gain = reverbLoopGain(decay);
    return std::sqrt(1.0f - gain * gain);
}

void Reverb::setParameters(float decay, float diffusion, float damping, float newMix)
{
    mix = std::clamp(newMix, 0.0f, 1.0f);

    impl->reverb.set_time(std::pow(reverbLoopGain(decay), trips));
    impl->reverb.set_diffusion(0.2f + std::clamp(diffusion, 0.0f, 1.0f) * 0.5f);

    // Their coefficient runs the other way round from a damping control: near
    // 1 it tracks the input and stays bright, near 0 it lags and goes dark.
    impl->reverb.set_lp(1.0f - std::clamp(damping, 0.0f, 1.0f) * 0.9f);

    impl->reverb.set_amount(mix);
}

void Reverb::process(float* left, float* right, int numFrames)
{
    if (mix <= 0.0f || numFrames <= 0)
        return;

    impl->reverb.Process(left, right, static_cast<std::size_t>(numFrames));
}
