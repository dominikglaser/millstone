#pragma once
#include <cstddef>
#include <vector>

// A spring tank, after the published models of Abel and Parker: what makes a spring sound like one is dispersion. A pulse
// sent down a spring arrives as a chirp, low frequencies first, rising towards a limit near 4 kHz, and each trip back and forth
// smears it further. That is the "drip" of a dub spring. A long cascade of stretched all-pass filters does exactly that.
//
// Three springs of different lengths, as in a long tank, on one mono drive, which is how a tank is built. The drive saturates,
// and KICK is the engineer banging the tank.
class SpringReverb {
public:
    void prepare(double sampleRate);
    void reset();

    // decay, drip and tone run 0..1.
    void setParameters(float decay, float drip, float tone);
    void kick();

    // Adds nothing dry: writes the tank's output.
    void process(const float* left, const float* right, float* outLeft, float* outRight, int numFrames);

private:
    static constexpr int kSprings = 3;
    struct Spring {
        int sections = 0;
        std::vector<float> history;   // sections + 1 rows of `stretch` samples
        std::vector<float> line;
        std::size_t lineMask = 0, lineWrite = 0;
        float loopSamples = 0;
        float lp1 = 0, lp2 = 0;
        double wobble = 0;
        float wobbleStep = 0;
    };

    Spring springs[kSprings];
    double sampleRate = 48000.0;
    int stretch = 6, historyIndex = 0;
    float allpass = 0.6f, feedback = 0.8f, outputLevel = 1.0f, toneCoef = 0.5f, inputLow = 0, lowCoef = 0.01f;
    float kickEnvelope = 0, kickDecay = 0.999f, kickPhase = 0, kickStep = 0;
    unsigned noiseState = 0x2468ace1u;
};

float springTailSeconds(float decay);
