// Developer check, built only with -DMILLSTONE_UI_SNAPSHOT=ON: plays every factory preset and checks it is valid,
// audible, level-matched and ends. It cannot judge whether a preset sounds good: that takes ears.
// Run: millstone_preset_test [--trim <vst/preset_trim.json>]  (--trim also writes the volume each preset needs)
#include "BinaryData.h"
#include "PluginEditor.h"
#include <complex>
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
    for (const auto& preset : library) testing += preset.unmatched;
    check((int)library.size() - testing >= 65, "the library has at least Init + 64 level-matched, has " + juce::String((int)library.size() - testing));
    printf("%d presets are not level-matched\n", testing);
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
        check(count >= 1 && category != "Testing", category + " is a real type");
    for (const auto& preset : library) {   // filing: every preset can be found, and every tag is one the browser offers
        static const juce::StringArray types { "Init", "Bass", "Lead", "Pad", "Keys", "Pluck", "Perc", "Sequence", "Riser/FX", "Texture", "Drone" };
        static const juce::StringArray styles { "Classic", "Techno", "Dub", "House", "Rave", "Ambient", "Cinematic", "Experimental" };
        static const juce::StringArray characters { "Dark", "Bright", "Warm", "Metallic", "Evolving", "Aggressive", "Soft" };
        check(types.contains(preset.type), preset.name + " has a type: " + preset.type);
        check(preset.name == "Init" || !preset.styles.isEmpty(), preset.name + " has a style");
        check(preset.name == "Init" || !preset.character.isEmpty(), preset.name + " has a character");
        for (const auto& w : preset.styles) check(styles.contains(w), preset.name + " style " + w);
        for (const auto& w : preset.character) check(characters.contains(w), preset.name + " character " + w);
        check(preset.pack.isNotEmpty() && preset.note.isNotEmpty(), preset.name + " has a pack and a note");
    }
    {   // What was worked out from the values, spot checked against sounds whose behaviour is known.
        const auto get = [&](const char* name) -> const MillstoneProcessor::Preset& { for (const auto& q : library) if (q.name == name) return q; return library[0]; };
        check(get("Four On The Floor").uses.contains("Clock") && get("Four On The Floor").plays.contains("Hold key"), "a clocked pattern says it plays by holding a key");
        check(get("Glass Marimba").uses.contains("Elements"), "Elements presets say so");
        check(get("Slow Tide Pad").uses.contains("Tides") && get("Slow Tide Pad").uses.contains("Patch Bay"), "Tides presets say so");
        check(get("Classic Lead").plays.contains("Wheel"), "the mod wheel lead says so");
        check(!get("Round Sub").uses.contains("Patch Bay"), "and an unpatched one does not");
    }

    // Each one, played. Fresh processor: presets must not depend on what came before.
    printf("%-22s %-11s %9s %8s %8s %7s %6s\n", "preset", "category", "loudest", "peak", "treble", "volume", "tail");
    struct Signature { juce::String name; float loud, treble; std::vector<float> map; };
    std::vector<Signature> signatures;
    float closestDistance = 1e9f; juce::String closestPair;
    static constexpr float duplicateDb = 1.0f;
    int fullVolume = 0;
    float worstUnder = 0.0f; juce::String worstUnderName;
    static constexpr float maxUnderDb = 7.0f;
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
        // A picture of the sound for the duplicate check: third-octave bands over time, in dB, whatever its overall level.
        std::vector<float> soundMap;
        {
            constexpr int order = 12, size = 1 << order, bandsCount = 30;
            std::vector<std::complex<double>> f((size_t)size);
            for (size_t at = 0; at + size <= x.size(); at += (size_t)(0.1 * sr)) {
                for (int i = 0; i < size; ++i) f[(size_t)i] = x[at + (size_t)i] * (0.5 - 0.5 * std::cos(6.283185307 * i / size));
                for (int i = 1, j = 0; i < size; ++i) { int bit = size >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(f[(size_t)i], f[(size_t)j]); }
                for (int len = 2; len <= size; len <<= 1) { const auto w = std::polar(1.0, -6.283185307 / len); for (int i = 0; i < size; i += len) { std::complex<double> u(1.0); for (int k = 0; k < len / 2; ++k, u *= w) { const auto c = f[(size_t)(i + k)], d2 = f[(size_t)(i + k + len / 2)] * u; f[(size_t)(i + k)] = c + d2; f[(size_t)(i + k + len / 2)] = c - d2; } } }
                double bands[bandsCount] = {};
                for (int bin = 1; bin < size / 2; ++bin) {
                    const double hz = bin * sr / size;
                    const int band = (int)std::floor(std::log2(juce::jmax(hz, 40.0) / 40.0) * 3.0);
                    if (band < bandsCount) bands[band] += std::norm(f[(size_t)bin]);
                }
                for (double energy : bands) soundMap.push_back((float)juce::jmax(-60.0, 10.0 * std::log10(energy + 1e-9)));
            }
            float top = -1e9f; for (float v : soundMap) top = juce::jmax(top, v);
            for (auto& v : soundMap) v = juce::jmax(-50.0f, v - top);   // silence is the same in every preset, so it must not count
        }
        const float volume = p.apvts.getRawParameterValue("volume")->load();
        printf("%-22s %-11s %9.1f %8.1f %8.1f %7.1f %6.1f\n", preset.name.toRawUTF8(), preset.category.toRawUTF8(), loudDb, peakDb, treble, volume, tail);
        check(!bad, preset.name + " produces only finite samples");
        check(loudDb > -40.0f, preset.name + " is audible");
        check(peakDb < -0.5f || preset.name == "Init", preset.name + " keeps below the output ceiling (peak " + juce::String(peakDb, 1) + ")");
        check(quietBlocks >= 40, preset.name + " ends within 16 s of key up");
        if (preset.name != "Init") {
            const bool capped = volume >= 99.9f && loudDb < targetDb;
            const bool peakLimited = peakDb > peakLimitDb - 1.5f && loudDb < targetDb;
            if (volume >= 99.9f && !preset.unmatched) ++fullVolume;
            if (capped && !preset.unmatched && targetDb - loudDb > worstUnder) { worstUnder = targetDb - loudDb; worstUnderName = preset.name; }
            if (capped) printf("info %s is at full volume, %.1f dB under the target\n", preset.name.toRawUTF8(), targetDb - loudDb);
            // Some depend on the shared random generator, so those are not held to this.
            check(preset.unmatched || capped || peakLimited || std::abs(loudDb - targetDb) <= toleranceDb, preset.name + " is level-matched (" + juce::String(loudDb, 1) + " dB, peak " + juce::String(peakDb, 1) + ", want " + juce::String(targetDb, 0) + ")");
            if (trimFile != juce::File())   // volume is squared in the gain law
                trims->setProperty(preset.name, juce::roundToInt(juce::jlimit(5.0f, 100.0f, volume * std::pow(10.0f, juce::jmin(targetDb - loudDb, peakLimitDb - peakDb) / 40.0f)) * 10) / 10.0);
        }
        for (const auto& s : signatures) {   // two presets that are really one sound waste a place in the library
            float distance = 0; int cells = 0;
            for (size_t k = 0; k < soundMap.size() && k < s.map.size(); ++k)
                if (soundMap[k] > -50.0f || s.map[k] > -50.0f) { distance += std::abs(soundMap[k] - s.map[k]); ++cells; }
            distance /= (float)juce::jmax(1, cells);
            if (distance < closestDistance) { closestDistance = distance; closestPair = preset.name + " and " + s.name; }
            if (distance < 3.0f) printf("info %s and %s sound close: %.1f dB apart\n", preset.name.toRawUTF8(), s.name.toRawUTF8(), distance);
            check(distance >= duplicateDb, preset.name + " and " + s.name + " are not the same sound (" + juce::String(distance, 2) + " dB apart)");
        }
        signatures.push_back({ preset.name, loudDb, treble, std::move(soundMap) });
    }
    printf("info the closest two sounds are %s, %.1f dB apart\n", closestPair.toRawUTF8(), closestDistance);
    printf("%d presets are at full volume\n", fullVolume);
    // Level and VOLUME both top out, so a thin or very filtered sound can sit at full volume a little under the target. That is
    // the engine's reach, not a mistake, but one far under it would be a hole in the library.
    check(worstUnder <= maxUnderDb, "nothing is far quieter than the rest, worst is " + worstUnderName + " at " + juce::String(worstUnder, 1) + " dB under");
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
    // Every cord in a preset has to earn its place: taking it out must change the sound.
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
        printf("%d cords in presets checked\n", cordsChecked);
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
        const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("millstone_browser_test").getChildFile("Presets");
        dir.getParentDirectory().deleteRecursively();
        MillstoneProcessor p;
        p.setUserPresetDirectory(dir);
        MillstoneEditor editor(p);
        std::function<juce::Component*(juce::Component&, const juce::String&)> find = [&](juce::Component& r, const juce::String& id) -> juce::Component* { if (r.getComponentID() == id) return &r; for (auto* c : r.getChildren()) if (auto* hit = find(*c, id)) return hit; return nullptr; };
        const auto click = [&](const juce::String& id) { auto* b = dynamic_cast<juce::Button*>(find(editor, id)); check(b != nullptr, "button exists: " + id); if (b != nullptr && b->onClick) b->onClick(); };
        auto* next = dynamic_cast<juce::Button*>(find(editor, "preset_next"));
        auto* prev = dynamic_cast<juce::Button*>(find(editor, "preset_prev"));
        auto* browserPage = find(editor, "preset_browser");
        auto* list = dynamic_cast<juce::ListBox*>(find(editor, "browser_list"));
        auto* search = dynamic_cast<juce::TextEditor*>(find(editor, "browser_search"));
        check(next && prev && browserPage && list && search, "browser controls exist");
        const auto rows = [&] { return list->getModel()->getNumRows(); };
        const auto tick = [&] { editor.refreshFromProcessor(); };
        check(!browserPage->isVisible(), "the browser starts closed");
        click("preset_open");
        check(browserPage->isVisible(), "the display opens it");
        check(rows() == (int)library.size(), "with every preset listed");
        check(list->getSelectedRow() == 0 && p.getCurrentPresetName() == "Init", "and Init selected, first");

        // Type narrows, a second type widens, another group narrows.
        int bass = 0, lead = 0; for (const auto& q : library) { bass += q.type == "Bass"; lead += q.type == "Lead"; }
        click("chip:Bass"); check(rows() == bass, "TYPE narrows, to " + juce::String(rows()));
        click("chip:Lead"); check(rows() == bass + lead, "a second type adds to it");
        int dubBassLead = 0; for (const auto& q : library) dubBassLead += (q.type == "Bass" || q.type == "Lead") && q.styles.contains("Dub");
        click("chip:Dub"); check(rows() == dubBassLead, "STYLE then narrows that, to " + juce::String(rows()));
        const juce::String before = p.getCurrentPresetName();
        check(before == "Init", "filtering does not change the sound, is " + before);
        click("browser_clear"); check(rows() == (int)library.size(), "CLEAR brings everything back");

        // Search.
        const auto type = [&](const char* text) { search->setText(text, false); search->onTextChange(); };
        type("hold key");
        check(rows() > 0 && rows() < (int)library.size(), "search finds things by what they do, " + juce::String(rows()));
        type("slow"); const int oneWord = rows(); type("slow tide pad"); check(rows() >= 1 && rows() < oneWord, "every word has to match, " + juce::String(rows()) + " of " + juce::String(oneWord));
        type("zzzz"); check(rows() == 0, "nothing found is fine");
        next->onClick(); check(p.getCurrentPresetName() == "Init", "and < > with nothing to step to do nothing");
        type("");

        // Choosing from the list loads it, and < > step through what is left.
        click("chip:Pad");
        const int pads = rows();
        list->selectRow(1);
        const auto second = p.getCurrentPresetName();
        check(second != "Init", "selecting a row loads it, " + second);
        check(browserPage->isVisible(), "and the browser stays open to audition");
        const auto isPad = [&] { for (const auto& q : library) if (q.name == p.getCurrentPresetName()) return q.type == "Pad"; return false; };
        next->onClick(); check(isPad() && p.getCurrentPresetName() != second, "> stays inside the filter");
        for (int i = 0; i < pads + 2; ++i) { next->onClick(); check(isPad(), "stepping wraps inside the filter"); }
        prev->onClick(); check(isPad(), "< as well");
        tick();
        check(list->getSelectedRow() >= 0, "the list follows");
        p.setCurrentProgram(1); tick();
        check(p.getCurrentPresetName() == library[1].name && list->getSelectedRow() < 0, "a host can pick outside the filter; nothing is forced");
        check(rows() == pads, "the filter is not reset by the timer");
        next->onClick(); check(isPad(), "and > returns to it");
        click("browser_clear");

        // Favourites and flags, in files, by name.
        list->selectRow(3);
        const auto starred = p.getCurrentPresetName();
        click("browser_star");
        check(p.isFavourite(starred), "the star makes a favourite");
        check(p.favouritesFile().loadFileAsString().contains(starred), "in its file");
        click("browser_favourites"); check(rows() == 1, "FAVOURITES shows it alone");
        click("browser_favourites");
        { MillstoneProcessor other; other.setUserPresetDirectory(dir); check(other.isFavourite(starred), "another instance sees it"); }
        p.setFlag(starred, "too loud\tfor a pad\nreally");
        check(p.flags()[starred] == "too loud for a pad really", "a flag's note stays on one line, is " + p.flags()[starred]);
        check(p.flagsFile().loadFileAsString().contains(starred + "\ttoo loud"), "and is readable in the file");
        click("browser_flagged"); check(rows() == 1, "FLAGGED shows it alone");
        click("browser_flagged");
        p.clearFlag(starred); check(p.flags().empty(), "a flag can be removed");
        click("browser_star"); check(!p.isFavourite(starred), "the star comes off again");

        // User presets are in the list, under the User pack.
        check(p.saveUserPreset("Mine"), "a user preset saves");
        click("preset_open");
        check(rows() == (int)library.size() + 1, "and is listed");
        click("chip:User"); check(rows() == 1, "the User pack holds it");
        click("browser_clear");
        p.deleteUserPreset("Mine");

        click("browser_close"); check(!browserPage->isVisible(), "CLOSE closes");
        click("browser_clear");
        click("preset_open"); click("rack_toggle"); check(!browserPage->isVisible() && p.rackOpen, "switching page closes the browser");
        click("rack_browse"); check(browserPage->isVisible() && browserPage->getHeight() > 1000, "the rack opens it too, at the rack's size");
        click("browser_close"); click("rack_back");
        p.browserFilter.types.add("Bass");
        {
            MillstoneEditor reopened(p);
            auto* l = dynamic_cast<juce::ListBox*>(find(reopened, "browser_list"));
            auto* open = dynamic_cast<juce::Button*>(find(reopened, "preset_open"));
            open->onClick();
            check(l->getModel()->getNumRows() == bass, "the filter survives closing the window");
        }
        p.browserFilter = {};
        dir.getParentDirectory().deleteRecursively();
    }
    {   // A big library must still be quick to parse. This is the 127 we have, scaled by what 1200 would cost.
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const auto json = juce::JSON::parse(juce::String::fromUTF8(BinaryData::presets_json, BinaryData::presets_jsonSize));
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        printf("info parsing %d presets: %.1f ms, about %.0f ms at 1200\n", (int)library.size(), ms, ms * 1200.0 / (double)library.size());
    }
    printf("\n%s (%d failed)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
