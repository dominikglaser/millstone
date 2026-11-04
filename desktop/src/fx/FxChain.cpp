#include "FxChain.h"
#include "Saturator.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr float kBeats[] = { 0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f, 3.0f, 4.0f };
constexpr float kOutputCeiling = 0.944f;      // -0.5 dB, so that the effects cannot be what turns a host's meter red
constexpr float kOutputKnee = 0.9f;           // of that ceiling
constexpr float kPlateLevel = 0.31f;          // held notes come out of the tank 5 dB under the input
}

float delayDivisionBeats(int option) {
    constexpr int count = static_cast<int>(sizeof(kBeats) / sizeof(kBeats[0]));
    return kBeats[std::clamp(option, 0, count - 1)];
}

void FxChain::prepare(double newSampleRate) {
    sampleRate = newSampleRate;
    rampStep = static_cast<float>(1.0 / (0.02 * sampleRate));
    phaser.prepare(sampleRate);
    chorus.prepare(sampleRate);
    delay.prepare(sampleRate);
    plate.prepare(sampleRate);
    spring.prepare(sampleRate);
    setSettings(settings);
    reset();
}

void FxChain::reset() {
    phaser.reset();
    chorus.reset();
    delay.reset();
    plate.reset();
    spring.reset();
    plateNow = springNow = 0.0f;
    plateDirty = springDirty = false;
}

void FxChain::setSettings(const Settings& s) {
    settings = s;
    phaser.setParameters(s.phaserRateHz, s.phaserDepth, s.phaserFeedback, s.phaserMix);
    chorus.setMode(s.chorusMode);
    chorus.setMix(s.chorusMix);
    delay.setParameters(s.delay);
    // The tank runs fully wet: the mix is done here, so that switching type can fade one out and the other in.
    plate.setParameters(s.reverbDecay, s.reverbDiffusion, s.reverbDamping, 1.0f);
    spring.setParameters(s.reverbDecay, s.reverbDiffusion, s.reverbDamping);
    if (s.springKick && !wasKicked && s.reverbType == ReverbType::spring) spring.kick();
    wasKicked = s.springKick;
}

double FxChain::tailSeconds() const {
    double tail = 0.0;
    if (settings.delay.mix > 0.0f)
        tail += tapeDelayTailSeconds(settings.delay.timeSeconds, settings.delay.repeats, settings.delay.heads);
    if (settings.reverbMix > 0.0f)
        tail += settings.reverbType == ReverbType::plate ? reverbTailSeconds(settings.reverbDecay) : springTailSeconds(settings.reverbDecay);
    return tail;
}

void FxChain::renderReverb(float* left, float* right, int numFrames) {
    const float plateTarget = settings.reverbType == ReverbType::plate ? settings.reverbMix : 0.0f;
    const float springTarget = settings.reverbType == ReverbType::spring ? settings.reverbMix : 0.0f;
    if (plateTarget <= 0.0f && plateNow <= 0.0f && springTarget <= 0.0f && springNow <= 0.0f) {
        // Off: clear the tanks once, so that turning it back up does not bring back an old tail.
        if (plateDirty) { plate.reset(); plateDirty = false; }
        if (springDirty) { spring.reset(); springDirty = false; }
        return;
    }

    const float plateGain = kPlateLevel * reverbLevelForDecay(settings.reverbDecay);
    for (int done = 0; done < numFrames; done += kChunk) {
        const int n = std::min(kChunk, numFrames - done);
        float* l = left + done;
        float* r = right + done;
        std::memcpy(dryLeft, l, sizeof(float) * (size_t)n);
        std::memcpy(dryRight, r, sizeof(float) * (size_t)n);

        const bool runPlate = plateTarget > 0.0f || plateNow > 0.0f, runSpring = springTarget > 0.0f || springNow > 0.0f;
        std::fill(wetLeft, wetLeft + n, 0.0f);
        std::fill(wetRight, wetRight + n, 0.0f);
        float plateStart = plateNow, springStart = springNow;
        const float plateEnd = std::abs(plateTarget - plateNow) <= rampStep * (float)n ? plateTarget : plateNow + std::copysign(rampStep * (float)n, plateTarget - plateNow);
        const float springEnd = std::abs(springTarget - springNow) <= rampStep * (float)n ? springTarget : springNow + std::copysign(rampStep * (float)n, springTarget - springNow);

        // Each law: up to half the dry stays whole and the wet rises, then the dry falls away.
        const auto wetGain = [](float m) { return std::min(1.0f, 2.0f * m); };
        const auto dryGain = [](float m) { return std::min(1.0f, 2.0f * (1.0f - m)); };

        if (runPlate) {
            float pl[kChunk], pr[kChunk];
            std::memcpy(pl, dryLeft, sizeof(float) * (size_t)n);
            std::memcpy(pr, dryRight, sizeof(float) * (size_t)n);
            plate.process(pl, pr, n);
            plateDirty = true;
            for (int i = 0; i < n; ++i) {
                const float m = plateStart + (plateEnd - plateStart) * (float)(i + 1) / (float)n;
                wetLeft[i] += plateGain * wetGain(m) * pl[i];
                wetRight[i] += plateGain * wetGain(m) * pr[i];
            }
        }
        if (runSpring) {
            float sl[kChunk], sr[kChunk];
            spring.process(dryLeft, dryRight, sl, sr, n);
            springDirty = true;
            for (int i = 0; i < n; ++i) {
                const float m = springStart + (springEnd - springStart) * (float)(i + 1) / (float)n;
                wetLeft[i] += wetGain(m) * sl[i];
                wetRight[i] += wetGain(m) * sr[i];
            }
        }
        for (int i = 0; i < n; ++i) {
            const float t = (float)(i + 1) / (float)n;
            const float m = std::max(plateStart + (plateEnd - plateStart) * t, springStart + (springEnd - springStart) * t);
            const float dry = dryGain(m);
            l[i] = dry * dryLeft[i] + wetLeft[i];
            r[i] = dry * dryRight[i] + wetRight[i];
        }
        plateNow = plateEnd;
        springNow = springEnd;
    }
}

void FxChain::process(float* left, float* right, int numFrames) {
    const bool colour = settings.phaserMix > 0.0f || (settings.chorusMode != ChorusMode::off && settings.chorusMix > 0.0f);
    const bool reverb = settings.reverbMix > 0.0f || plateNow > 0.0f || springNow > 0.0f || plateDirty || springDirty;
    const bool output = settings.outputGain != 1.0f || outputNow != 1.0f;
    if (!colour && !delay.isActive() && !reverb && !output) return;   // the voices, untouched

    if (colour) {
        phaser.process(left, numFrames);
        chorus.process(left, left, right, numFrames);
    }
    delay.process(left, right, numFrames);
    renderReverb(left, right, numFrames);

    // Nothing above has made a number that is not a number; if something ever does, it must not stay in a loop.
    float check = 0.0f;
    for (int i = 0; i < numFrames; ++i) check += left[i] + right[i];
    if (!std::isfinite(check)) {
        reset();
        std::fill(left, left + numFrames, 0.0f);
        std::fill(right, right + numFrames, 0.0f);
        return;
    }
    const float step = (settings.outputGain - outputNow) / (float)std::max(numFrames, 1);
    for (int i = 0; i < numFrames; ++i) {
        outputNow += step;
        left[i] *= outputNow;
        right[i] *= outputNow;
        left[i] = kOutputCeiling * softClipAbove(left[i] / kOutputCeiling, kOutputKnee);
        right[i] = kOutputCeiling * softClipAbove(right[i] / kOutputCeiling, kOutputKnee);
    }
    outputNow = settings.outputGain;
}
