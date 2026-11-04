// Small DSP building blocks for Millstone: ADSR envelope, LFO, 24 dB ladder filter, noise.
// All of them run at 44.1 kHz, per sample or per control chunk, and never allocate.
#pragma once
#include <math.h>
#include <stdint.h>

namespace ms {

static const float kRate = 44100.0f;

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

// Rational tanh approximation, good to about 1e-3 over -4..4 and saturating cleanly beyond.
static inline float fast_tanh(float x) {
    if (x > 4.97f) return 1.0f;
    if (x < -4.97f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// ---- ADSR envelope ----
struct Env {
    enum Stage { IDLE, ATTACK, DECAY, RELEASE };
    Stage stage = IDLE;
    float value = 0.0f;
    float a_coef = 0.01f, d_coef = 0.001f, r_coef = 0.001f, sustain = 1.0f;

    // attack 2 ms..3 s, decay and release 4 ms..10 s (knobs 0..1), sustain 0..1
    void set(float attack01, float decay01, float sustain01, float release01) {
        float ta = 0.002f * powf(1500.0f, clampf(attack01, 0.0f, 1.0f));
        float td = 0.004f * powf(2500.0f, clampf(decay01, 0.0f, 1.0f));
        float tr = 0.004f * powf(2500.0f, clampf(release01, 0.0f, 1.0f));
        a_coef = 1.0f - expf(-1.7918f / (ta * kRate));   // ln 6: a 1.2 target reaches 1.0 in ta
        d_coef = 1.0f - expf(-6.9078f / (td * kRate));   // ln 1000: -60 dB in td
        r_coef = 1.0f - expf(-6.9078f / (tr * kRate));
        sustain = clampf(sustain01, 0.0f, 1.0f);
    }
    void gate_on() { stage = ATTACK; }            // keeps the current value: no click on a retrigger
    void gate_off() { if (stage != IDLE) stage = RELEASE; }
    void reset() { stage = IDLE; value = 0.0f; }
    bool idle() const { return stage == IDLE; }
    inline float tick() {
        switch (stage) {
            case ATTACK:
                value += (1.2f - value) * a_coef;
                if (value >= 1.0f) { value = 1.0f; stage = DECAY; }
                break;
            case DECAY:
                value += (sustain - value) * d_coef;
                break;
            case RELEASE:
                value += (0.0f - value) * r_coef;
                if (value < 1e-4f) { value = 0.0f; stage = IDLE; }
                break;
            default: break;
        }
        return value;
    }
};

// ---- LFO: 0.01..350 Hz, tri / square / saw / ramp / sample-and-hold, bipolar -1..1 ----
struct Lfo {
    enum Shape { TRI, SQR, SAW, RAMP, SH };
    float phase = 0.0f;
    float hold = 0.0f;
    uint32_t rng = 0x9e3779b9u;

    static float rate_hz(float knob01) { return 0.01f * powf(35000.0f, clampf(knob01, 0.0f, 1.0f)); }
    void retrigger() { phase = 0.0f; hold = next_random(); }
    // Follow an outside clock. Passing the top of the cycle still counts as a new cycle, so S/H keeps stepping.
    void set_phase(float p) { if (p < phase - 0.5f) hold = next_random(); phase = p; }
    float next_random() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float)(rng & 0xffffff) / 8388608.0f - 1.0f;
    }
    // advance by `frames` samples at `hz`, return the value at the new phase
    float step(float hz, int frames, int shape) {
        float inc = hz * frames / kRate;
        phase += inc;
        if (phase >= 1.0f) { phase -= floorf(phase); hold = next_random(); }
        switch (shape) {
            case TRI: return phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase;
            case SQR: return phase < 0.5f ? 1.0f : -1.0f;
            case SAW: return 1.0f - 2.0f * phase;
            case RAMP: return 2.0f * phase - 1.0f;
            default: return hold;
        }
    }
};

// ---- 24 dB/oct ladder filter, zero-delay feedback (TPT), LP or HP (Xpander-style pole mixing) ----
struct Ladder {
    float s[4] = {0, 0, 0, 0};   // stage states
    float G = 0.0f, g1 = 1.0f;   // per-stage coefficient, 1/(1+g)
    float k = 0.0f;              // feedback 0..4+
    int hp = 0;

    void reset() { s[0] = s[1] = s[2] = s[3] = 0.0f; }
    void set(float hz, float reso01, int highpass) {
        hz = clampf(hz, 20.0f, 18000.0f);
        float g = tanf((float)M_PI * hz / kRate);
        G = g / (1.0f + g);
        g1 = 1.0f / (1.0f + g);
        k = powf(clampf(reso01, 0.0f, 1.0f), 1.3f) * 4.3f;
        hp = highpass;
    }
    inline float tick(float x) {
        // instantaneous response of the four stages to zero input: S = G^3 s0' + G^2 s1' + G s2' + s3', s' = s/(1+g)
        float S = ((s[0] * G + s[1]) * G + s[2]) * G + s[3];
        S *= g1;
        float G4 = G * G * G * G;
        float u = (x - k * S) / (1.0f + k * G4);
        u = fast_tanh(u);
        float y[4];
        float in = u;
        for (int i = 0; i < 4; i++) {
            float v = (in - s[i]) * G;
            y[i] = v + s[i];
            s[i] = y[i] + v;
            in = y[i];
        }
        if (hp) return u - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + y[3];
        return y[3];
    }
};

struct Noise {
    uint32_t rng = 0x12345678u;
    inline float tick() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float)(rng & 0xffffff) / 8388608.0f - 1.0f;
    }
};

}  // namespace ms
