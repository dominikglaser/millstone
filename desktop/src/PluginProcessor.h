// Millstone desktop plugin (VST3 / AU / Standalone) built with JUCE around the shared engine.
// The engine (src/millstone.cpp, mpc_engine()) is byte-identical to the MPC build; this file is only
// the host shell: parameters, MIDI and audio plumbing. Converting to the MPC plugin is a rebuild with
// the MPC wrapper instead of this one.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine_host.h"
#include "fx/FxChain.h"
#include <array>
#include <map>

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
    double getTailLengthSeconds() const override { return 10.0 + fx.tailSeconds(); }   // the longest release, then the echoes

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
    bool rackOpen = false;                                         // showing the effects rack rather than the panel
    int clockChannel = 0, clockSetting = 0;                        // which clock output and LCD setting it is showing
    std::array<int, 5> fxPreset { -1, -1, -1, -1, -1 };                // what each rack unit's screen last loaded
    std::atomic<float> delaySecondsNow { 0.375f };                 // for the echo's screen

    struct Preset {
        juce::String name, category;
        std::vector<std::pair<juce::String, float>> values;   // parameter id, value in its own units or option index
        juce::String type, pack, note;
        juce::StringArray styles, character, uses, plays;
        bool unmatched = false;                               // not level-matched against the rest of the library
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

    // What you think of them. Plain text beside the user presets, by name, so reordering the library cannot confuse them and
    // the flags can simply be handed back to whoever is writing the next batch.
    juce::File favouritesFile() const;
    juce::File flagsFile() const;
    bool isFavourite(const juce::String& name) const { return favourites().contains(name); }
    void setFavourite(const juce::String& name, bool on);
    juce::StringArray favourites() const;
    std::map<juce::String, juce::String> flags() const;                 // preset name, your note
    void setFlag(const juce::String& name, const juce::String& note);
    void clearFlag(const juce::String& name);

    // What the browser is showing, kept here so that closing the window does not lose it.
    struct BrowserFilter {
        juce::String search;
        juce::StringArray types, styles, character, uses, plays, packs;
        bool favouritesOnly = false, flaggedOnly = false;
    };
    BrowserFilter browserFilter;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void pushParamsToEngine();
    void updateEffects();
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
    FxChain fx;
    struct FxValues {
        std::atomic<float>* phRate, *phDepth, *phFb, *phMix, *chMode, *chMix;
        std::atomic<float>* dlSync, *dlTime, *dlDiv, *dlFb, *dlIn, *dlThrow, *dlHeads, *dlPp, *dlWidth, *dlLow, *dlHigh, *dlDrive, *dlWow, *dlDuck, *dlMix;
        std::atomic<float>* rvType, *rvDecay, *rvDiff, *rvDamp, *rvKick, *rvMix, *clockBpm;
    } fxValues {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MillstoneProcessor)
};
