#include "PluginProcessor.h"

namespace {
// Build a stable JUCE parameter id from an engine key (keys are already id-safe: letters, digits, _).
juce::String pid(const char* key) { return juce::String(key); }
}

juce::AudioProcessorValueTreeState::ParameterLayout MillstoneProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < NPARAMS; ++i) {
        const param_t& p = PARAMS[i];
        if (p.momentary) continue;   // model prev/next steppers: not exposed; the model Choice covers it
        if (p.nopts > 0) {
            juce::StringArray choices;
            for (int o = 0; o < p.nopts; ++o) choices.add(p.opts[o]);
            const int def = juce::jlimit(0, p.nopts - 1, (int)std::lround(p.def * (p.nopts - 1)));
            layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(pid(p.key), 1), p.name, choices, def));
        } else {
            const float interval = p.int_display ? 1.0f : 0.0f;
            juce::NormalisableRange<float> range(p.min, p.max, interval);
            const float def = p.min + (p.max - p.min) * p.def;
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(pid(p.key), 1), p.name, range, def));
        }
    }
    return layout;
}

MillstoneProcessor::MillstoneProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", makeLayout()) {
    rawValues.assign(NPARAMS, nullptr);
    for (int i = 0; i < NPARAMS; ++i)
        if (!PARAMS[i].momentary)
            rawValues[i] = apvts.getRawParameterValue(pid(PARAMS[i].key));
}

void MillstoneProcessor::prepareToPlay(double sampleRate, int) {
    engine.prepare(sampleRate);
    pushParamsToEngine();
}

bool MillstoneProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MillstoneProcessor::pushParamsToEngine() {
    char buf[64];
    for (int i = 0; i < NPARAMS; ++i) {
        if (!rawValues[i]) continue;
        const float v = rawValues[i]->load();
        if (PARAMS[i].nopts > 0 || PARAMS[i].int_display)
            std::snprintf(buf, sizeof buf, "%d", (int)std::lround(v));
        else
            std::snprintf(buf, sizeof buf, "%g", v);
        engine.setParam(PARAMS[i].key, buf);
    }
}

void MillstoneProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // Host tempo for the synced LFO.
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) {
                char t[32];
                std::snprintf(t, sizeof t, "%g", *bpm);
                engine.setParam("lfo_bpm", t);
            }
    }

    // APVTS is the source of truth; keep the engine in step with it.
    pushParamsToEngine();

    // MIDI: applied at block start (host blocks are a few ms; fine for playing). Note/CC/bend/AT all pass through.
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        engine.midi(m.getRawData(), m.getRawDataSize());
    }

    float* L = buffer.getWritePointer(0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : L;
    engine.render(L, R, n);
    if (buffer.getNumChannels() == 1) {} // mono host: L already holds the signal
}

void MillstoneProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void MillstoneProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MillstoneProcessor(); }
