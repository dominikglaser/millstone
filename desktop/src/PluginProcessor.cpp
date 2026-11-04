#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include <cstring>
#include <limits>
#include <map>

namespace {
// Build a stable JUCE parameter id from an engine key (keys are already id-safe: letters, digits, _).
juce::String pid(const char* key) { return juce::String(key); }

// Hidden: the model prev/next steppers (the model choice covers them) and the MPC skin's popup open/closed flags.
bool exposedToHost(const param_t& p) { return !p.momentary && p.popup_of < 0; }

// The effects run in this plugin, after the engine, so the engine is not told about them.
bool isEffect(const param_t& p) { return std::strncmp(p.key, "fx_", 3) == 0; }
}

juce::AudioProcessorValueTreeState::ParameterLayout MillstoneProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < NPARAMS; ++i) {
        const param_t& p = PARAMS[i];
        if (exposedToHost(p) == false) continue;
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
        if (exposedToHost(PARAMS[i]) && !isEffect(PARAMS[i]))
            rawValues[i] = apvts.getRawParameterValue(pid(PARAMS[i].key));
    const auto value = [this](const char* key) { return apvts.getRawParameterValue(key); };
    fxValues = { value("fx_ph_rate"), value("fx_ph_depth"), value("fx_ph_fb"), value("fx_ph_mix"), value("fx_ch_mode"), value("fx_ch_mix"),
                 value("fx_dl_sync"), value("fx_dl_time"), value("fx_dl_div"), value("fx_dl_fb"), value("fx_dl_in"), value("fx_dl_throw"), value("fx_dl_heads"),
                 value("fx_dl_pp"), value("fx_dl_width"), value("fx_dl_low"), value("fx_dl_high"), value("fx_dl_drive"), value("fx_dl_wow"), value("fx_dl_duck"), value("fx_dl_mix"),
                 value("fx_rv_type"), value("fx_rv_decay"), value("fx_rv_diff"), value("fx_rv_damp"), value("fx_rv_kick"), value("fx_rv_mix"), value("ck_bpm") };
}

juce::AudioProcessorEditor* MillstoneProcessor::createEditor() { return new MillstoneEditor(*this); }

void MillstoneProcessor::prepareToPlay(double sampleRate, int) {
    engine.prepare(sampleRate);
    pushedValues.assign(NPARAMS, std::numeric_limits<float>::quiet_NaN());   // a new engine knows nothing yet
    pushedBpm = -1.0;
    pushParamsToEngine();
    fx.prepare(sampleRate);
    updateEffects();
}

bool MillstoneProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MillstoneProcessor::pushParamsToEngine() {
    char buf[64];
    for (int i = 0; i < NPARAMS; ++i) {
        if (!rawValues[i]) continue;
        const float v = rawValues[i]->load();
        if (v == pushedValues[(size_t)i]) continue;
        pushedValues[(size_t)i] = v;
        if (PARAMS[i].nopts > 0 || PARAMS[i].int_display)
            std::snprintf(buf, sizeof buf, "%d", (int)std::lround(v));
        else
            std::snprintf(buf, sizeof buf, "%g", v);
        engine.setParam(PARAMS[i].key, buf);
    }
}

void MillstoneProcessor::updateEffects() {
    const auto percent = [](std::atomic<float>* v) { return v->load() * 0.01f; };
    const auto option = [](std::atomic<float>* v) { return (int)std::lround(v->load()); };
    FxChain::Settings s;
    s.phaserRateHz = 0.05f * std::pow(160.0f, percent(fxValues.phRate));   // 0.05 to 8 Hz, evenly by ear
    s.phaserDepth = percent(fxValues.phDepth);
    s.phaserFeedback = percent(fxValues.phFb) * 0.9f;
    s.phaserMix = percent(fxValues.phMix);
    s.chorusMode = static_cast<ChorusMode>(option(fxValues.chMode));
    s.chorusMix = percent(fxValues.chMix);

    // The song's tempo, or with no song playing (the standalone app) the instrument's own clock.
    const double bpm = pushedBpm > 0.0 ? pushedBpm : (double)fxValues.clockBpm->load();
    s.delay.timeSeconds = option(fxValues.dlSync) == 1 ? (float)(delayDivisionBeats(option(fxValues.dlDiv)) * 60.0 / bpm) : fxValues.dlTime->load() * 0.001f;
    s.delay.repeats = percent(fxValues.dlFb);
    s.delay.send = option(fxValues.dlIn) == 0 ? 1.0f : 0.0f;   // ON: everything goes in. THROW: only what THROW lets in.
    s.delay.throwOn = option(fxValues.dlThrow) == 1;
    static constexpr int headsMask[] = { 1, 2, 4, 3, 5, 6, 7 };
    s.delay.heads = headsMask[juce::jlimit(0, 6, option(fxValues.dlHeads))];
    s.delay.pingPong = option(fxValues.dlPp) == 1;
    s.delay.width = percent(fxValues.dlWidth);
    s.delay.lowCutHz = 20.0f * std::pow(100.0f, percent(fxValues.dlLow));    // 20 Hz to 2 kHz
    s.delay.highCutHz = 150.0f * std::pow(100.0f, percent(fxValues.dlHigh)); // 150 Hz to 15 kHz
    s.delay.drive = percent(fxValues.dlDrive);
    s.delay.wow = percent(fxValues.dlWow);
    s.delay.duck = percent(fxValues.dlDuck);
    s.delay.mix = percent(fxValues.dlMix);

    s.reverbType = option(fxValues.rvType) == 1 ? FxChain::ReverbType::spring : FxChain::ReverbType::plate;
    s.reverbDecay = percent(fxValues.rvDecay);
    s.reverbDiffusion = percent(fxValues.rvDiff);
    s.reverbDamping = percent(fxValues.rvDamp);
    s.reverbMix = percent(fxValues.rvMix);
    s.springKick = option(fxValues.rvKick) == 1;
    fx.setSettings(s);
}

void MillstoneProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // Host tempo for the synced LFO.
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            const auto bpm = pos->getBpm();
            if (bpm && *bpm != pushedBpm) {
                pushedBpm = *bpm;
                char t[32];
                std::snprintf(t, sizeof t, "%g", *bpm);
                engine.setParam("lfo_bpm", t);
            }
            const auto ppq = pos->getPpqPosition();
            engine.songPosition(ppq ? *ppq : -1.0, bpm ? *bpm : 0.0, pos->getIsPlaying(), n);
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
    updateEffects();
    if (R != L) fx.process(L, R, n);
}

void MillstoneProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    // Keep the engine running so a note-off that arrives while bypassed still ends its note.
    processBlock(buffer, midi);
    buffer.clear();
}

void MillstoneProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto state = apvts.copyState();
    state.setProperty("preset", currentPresetName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void MillstoneProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto state = juce::ValueTree::fromXml(*xml);
        currentPresetName = state.getProperty("preset").toString();   // empty for sets saved before presets existed
        apvts.replaceState(state);
    }
}

// ---- patch bay --------------------------------------------------------------------------------------------------
float MillstoneProcessor::cordValue(int cord, const char* field) const {
    return apvts.getRawParameterValue("c" + juce::String(cord + 1) + "_" + field)->load();
}

void MillstoneProcessor::setCordValue(int cord, const char* field, float value) {
    auto* p = apvts.getParameter("c" + juce::String(cord + 1) + "_" + field);
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(value));
    p->endChangeGesture();
}

int MillstoneProcessor::cordSource(int cord) const { return juce::roundToInt(cordValue(cord, "src")); }
int MillstoneProcessor::cordDest(int cord) const { return juce::roundToInt(cordValue(cord, "dst")); }
float MillstoneProcessor::cordAmount(int cord) const { return cordValue(cord, "amt"); }

int MillstoneProcessor::connectCord(int source, int dest, float amount) {
    if (source <= 0 || dest <= 0) return -1;
    int freeCord = -1;
    for (int c = numCords - 1; c >= 0; --c) {
        if (cordSource(c) == source && cordDest(c) == dest) return c;
        if (cordSource(c) == 0 || cordDest(c) == 0) freeCord = c;
    }
    if (freeCord < 0) return -1;
    setCordValue(freeCord, "amt", amount);
    setCordValue(freeCord, "src", (float)source);
    setCordValue(freeCord, "dst", (float)dest);
    return freeCord;
}

void MillstoneProcessor::disconnectCord(int cord) {
    setCordValue(cord, "src", 0.0f);
    setCordValue(cord, "dst", 0.0f);
}

// ---- presets ----------------------------------------------------------------------------------------------------
const std::vector<MillstoneProcessor::Preset>& MillstoneProcessor::factoryPresets() {
    static const std::vector<Preset> library = [] {
        std::vector<Preset> out;
        const auto json = juce::JSON::parse(juce::String::fromUTF8(BinaryData::presets_json, BinaryData::presets_jsonSize));
        if (const auto* list = json["presets"].getArray())
            for (const auto& entry : *list) {
                Preset preset { entry["name"].toString(), entry["category"].toString(), {} };
                if (const auto* obj = entry["values"].getDynamicObject())
                    for (const auto& property : obj->getProperties()) {
                        const auto key = property.name.toString();
                        const param_t* def = nullptr;
                        for (int i = 0; i < NPARAMS && def == nullptr; ++i)
                            if (key == PARAMS[i].key) def = &PARAMS[i];
                        if (def == nullptr) { jassertfalse; continue; }
                        float value = (float)property.value;
                        if (property.value.isString())
                            for (int o = 0; o < def->nopts; ++o)
                                if (property.value.toString() == def->opts[o]) value = (float)o;
                        preset.values.emplace_back(key, value);
                    }
                out.push_back(std::move(preset));
            }
        return out;
    }();
    return library;
}

void MillstoneProcessor::applyDefaultsThenValues(const std::vector<std::pair<juce::String, float>>& values) {
    // Each parameter is written once, straight to where it is going, and only if it has to move. Resetting everything first
    // would let the audio thread catch defaults in between: a jump in volume, or a voice mode flip that cuts held notes.
    std::map<juce::String, float> wanted;
    for (const auto& [key, value] : values) wanted[key] = value;
    for (auto* base : getParameters()) {
        auto* p = dynamic_cast<juce::RangedAudioParameter*>(base);
        if (p == nullptr) continue;
        const auto it = wanted.find(p->getParameterID());
        const float target = it != wanted.end() ? p->convertTo0to1(it->second) : p->getDefaultValue();   // not listed: its default
        if (std::abs(p->getValue() - target) > 1e-6f) p->setValueNotifyingHost(target);
    }
}

int MillstoneProcessor::getCurrentProgram() {
    const auto& library = factoryPresets();
    for (int i = 0; i < (int)library.size(); ++i)
        if (library[(size_t)i].name == currentPresetName) return i;
    return 0;
}

void MillstoneProcessor::setCurrentProgram(int index) {
    const auto& library = factoryPresets();
    if (!juce::isPositiveAndBelow(index, (int)library.size())) return;
    // A host re-selecting what is already loaded, as some do when restoring, must not undo the user's tweaks.
    if (library[(size_t)index].name == currentPresetName) return;
    applyDefaultsThenValues(library[(size_t)index].values);
    currentPresetName = library[(size_t)index].name;
}

const juce::String MillstoneProcessor::getProgramName(int index) {
    const auto& library = factoryPresets();
    return juce::isPositiveAndBelow(index, (int)library.size()) ? library[(size_t)index].name : juce::String();
}

juce::File MillstoneProcessor::userPresetDirectory() const {
    if (userDirectoryOverride != juce::File()) return userDirectoryOverride;
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Application Support/Glaser/Millstone/Presets");
}

juce::File MillstoneProcessor::userPresetFile(const juce::String& name) const {
    for (const auto& file : userPresetDirectory().findChildFiles(juce::File::findFiles, false, "*.millstone"))
        if (auto xml = juce::XmlDocument::parse(file))
            if (xml->getStringAttribute("name") == name) return file;
    return userPresetDirectory().getChildFile(juce::File::createLegalFileName(name) + ".millstone");
}

juce::StringArray MillstoneProcessor::userPresetNames() const {
    juce::StringArray names;
    for (const auto& file : userPresetDirectory().findChildFiles(juce::File::findFiles, false, "*.millstone"))
        if (auto xml = juce::XmlDocument::parse(file))
            names.add(xml->getStringAttribute("name", file.getFileNameWithoutExtension()));
    names.sortNatural();
    return names;
}

bool MillstoneProcessor::saveUserPreset(const juce::String& name) {
    const auto trimmed = name.trim();
    if (trimmed.isEmpty()) return false;
    for (const auto& factory : factoryPresets())
        if (factory.name.equalsIgnoreCase(trimmed)) return false;   // the library keeps its names
    if (!userPresetDirectory().createDirectory()) return false;
    juce::XmlElement xml("MillstonePreset");
    xml.setAttribute("name", trimmed);
    for (auto* base : getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(base)) {
            auto* item = xml.createNewChildElement("param");
            item->setAttribute("id", p->getParameterID());
            item->setAttribute("value", (double)p->convertFrom0to1(p->getValue()));
        }
    if (!xml.writeTo(userPresetFile(trimmed))) return false;
    currentPresetName = trimmed;
    return true;
}

bool MillstoneProcessor::loadUserPreset(const juce::String& name) {
    auto xml = juce::XmlDocument::parse(userPresetFile(name));
    if (xml == nullptr || !xml->hasTagName("MillstonePreset")) return false;
    std::vector<std::pair<juce::String, float>> values;
    for (auto* item : xml->getChildWithTagNameIterator("param"))
        values.emplace_back(item->getStringAttribute("id"), (float)item->getDoubleAttribute("value"));
    applyDefaultsThenValues(values);   // a parameter newer than the file gets its default
    currentPresetName = name;
    return true;
}

bool MillstoneProcessor::deleteUserPreset(const juce::String& name) {
    const auto file = userPresetFile(name);
    if (!file.existsAsFile() || !file.deleteFile()) return false;
    if (currentPresetName == name) currentPresetName.clear();   // the sound stays, it just no longer has that name
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MillstoneProcessor(); }
