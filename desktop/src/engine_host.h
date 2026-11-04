// EngineHost: drives the shared Millstone engine (mpc_engine(), src/millstone.cpp) at a host sample
// rate. The engine is fixed at 44.1 kHz, interleaved int16 stereo; this resamples its output to the
// host rate with 4-point Catmull-Rom interpolation and forwards MIDI and parameters. It is JUCE-free
// on purpose, so the audio path is the same code whether it runs under the desktop plugin, a test
// harness, or (unchanged engine) the MPC build.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "engine.h"
}
#include "params.h"

class EngineHost {
public:
    EngineHost() = default;
    ~EngineHost() { destroy(); }

    void prepare(double hostSampleRate) {
        destroy();
        eng_ = mpc_engine();
        dsp_ = eng_ ? eng_->create(nullptr) : nullptr;
        hostRate_ = hostSampleRate > 0 ? hostSampleRate : 44100.0;
        ratio_ = kEngineRate / hostRate_;   // engine input samples consumed per host output sample
        bufL_.reserve(kReserve);   // so the audio thread does not allocate at any normal block size
        bufR_.reserve(kReserve);
        bufL_.assign(4, 0.0f);
        bufR_.assign(4, 0.0f);
        readPos_ = 1.0;                      // leaves bufX_[idx-1] valid from the first sample
    }

    bool ready() const { return dsp_ != nullptr; }

    void destroy() {
        if (eng_ && dsp_) eng_->destroy(dsp_);
        dsp_ = nullptr;
    }

    void setParam(const char* key, const char* value) {
        if (eng_ && dsp_) eng_->set_param(dsp_, key, value);
    }

    void songPosition(double ppq, double bpm, bool playing, int frames) {
        if (eng_ && dsp_ && eng_->song_pos) eng_->song_pos(dsp_, ppq, bpm, playing ? 1 : 0, frames);
    }

    void midi(const uint8_t* bytes, int len) {
        if (eng_ && dsp_ && len >= 1) eng_->midi(dsp_, bytes, len);
    }

    // Render n host-rate frames into two float channels in [-1, 1].
    void render(float* outL, float* outR, int n) {
        if (!dsp_) {
            std::memset(outL, 0, sizeof(float) * (size_t)n);
            std::memset(outR, 0, sizeof(float) * (size_t)n);
            return;
        }
        const int lastNeeded = (int)std::floor(readPos_ + (double)(n > 0 ? n - 1 : 0) * ratio_) + 2;
        while ((int)bufL_.size() <= lastNeeded) renderEngineChunk();

        for (int i = 0; i < n; ++i) {
            const double p = readPos_ + (double)i * ratio_;
            const int idx = (int)std::floor(p);
            const float f = (float)(p - idx);
            outL[i] = catmull(bufL_[idx - 1], bufL_[idx], bufL_[idx + 1], bufL_[idx + 2], f);
            outR[i] = catmull(bufR_[idx - 1], bufR_[idx], bufR_[idx + 1], bufR_[idx + 2], f);
        }
        readPos_ += (double)n * ratio_;

        const int drop = (int)std::floor(readPos_) - 1;   // keep one sample behind for the next block's idx-1
        if (drop > 0) {
            bufL_.erase(bufL_.begin(), bufL_.begin() + drop);
            bufR_.erase(bufR_.begin(), bufR_.begin() + drop);
            readPos_ -= drop;
        }
    }

private:
    static constexpr double kEngineRate = 44100.0;
    static constexpr int kChunk = 32;    // engine frames rendered per refill; rendered ahead, so this is added latency
    static constexpr size_t kReserve = 32768;

    static inline float catmull(float a, float b, float c, float d, float t) {
        // Catmull-Rom: smooth 4-point interpolation, b..c is the active segment
        const float t2 = t * t, t3 = t2 * t;
        return 0.5f * ((2.0f * b) + (-a + c) * t + (2.0f * a - 5.0f * b + 4.0f * c - d) * t2 +
                       (-a + 3.0f * b - 3.0f * c + d) * t3);
    }

    void renderEngineChunk() {
        int16_t tmp[kChunk * 2];
        eng_->render(dsp_, tmp, kChunk);
        for (int i = 0; i < kChunk; ++i) {
            bufL_.push_back(tmp[2 * i] * (1.0f / 32768.0f));
            bufR_.push_back(tmp[2 * i + 1] * (1.0f / 32768.0f));
        }
    }

    const mpc_engine_t* eng_ = nullptr;
    void* dsp_ = nullptr;
    double hostRate_ = 44100.0;
    double ratio_ = 1.0;
    double readPos_ = 1.0;
    std::vector<float> bufL_, bufR_;
};
