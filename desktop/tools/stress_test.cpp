// Developer check, built only with -DMILLSTONE_UI_SNAPSHOT=ON: the plugin under conditions a host can create.
// Every parameter must do something, instances must not disturb each other, and nothing may break while audio is running.
#include "PluginEditor.h"
#include <atomic>
#include <cstdio>
#include <thread>

static int failures = 0;
static void check(bool ok, const juce::String& what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8()); if (!ok) ++failures; }

struct Head : juce::AudioPlayHead {
    double ppq = 3.3;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setBpm(120.0); i.setPpqPosition(ppq); i.setIsPlaying(true); return i; }
};

static void setValue(MillstoneProcessor& p, const juce::String& key, float v) { auto* q = p.apvts.getParameter(key); jassert(q); q->setValueNotifyingHost(q->convertTo0to1(v)); }
static void setOption(MillstoneProcessor& p, const juce::String& key, const juce::String& v) { auto* q = p.apvts.getParameter(key); const int i = q->getAllValueStrings().indexOf(v); jassert(i >= 0); q->setValueNotifyingHost(q->convertTo0to1((float)i)); }
static int option(MillstoneProcessor& p, const juce::String& key, const juce::String& v) { return p.apvts.getParameter(key)->getAllValueStrings().indexOf(v); }

// Sets up whatever a parameter needs before it can be heard.
static void contextFor(MillstoneProcessor& p, const juce::String& key) {
    const auto connect = [&](const char* from, const char* to, float amount) { p.connectCord(option(p, "c1_src", from), option(p, "c1_dst", to), amount); };
    setValue(p, "cutoff", 50); setValue(p, "reso", 20);
    setOption(p, "o2_on", "ON"); setOption(p, "o3_on", "ON");
    if (key.startsWith("e_") || key == "o1_type") {
        setOption(p, "o1_type", "ELEMENTS"); setOption(p, "vca_mode", "ON");
        setValue(p, "e_strike", 70); setValue(p, "e_bow", 40); setValue(p, "e_blow", 40); setValue(p, "e_space", 30);
        setOption(p, "o2_on", "OFF"); setOption(p, "o3_on", "OFF");
    } else if (key.startsWith("t_")) {
        setOption(p, "t_trig", key == "t_div" ? "CLOCK" : "FREE"); setOption(p, "t_mode", "LOOP"); setOption(p, "t_out", "PHASE");
        setValue(p, "t_rate", 55); setValue(p, "t_shift", 70); setValue(p, "t_slope", 30); setValue(p, "t_shape", 30); setValue(p, "t_smooth", 30);
        connect("TIDES 1", "CUTOFF", 60); connect("TIDES 2", "O1 PITCH", 20); connect("TIDES 3", "O1 TIMBRE", 50);
    } else if (key.startsWith("ck_") || (key.startsWithChar('k') && key.containsChar('_'))) {
        const int channel = key.startsWith("ck_") ? 1 : key.substring(1, 2).getIntValue();
        const juce::String k = "k" + juce::String(channel) + "_";
        connect(("CLOCK " + juce::String(channel)).toRawUTF8(), "CUTOFF", 60);
        setOption(p, k + "div", "x2");
        if (key.endsWith("_fill") || key.endsWith("_rot")) { setValue(p, k + "steps", 8); setValue(p, k + "fill", 3); }
        if (key.endsWith("_steps")) setValue(p, k + "fill", 3);
        if (key.endsWith("_shape")) setOption(p, k + "div", "x1");
        if (key.endsWith("_offset") || key.endsWith("_level")) setValue(p, k + "level", 60);
        if (key.endsWith("_swing")) setOption(p, k + "div", "x4");
    } else if (key.startsWithChar('c') && (key.endsWith("_src") || key.endsWith("_dst") || key.endsWith("_amt"))) {
        const juce::String stem = key.upToLastOccurrenceOf("_", false, false);
        setOption(p, stem + "_src", "LFO"); setOption(p, stem + "_dst", "CUTOFF"); setValue(p, stem + "_amt", 80);
    } else if (key.startsWith("o1_") || key.startsWith("o2_") || key.startsWith("o3_")) {
        const auto o = key.substring(0, 3);
        setValue(p, o + "fm", 40); setValue(p, o + "timbre_mod", 60); setValue(p, o + "morph_mod", 60); setValue(p, "cutoff", 90);
        if (key.endsWith("_mix")) setOption(p, o + "engine", "Waveshaper");
        if (key.endsWith("_amp")) { setValue(p, "attack", 40); setValue(p, "lpg_decay", 30); }
        if (key.endsWith("_harm") || key.endsWith("_timbre") || key.endsWith("_morph")) setOption(p, o + "engine", "FM 2-Op");
    } else if (key == "lfo_retrig" || key == "lfo_rate" || key == "lfo_shape" || key == "lfo_sync" || key == "lfo_div" || key == "lfo_lock" || key == "f_src" || key == "wheel_lfo" || key == "at_lfo") {
        setValue(p, "f_mod", 100); setOption(p, "f_src", "LFO");
        if (key == "lfo_div" || key == "lfo_lock") setOption(p, "lfo_sync", "ON");
        if (key == "lfo_lock") setOption(p, "lfo_div", "1 BAR");
        if (key == "at_lfo") setValue(p, "wheel_lfo", 50);
    } else if (key == "lpg_decay" || key == "lpg_colour") {
        setOption(p, "vca_mode", "LPG");
    } else if (key.startsWith("m_") ) {
        setValue(p, "f_mod", 100); setValue(p, "m_attack", 40); setValue(p, "m_decay", 40); setValue(p, "m_sustain", 40); setValue(p, "m_release", 50);
    } else if (key == "f_mod") {
        setOption(p, "f_src", "ENV");
    } else if (key == "glide") {
        setOption(p, "voice_mode", "MONO");
    } else if (key == "keytrack") {
        setValue(p, "reso", 60);
    }
}

// A little performance that gives everything a chance to be heard.
static std::vector<float> perform(MillstoneProcessor& p, int blockSize = 256, Head* head = nullptr) {
    p.setRateAndBufferSizeDetails(44100, blockSize);
    p.prepareToPlay(44100, blockSize);
    Head own;
    if (head == nullptr) head = &own;
    p.setPlayHead(head);
    juce::AudioBuffer<float> b(2, blockSize);
    juce::MidiBuffer m;
    std::vector<float> out;
    const auto run = [&](double seconds) { const int blocks = (int)(seconds * 44100 / blockSize); for (int i = 0; i < blocks; ++i) { b.clear(); p.processBlock(b, m); m.clear(); head->ppq += blockSize / 44100.0 * 2.0; out.insert(out.end(), b.getReadPointer(0), b.getReadPointer(0) + blockSize); } };
    m.addEvent(juce::MidiMessage::controllerEvent(1, 1, 90), 0);
    m.addEvent(juce::MidiMessage::pitchWheel(1, 12000), 0);
    m.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8)70), 0);
    run(0.25);
    m.addEvent(juce::MidiMessage::noteOn(1, 74, (juce::uint8)100), 0);
    m.addEvent(juce::MidiMessage::channelPressureChange(1, 100), 0);
    run(0.9);
    m.addEvent(juce::MidiMessage::noteOff(1, 67), 0);
    m.addEvent(juce::MidiMessage::noteOff(1, 74), 0);
    run(0.5);
    m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    run(0.4);
    p.setPlayHead(nullptr);
    return out;
}

static double difference(const std::vector<float>& a, const std::vector<float>& b) {
    const size_t n = juce::jmin(a.size(), b.size());
    double d = 0;
    for (size_t i = 0; i < n; ++i) d += std::abs(a[i] - b[i]);
    return d / (double)juce::jmax<size_t>(1, n);
}

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    { // Every parameter a host can see has to do something.
        MillstoneProcessor list;
        juce::StringArray dead;
        int tried = 0;
        for (auto* base : list.getParameters()) {
            auto* q = dynamic_cast<juce::RangedAudioParameter*>(base);
            if (q == nullptr) continue;
            const auto key = q->getParameterID();
            const auto render = [&](float normalised) {
                MillstoneProcessor p;
                contextFor(p, key);
                auto* r = p.apvts.getParameter(key);
                r->setValueNotifyingHost(normalised);
                return perform(p);
            };
            ++tried;
            // Options are compared all against the first, as the last is not always the one that shows: T RATE into an empty slot, say.
            const auto first = render(0.0f);
            double best = difference(first, render(1.0f));
            if (best <= 2e-5 && q->getNumSteps() > 2 && q->getNumSteps() < 60) for (int i = 1; i < q->getNumSteps() - 1 && best <= 2e-5; ++i) best = juce::jmax(best, difference(first, render((float)i / (float)(q->getNumSteps() - 1))));
            if (best <= 2e-5) dead.add(key);
        }
        printf("%d parameters swept\n", tried);
        check(dead.isEmpty(), "every parameter changes the sound" + (dead.isEmpty() ? juce::String() : ", dead: " + dead.joinIntoString(" ")));
    }
    { // Two instances must not disturb each other. Virtual Analog has no random numbers in it, so it must repeat exactly.
        const auto setUp = [](MillstoneProcessor& p, int blockSize) { setValue(p, "cutoff", 55); setOption(p, "o2_on", "OFF"); p.setRateAndBufferSizeDetails(44100, blockSize); p.prepareToPlay(44100, blockSize); };
        MillstoneProcessor solo; setUp(solo, 256);
        MillstoneProcessor a; setUp(a, 256);
        MillstoneProcessor b; setUp(b, 256);
        setOption(b, "o1_on", "OFF"); setOption(b, "o3_on", "ON"); setOption(b, "o3_engine", "Swarm"); setValue(b, "cutoff", 90); setValue(b, "noise", 50);
        b.connectCord(option(b, "c1_src", "LFO"), option(b, "c1_dst", "CUTOFF"), 80);
        juce::AudioBuffer<float> x(2, 256), y(2, 256), z(2, 256);
        juce::MidiBuffer ma, mb, mc;
        ma.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0); mb = ma; mc = ma;
        double diff = 0;
        for (int i = 0; i < 300; ++i) {
            x.clear(); y.clear(); z.clear();
            solo.processBlock(x, ma);
            a.processBlock(y, mb);
            b.processBlock(z, mc);
            ma.clear(); mb.clear(); mc.clear();
            for (int s = 0; s < 256; ++s) diff += std::abs(x.getSample(0, s) - y.getSample(0, s));
        }
        check(diff < 1e-6, "one instance does not affect another, difference " + juce::String(diff));
        setOption(b, "o2_on", "ON");
        check(a.apvts.getRawParameterValue("o1_on")->load() > 0.5f && a.apvts.getRawParameterValue("o2_on")->load() < 0.5f, "switches belong to their own instance");
    }
    { // The block size a host happens to use must not change the sound.
        const auto play = [](int blockSize) { MillstoneProcessor p; setValue(p, "cutoff", 55); Head head; head.ppq = 0; return perform(p, blockSize, &head); };
        const auto base = play(256);
        for (int size : {64, 128, 512, 1024}) { auto other = play(size); auto first = base; first.resize(10000); other.resize(10000); /* up to the second note: MIDI lands on block starts, so later notes differ by design */ double d = difference(first, other); check(d < 1e-5, "block size " + juce::String(size) + " sounds the same, difference " + juce::String(d, 6)); }
        // Changing size mid-stream is what happens when a host re-prepares.
        MillstoneProcessor p; p.setRateAndBufferSizeDetails(44100, 256); p.prepareToPlay(44100, 256);
        juce::Random r(7); juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
        bool ok = true;
        for (int i = 0; i < 2000; ++i) { const int n = 1 + r.nextInt(4096); juce::AudioBuffer<float> b(2, n); b.clear(); p.processBlock(b, m); m.clear(); for (int s = 0; s < n; ++s) if (!std::isfinite(b.getSample(0, s)) || std::abs(b.getSample(0, s)) > 1.001f) ok = false; }
        check(ok, "random block sizes from 1 to 4096 stay valid");
        for (double rate : {22050.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0}) { p.prepareToPlay(rate, 512); juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer note; note.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0); double e = 0; for (int i = 0; i < 40; ++i) { b.clear(); p.processBlock(b, note); note.clear(); e += b.getRMSLevel(0, 0, 512); } check(e > 0.01, "sounds after re-preparing at " + juce::String(rate / 1000, 1) + " kHz"); }
    }
    { // Save and reload every parameter, at random values.
        juce::Random r(11);
        int bad = 0;
        for (int round = 0; round < 20; ++round) {
            MillstoneProcessor a;
            for (auto* q : a.getParameters()) q->setValueNotifyingHost(r.nextFloat());
            juce::MemoryBlock state; a.getStateInformation(state);
            MillstoneProcessor b; b.setStateInformation(state.getData(), (int)state.getSize());
            for (auto* q : a.getParameters()) { auto* ra = dynamic_cast<juce::RangedAudioParameter*>(q); if (ra && std::abs(ra->getValue() - b.apvts.getParameter(ra->getParameterID())->getValue()) > 1e-5f) ++bad; }
        }
        check(bad == 0, "random settings survive save and reload, " + juce::String(bad) + " differ");
        juce::MemoryBlock junk; junk.setSize(4096); r.fillBitsRandomly(junk.getData(), 4096);
        MillstoneProcessor p; p.setStateInformation(junk.getData(), (int)junk.getSize());
        check(true, "corrupt saved state does not crash");
        check(p.apvts.getParameter("cutoff") != nullptr, "and the plugin is intact after it");
    }
    { // The audio thread keeps running while the message thread does everything a user can.
        MillstoneProcessor p;
        p.setRateAndBufferSizeDetails(44100, 128);
        p.prepareToPlay(44100, 128);
        std::atomic<bool> stop { false };
        std::atomic<int> badSamples { 0 }, blocks { 0 };
        std::thread audio([&] {
            juce::AudioBuffer<float> b(2, 128);
            juce::MidiBuffer m;
            juce::Random r(3);
            while (!stop) {
                b.clear();
                if (r.nextInt(20) == 0) m.addEvent(juce::MidiMessage::noteOn(1, 36 + r.nextInt(48), (juce::uint8)(1 + r.nextInt(127))), 0);
                if (r.nextInt(20) == 0) m.addEvent(juce::MidiMessage::noteOff(1, 36 + r.nextInt(48)), 0);
                p.processBlock(b, m);
                m.clear();
                for (int s = 0; s < 128; ++s) if (!std::isfinite(b.getSample(0, s)) || std::abs(b.getSample(0, s)) > 1.001f) ++badSamples;
                ++blocks;
            }
        });
        {
            MillstoneEditor editor(p);
            juce::Random r(5);
            const auto end = juce::Time::getMillisecondCounter() + 4000;
            int loads = 0;
            juce::MemoryBlock state;
            while (juce::Time::getMillisecondCounter() < end) {
                p.setCurrentProgram(r.nextInt(p.getNumPrograms()));
                ++loads;
                p.connectCord(1 + r.nextInt(24), 1 + r.nextInt(42), (float)(r.nextInt(200) - 100));
                p.disconnectCord(r.nextInt(MillstoneProcessor::numCords));
                editor.setPatchBayOpen(r.nextBool());
                editor.refreshFromProcessor();
                if (loads % 8 == 0) { p.getStateInformation(state); p.setStateInformation(state.getData(), (int)state.getSize()); }
            }
            printf("%d preset loads, %d audio blocks meanwhile\n", loads, blocks.load());
            check(loads > 20 && blocks > 100, "both threads really ran");
        }
        stop = true;
        audio.join();
        check(badSamples == 0, "nothing goes wrong while presets and cords change under running audio, " + juce::String(badSamples.load()) + " bad samples");
    }
    printf("\n%s (%d failed)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
