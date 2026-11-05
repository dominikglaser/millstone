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
                 value("fx_rv_type"), value("fx_rv_decay"), value("fx_rv_diff"), value("fx_rv_damp"), value("fx_rv_kick"), value("fx_rv_mix"), value("ck_bpm"), value("fx_out") };
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
    delaySecondsNow.store(s.delay.timeSeconds);
    s.delay.repeats = percent(fxValues.dlFb);
    s.delay.send = option(fxValues.dlIn) == 0 ? 1.0f : 0.0f;   // OPEN: everything goes in. CLOSED: only what THROW lets in.
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
    s.outputGain = std::pow(10.0f, fxValues.output->load() * 0.05f);
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
    state.setProperty("fxPreset", currentFxRack, nullptr);
    state.setProperty("fxLock", fxLock, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, dest);
}

void MillstoneProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto state = juce::ValueTree::fromXml(*xml);
        currentPresetName = state.getProperty("preset").toString();   // empty for sets saved before presets existed
        currentFxRack = state.getProperty("fxPreset").toString();
        fxLock = (bool)state.getProperty("fxLock", true);
        currentFxValues.clear();
        for (const auto& rack : allFxRacks()) if (rack.name == currentFxRack) for (const auto& [key, value] : rack.values) currentFxValues[key] = value;
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
// ---- FX presets ------------------------------------------------------------------------------------------------------
namespace {
const std::map<juce::String, const param_t*>& paramsByKey() {
    static const auto map = [] { std::map<juce::String, const param_t*> m; for (int i = 0; i < NPARAMS; ++i) m[PARAMS[i].key] = &PARAMS[i]; return m; }();
    return map;
}
juce::StringArray varWords(const juce::var& list) { juce::StringArray result; if (const auto* a = list.getArray()) for (const auto& v : *a) result.add(v.toString()); return result; }
}

const std::vector<MillstoneProcessor::FxRack>& MillstoneProcessor::factoryFxRacks() {
    static const std::vector<FxRack> library = [] {
        std::vector<FxRack> out;
        const auto json = juce::JSON::parse(juce::String::fromUTF8(BinaryData::fx_presets_json, BinaryData::fx_presets_jsonSize));
        if (const auto* list = json["presets"].getArray())
            for (const auto& entry : *list) {
                FxRack rack;
                rack.name = entry["name"].toString();
                rack.note = entry["note"].toString();
                rack.styles = varWords(entry["styles"]);
                rack.uses = varWords(entry["uses"]);
                if (const auto* obj = entry["values"].getDynamicObject())
                    for (const auto& property : obj->getProperties()) {
                        const auto found = paramsByKey().find(property.name.toString());
                        if (found == paramsByKey().end()) { jassertfalse; continue; }
                        float value = (float)property.value;
                        if (property.value.isString())
                            for (int o = 0; o < found->second->nopts; ++o)
                                if (property.value.toString() == found->second->opts[o]) value = (float)o;
                        rack.values.emplace_back(found->first, value);
                    }
                out.push_back(std::move(rack));
            }
        return out;
    }();
    return library;
}

juce::File MillstoneProcessor::fxRackDirectory() const { return userPresetDirectory().getSiblingFile("FX Presets"); }

std::vector<MillstoneProcessor::FxRack> MillstoneProcessor::allFxRacks() const {
    auto all = factoryFxRacks();
    auto files = fxRackDirectory().findChildFiles(juce::File::findFiles, false, "*.millstonefx");
    files.sort();
    for (const auto& file : files) {
        auto xml = juce::XmlDocument::parse(file);
        if (xml == nullptr || !xml->hasTagName("MillstoneFxPreset")) continue;
        FxRack rack;
        rack.name = xml->getStringAttribute("name", file.getFileNameWithoutExtension());
        rack.note = "Your preset";
        rack.user = true;
        if (xml->getStringAttribute("style").isNotEmpty()) rack.styles.add(xml->getStringAttribute("style"));
        for (auto* item : xml->getChildWithTagNameIterator("param")) {
            const auto id = item->getStringAttribute("id");
            const float value = (float)item->getDoubleAttribute("value");
            rack.values.emplace_back(id, value);
            if ((id == "fx_dl_mix" || id == "fx_rv_mix" || id == "fx_ph_mix" || id == "fx_ch_mode") && value > 0.0f)
                rack.uses.add(id == "fx_dl_mix" ? "Delay" : id == "fx_rv_mix" ? "Reverb" : id == "fx_ph_mix" ? "Phaser" : "Chorus");
        }
        all.push_back(std::move(rack));
    }
    return all;
}

bool MillstoneProcessor::loadFxRack(const juce::String& name) {
    for (const auto& rack : allFxRacks()) {
        if (rack.name != name) continue;
        std::map<juce::String, float> wanted;
        for (const auto& [key, value] : rack.values) wanted[key] = value;
        for (auto* base : getParameters()) {
            auto* p = dynamic_cast<juce::RangedAudioParameter*>(base);
            if (p == nullptr || !isFxKey(p->getParameterID())) continue;
            const auto it = wanted.find(p->getParameterID());
            const float target = it != wanted.end() ? p->convertTo0to1(it->second) : p->getDefaultValue();
            if (std::abs(p->getValue() - target) > 1e-6f) { p->beginChangeGesture(); p->setValueNotifyingHost(target); p->endChangeGesture(); }
        }
        currentFxRack = name;
        currentFxValues = wanted;
        for (int unit = 0; unit < 4; ++unit) fxPreset[(size_t)unit] = -1;   // the effects' own screens no longer say what they last loaded (the fifth is Tides)
        return true;
    }
    return false;
}

bool MillstoneProcessor::saveFxRack(const juce::String& name, const juce::String& style) {
    const auto trimmed = name.trim();
    if (trimmed.isEmpty()) return false;
    for (const auto& factory : factoryFxRacks())
        if (factory.name.equalsIgnoreCase(trimmed)) return false;   // the library keeps its names
    if (!fxRackDirectory().createDirectory()) return false;
    juce::XmlElement xml("MillstoneFxPreset");
    xml.setAttribute("name", trimmed);
    xml.setAttribute("style", style);
    for (auto* base : getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(base))
            if (isFxKey(p->getParameterID()) && p->getParameterID() != "fx_dl_throw" && p->getParameterID() != "fx_rv_kick") {   // those two are held by hand, not set
                auto* item = xml.createNewChildElement("param");
                item->setAttribute("id", p->getParameterID());
                item->setAttribute("value", (double)p->convertFrom0to1(p->getValue()));
            }
    if (!xml.writeTo(fxRackDirectory().getChildFile(juce::File::createLegalFileName(trimmed) + ".millstonefx"))) return false;
    currentFxRack = trimmed;
    currentFxValues.clear();
    for (auto* item : xml.getChildWithTagNameIterator("param")) currentFxValues[item->getStringAttribute("id")] = (float)item->getDoubleAttribute("value");
    return true;
}

bool MillstoneProcessor::deleteFxRack(const juce::String& name) {
    for (const auto& file : fxRackDirectory().findChildFiles(juce::File::findFiles, false, "*.millstonefx")) {
        auto xml = juce::XmlDocument::parse(file);
        if (xml != nullptr && xml->getStringAttribute("name", file.getFileNameWithoutExtension()) == name) {
            if (!file.deleteFile()) return false;
            if (currentFxRack == name) currentFxRack.clear();   // the sound stays, it just no longer has that name
            return true;
        }
    }
    return false;
}

// Called by the screen on every tick, so it works from what was loaded, not from the files.
bool MillstoneProcessor::fxRackEdited() const {
    if (currentFxRack.isEmpty()) return false;
    for (auto* base : getParameters()) {
        auto* p = dynamic_cast<juce::RangedAudioParameter*>(base);
        if (p == nullptr || !isFxKey(p->getParameterID()) || p->getParameterID() == "fx_dl_throw" || p->getParameterID() == "fx_rv_kick") continue;
        const auto it = currentFxValues.find(p->getParameterID());
        const float target = it != currentFxValues.end() ? p->convertTo0to1(it->second) : p->getDefaultValue();
        if (std::abs(p->getValue() - target) > 0.002f) return true;
    }
    return false;
}

const std::vector<MillstoneProcessor::Preset>& MillstoneProcessor::factoryPresets() {
    static const std::vector<Preset> library = [] {
        std::vector<Preset> out;
        std::map<juce::String, const param_t*> params;   // a lookup per value of per preset: this is what a big library spends its time on
        for (int i = 0; i < NPARAMS; ++i) params[PARAMS[i].key] = &PARAMS[i];
        const auto words = [](const juce::var& list) { juce::StringArray result; if (const auto* a = list.getArray()) for (const auto& v : *a) result.add(v.toString()); return result; };
        const auto json = juce::JSON::parse(juce::String::fromUTF8(BinaryData::presets_json, BinaryData::presets_jsonSize));
        if (const auto* list = json["presets"].getArray()) {
            out.reserve((size_t)list->size());
            for (const auto& entry : *list) {
                Preset preset;
                preset.name = entry["name"].toString();
                preset.category = entry["category"].toString();
                preset.type = entry["type"].toString();
                preset.pack = entry["pack"].toString();
                preset.note = entry["note"].toString();
                preset.styles = words(entry["styles"]);
                preset.character = words(entry["character"]);
                preset.uses = words(entry["uses"]);
                preset.plays = words(entry["plays"]);
                preset.unmatched = (bool)entry.getProperty("unmatched", false);
                if (const auto* obj = entry["values"].getDynamicObject()) {
                    preset.values.reserve((size_t)obj->getProperties().size());
                    for (const auto& property : obj->getProperties()) {
                        const auto key = property.name.toString();
                        const auto found = params.find(key);
                        if (found == params.end()) { jassertfalse; continue; }
                        const param_t* def = found->second;
                        float value = (float)property.value;
                        if (property.value.isString())
                            for (int o = 0; o < def->nopts; ++o)
                                if (property.value.toString() == def->opts[o]) value = (float)o;
                        preset.values.emplace_back(key, value);
                    }
                }
                out.push_back(std::move(preset));
            }
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
        if (fxLock && isFxKey(p->getParameterID())) continue;     // the effects stay as they are
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

// ---- favourites and flags ----------------------------------------------------------------------------------------
juce::File MillstoneProcessor::favouritesFile() const { return userPresetDirectory().getSiblingFile("favourites.txt"); }
juce::File MillstoneProcessor::flagsFile() const { return userPresetDirectory().getSiblingFile("preset-flags.txt"); }

juce::StringArray MillstoneProcessor::favourites() const {
    juce::StringArray lines;
    favouritesFile().readLines(lines);
    lines.removeEmptyStrings();
    return lines;
}

void MillstoneProcessor::setFavourite(const juce::String& name, bool on) {
    auto lines = favourites();
    lines.removeString(name);
    if (on) lines.add(name);
    favouritesFile().getParentDirectory().createDirectory();
    favouritesFile().replaceWithText(lines.joinIntoString("\n") + "\n");
}

std::map<juce::String, juce::String> MillstoneProcessor::flags() const {
    std::map<juce::String, juce::String> out;
    juce::StringArray lines;
    flagsFile().readLines(lines);
    for (const auto& line : lines)
        if (line.isNotEmpty()) out[line.upToFirstOccurrenceOf("\t", false, false)] = line.fromFirstOccurrenceOf("\t", false, false);
    return out;
}

namespace {
void writeFlags(const juce::File& file, const std::map<juce::String, juce::String>& flags) {
    juce::String text;
    for (const auto& [name, note] : flags) text << name << "\t" << note << "\n";
    file.getParentDirectory().createDirectory();
    file.replaceWithText(text);
}
}

void MillstoneProcessor::setFlag(const juce::String& name, const juce::String& note) {
    auto all = flags();
    all[name] = note.replaceCharacters("\t\r\n", "   ").trim();
    writeFlags(flagsFile(), all);
}

void MillstoneProcessor::clearFlag(const juce::String& name) {
    auto all = flags();
    all.erase(name);
    writeFlags(flagsFile(), all);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MillstoneProcessor(); }
