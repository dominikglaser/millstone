// An 8-output clock for triggering and modulating, in the spirit of ALM's Pamela's Workout. This is an original design, not
// a port. One clock is shared by the whole instrument: every output is a division or multiple of the same beat.
#pragma once
#include <math.h>
#include <stdint.h>

namespace ms {

struct ClockChannelSettings {
    float rate = 1.0f;          // cycles per beat
    int shape = 0;
    float width = 0.5f;         // of the cycle, for GATE
    float swing = 0.0f;         // 0..1: how late odd steps land, up to half a step
    float probability = 1.0f;
    float phase = 0.0f;         // 0..1 of a cycle
    float level = 1.0f;
    float offset = 0.0f;
    int steps = 0;              // Euclidean pattern length; 0 is off
    int fill = 0;
    int rotate = 0;
};

class WorkoutClock {
public:
    enum Shape { GATE, TRIG, SAW, RAMP, TRI, SINE, SAMPLE_HOLD };
    enum { kChannels = 8 };

    float out[kChannels] = {};

    // Does Euclidean step `index` of a pattern fire? Spreads `fill` hits evenly over `steps`.
    static bool euclid(int steps, int fill, int rotate, long long index) {
        if (steps <= 0) return true;
        if (fill <= 0) return false;
        if (fill >= steps) return true;
        const long long at = (index + rotate) % steps;
        return (at * fill) % steps < fill;
    }

    // position: the song position in beats. seconds_per_beat keeps TRIG a fixed length at any tempo.
    void process(const ClockChannelSettings *settings, double position, float seconds_per_beat) {
        for (int c = 0; c < kChannels; ++c) out[c] = channel(c, settings[c], position, seconds_per_beat);
    }

private:
    float next_random() {   // 0..1
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float)(rng & 0xffffff) / 16777216.0f;
    }

    float channel(int c, const ClockChannelSettings &s, double position, float seconds_per_beat) {
        const double cycles = (position < 0.0 ? 0.0 : position) * s.rate + s.phase;
        const long long step = (long long)floor(cycles);
        float t = (float)(cycles - (double)step);
        if (step != last_step[c]) {   // decided once per step, so a step never flickers
            last_step[c] = step;
            passes[c] = next_random() < s.probability;
            held[c] = next_random() * 2.0f - 1.0f;
        }
        // Swing: an odd step starts late. Until then the output is still finishing the step before.
        const float late = (step & 1) ? s.swing * 0.5f : 0.0f;
        const bool started = t >= late;
        const float u = started ? (t - late) / (1.0f - late) : 1.0f;
        const bool fires = started && passes[c] && euclid(s.steps, s.fill, s.rotate, step);

        float value = 0.0f;
        if (fires) {
            switch (s.shape) {
                case GATE: value = u < s.width ? 1.0f : 0.0f; break;
                case TRIG: value = u * seconds_per_beat / s.rate < 0.008f ? 1.0f : 0.0f; break;
                case SAW: value = 1.0f - 2.0f * u; break;
                case RAMP: value = 2.0f * u - 1.0f; break;
                case TRI: value = u < 0.5f ? 4.0f * u - 1.0f : 3.0f - 4.0f * u; break;
                case SINE: value = sinf(u * 6.2831853f); break;
                default: value = held[c]; break;
            }
        }
        const float result = s.level * value + s.offset;
        return result < -1.0f ? -1.0f : result > 1.0f ? 1.0f : result;
    }

    long long last_step[kChannels] = {-1, -1, -1, -1, -1, -1, -1, -1};
    bool passes[kChannels] = {true, true, true, true, true, true, true, true};
    float held[kChannels] = {};
    uint32_t rng = 0x51f15e;
};

}  // namespace ms
