#pragma once
#include <cstddef>
#include <vector>

// A tape echo for dub, in the spirit of the Space Echo and Echoplex machines the style was built on.
//
// What makes it that rather than a clean delay:
//  - Three playback heads at one, two and three times TIME, any of them on, for the shuffling patterns.
//  - REPEATS passes 100%: the loop then builds until the tape saturates, and keeps going on its own.
//  - Filters in the loop, so each repeat is thinner and darker than the last and bass cannot pile up.
//  - Tape saturation on the record side, which is also what keeps the runaway bounded.
//  - Wow and flutter on the tape speed, which the repeats accumulate.
//  - Moving TIME moves the tape. The heads glide there, bending the pitch of everything already on it, and never reverse.
//  - SEND and THROW: the engineer's move of letting only one chosen note or chord into the echo.
//  - DUCK holds the echoes down while the input plays, so a tight skank stays tight and the echoes answer it.
//
// Ping-pong chains the two lines. Crossed feedback does nothing on a centred signal, because both sides then carry the same.
class TapeDelay {
public:
    static constexpr float kLongestSeconds = 8.0f;
    static constexpr float kStrongestRepeats = 1.2f;

    struct Parameters {
        float timeSeconds = 0.375f;   // the spacing of head 1
        float repeats = 0.45f;        // 0..1.2, loop gain
        float lowCutHz = 50.0f;
        float highCutHz = 3500.0f;
        float drive = 0.25f;          // 0..1
        float wow = 0.2f;             // 0..1
        float duck = 0.0f;            // 0..1
        float send = 1.0f;            // 0..1, how much reaches the tape
        bool throwOn = false;         // full send, whatever SEND says
        int heads = 1;                // bit 0 head 1, bit 1 head 2, bit 2 head 3
        bool pingPong = false;
        float width = 1.0f;           // of a ping-pong: 0 both bounces centred, 1 hard left and right
        float mix = 0.0f;             // up to 0.5 the dry stays whole and the echo rises; beyond it the dry falls away
    };

    void prepare(double sampleRate);
    void reset();
    void setParameters(const Parameters&);
    bool isActive() const { return parameters.mix > 0.0f || mixNow > 0.0f; }
    void process(float* left, float* right, int numFrames);

private:
    struct Line {
        std::vector<float> samples;
        std::size_t mask = 0, writeIndex = 0;
        void prepare(std::size_t length);
        void clear();
        void write(float x) { samples[writeIndex] = x; writeIndex = (writeIndex + 1) & mask; }
        float read(float delaySamples) const;
    };
    // Two state-variable sections each way: 24 dB per octave, Butterworth. Gentler filters than this were measured to do
    // next to nothing to the first echo of a bass-heavy sound.
    struct Section { float s1 = 0, s2 = 0; };
    struct Tone { Section low[2], high[2]; };
    struct Coefs { float a1[2] {}, a2[2] {}, a3[2] {}, k[2] {}; };

    double sampleRate = 48000.0;
    Parameters parameters;
    Line lines[2];
    Tone tones[2];

    float inertia = 0, position = 0;     // the tape: smoothed target, then the slew-limited place of head 1
    bool snapTape = true;
    float headGain[3] = { 1, 0, 0 };
    float sendNow = 1, mixNow = 0, duckEnvelope = 0;
    double wowPhase = 0, wowPhase2 = 0, flutterPhase = 0;
    float drift = 0, driftTarget = 0;
    int driftCountdown = 0;
    unsigned noiseState = 0x12345678u;
    Coefs lowPass, highPass;
    float smoothFast = 1, smoothTape = 1, attackCoef = 1, releaseCoef = 1;
};

// How long it can ring after the input stops, to 60 dB down. Past 100% it never stops, so a host gets the cap.
float tapeDelayTailSeconds(float timeSeconds, float repeats, int heads);
