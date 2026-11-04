#pragma once
#include "Chorus.h"
#include "Phaser.h"
#include "Reverb.h"
#include "SpringReverb.h"
#include "TapeDelay.h"

// The effects after the voices: phaser, chorus, tape delay, reverb. In that order so that the echoes go into the reverb, as they do
// on a dub desk. They live in the plugin rather than the shared engine, and when all of them are off the signal is not touched.
class FxChain {
public:
    enum class ReverbType { plate, spring };

    struct Settings {
        float phaserRateHz = 0.4f, phaserDepth = 0.7f, phaserFeedback = 0.3f, phaserMix = 0.0f;
        ChorusMode chorusMode = ChorusMode::off;
        float chorusMix = 1.0f;
        TapeDelay::Parameters delay;
        ReverbType reverbType = ReverbType::plate;
        float reverbDecay = 0.5f, reverbDiffusion = 0.6f, reverbDamping = 0.4f, reverbMix = 0.0f;
        bool springKick = false;
        float outputGain = 1.0f;   // make-up, after everything
    };

    void prepare(double sampleRate);
    void reset();
    void setSettings(const Settings&);

    // Stereo in place. The voices are centred, so the left channel is what the phaser and the chorus take.
    void process(float* left, float* right, int numFrames);

    double tailSeconds() const;

private:
    void renderReverb(float* left, float* right, int numFrames);

    Settings settings;
    Phaser phaser;
    Chorus chorus;
    TapeDelay delay;
    Reverb plate;
    SpringReverb spring;
    double sampleRate = 48000.0;
    float plateNow = 0, springNow = 0, rampStep = 0.001f, outputNow = 1.0f;
    bool wasKicked = false, plateDirty = false, springDirty = false;
    static constexpr int kChunk = 256;
    float wetLeft[kChunk] {}, wetRight[kChunk] {}, dryLeft[kChunk] {}, dryRight[kChunk] {};
};

// TIME for a synced delay, as a number of beats: the option order of fx_dl_div.
float delayDivisionBeats(int option);
