// Developer check, built only with -DMILLSTONE_UI_SNAPSHOT=ON: plays every factory preset and checks it is valid,
// audible, level-matched and ends. It cannot judge whether a preset sounds good: that takes ears.
// Run: millstone_preset_test [--trim <vst/preset_trim.json>]  (--trim also writes the volume each preset needs)
#include "PluginEditor.h"
#include <cstdio>
#include <map>

static int failures = 0;
static void check(bool ok, const juce::String& what) {
    if (!ok) { printf("FAIL %s\n", what.toRawUTF8()); ++failures; }
}

// Louder than the level we match to, so a preset that cannot reach it shows up.
static constexpr float targetDb = -18.0f, toleranceDb = 2.5f;
// A short hit has little energy in 100 ms, so matching it on level alone would drive it into the ceiling.
static constexpr float peakLimitDb = -4.0f;

static int noteFor(const juce::String& category) {
    if (category == "Bass") return 36;
    if (category == "Pad" || category == "Texture") return 48;
    return 60;
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    const auto& library = MillstoneProcessor::factoryPresets();
    const juce::File trimFile = argc > 2 ? juce::File(juce::String(argv[2])) : juce::File();
    auto trims = std::make_unique<juce::DynamicObject>();
    if (trimFile != juce::File() && trimFile.existsAsFile())
        if (auto* o = juce::JSON::parse(trimFile).getDynamicObject())
            for (const auto& p : o->getProperties()) trims->setProperty(p.name, p.value);

    // The library itself.
    std::map<juce::String, int> perCategory;
    juce::StringArray names;
    int testing = 0;
    for (const auto& preset : library) testing += preset.category == "Testing";
    check((int)library.size() - testing == 65, "65 library presets: Init + 64, have " + juce::String((int)library.size() - testing));
    printf("%d presets in Testing\n", testing);
    check(!library.empty() && library[0].name == "Init", "Init comes first");
    MillstoneProcessor probe;
    int exposed = 0;
    for (auto* q : probe.getParameters()) exposed += dynamic_cast<juce::RangedAudioParameter*>(q) != nullptr;
    for (const auto& preset : library) {
        ++perCategory[preset.category];
        check(!names.contains(preset.name), "unique name: " + preset.name);
        names.add(preset.name);
        check(preset.name.length() <= 24, "name fits the MPC's 24 characters: " + preset.name);
        check((int)preset.values.size() == exposed, preset.name + " lists every parameter (" + juce::String((int)preset.values.size()) + " of " + juce::String(exposed) + ")");
        for (const auto& [key, value] : preset.values) {
            auto* p = probe.apvts.getParameter(key);
            check(p != nullptr, preset.name + " names a parameter that exists: " + key);
            if (p == nullptr) continue;
            const auto range = p->getNormalisableRange();
            check(value >= range.start && value <= range.end, preset.name + ": " + key + " in range");
        }
    }
    for (const auto& [category, count] : perCategory)
        if (category != "Init" && category != "Testing") check(count == 8, category + " has 8 presets, has " + juce::String(count));

    // Each one, played. Fresh processor: presets must not depend on what came before.
    printf("%-22s %-11s %9s %8s %8s %7s %6s\n", "preset", "category", "loudest", "peak", "treble", "volume", "tail");
    struct Signature { juce::String name; float loud, treble; };
    std::vector<Signature> signatures;
    int fullVolume = 0;
    for (int i = 0; i < (int)library.size(); ++i) {
        const auto& preset = library[(size_t)i];
        MillstoneProcessor p;
        const double sr = 44100; const int block = 256;
        p.setRateAndBufferSizeDetails(sr, block);
        p.prepareToPlay(sr, block);
        if (i > 0) p.setCurrentProgram(i);
        check(p.getCurrentPresetName() == preset.name, preset.name + " loads by name");
        for (const auto& [key, value] : preset.values) {
            auto* q = p.apvts.getParameter(key);
            check(q != nullptr && std::abs(q->convertFrom0to1(q->getValue()) - value) <= 0.5f + std::abs(value) * 0.001f, preset.name + " sets " + key);
        }
        juce::MidiBuffer m;
        const int note = noteFor(preset.category);
        m.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)100), 0);
        juce::AudioBuffer<float> b(2, block);
        std::vector<float> x;
        const int held = (int)(6.0 * sr);
        while ((int)x.size() < held) { b.clear(); p.processBlock(b, m); m.clear(); x.insert(x.end(), b.getReadPointer(0), b.getReadPointer(0) + block); }
        m.addEvent(juce::MidiMessage::noteOff(1, note), 0);
        double tail = 0; bool bad = false; float peak = 0;
        for (float v : x) { if (!std::isfinite(v)) bad = true; peak = juce::jmax(peak, std::abs(v)); }
        int quietBlocks = 0;
        for (int n = 0; n < (int)(16.0 * sr / block) && quietBlocks < 40; ++n) {
            b.clear(); p.processBlock(b, m); m.clear();
            float top = 0; for (int s = 0; s < block; ++s) { const float v = b.getSample(0, s); if (!std::isfinite(v)) bad = true; top = juce::jmax(top, std::abs(v)); peak = juce::jmax(peak, std::abs(v)); }
            quietBlocks = top < 3e-5f ? quietBlocks + 1 : 0;
            tail = (n + 1) * block / sr;
        }
        // Loudest 50 ms: a percussive preset is judged on its hit, a pad on its bloom.
        const size_t w = (size_t)(0.05 * sr); double loud = 0;
        for (size_t a = 0; a + w < x.size(); a += w / 2) { double e = 0; for (size_t k = a; k < a + w; ++k) e += x[k] * x[k]; loud = juce::jmax(loud, std::sqrt(e / (double)w)); }
        double d = 0, e = 0; for (size_t k = 2; k < x.size(); ++k) { const double v = x[k] - 2 * x[k - 1] + x[k - 2]; d += v * v; e += x[k] * x[k]; }
        const float treble = 20 * std::log10((float)std::sqrt(d / juce::jmax(1e-12, e)) + 1e-6f);
        const float loudDb = 20 * std::log10((float)loud + 1e-9f), peakDb = 20 * std::log10(peak + 1e-9f);
        const float volume = p.apvts.getRawParameterValue("volume")->load();
        printf("%-22s %-11s %9.1f %8.1f %8.1f %7.1f %6.1f\n", preset.name.toRawUTF8(), preset.category.toRawUTF8(), loudDb, peakDb, treble, volume, tail);
        check(!bad, preset.name + " produces only finite samples");
        check(loudDb > -40.0f, preset.name + " is audible");
        check(peakDb < -0.5f || preset.name == "Init", preset.name + " keeps below the output ceiling (peak " + juce::String(peakDb, 1) + ")");
        check(quietBlocks >= 40, preset.name + " ends within 16 s of key up");
        if (preset.name != "Init") {
            const bool capped = volume >= 99.9f && loudDb < targetDb;
            const bool peakLimited = peakDb > peakLimitDb - 1.5f && loudDb < targetDb;
            if (volume >= 99.9f && preset.category != "Testing") ++fullVolume;
            if (capped) printf("info %s is at full volume, %.1f dB under the target\n", preset.name.toRawUTF8(), targetDb - loudDb);
            // Testing presets are temporary and some depend on the shared random generator, so only the library is held to this.
            check(preset.category == "Testing" || capped || peakLimited || std::abs(loudDb - targetDb) <= toleranceDb, preset.name + " is level-matched (" + juce::String(loudDb, 1) + " dB, peak " + juce::String(peakDb, 1) + ", want " + juce::String(targetDb, 0) + ")");
            if (trimFile != juce::File())   // volume is squared in the gain law
                trims->setProperty(preset.name, juce::roundToInt(juce::jlimit(5.0f, 100.0f, volume * std::pow(10.0f, juce::jmin(targetDb - loudDb, peakLimitDb - peakDb) / 40.0f)) * 10) / 10.0);
        }
        for (const auto& s : signatures)
            if (std::abs(s.loud - loudDb) < 0.05f && std::abs(s.treble - treble) < 0.05f)
                printf("info %s and %s measure alike\n", preset.name.toRawUTF8(), s.name.toRawUTF8());
        signatures.push_back({ preset.name, loudDb, treble });
    }
    printf("%d presets are at full volume\n", fullVolume);
    check(fullVolume <= 10, "VOLUME keeps headroom on nearly every preset");
    if (trimFile != juce::File()) trimFile.replaceWithText(juce::JSON::toString(juce::var(trims.release()), false) + "\n");

    // Loading.
    {
        MillstoneProcessor p;
        p.setCurrentProgram(5);
        const auto fifth = p.getCurrentPresetName();
        auto* cutoff = p.apvts.getParameter("cutoff");
        cutoff->setValueNotifyingHost(0.123f);
        p.setCurrentProgram(5);
        check(std::abs(cutoff->getValue() - 0.123f) < 1e-4f, "re-selecting the current preset keeps tweaks");
        juce::MemoryBlock state; p.getStateInformation(state);
        MillstoneProcessor q; q.setStateInformation(state.getData(), (int)state.getSize());
        check(q.getCurrentPresetName() == fifth, "the preset name survives save and reload");
        check(std::abs(q.apvts.getParameter("cutoff")->getValue() - 0.123f) < 1e-4f, "tweaks survive save and reload");
        check(q.getCurrentProgram() == 5, "the host sees the right program after reload");
    }
    // Every cord in a Testing preset has to earn its place: taking it out must change the sound.
    {
        const auto play = [&](int program, int withoutCord) {
            MillstoneProcessor p;
            p.setRateAndBufferSizeDetails(44100, 256);
            p.prepareToPlay(44100, 256);
            p.setCurrentProgram(program);
            if (withoutCord >= 0) p.disconnectCord(withoutCord);
            juce::MidiBuffer m;
            m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
            m.addEvent(juce::MidiMessage::controllerEvent(1, 1, 127), 0);        // wheel
            m.addEvent(juce::MidiMessage::channelPressureChange(1, 90), 0);      // pressure
            juce::AudioBuffer<float> b(2, 256);
            std::vector<float> x;
            for (int i = 0; i < 700; ++i) { b.clear(); p.processBlock(b, m); m.clear(); x.insert(x.end(), b.getReadPointer(0), b.getReadPointer(0) + 256); }
            return x;
        };
        int cordsChecked = 0;
        for (int i = 0; i < (int)library.size(); ++i) {
            if (library[(size_t)i].category != "Testing") continue;
            MillstoneProcessor probeCords;
            probeCords.setCurrentProgram(i);
            const auto whole = play(i, -1);
            double energy = 0; for (float v : whole) energy += v * v;
            check(energy > 0.01, library[(size_t)i].name + " makes sound");
            for (int c = 0; c < MillstoneProcessor::numCords; ++c) {
                if (probeCords.cordSource(c) == 0 || probeCords.cordDest(c) == 0) continue;
                ++cordsChecked;
                const auto without = play(i, c);
                double d = 0; for (size_t k = 0; k < whole.size(); ++k) d += std::abs(whole[k] - without[k]);
                d /= (double)whole.size();
                check(d > 1e-4, library[(size_t)i].name + ": cord " + juce::String(c + 1) + " changes the sound, by " + juce::String(d, 6));
            }
        }
        printf("%d cords in Testing presets checked\n", cordsChecked);
    }

    // Loading writes each parameter once, and leaves alone what is not changing.
    {
        struct Watch : juce::AudioProcessorParameter::Listener {
            int writes = 0;
            void parameterValueChanged(int, float) override { ++writes; }
            void parameterGestureChanged(int, bool) override {}
        };
        const auto findPreset = [&](const char* name) { for (int i = 0; i < (int)library.size(); ++i) if (library[(size_t)i].name == name) return i; return -1; };
        MillstoneProcessor p;
        p.setCurrentProgram(findPreset("Classic Lead"));
        Watch volume, mode, cord;
        p.apvts.getParameter("volume")->addListener(&volume);
        p.apvts.getParameter("voice_mode")->addListener(&mode);
        p.apvts.getParameter("c1_src")->addListener(&cord);
        p.setCurrentProgram(findPreset("Acid Line"));   // LEGATO to LEGATO, a different volume, no cords in either
        check(volume.writes == 1, "a load writes VOLUME once, not via its default, got " + juce::String(volume.writes));
        check(mode.writes == 0, "a load leaves an unchanged voice mode alone, got " + juce::String(mode.writes));
        check(cord.writes == 0, "a load leaves an unchanged cord alone, got " + juce::String(cord.writes));
        p.apvts.getParameter("volume")->removeListener(&volume);
        p.apvts.getParameter("voice_mode")->removeListener(&mode);
        p.apvts.getParameter("c1_src")->removeListener(&cord);

        // A note held across a load with the same voice mode keeps sounding.
        MillstoneProcessor q;
        q.setRateAndBufferSizeDetails(44100, 256);
        q.prepareToPlay(44100, 256);
        q.setCurrentProgram(findPreset("Classic Lead"));
        juce::MidiBuffer m;
        m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
        juce::AudioBuffer<float> b(2, 256);
        for (int i = 0; i < 80; ++i) { b.clear(); q.processBlock(b, m); m.clear(); }
        q.setCurrentProgram(findPreset("Acid Line"));
        float tail = 0;
        for (int i = 0; i < 120; ++i) { b.clear(); q.processBlock(b, m); if (i > 60) tail = juce::jmax(tail, b.getMagnitude(0, 256)); }
        check(tail > 1e-3f, "a held note survives loading a preset with the same voice mode, peak " + juce::String(tail, 4));

        // A saved preset from before a parameter existed gets that parameter's default.
        const auto dir = juce::File::createTempFile("millstone_old_preset");
        dir.createDirectory();
        dir.getChildFile("old.millstone").replaceWithText("<MillstonePreset name=\"Old\"><param id=\"cutoff\" value=\"33\"/></MillstonePreset>");
        MillstoneProcessor u;
        u.setUserPresetDirectory(dir);
        u.connectCord(1, 2);
        u.apvts.getParameter("t_rate")->setValueNotifyingHost(0.9f);
        check(u.loadUserPreset("Old"), "an older user preset still loads");
        auto* uc = u.apvts.getParameter("cutoff"), *ut = u.apvts.getParameter("t_rate");
        check(std::abs(uc->convertFrom0to1(uc->getValue()) - 33.0f) < 0.01f && std::abs(ut->getValue() - ut->getDefaultValue()) < 1e-5f && u.cordSource(0) == 0, "and fills in what it does not list");
        dir.deleteRecursively();
    }
    // No leaks between presets: anything one sets is restored by the next.
    {
        MillstoneProcessor a, b;
        a.setCurrentProgram(10); a.setCurrentProgram(40);
        b.setCurrentProgram(40);
        int differ = 0;
        for (auto* q : a.getParameters()) { auto* r = dynamic_cast<juce::RangedAudioParameter*>(q); if (r && std::abs(r->getValue() - b.apvts.getParameter(r->getParameterID())->getValue()) > 1e-5f) ++differ; }
        check(differ == 0, "presets do not leak into each other, " + juce::String(differ) + " differ");
    }
    // User presets, in a scratch folder.
    {
        const auto dir = juce::File::createTempFile("millstone_presets");
        dir.createDirectory();
        MillstoneProcessor p; p.setUserPresetDirectory(dir);
        check(!p.saveUserPreset("Init"), "cannot take a factory name");
        check(!p.saveUserPreset("   "), "cannot save without a name");
        p.apvts.getParameter("cutoff")->setValueNotifyingHost(0.25f);
        check(p.saveUserPreset("My: Sound/1"), "saves a name a filesystem cannot hold");
        check(p.userPresetNames().contains("My: Sound/1"), "it lists under its real name");
        check(p.getCurrentPresetName() == "My: Sound/1", "and becomes current");
        p.apvts.getParameter("cutoff")->setValueNotifyingHost(0.9f);
        check(p.loadUserPreset("My: Sound/1"), "loads back");
        check(std::abs(p.apvts.getParameter("cutoff")->getValue() - 0.25f) < 1e-3f, "with its values");
        check(p.saveUserPreset("My: Sound/1") && p.userPresetNames().size() == 1, "saving again replaces it");
        check(p.deleteUserPreset("My: Sound/1") && p.userPresetNames().isEmpty(), "deletes");
        check(p.getCurrentPresetName().isEmpty(), "deleting the current one clears the name");
        dir.deleteRecursively();
    }
    // The browser.
    {
        MillstoneProcessor p;
        MillstoneEditor editor(p);
        std::function<juce::Component*(juce::Component&, const juce::String&)> find = [&](juce::Component& r, const juce::String& id) -> juce::Component* { if (r.getComponentID() == id) return &r; for (auto* c : r.getChildren()) if (auto* h = find(*c, id)) return h; return nullptr; };
        auto* list = dynamic_cast<juce::ComboBox*>(find(editor, "preset_list"));
        auto* cat = dynamic_cast<juce::ComboBox*>(find(editor, "preset_category"));
        auto* next = dynamic_cast<juce::Button*>(find(editor, "preset_next"));
        auto* prev = dynamic_cast<juce::Button*>(find(editor, "preset_prev"));
        check(list && cat && next && prev, "browser controls exist");
        check(list->getNumItems() == (int)library.size(), "All lists every preset");
        check(list->getText() == "Init", "starts on Init");
        next->onClick(); check(p.getCurrentPresetName() == library[1].name, "> steps forward");
        prev->onClick(); prev->onClick(); check(p.getCurrentPresetName() == library.back().name, "< wraps around");
        int padId = 0; for (int i = 0; i < cat->getNumItems(); ++i) if (cat->getItemText(i) == "Pad") padId = cat->getItemId(i);
        cat->setSelectedId(padId, juce::sendNotificationSync);
        check(list->getNumItems() == 8, "a category narrows the list to 8");
        check(p.getCurrentPresetName() == "Warm Strings", "and loads its first preset, is " + p.getCurrentPresetName());
        next->onClick(); check(p.getCurrentPresetName() == "Analog Pad", "> stays inside the category");
        const auto tick = [&] { editor.refreshFromProcessor(); };
        p.setCurrentProgram(1);   // the host picks a preset outside the category being shown
        tick();
        check(list->getText() == library[1].name && cat->getText() == "All", "the browser follows the host, shows " + list->getText());
        cat->setSelectedId(padId, juce::sendNotificationSync);
        const int stayed = list->getNumItems();
        tick();
        check(cat->getText() == "Pad" && stayed == 8, "the filter is not reset by the timer");
    }
    printf("\n%s (%d failed)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
