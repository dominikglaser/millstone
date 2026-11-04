// Millstone's panel: one screen, an old instrument with wood cheeks and a worn brushed-metal face. Controls are placed by
// desktop/panel.conf (embedded at build time). All the artwork is drawn in code, so it stays sharp at any window size.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class MillstoneEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MillstoneEditor(MillstoneProcessor&);
    ~MillstoneEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshFromProcessor() { timerCallback(); }   // what the UI timer does; tests call it directly
    void setPatchBayOpen(bool);                        // the drawer below the panel
    void setRackOpen(bool);                            // the effects rack, in place of the panel
    void setBrowserOpen(bool);                         // the preset browser, over whichever page is showing

    // For tests: how many art pixels the wear changed on each kind of surface.
    struct WearCheck { int onFlat = 0, onRaised = 0, onRecessed = 0, tried = 0, drawn = 0; };
    static WearCheck checkWear();
    static juce::Point<float> jackPosition(bool isOutput, int index);   // in the patch bay's own coordinates
    static juce::StringArray layoutProblems();                          // printing or controls that collide or leave their frame

private:
    void timerCallback() override;
    void fitWindowHeight();                            // tall for the rack or the open drawer

    struct LookAndFeel;
    struct PresetBar;
    struct PresetBrowser;
    struct PatchBay;
    struct ClockOutputs;
    struct RackMaster;
    struct RackPage;
    struct ClockLcd;

    MillstoneProcessor& processor;
    std::unique_ptr<LookAndFeel> lnf;
    std::function<void(juce::Slider*, juce::RangedAudioParameter*)> readoutTarget;
    int widthBeforeDrawer = 0;
    bool windowIsTall = false;
    juce::Image art;           // the static panel: metal, wood, printing, wear
    juce::Component content;   // laid out at the design size, scaled to the window
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::unique_ptr<PresetBar> presetBar;
    std::unique_ptr<PresetBrowser> browser;
    std::unique_ptr<RackPage> rack;
    std::unique_ptr<ClockOutputs> clockOutputs;
    std::unique_ptr<ClockLcd> clockLcd;
    std::unique_ptr<PatchBay> patchBay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MillstoneEditor)
};
