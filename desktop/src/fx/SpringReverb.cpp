#include "SpringReverb.h"
#include "Saturator.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr double kTwoPi = 6.283185307179586;
constexpr float kChirpLimitHz = 4300.0f;     // where the dispersion is strongest: the stretch that puts the all-pass's peak here
constexpr float kTrackSeconds[3] = { 0.066f, 0.079f, 0.093f };   // round trip, spring by spring
constexpr int kSections[3] = { 100, 120, 140 };
constexpr float kOutputGain = 0.49f;   // held notes come out of the tank 5 dB under the input
constexpr float kKickLevel = 2.6f;
float feedbackFor(float decay) { return 0.5f + 0.42f * std::clamp(decay, 0.0f, 1.0f); }
}

float springTailSeconds(float decay) {
    // A trip is the track plus a low-frequency chirp, a little over 0.1 s at most.
    return 0.13f * std::log(0.001f) / std::log(feedbackFor(decay));
}

void SpringReverb::prepare(double newSampleRate) {
    sampleRate = newSampleRate;
    stretch = std::max(2, static_cast<int>(std::lround(sampleRate / (2.0 * kChirpLimitHz))));
    for (int s = 0; s < kSprings; ++s) {
        auto& spring = springs[s];
        spring.sections = kSections[s];
        spring.history.assign(static_cast<std::size_t>((spring.sections + 1) * stretch), 0.0f);
        spring.loopSamples = kTrackSeconds[s] * static_cast<float>(sampleRate);
        std::size_t size = 16;
        while (size < static_cast<std::size_t>(spring.loopSamples * 1.2f) + 8) size *= 2;
        spring.line.assign(size, 0.0f);
        spring.lineMask = size - 1;
        spring.wobbleStep = static_cast<float>(kTwoPi * (0.31 + 0.17 * s) / sampleRate);
    }
    lowCoef = static_cast<float>(1.0 - std::exp(-kTwoPi * 120.0 / sampleRate));
    kickDecay = static_cast<float>(std::exp(-1.0 / (0.03 * sampleRate)));
    kickStep = static_cast<float>(kTwoPi * 55.0 / sampleRate);
    reset();
}

void SpringReverb::reset() {
    for (auto& spring : springs) {
        std::fill(spring.history.begin(), spring.history.end(), 0.0f);
        std::fill(spring.line.begin(), spring.line.end(), 0.0f);
        spring.lineWrite = 0;
        spring.lp1 = spring.lp2 = 0.0f;
    }
    historyIndex = 0;
    inputLow = 0.0f;
    kickEnvelope = 0.0f;
}

void SpringReverb::setParameters(float decay, float drip, float tone) {
    feedback = feedbackFor(decay);
    outputLevel = kOutputGain * std::sqrt(1.0f - feedback * feedback);   // held notes at the same level at any DECAY
    allpass = 0.45f + 0.3f * std::clamp(drip, 0.0f, 1.0f);
    const float cutoff = 6500.0f * std::pow(0.2f, std::clamp(tone, 0.0f, 1.0f));
    toneCoef = static_cast<float>(1.0 - std::exp(-kTwoPi * cutoff / sampleRate));
}

void SpringReverb::kick() {
    kickEnvelope = 1.0f;
    kickPhase = 0.0f;
}

void SpringReverb::process(const float* left, const float* right, float* outLeft, float* outRight, int numFrames) {
    const float a = allpass;
    for (int i = 0; i < numFrames; ++i) {
        // The transducer: no real bass gets into a spring, and a hot signal hits the limit of its travel.
        const float in = 0.5f * (left[i] + right[i]);
        inputLow += (in - inputLow) * lowCoef;
        float drive = in - inputLow;
        if (kickEnvelope > 0.0001f) {
            noiseState = noiseState * 1664525u + 1013904223u;
            const float noise = static_cast<float>(noiseState >> 16) / 32768.0f - 1.0f;
            drive += kKickLevel * kickEnvelope * (0.7f * noise + std::sin(kickPhase));
            kickPhase += kickStep;
            kickEnvelope *= kickDecay;
        }
        drive = softClip(drive * 1.4f);

        float output[kSprings];
        for (int s = 0; s < kSprings; ++s) {
            auto& spring = springs[s];
            const auto read = [&spring](float delay) {
                const float exact = static_cast<float>(spring.lineWrite) - delay + static_cast<float>(spring.lineMask + 1);
                const std::size_t whole = static_cast<std::size_t>(exact);
                const float t = exact - static_cast<float>(whole);
                const float x0 = spring.line[whole & spring.lineMask], x1 = spring.line[(whole + 1) & spring.lineMask];
                return x0 + t * (x1 - x0);
            };
            spring.wobble += spring.wobbleStep;
            if (spring.wobble > kTwoPi) spring.wobble -= kTwoPi;
            const float wobble = static_cast<float>(std::sin(spring.wobble)) * spring.loopSamples * 0.0025f;

            // The first echo has only come one way down the spring; after that it is there and back each time.
            output[s] = read(spring.loopSamples * 0.5f + wobble);

            float x = drive + feedback * read(spring.loopSamples + wobble);
            spring.lp1 += (x - spring.lp1) * toneCoef;
            spring.lp2 += (spring.lp1 - spring.lp2) * toneCoef;
            x = spring.lp2;

            // The chirp. Each section's output is the next one's input, so a row of history serves both.
            const int rows = spring.sections;
            float* row = spring.history.data() + historyIndex;
            for (int n = 0; n < rows; ++n, row += stretch) {
                const float y = a * (x - row[stretch]) + row[0];
                row[0] = x;
                x = y;
            }
            row[0] = x;

            spring.line[spring.lineWrite] = x;
            spring.lineWrite = (spring.lineWrite + 1) & spring.lineMask;
        }
        if (++historyIndex >= stretch) historyIndex = 0;

        outLeft[i] = outputLevel * (output[0] + 0.6f * output[1]);
        outRight[i] = outputLevel * (output[2] + 0.6f * output[1]);
    }
}
