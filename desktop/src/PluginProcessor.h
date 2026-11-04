// Millstone desktop plugin (VST3 / AU / Standalone) built with JUCE around the shared engine.
// The engine (src/millstone.cpp, mpc_engine()) is byte-identical to the MPC build; this file is only
// the host shell: parameters, MIDI and audio plumbing. Converting to the MPC plugin is a rebuild with
// the MPC wrapper instead of this one.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine_host.h"

class MillstoneProcessor : public juce::AudioProcessor {
public:
    MillstoneProcessor();
    ~MillstoneProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Millstone"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }   // the longest release

    // Factory presets are also the host's program list. User presets are not: hosts handle a program count that
    // changes at runtime badly.
    int getNumPrograms() override { return (int)factoryPresets().size(); }
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    // Patch bay. A cord is a source option, a destination option and an amount, kept as ordinary parameters so that
    // Live sets and presets carry them. 0 means unplugged at either end.
    static constexpr int numCords = 12;
    int cordSource(int cord) const;
    int cordDest(int cord) const;
    float cordAmount(int cord) const;
    int connectCord(int source, int dest, float amount = 50.0f);   // the cord used, the existing one if already patched, or -1 if all are in use
    void disconnectCord(int cord);
    bool patchBayOpen = false;                                     // the drawer, while the window is closed and reopened
    int drawerPage = 0, clockChannel = 0, clockSetting = 0;        // which module, clock output and LCD setting it is showing

    struct Preset {
        juce::String name, category;
        std::vector<std::pair<juce::String, float>> values;   // parameter id, value in its own units or option index
    };
    static const std::vector<Preset>& factoryPresets();       // from vst/presets.json, shared with the MPC build

    // Saved by name, not number, so reordering the library cannot point a Live set at the wrong sound.
    juce::String getCurrentPresetName() const { return currentPresetName; }

    // One file each, with the name inside the file so it can hold what a filename cannot.
    juce::File userPresetDirectory() const;
    void setUserPresetDirectory(const juce::File& dir) { userDirectoryOverride = dir; }   // for tests
    juce::StringArray userPresetNames() const;
    bool saveUserPreset(const juce::String& name);
    bool loadUserPreset(const juce::String& name);
    bool deleteUserPreset(const juce::String& name);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void pushParamsToEngine();
    juce::File userPresetFile(const juce::String& name) const;
    void applyDefaultsThenValues(const std::vector<std::pair<juce::String, float>>&);
    float cordValue(int cord, const char* field) const;
    void setCordValue(int cord, const char* field, float value);

    EngineHost engine;
    juce::String currentPresetName { "Init" };
    juce::File userDirectoryOverride;
    // One cached raw-value pointer per engine parameter we expose (nullptr for momentary ones we skip).
    std::vector<std::atomic<float>*> rawValues;
    std::vector<float> pushedValues;   // what the engine last got: only changes are sent, as a lookup is by name
    double pushedBpm = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MillstoneProcessor)
};
