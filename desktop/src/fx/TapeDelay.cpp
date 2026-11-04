#include "TapeDelay.h"
#include "Saturator.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr double kTwoPi = 6.283185307179586;
constexpr float kTailCapSeconds = 60.0f;

float onePoleCoef(float seconds, double sampleRate) {
    return static_cast<float>(1.0 - std::exp(-1.0 / (seconds * sampleRate)));
}
int longestHead(int heads) { return (heads & 4) ? 3 : (heads & 2) ? 2 : 1; }
}

float tapeDelayTailSeconds(float timeSeconds, float repeats, int heads) {
    const float longest = std::min(timeSeconds * static_cast<float>(longestHead(heads)), TapeDelay::kLongestSeconds);
    if (repeats >= 0.98f) return kTailCapSeconds;   // saturation, not the gain, is what ends it near here
    if (repeats <= 0.0f) return longest;
    return std::min(kTailCapSeconds, longest * std::ceil(std::log(0.001f) / std::log(repeats)));
}

void TapeDelay::Line::prepare(std::size_t length) {
    std::size_t size = 16;
    while (size < length) size *= 2;
    samples.assign(size, 0.0f);
    mask = size - 1;
    writeIndex = 0;
}

void TapeDelay::Line::clear() {
    std::fill(samples.begin(), samples.end(), 0.0f);
    writeIndex = 0;
}

// Cubic, because this read point moves all the time and repeats go through it many times over.
float TapeDelay::Line::read(float delaySamples) const {
    delaySamples = std::clamp(delaySamples, 2.0f, static_cast<float>(mask - 3));
    const float exact = static_cast<float>(writeIndex) - delaySamples + static_cast<float>(mask + 1);
    const std::size_t whole = static_cast<std::size_t>(exact);
    const float t = exact - static_cast<float>(whole);
    const float a = samples[(whole - 1) & mask], b = samples[whole & mask], c = samples[(whole + 1) & mask], d = samples[(whole + 2) & mask];
    return b + 0.5f * t * (c - a + t * (2.0f * a - 5.0f * b + 4.0f * c - d + t * (3.0f * (b - c) + d - a)));
}

void TapeDelay::prepare(double newSampleRate) {
    sampleRate = newSampleRate;
    const auto length = static_cast<std::size_t>(sampleRate * kLongestSeconds) + 8;
    for (auto& line : lines) line.prepare(length);
    smoothFast = onePoleCoef(0.006f, sampleRate);
    smoothTape = onePoleCoef(0.18f, sampleRate);
    attackCoef = onePoleCoef(0.004f, sampleRate);
    releaseCoef = onePoleCoef(0.22f, sampleRate);
    setParameters(parameters);
    reset();
}

void TapeDelay::reset() {
    for (auto& line : lines) line.clear();
    for (auto& tone : tones) tone = Tone {};
    snapTape = true;
    duckEnvelope = 0;
}

void TapeDelay::setParameters(const Parameters& p) {
    parameters = p;
    parameters.repeats = std::clamp(parameters.repeats, 0.0f, kStrongestRepeats);
    parameters.mix = std::clamp(parameters.mix, 0.0f, 1.0f);
    parameters.heads = parameters.heads & 7 ? parameters.heads & 7 : 1;
    const auto design = [this](Coefs& c, float hz) {
        static constexpr float damping[2] = { 0.7654f, 1.8478f };   // 1/Q of a fourth-order Butterworth's two sections
        const float g = std::tan(static_cast<float>(kTwoPi * 0.5) * std::min(hz, static_cast<float>(sampleRate) * 0.45f) / static_cast<float>(sampleRate));
        for (int n = 0; n < 2; ++n) {
            c.k[n] = damping[n];
            c.a1[n] = 1.0f / (1.0f + g * (g + damping[n]));
            c.a2[n] = g * c.a1[n];
            c.a3[n] = g * c.a2[n];
        }
    };
    design(lowPass, parameters.highCutHz);
    design(highPass, parameters.lowCutHz);
}

void TapeDelay::process(float* left, float* right, int numFrames) {
    if (!isActive()) return;

    const float sr = static_cast<float>(sampleRate);
    const float target = std::clamp(parameters.timeSeconds, 0.005f, kLongestSeconds) * sr;
    const float maxDelay = kLongestSeconds * sr;
    if (snapTape) { inertia = position = target; snapTape = false; }

    // The heads move with the tape, so the farthest one moves fastest. Holding it under the speed of the tape itself means
    // a twist of TIME bends everything's pitch but nothing plays backwards.
    const float sendTarget = parameters.throwOn ? 1.0f : std::clamp(parameters.send, 0.0f, 1.0f);
    if (mixNow <= 0.0f) sendNow = sendTarget;   // starting up: nothing to fade from, and a note must not slip through
    const float slew = 0.9f / static_cast<float>(longestHead(parameters.heads));

    const float drive = std::clamp(parameters.drive, 0.0f, 1.0f);
    const float gain = 1.0f + 5.0f * drive, ceiling = 1.2f / gain, bias = 0.12f * drive, biasOut = softClip(bias);
    // Enough to hold the echo level roughly steady for a normal signal as it saturates. DRIVE is a colour, not a volume.
    const float makeup = std::sqrt(std::sqrt(gain));
    const auto record = [=](float x) { return ceiling * (softClip(x / ceiling + bias) - biasOut); };

    const float repeats = parameters.repeats;
    const float wow = std::clamp(parameters.wow, 0.0f, 1.0f);
    const float duck = std::clamp(parameters.duck, 0.0f, 1.0f);
    const double wowStep = 0.53 / sampleRate, wow2Step = 1.27 / sampleRate, flutterStep = 6.6 / sampleRate;
    const float mixStep = 1.0f / (0.012f * sr);

    for (int i = 0; i < numFrames; ++i) {
        inertia += (target - inertia) * smoothTape;
        position += std::clamp(inertia - position, -slew, slew);
        sendNow += (sendTarget - sendNow) * smoothFast;
        mixNow += std::clamp(parameters.mix - mixNow, -mixStep, mixStep);

        // Slow wander, which grows with the length of tape in the path, and a small fast flutter that does not.
        float shift = 0.0f;
        if (wow > 0.0f) {
            if (--driftCountdown <= 0) {
                noiseState = noiseState * 1664525u + 1013904223u;
                driftTarget = static_cast<float>(noiseState >> 16) / 32768.0f - 1.0f;
                driftCountdown = static_cast<int>(sampleRate * 0.4);
            }
            drift += (driftTarget - drift) * 0.00002f;
            wowPhase += wowStep; if (wowPhase >= 1.0) wowPhase -= 1.0;
            wowPhase2 += wow2Step; if (wowPhase2 >= 1.0) wowPhase2 -= 1.0;
            flutterPhase += flutterStep; if (flutterPhase >= 1.0) flutterPhase -= 1.0;
            const float slow = 0.65f * static_cast<float>(std::sin(kTwoPi * wowPhase)) + 0.25f * static_cast<float>(std::sin(kTwoPi * wowPhase2)) + 0.4f * drift;
            const float fast = static_cast<float>(std::sin(kTwoPi * flutterPhase));
            shift = wow * (slow * (0.0012f * sr + 0.0025f * position) + fast * 0.00012f * sr);
        }

        float summed[2] = { 0.0f, 0.0f }, total = 0.0f;
        for (int h = 0; h < 3; ++h) {
            const float want = (parameters.heads >> h) & 1 ? 1.0f : 0.0f;
            headGain[h] += (want - headGain[h]) * smoothFast;
            if (headGain[h] < 0.0005f) continue;
            total += headGain[h];
            const float at = std::min(position * static_cast<float>(h + 1) + shift, maxDelay);
            summed[0] += headGain[h] * lines[0].read(at);
            summed[1] += headGain[h] * lines[1].read(at);
        }
        // Several heads must not raise the loop gain, or their sum would run away before REPEATS reaches 100%.
        const float loopScale = total > 1.0f ? 1.0f / total : 1.0f;
        const float outScale = total > 1.0f ? 1.0f / std::sqrt(total) : 1.0f;

        for (int c = 0; c < 2; ++c) {
            auto& tone = tones[c];
            float x = summed[c];
            for (int n = 0; n < 2; ++n) {   // the high cut
                auto& z = tone.high[n];
                const float v3 = x - z.s2, v1 = lowPass.a1[n] * z.s1 + lowPass.a2[n] * v3, v2 = z.s2 + lowPass.a2[n] * z.s1 + lowPass.a3[n] * v3;
                z.s1 = 2.0f * v1 - z.s1;
                z.s2 = 2.0f * v2 - z.s2;
                x = v2;
            }
            for (int n = 0; n < 2; ++n) {   // the low cut
                auto& z = tone.low[n];
                const float v3 = x - z.s2, v1 = highPass.a1[n] * z.s1 + highPass.a2[n] * v3, v2 = z.s2 + highPass.a2[n] * z.s1 + highPass.a3[n] * v3;
                z.s1 = 2.0f * v1 - z.s1;
                z.s2 = 2.0f * v2 - z.s2;
                x = x - highPass.k[n] * v1 - v2;
            }
            summed[c] = x;
        }

        const float dryLeft = left[i], dryRight = right[i];
        const float loudest = std::max(std::abs(dryLeft), std::abs(dryRight));
        duckEnvelope += (loudest - duckEnvelope) * (loudest > duckEnvelope ? attackCoef : releaseCoef);

        if (parameters.pingPong) {
            lines[0].write(record((dryLeft + dryRight) * 0.5f * sendNow + repeats * loopScale * summed[1]));
            lines[1].write(record(repeats * loopScale * summed[0]));
        } else {
            lines[0].write(record(dryLeft * sendNow + repeats * loopScale * summed[0]));
            lines[1].write(record(dryRight * sendNow + repeats * loopScale * summed[1]));
        }

        const float held = 1.0f - duck * std::min(1.0f, duckEnvelope * 4.0f);
        const float wet = 0.8f * std::min(1.0f, 2.0f * mixNow) * makeup * outScale * held;
        const float dry = std::min(1.0f, 2.0f * (1.0f - mixNow));
        // Narrowing is done on what is heard, not on what is on the tape, so moving WIDTH cannot upset the loop.
        const float width = parameters.pingPong ? std::clamp(parameters.width, 0.0f, 1.0f) : 1.0f;
        const float middle = 0.5f * (summed[0] + summed[1]), side = 0.5f * (summed[0] - summed[1]) * width;
        left[i] = dry * dryLeft + wet * (middle + side);
        right[i] = dry * dryRight + wet * (middle - side);
    }

    // Faded right out: empty the tape, so that bringing the echo back does not replay what was on it.
    if (parameters.mix <= 0.0f && mixNow <= 0.0f) reset();
}
