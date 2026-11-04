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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    int uiTab = 0;   // the page showing, so it survives closing and reopening the window

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void pushParamsToEngine();

    EngineHost engine;
    // One cached raw-value pointer per engine parameter we expose (nullptr for momentary ones we skip).
    std::vector<std::atomic<float>*> rawValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MillstoneProcessor)
};
