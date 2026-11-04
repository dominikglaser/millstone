// Millstone's panel. It draws the same pages as the MPC skin by reading vst/layout.conf (embedded at build
// time), so the two UIs share one layout. Edit the layout file, not coordinates in the code.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class MillstoneEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MillstoneEditor(MillstoneProcessor&);
    ~MillstoneEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Which page is showing; also used by the UI snapshot test.
    void showTab(int index);
    int getNumTabs() const { return (int)pages.size(); }
    void refreshFromProcessor() { timerCallback(); }   // what the UI timer does; tests call it directly

private:
    void timerCallback() override;

    struct LookAndFeel;
    struct Page;
    struct PresetBar;

    MillstoneProcessor& processor;
    std::unique_ptr<LookAndFeel> lnf;
    juce::Component content;   // laid out at the design size, scaled to the window
    juce::OwnedArray<juce::TextButton> tabButtons;
    std::vector<std::unique_ptr<Page>> pages;
    std::unique_ptr<PresetBar> presetBar;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MillstoneEditor)
};
