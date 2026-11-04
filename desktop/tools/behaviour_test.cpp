// Developer check, built only with -DMILLSTONE_UI_SNAPSHOT=ON: drives the whole plugin (parameters + MIDI in,
// audio out) and measures whether each section behaves like a synth should. Run: millstone_behaviour_test
#include "PluginProcessor.h"
#include "dsp_bits.h"
#include "workout_clock.h"
#include <cstdio>
#include <memory>
#include <vector>

using Vec = std::vector<float>;
static int failures = 0;
static void check(bool ok, const char* name, const juce::String& info = {}) {
    printf("%s %-58s %s\n", ok ? "ok  " : "FAIL", name, info.toRawUTF8());
    if (!ok) ++failures;
}

struct Head : juce::AudioPlayHead {
    double bpm = 120.0, ppq = 0.0; bool playing = false;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setBpm(bpm); i.setPpqPosition(ppq); i.setIsPlaying(playing); return i; }
};

struct Synth {
    std::unique_ptr<MillstoneProcessor> p = std::make_unique<MillstoneProcessor>();
    double sr; int block; juce::MidiBuffer m; Head head;
    explicit Synth(double rate = 44100, int blockSize = 256) : sr(rate), block(blockSize) {
        p->setRateAndBufferSizeDetails(sr, block);
        p->prepareToPlay(sr, block);
        p->setPlayHead(&head);
    }
    void set(const char* k, float v) { auto* q = p->apvts.getParameter(k); jassert(q); q->setValueNotifyingHost(q->convertTo0to1(v)); }
    void opt(const char* k, const char* name) {
        auto* q = p->apvts.getParameter(k); jassert(q);
        const int i = q->getAllValueStrings().indexOf(name); jassert(i >= 0);
        q->setValueNotifyingHost(q->convertTo0to1((float)i));
    }
    void on(int n, int v = 100) { m.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8)v), 0); }
    void off(int n) { m.addEvent(juce::MidiMessage::noteOff(1, n), 0); }
    void cc(int c, int v) { m.addEvent(juce::MidiMessage::controllerEvent(1, c, v), 0); }
    void bend(int v) { m.addEvent(juce::MidiMessage::pitchWheel(1, v), 0); }
    void at(int v) { m.addEvent(juce::MidiMessage::channelPressureChange(1, v), 0); }
    void polyAt(int n, int v) { m.addEvent(juce::MidiMessage::aftertouchChange(1, n, v), 0); }
    Vec run(double secs, bool bypass = false) {
        Vec out; const int total = (int)(secs * sr); out.reserve((size_t)total);
        juce::AudioBuffer<float> b(2, block);
        while ((int)out.size() < total) {
            b.clear();
            if (bypass) p->processBlockBypassed(b, m); else p->processBlock(b, m);
            m.clear();
            if (head.playing) head.ppq += block / sr * head.bpm / 60.0;
            out.insert(out.end(), b.getReadPointer(0), b.getReadPointer(0) + block);
        }
        out.resize((size_t)total);
        return out;
    }
    void clean(float vol = 35) {
        set("cutoff", 100); set("reso", 0); set("f_mod", 0); set("vel_amp", 0); set("vel_filt", 0); set("attack", 0);
        set("decay", 20); set("sustain", 100); set("release", 10); set("m_attack", 0); set("m_decay", 20); set("m_sustain", 100); set("m_release", 10); set("volume", vol); set("o1_level", 100); set("o2_level", 100); set("o3_level", 100);
        set("o2_engine", 0); set("o3_engine", 0); opt("o2_on", "OFF"); opt("o3_on", "OFF"); set("o2_fine", 0); set("keytrack", 0); set("drive", 0);
    }
    void noiseOnly(float n = 100) { opt("o1_on", "OFF"); opt("o2_on", "OFF"); opt("o3_on", "OFF"); set("noise", n); }
};

static float rms(const Vec& x, size_t a = 0, size_t b = (size_t)-1) {
    b = juce::jmin(b, x.size()); if (b <= a) return 0; double s = 0; for (size_t i = a; i < b; ++i) s += x[i] * x[i];
    return (float)std::sqrt(s / double(b - a));
}
static float rmsAt(const Vec& x, double t, double w, double sr) { return rms(x, (size_t)(t * sr), (size_t)((t + w) * sr)); }
static float db(float a, float b) { return 20.0f * std::log10((a + 1e-9f) / (b + 1e-9f)); }
static float tone(const Vec& x, double f, double sr, size_t a = 0, size_t b = (size_t)-1) {
    b = juce::jmin(b, x.size()); double w = 2 * M_PI * f / sr, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (size_t i = a; i < b; ++i) { double t = x[i] + c * s1 - s2; s2 = s1; s1 = t; }
    return (float)(std::sqrt(juce::jmax(0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) * 2 / double(b - a));
}
static float hannTone(const Vec& x, double f, double sr, size_t a) {   // for weak sidebands: a plain tone() leaks from the carrier
    Vec w(x.begin() + (long)a, x.end());
    for (size_t i = 0; i < w.size(); ++i) w[i] *= 0.5f - 0.5f * (float)std::cos(2 * M_PI * (double)i / (double)w.size());
    return tone(w, f, sr);
}
static float band(const Vec& x, double f, double sr) {   // mean level around f, for noise
    double s = 0; int n = 0; for (double g = f * 0.9; g <= f * 1.1; g += f * 0.01, ++n) { const float t = tone(x, g, sr); s += t * t; }
    return (float)std::sqrt(s / n);
}
static float pitch(const Vec& x, double sr, size_t a = 0, size_t b = (size_t)-1) {   // autocorrelation
    b = juce::jmin(b, x.size()); if (b <= a + 2000) return 0;
    const int n = (int)(b - a); const int minLag = (int)(sr / 4000), maxLag = juce::jmin(n / 2, (int)(sr / 30));
    std::vector<double> r((size_t)maxLag + 2); double r0 = 0; const int m = n - maxLag - 1;
    for (int i = 0; i < m; ++i) r0 += x[a + (size_t)i] * x[a + (size_t)i];
    if (r0 <= 1e-12) return 0;
    for (int l = minLag; l <= maxLag + 1; ++l) { double s = 0; for (int i = 0; i < m; ++i) s += x[a + (size_t)i] * x[a + (size_t)(i + l)]; r[(size_t)l] = s / r0; }
    double best = 0; for (int l = minLag; l <= maxLag; ++l) best = juce::jmax(best, r[(size_t)l]);
    for (int l = minLag + 1; l <= maxLag; ++l)
        if (r[(size_t)l] > 0.9 * best && r[(size_t)l] >= r[(size_t)l - 1] && r[(size_t)l] >= r[(size_t)l + 1]) {
            const double d = r[(size_t)l - 1] - 2 * r[(size_t)l] + r[(size_t)l + 1];
            return (float)(sr / (l + (d != 0 ? 0.5 * (r[(size_t)l - 1] - r[(size_t)l + 1]) / d : 0)));
        }
    return 0;
}
static bool near(float v, float want, float tol) { return std::abs(v - want) <= std::abs(want) * tol; }
static int crossings(const Vec& x, double sr, double winMs = 20) {   // level transitions of a square LFO
    const size_t w = (size_t)(winMs * 0.001 * sr); std::vector<float> e;
    for (size_t i = 0; i + w < x.size(); i += w) e.push_back(rms(x, i, i + w));
    auto sorted = e; std::sort(sorted.begin(), sorted.end());
    const float lo = sorted[sorted.size() / 10], hi = sorted[sorted.size() * 9 / 10];
    if (hi < 3.0f * lo) return 0;   // no real movement
    const float up = lo + 0.65f * (hi - lo), down = lo + 0.35f * (hi - lo);
    int c = 0; bool high = e[0] > up;
    for (float v : e) { if (!high && v > up) { high = true; ++c; } else if (high && v < down) { high = false; ++c; } }
    return c;
}
static float hz_(int n) { return 440.0f * std::pow(2.0f, (n - 69) / 12.0f); }
static juce::String num(double v, int d = 2) { return juce::String(v, d); }

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    const double sr = 44100;

    puts("--- oscillators are independent, default models, toggled while a note is held");
    {
        Synth s; s.clean(); s.set("o2_engine", 5); s.set("o3_engine", 8);   // the shipped Wavetable / Swarm
        const char* k[3] = {"o1_on", "o2_on", "o3_on"};
        s.on(48); s.run(0.2);
        for (int mask = 7; mask >= 0; --mask) {
            for (int i = 0; i < 3; ++i) s.opt(k[i], mask & (1 << i) ? "ON" : "OFF");
            s.run(0.1); const float level = rms(s.run(0.2), 0);
            check(mask ? level > 1e-3f : level < 1e-5f, (juce::String("toggle mask o1/o2/o3 = ") + juce::String(mask & 1) + juce::String((mask >> 1) & 1) + juce::String((mask >> 2) & 1)).toRawUTF8(), "rms " + num(level, 4));
        }
    }
    { // Each slot's contribution is measured by giving it its own octave.
        Synth s; s.clean(); s.set("o2_oct", 1); s.set("o3_oct", 2); s.opt("o2_on", "ON"); s.opt("o3_on", "ON");
        s.on(48); s.run(0.2);
        const float f = 130.81f;
        s.opt("o1_on", "OFF"); auto a = s.run(0.4);
        check(tone(a, f, sr) < 0.1f * tone(a, 2 * f, sr) && tone(a, 2 * f, sr) > 1e-3f, "OSC 1 off removes only OSC 1", "131Hz " + num(tone(a, f, sr), 4) + " 262Hz " + num(tone(a, 2 * f, sr), 4));
        s.opt("o1_on", "ON"); s.opt("o2_on", "OFF"); s.run(0.05); auto b = s.run(0.4);
        check(tone(b, f, sr) > 1e-3f, "OSC 2 off leaves OSC 1 sounding", "131Hz " + num(tone(b, f, sr), 4));
    }
    { // A voice's oscillators must be silent separately per level too.
        Synth s; s.clean(); s.set("o1_level", 0); s.on(60); check(rms(s.run(0.3), 2000) < 1e-5f, "OSC level 0 is silent");
        Synth a; a.clean(); a.on(60); float full = rms(a.run(0.4), 5000); Synth h; h.clean(); h.set("o1_level", 50); h.on(60);
        check(near(db(full, rms(h.run(0.4), 5000)), 12.0f, 0.25f), "OSC level 50 is -12 dB (squared law)", num(db(full, rms(h.run(0.1)))));
    }

    puts("--- pitch");
    { auto measure = [&](std::function<void(Synth&)> f, int note = 69, double rate = 44100) { Synth s(rate); s.clean(); f(s); s.on(note); s.run(0.15); return pitch(s.run(0.5), rate); };
      auto rep = [&](const char* n, float got, float want, float tol = 0.005f) { check(near(got, want, tol), n, num(got) + " Hz, want " + num(want)); };
      rep("A4 = 440 Hz", measure([](Synth&) {}), 440);
      rep("note 57 = 220 Hz", measure([](Synth&) {}, 57), 220);
      rep("OCT +1", measure([](Synth& s) { s.set("o1_oct", 1); }), 880);
      rep("OCT -2", measure([](Synth& s) { s.set("o1_oct", -2); }), 110);
      rep("SEMI +7", measure([](Synth& s) { s.set("o1_semi", 7); }), 659.26f);
      rep("FINE +100 = 1 semitone", measure([](Synth& s) { s.set("o1_fine", 100); }), 466.16f);
      rep("48 kHz host keeps pitch", measure([](Synth&) {}, 69, 48000), 440);
      rep("96 kHz host keeps pitch", measure([](Synth&) {}, 69, 96000), 440);
      rep("bend full up, range 2", measure([](Synth& s) { s.bend(16383); }), 493.88f);
      rep("bend full up, range 12", measure([](Synth& s) { s.set("bend_range", 12); s.bend(16383); }), 880);
      rep("bend full down, range 12", measure([](Synth& s) { s.set("bend_range", 12); s.bend(0); }), 220);
      rep("bend range 0 ignores the wheel", measure([](Synth& s) { s.set("bend_range", 0); s.bend(16383); }), 440);
    }
    { Synth s; s.clean(); s.set("o1_fm", 100); s.set("m_sustain", 0); s.set("m_decay", 60); s.on(69);
      auto x = s.run(1.0); const float early = pitch(x, sr, 0, 3000), late = pitch(x, sr, 30000, 44000);
      check(early > 1.05f * 440 || early == 0, "FM (ENV) + bends pitch up at the attack", num(early));
      Synth u; u.clean(); u.set("o1_harm", 0); u.set("o1_fm", -100); u.set("m_decay", 60); u.set("m_sustain", 30); u.on(69); auto y = u.run(1.0);
      check(pitch(y, sr, 0, 3000) < pitch(y, sr, 30000, 44000), "FM (ENV) - bends pitch down at the attack", num(pitch(y, sr, 0, 3000)) + " -> " + num(pitch(y, sr, 30000, 44000)));
      (void)late; }

    puts("--- filter (noise through it, then self-oscillation)");
    { auto nf = [&](float cut, const char* mode, float reso = 0, float vol = 100) { Synth s; s.clean(vol); s.noiseOnly(); s.set("cutoff", cut); s.set("reso", reso); s.opt("f_mode", mode); s.on(60); s.run(0.05); return s.run(3.0); };
      auto lp = nf(50, "LP"); // 50% = 640 Hz
      float slope = db(band(lp, 1280, sr), band(lp, 2560, sr));
      check(near(slope, 24, 0.25f), "LP slope is 24 dB/octave", num(slope) + " dB");
      float atCut = db(band(lp, 80, sr), band(lp, 640, sr));
      check(atCut > 6 && atCut < 18, "LP cutoff 50% sits at 640 Hz (-12 dB nominal)", num(atCut) + " dB below passband");
      auto hp = nf(50, "HP", 0, 50);   // quieter: output saturation would fill in the stopband
      // Sine tones rather than noise: output saturation and 16-bit quantisation fill in a noise stopband.
      auto hpTone = [&](int note, float f) { Synth t; t.clean(100); t.set("o1_engine", 2); t.set("o1_timbre", 0); t.set("o1_level", 30); t.set("cutoff", 50); t.opt("f_mode", "HP"); t.on(note); t.run(0.1); auto x = t.run(1.0); return tone(x, f, sr); };
      const float h165 = hpTone(52, 164.81f), h330 = hpTone(64, 329.63f), h2637 = hpTone(100, 2637.0f);
      check(near(db(h330, h165), 21.0f, 0.2f), "HP slope below cutoff: 21 dB from fc/4 to fc/2 (4 poles)", num(db(h330, h165)) + " dB");
      check(near(db(h2637, h330), 27.0f, 0.2f), "HP stopband: 27 dB down at fc/2", num(db(h2637, h330)) + " dB");
      check(band(hp, 5120, sr) > 3 * band(hp, 100, sr), "HP passes highs, cuts lows", num(db(band(hp, 5120, sr), band(hp, 100, sr))) + " dB");
      auto op = nf(100, "LP");
      check(band(op, 10000, sr) > 0.3f * band(op, 100, sr), "cutoff 100 is effectively open", num(db(band(op, 100, sr), band(op, 10000, sr))) + " dB");
      auto rs = nf(50, "LP", 75);
      check(db(band(rs, 640, sr), band(lp, 640, sr)) > 6, "resonance boosts around cutoff", "+" + num(db(band(rs, 640, sr), band(lp, 640, sr))) + " dB");
      auto selfOsc = [&](int note, float kt) { Synth s; s.clean(30); s.noiseOnly(3); s.set("cutoff", 50); s.set("reso", 100); s.set("keytrack", kt); s.on(note); s.run(0.5); return pitch(s.run(0.6), sr); };
      float f60 = selfOsc(60, 0), f72 = selfOsc(72, 0), g60 = selfOsc(60, 100), g72 = selfOsc(72, 100);
      check(f60 > 450 && f60 < 800, "reso 100 self-oscillates near cutoff", num(f60) + " Hz");
      check(near(f72 / juce::jmax(1.f, f60), 1.0f, 0.03f), "KEY TRACK 0: filter ignores the note", num(f72 / juce::jmax(1.f, f60)));
      check(near(g72 / juce::jmax(1.f, g60), 2.0f, 0.04f), "KEY TRACK 100: one octave per octave", num(g72 / juce::jmax(1.f, g60)));
    }
    { auto bright = [&](float mod, bool early) { Synth s; s.clean(30); s.noiseOnly(); s.set("cutoff", 45); s.set("f_mod", mod); s.set("m_decay", 85); s.set("m_sustain", 0); s.opt("vca_mode", "ON"); s.on(60); auto x = s.run(4.0); return early ? rms(x, 100, 2000) : rms(x, 150000, 170000); };
      check(bright(100, true) > 2 * bright(100, false), "env + opens the filter at the attack", num(db(bright(100, true), bright(100, false))) + " dB");
      check(bright(-100, true) < 0.5f * bright(-100, false), "env - closes the filter at the attack", num(db(bright(-100, false), bright(-100, true))) + " dB");
      auto vel = [&](int v) { Synth s; s.clean(30); s.noiseOnly(); s.set("cutoff", 40); s.set("vel_filt", 100); s.on(60, v); return rms(s.run(0.5), 5000); };
      check(vel(127) > 1.5f * vel(30), "VEL > VCF: harder notes are brighter", num(db(vel(127), vel(30))) + " dB");
      auto drive = [&](float d) { Synth s; s.clean(15); s.set("o1_engine", 2); s.set("o1_timbre", 0); s.set("o1_level", 40); s.set("drive", d); s.on(57); auto x = s.run(0.6); const float f = tone(x, 220, sr, 5000), h = tone(x, 660, sr, 5000) + tone(x, 1100, sr, 5000); return h / juce::jmax(1e-6f, f); };
      check(drive(100) > 1.05f * drive(0), "DRIVE adds harmonics", num(drive(0), 3) + " -> " + num(drive(100), 3));
    }

    puts("--- filter tuning");
    { auto lpGain = [](float reso, double f) { ms::Ladder l; l.set(1000, reso, 0); Vec y; for (int i = 0; i < 44100; ++i) y.push_back(l.tick(0.05f * (float)std::sin(2 * M_PI * f * i / 44100))); return tone(y, f, 44100, 22050) / 0.05f; };
      check(db(lpGain(0, 100), 1) > -1 && db(lpGain(0, 100), 1) < 1, "reso 0 leaves the passband at unity", num(db(lpGain(0, 100), 1)) + " dB");
      check(db(lpGain(0.9f, 100), 1) > -8, "resonance keeps some bass: under 8 dB lost at 90% (was 13.5)", num(db(lpGain(0.9f, 100), 1)) + " dB");
      check(db(lpGain(0.9f, 100), 1) < -3, "...and still thins it, ladder style", num(db(lpGain(0.9f, 100), 1)) + " dB");
      auto osc = [](float reso, float fc, double* freq) { ms::Ladder l; l.set(fc, reso, 0); Vec y; for (int i = 0; i < 132300; ++i) y.push_back(l.tick(i < 200 ? 0.01f : 0.0f)); float p = 0; int n = 0; double a = -1, b = -1; for (size_t i = 88200; i < y.size(); ++i) { p = juce::jmax(p, std::abs(y[i])); if (y[i - 1] < 0 && y[i] >= 0) { const double t = i - 1 + (-y[i - 1]) / (y[i] - y[i - 1]); if (a < 0) a = t; b = t; ++n; } } if (freq) *freq = n > 1 ? (n - 1) * 44100.0 / (b - a) : 0; return p; };
      double f = 0; check(osc(0.85f, 1000, nullptr) < 1e-3f, "85% resonance does not self-oscillate"); check(osc(0.90f, 1000, nullptr) > 0.03f, "90% resonance self-oscillates", num(osc(0.90f, 1000, nullptr), 3));
      check(osc(1.0f, 1000, nullptr) > 0.18f, "full self-oscillation is strong (was 0.136)", num(osc(1.0f, 1000, nullptr), 3));
      for (float fc : {55.f, 440.f, 3520.f}) { osc(1.0f, fc, &f); check(std::abs(1200 * std::log2(f / fc)) < 2, (juce::String("self-oscillation in tune at ") + juce::String(fc, 0) + " Hz").toRawUTF8(), num(1200 * std::log2(f / fc)) + " cents"); }
      auto hpGain = [](float reso) { ms::Ladder l; l.set(1000, reso, 1); Vec y; for (int i = 0; i < 44100; ++i) y.push_back(l.tick(0.05f * (float)std::sin(2 * M_PI * 8000 * i / 44100))); return tone(y, 8000, 44100, 22050) / 0.05f; };
      check(std::abs(db(hpGain(0.9f), hpGain(0))) < 0.5f, "HP passband is unaffected by resonance", num(db(hpGain(0.9f), hpGain(0))) + " dB"); }

    puts("--- default patch");
    { auto peak = [](const Vec& x, size_t a = 0) { float p = 0; for (size_t i = a; i < x.size(); ++i) p = juce::jmax(p, std::abs(x[i])); return p; };
      { Synth s; s.on(60); const float p = db(peak(s.run(1.0)), 1); check(p > -18 && p < -8, "one note peaks between -18 and -8 dBFS (was -19.7)", num(p) + " dB"); }
      { Synth s; for (int n : {48, 55, 60, 64, 67, 72, 76, 79}) s.on(n); auto x = s.run(1.0); const float p = db(peak(x), 1); check(p < -0.4f, "an 8-note chord stays under the ceiling", num(p) + " dB"); }
      auto highs = [](const Vec& x, size_t a, size_t b) { double d = 0, e = 0; for (size_t i = a + 2; i < b; ++i) { const double v = x[i] - 2 * x[i - 1] + x[i - 2]; d += v * v; e += x[i] * x[i]; } return (float)std::sqrt(d / juce::jmax(1e-12, e)); };   // treble share: a detuned pair smears single-frequency readings
      { Synth s; s.on(60); auto x = s.run(1.2); const float d = db(highs(x, 400, 2600), highs(x, 40000, 50000)); check(d > 6, "the filter envelope is clearly audible", num(d) + " dB more treble at the attack"); }
      { auto selfOsc = [&](int note) { Synth s; s.clean(30); s.noiseOnly(3); s.set("cutoff", 50); s.set("reso", 100); s.set("keytrack", s.p->apvts.getParameter("keytrack")->convertFrom0to1(s.p->apvts.getParameter("keytrack")->getDefaultValue())); s.on(note); s.run(0.5); return pitch(s.run(0.6), sr); }; const float r = selfOsc(72) / juce::jmax(1.f, selfOsc(60)); check(near(r, 1.414f, 0.04f), "default KEY TRACK: half an octave per octave", num(r, 3)); }
      { Synth s; check(s.p->apvts.getParameter("o2_on")->getCurrentValueAsText() == "ON" && s.p->apvts.getParameter("o3_on")->getCurrentValueAsText() == "OFF", "OSC 1 and 2 on, OSC 3 off"); }
    }

    puts("--- envelope");
    { auto lvl = [&](float a, float d, float sus) { Synth s; s.clean(); s.set("attack", a); s.set("decay", d); s.set("sustain", sus); s.on(69); return s; };
      { auto s = lvl(50, 10, 100); auto x = s.run(0.5); const float peak = rmsAt(x, 0.4, 0.05, sr); double t90 = -1; for (double t = 0; t < 0.4; t += 0.002) if (rmsAt(x, t, 0.005, sr) > 0.9f * peak) { t90 = t; break; }
        check(t90 > 0.04 && t90 < 0.09, "ATTACK 50: ~60 ms to 90% (77 ms to peak)", num(t90 * 1000, 0) + " ms"); }
      { auto s = lvl(0, 10, 100); auto x = s.run(0.2); check(rmsAt(x, 0.005, 0.005, sr) > 0.7f * rmsAt(x, 0.1, 0.05, sr), "ATTACK 0 is immediate"); }
      { auto a = lvl(0, 30, 100), b = lvl(0, 30, 50); float da = rmsAt(a.run(0.8), 0.6, 0.1, sr), db_ = rmsAt(b.run(0.8), 0.6, 0.1, sr); check(near(da / db_, 2.0f, 0.1f), "SUSTAIN 50 holds half the level", num(da / db_)); }
      { auto s = lvl(0, 50, 0); auto x = s.run(0.5); const float peak = rmsAt(x, 0.003, 0.004, sr); double t40 = -1; for (double t = 0.003; t < 0.4; t += 0.002) if (rmsAt(x, t, 0.005, sr) < 0.01f * peak) { t40 = t; break; }
        check(t40 > 0.09 && t40 < 0.18, "DECAY 50: -40 dB in ~133 ms (-60 dB in 200 ms)", num(t40 * 1000, 0) + " ms");
        check(rmsAt(x, 0.45, 0.05, sr) < 1e-3f, "SUSTAIN 0 decays to silence while held"); }
      { auto s = lvl(0, 10, 100); s.set("release", 50); s.run(0.3); const float held = rms(s.run(0.05)); s.off(69); auto x = s.run(0.5); double t40 = -1; for (double t = 0; t < 0.4; t += 0.002) if (rmsAt(x, t, 0.005, sr) < 0.01f * held) { t40 = t; break; }
        check(t40 > 0.09 && t40 < 0.18, "RELEASE 50: -40 dB in ~133 ms, DECAY irrelevant", num(t40 * 1000, 0) + " ms"); check(rms(s.run(0.3)) < 1e-5f, "released note ends in silence"); }
      { Synth a(48000), b(96000); for (auto* s : {&a, &b}) { s->clean(); s->set("attack", 50); s->set("decay", 10); s->on(69); } auto xa = a.run(0.5), xb = b.run(0.5); float ra = rmsAt(xa, 0.03, 0.005, 48000), rb = rmsAt(xb, 0.03, 0.005, 96000); check(near(ra / rb, 1.0f, 0.1f), "envelope timing is sample-rate independent", num(ra / rb)); }
    }

    puts("--- envelope ranges and independence");
    { auto time60 = [](float d, float r, bool release) { ms::Env e; e.set(0, d, 0, r); e.gate_on(); int n = 0; while (e.stage != ms::Env::DECAY) { e.tick(); ++n; if (n > 1000000) break; } if (release) { e.set(0, 0, 1, r); for (int i = 0; i < 44100; ++i) e.tick(); e.gate_off(); } const float start = e.value; n = 0; while (e.value > start * 0.001f && n < 44100 * 30) { e.tick(); ++n; } return n / 44100.0f; };
      check(near(time60(1, 0, false), 10.0f, 0.03f), "DECAY maximum is 10 s (was 5 s)", num(time60(1, 0, false)) + " s");
      check(near(time60(0.55f, 0, false), 0.298f, 0.03f), "DECAY is twice as long at the old default", num(time60(0.55f, 0, false)) + " s (was 0.149)");
      check(near(time60(0, 1, true), 10.0f, 0.03f), "RELEASE maximum is 10 s", num(time60(0, 1, true)) + " s");
      auto bright = [&](std::function<void(Synth&)> f, double at) { Synth s; s.clean(30); s.noiseOnly(); s.set("cutoff", 45); s.set("f_mod", 100); s.opt("vca_mode", "ON"); f(s); s.on(60); auto x = s.run(at + 0.1); return rmsAt(x, at, 0.1, sr); };
      const float vcaLong = bright([](Synth& s) { s.set("m_sustain", 0); s.set("m_decay", 40); s.set("decay", 100); }, 0.5), modLong = bright([](Synth& s) { s.set("m_sustain", 0); s.set("m_decay", 90); s.set("decay", 0); }, 0.5);
      check(modLong > 1.5f * vcaLong, "filter follows the MOD envelope, not the VCA envelope", num(db(modLong, vcaLong)) + " dB");
      auto level = [&](float vcaDecay, float modDecay) { Synth s; s.clean(); s.set("sustain", 0); s.set("decay", vcaDecay); s.set("m_decay", modDecay); s.set("m_sustain", 0); s.on(69); auto x = s.run(0.5); return rmsAt(x, 0.15, 0.05, sr); };
      check(near(level(60, 0), level(60, 100), 0.03f), "VCA level ignores the MOD envelope", num(level(60, 0), 4) + " vs " + num(level(60, 100), 4));
      Synth r; r.clean(30); r.noiseOnly(); r.set("cutoff", 30); r.set("f_mod", 100); r.set("m_release", 90); r.set("release", 80); r.on(60); r.run(0.3); const float open = rms(r.run(0.1)); r.off(60); r.run(0.3); const float rel = rms(r.run(0.05)); check(rel > 0.1f * open, "MOD RELEASE keeps the filter open after key up", num(db(open, rel)) + " dB down");
      auto cv = [&](const char* src) { Synth s; s.clean(30); s.set("o1_timbre_mod", 100); s.set("o1_morph_mod", 100); s.opt("o1_cv_src", src); s.set("o1_timbre", 0); s.set("o1_morph", 0); s.set("m_sustain", 0); s.set("m_decay", 60); s.set("lfo_rate", 0); s.opt("lfo_shape", "TRI"); s.on(57); auto x = s.run(1.0); double d0 = 0, e0 = 0, d1 = 0, e1 = 0; for (size_t i = 501; i < 4000; ++i) { d0 += (x[i] - x[i - 1]) * (x[i] - x[i - 1]); e0 += x[i] * x[i]; } for (size_t i = 30001; i < 34000; ++i) { d1 += (x[i] - x[i - 1]) * (x[i] - x[i - 1]); e1 += x[i] * x[i]; } return std::abs(db((float)std::sqrt(d0 / e0), (float)std::sqrt(d1 / e1))); };
      check(cv("ENV") > cv("LFO") + 1.0f, "CV SOURCE ENV moves timbre/morph with the MOD envelope", num(cv("LFO")) + " -> " + num(cv("ENV")) + " dB");
    }

    puts("--- VCA");
    { auto vel = [&](float amt, int v) { Synth s; s.clean(); s.set("vel_amp", amt); s.on(69, v); return rms(s.run(0.3), 3000); };
      check(near(vel(100, 127) / vel(100, 64), 2.0f, 0.1f), "VEL > AMP 100: half velocity is half level", num(vel(100, 127) / vel(100, 64)));
      check(near(vel(0, 127) / vel(0, 30), 1.0f, 0.02f), "VEL > AMP 0: velocity ignored", num(vel(0, 127) / vel(0, 30)));
      Synth v; v.clean(0); v.on(69); check(rms(v.run(0.3)) < 1e-5f, "VOLUME 0 is silent");
      { Synth s; s.clean(); s.opt("vca_mode", "ON"); s.set("sustain", 0); s.set("decay", 10); s.on(69); auto x = s.run(1.0); check(near(rmsAt(x, 0.8, 0.1, sr) / rmsAt(x, 0.05, 0.1, sr), 1.0f, 0.1f), "ON: constant while held, ignores the envelope"); s.off(69); s.run(0.05); check(rms(s.run(0.2)) < 1e-4f, "ON: stops at key up"); }
      auto lpg = [&](float d) { Synth s; s.clean(); s.opt("vca_mode", "LPG"); s.set("lpg_decay", d); s.on(57); auto x = s.run(2.0); return std::vector<float>{ rmsAt(x, 0.02, 0.05, sr), rmsAt(x, 0.5, 0.1, sr) }; };
      { auto a = lpg(20), b = lpg(80); check(a[1] < 0.3f * a[0], "LPG: note strikes then dies away while held", num(a[0], 4) + " -> " + num(a[1], 4)); check(b[1] > 1.5f * a[1], "LPG DECAY lengthens the strike", num(a[1], 4) + " -> " + num(b[1], 4)); }
      { auto c = [&](float col) { Synth s; s.clean(); s.opt("vca_mode", "LPG"); s.set("lpg_decay", 60); s.set("lpg_colour", col); s.on(57); auto x = s.run(0.5); const float f = tone(x, 220, sr, 3000, 12000), h = tone(x, 1320, sr, 3000, 12000); return h / juce::jmax(1e-6f, f); }; check(std::abs(c(0) - c(100)) > 0.02f * juce::jmax(c(0), c(100)), "LPG COLOUR changes the tone", num(c(0), 3) + " vs " + num(c(100), 3)); }
    }

    puts("--- LFO: shapes and range, unit level");
    { using ms::Lfo; check(near(Lfo::rate_hz(0), 0.01f, 0.01f) && near(Lfo::rate_hz(1), 350.f, 0.01f), "rate spans 0.01..350 Hz");
      for (int shape = 0; shape < 4; ++shape) { Lfo l; float lo = 9, hi = -9, first = 0; for (int i = 0; i < 3675; ++i) { const float v = l.step(1.0f, 12, shape); if (!i) first = v; lo = juce::jmin(lo, v); hi = juce::jmax(hi, v); } (void)first; check(lo < -0.97f && hi > 0.97f && hi <= 1.0f && lo >= -1.0f, (juce::String("bipolar +-1, shape ") + juce::String(shape)).toRawUTF8()); }
      { Lfo l; float a = l.step(1, 12, Lfo::SAW), b = l.step(1, 12, Lfo::SAW); check(b < a, "SAW falls"); Lfo r; a = r.step(1, 12, Lfo::RAMP); b = r.step(1, 12, Lfo::RAMP); check(b > a, "RAMP rises"); }
      { Lfo l; l.retrigger(); int changes = 0; float last = l.step(10, 12, Lfo::SH); for (int i = 0; i < 3675; ++i) { const float v = l.step(10, 12, Lfo::SH); if (v != last) ++changes; last = v; } check(changes >= 9 && changes <= 11, "S/H steps once per cycle", juce::String(changes) + " steps in 1 s at 10 Hz"); } }
    auto lfoPatch = [&](Synth& s) { s.clean(30); s.set("cutoff", 30); s.set("f_mod", 100); s.opt("f_src", "LFO"); s.opt("lfo_shape", "SQR"); s.opt("vca_mode", "ON"); };
    puts("--- LFO: whole plugin");
    { { Synth s; lfoPatch(s); s.set("lfo_rate", 50.57f); s.on(60); int c = crossings(s.run(4.0), sr); check(c >= 14 && c <= 18, "RATE 50.6 is 2 Hz: 16 transitions in 4 s", juce::String(c)); }
      { Synth s; lfoPatch(s); s.opt("lfo_sync", "ON"); s.opt("lfo_div", "1/4"); s.head.bpm = 120; s.on(60); s.run(0.01); int c = crossings(s.run(4.0), sr); check(c >= 14 && c <= 18, "SYNC 1/4 at 120 BPM is 2 Hz", juce::String(c)); }
      { Synth s; lfoPatch(s); s.opt("lfo_sync", "ON"); s.opt("lfo_div", "1/8"); s.head.bpm = 120; s.on(60); s.run(0.01); int c = crossings(s.run(4.0), sr); check(c >= 30 && c <= 34, "SYNC 1/8 at 120 BPM is 4 Hz", juce::String(c)); }
      { Synth s; lfoPatch(s); s.opt("lfo_sync", "ON"); s.opt("lfo_div", "1/4"); s.head.bpm = 60; s.on(60); s.run(0.01); int c = crossings(s.run(4.0), sr); check(c >= 7 && c <= 9, "SYNC follows host tempo (60 BPM = 1 Hz)", juce::String(c)); }
      { Synth s; lfoPatch(s); s.set("lfo_rate", 30); s.opt("lfo_retrig", "ON"); bool allHigh = true; float hi = 0; s.on(60); hi = rms(s.run(0.04)); s.off(60); s.run(0.03); for (double gap : {0.37, 0.61, 1.13, 0.83}) { s.run(gap); s.on(60); const float r = rms(s.run(0.04)); s.off(60); s.run(0.03); if (r < 0.7f * hi) allHigh = false; } check(allHigh, "RETRIG restarts the LFO on each note"); }
      { Synth s; lfoPatch(s); s.opt("voice_mode", "LEGATO"); s.opt("lfo_retrig", "ON"); s.set("lfo_rate", 40); s.on(60); auto x = s.run(0.6); const float high = rmsAt(x, 0.01, 0.1, sr); s.run(0.0); const double lowAt = 1.0 / (2 * 0.01 * std::pow(35000.0, 0.4)); s.m.clear(); s.run(juce::jmax(0.0, lowAt - 0.6 + 0.05)); s.on(62); auto y = s.run(0.02); check(rms(y) < 0.7f * high, "LEGATO overlap does not restart the LFO", num(rms(y), 4) + " vs high " + num(high, 4)); }
      { Synth s; lfoPatch(s); s.set("lfo_rate", 50.57f); s.set("wheel_lfo", 100); s.on(60); s.cc(1, 0); int none = crossings(s.run(2.0), sr); s.cc(1, 127); int full = crossings(s.run(2.0), sr); check(none == 0 && full >= 6, "WHEEL > LFO 100: wheel sets LFO depth", juce::String(none) + " vs " + juce::String(full)); }
      { Synth s; lfoPatch(s); s.set("lfo_rate", 50.57f); s.set("wheel_lfo", 100); s.set("at_lfo", 100); s.on(60); s.cc(1, 0); s.at(127); int c = crossings(s.run(2.0), sr); check(c >= 6, "AT > LFO: channel pressure adds LFO depth", juce::String(c)); }
      { Synth s; lfoPatch(s); s.set("lfo_rate", 50.57f); s.set("wheel_lfo", 100); s.set("at_lfo", 100); s.on(60); s.cc(1, 0); s.polyAt(60, 127); int c = crossings(s.run(2.0), sr); check(c >= 6, "AT > LFO: poly pressure adds LFO depth", juce::String(c)); }
      { Synth s; s.clean(); s.set("vibrato", 100); s.opt("lfo_shape", "SQR"); s.set("lfo_rate", 45); s.on(69); auto x = s.run(1.2); float a = pitch(x, sr, 500, 8000); const float period = 1.0f / (0.01f * std::pow(35000.0f, 0.45f)); float b = pitch(x, sr, (size_t)((period * 0.5f + 0.02f) * sr), (size_t)((period * 0.5f + 0.2f) * sr)); check(near(a / juce::jmax(1.f, b), 4.0f, 0.04f), "OSC MOD 100 with SQR: +-1 octave vibrato", num(a) + " / " + num(b)); }
      auto bright = [](const Vec& x, size_t a, size_t b) { double d = 0, e = 0; for (size_t i = a + 1; i < b; ++i) { d += (x[i] - x[i - 1]) * (x[i] - x[i - 1]); e += x[i] * x[i]; } return (float)std::sqrt(d / juce::jmax(1e-12, e)); };
      for (const char* key : {"o1_timbre_mod", "o1_morph_mod"}) { auto w = [&](float mod) { Synth s; s.clean(30); s.set(key, mod); s.opt("lfo_shape", "SQR"); s.set("lfo_rate", 45); s.on(57); return s.run(1.0); }; auto diff = [&](const Vec& x) { return std::abs(db(bright(x, 2000, 6000), bright(x, 22000, 26000))); }; check(diff(w(100)) > diff(w(0)) + 1, (juce::String(key) + " applies the LFO").toRawUTF8(), num(diff(w(0))) + " -> " + num(diff(w(100))) + " dB"); }
    }

    puts("--- LFO: synced phase, FREE vs BAR");
    { auto locked = [&](const char* mode, const char* div, double preRoll) { auto s = std::make_unique<Synth>(); lfoPatch(*s); s->opt("lfo_sync", "ON"); s->opt("lfo_div", div); s->opt("lfo_lock", mode); s->head.bpm = 120; s->on(60); s->run(preRoll); s->head.playing = true; s->head.ppq = 0; return s; };
      auto high = [&](float level, float ref) { return level > 0.5f * ref; };
      { auto s = locked("BAR", "1/4", 0.137); auto x = s->run(2.0); const float hi = rms(x); int right = 0; for (int beat = 0; beat < 3; ++beat) { const double t = beat * 0.5; right += high(rmsAt(x, t + 0.1, 0.05, sr), hi) ? 1 : 0; right += high(rmsAt(x, t + 0.35, 0.05, sr), hi) ? 0 : 1; } check(right == 6, "BAR: 1/4 is high then low inside every beat", juce::String(right) + "/6"); }
      { auto first = [&](double preRoll) { auto s = locked("BAR", "1/4", preRoll); auto x = s->run(0.2); return rmsAt(x, 0.1, 0.05, sr); }; check(near(first(0.137), first(0.311), 0.2f), "BAR: same result whatever the phase before play", num(first(0.137), 4) + " vs " + num(first(0.311), 4)); }
      { auto s = locked("BAR", "1 BAR", 0.137); auto x = s->run(4.0); const float hi = rms(x); check(high(rmsAt(x, 0.5, 0.1, sr), hi) && !high(rmsAt(x, 1.5, 0.1, sr), hi) && high(rmsAt(x, 2.5, 0.1, sr), hi), "BAR: 1 BAR division spans 4 beats"); }
      { auto s = locked("BAR", "1/4", 0.137); auto x = s->run(0.3); const float hi = rms(x, 0, 8000); s->head.ppq = 10.75; auto y = s->run(0.02); check(rms(y, 0, 400) < 0.5f * hi, "BAR: follows a jump in the song at once", num(rms(y, 0, 400), 4)); }
      { auto s = locked("BAR", "1/4", 0.137); s->opt("lfo_retrig", "ON"); s->off(60); s->run(0.3); s->head.ppq = 10.75; s->on(60); auto y = s->run(0.02); check(rms(y, 200, 800) < 0.002f, "BAR wins over RETRIG", num(rms(y, 200, 800), 4)); }
      { auto s = locked("BAR", "1/4", 0.137); s->run(0.2); s->head.playing = false; int c = crossings(s->run(4.0), sr); check(c >= 14 && c <= 18, "BAR: stopped transport keeps running at the tempo", juce::String(c)); }
      { auto s = locked("BAR", "1/4", 0.137); int c = crossings(s->run(8.0), sr); check(c >= 30 && c <= 33, "BAR: rate is unchanged while playing", juce::String(c)); }
      { auto s = locked("FREE", "1/4", 0.0); auto x = s->run(0.2); const float a = rmsAt(x, 0.05, 0.05, sr); s->head.ppq = 10.75; auto y = s->run(0.15); const float b = rmsAt(y, 0.0, 0.05, sr); check(near(a, b, 0.2f), "FREE: ignores the song position", num(a, 4) + " vs " + num(b, 4)); }
      { auto s = locked("BAR", "1/4", 0.0); s->opt("lfo_sync", "OFF"); s->set("lfo_rate", 40); s->run(0.1); s->head.ppq = 10.75; auto x = s->run(0.02); const float y = rms(x, 200, 800); check(y > 0.002f, "BAR has no effect when SYNC is off", num(y, 4)); }
      { ms::Lfo l; l.phase = 0.9f; l.hold = 0.25f; l.set_phase(0.95f); const float same = l.hold; l.set_phase(0.02f); check(same == 0.25f && l.hold != 0.25f, "BAR: S/H steps once as the bar wraps"); }
    }

    puts("--- pressure");
    { auto c = [&](float amt, int pres, bool poly) { Synth s; s.clean(30); s.noiseOnly(); s.set("cutoff", 40); s.set("at_cutoff", amt); s.on(60); if (poly) s.polyAt(60, pres); else s.at(pres); return rms(s.run(0.5), 5000); };
      check(c(100, 127, false) > 1.5f * c(100, 0, false), "AT > CUTOFF + opens the filter");
      check(c(-100, 127, false) < 0.6f * c(-100, 0, false), "AT > CUTOFF - closes the filter");
      check(c(100, 127, true) > 1.5f * c(100, 0, true), "poly pressure works on AT > CUTOFF");
      auto m = [&](int pres) { Synth s; s.clean(30); s.set("o1_morph", 0); s.set("at_morph", 100); s.on(57); s.at(pres); auto x = s.run(0.5); return tone(x, 660, sr, 3000) / juce::jmax(1e-6f, tone(x, 220, sr, 3000)); };
      check(std::abs(m(127) - m(0)) > 0.01f, "AT > MORPH changes the oscillator", num(m(0), 3) + " -> " + num(m(127), 3)); }

    puts("--- glide");
    { { Synth s; s.clean(); s.opt("voice_mode", "MONO"); s.set("glide", 50); s.on(57); s.run(0.8); s.on(69); auto x = s.run(1.0); float e = pitch(x, sr, 500, 3500), l = pitch(x, sr, 30000, 42000); check(e < 400 && near(l, 440, 0.01f), "GLIDE slides to the new note (mono)", num(e) + " -> " + num(l)); }
      { Synth s; s.clean(); s.opt("voice_mode", "MONO"); s.on(57); s.run(0.3); s.on(69); auto x = s.run(0.4); check(near(pitch(x, sr, 500, 5000), 440, 0.02f), "GLIDE 0 jumps at once"); }
      { Synth s; s.clean(); s.set("glide", 50); s.on(57); s.run(0.3); s.off(57); s.run(0.3); s.on(69); auto x = s.run(1.0); check(pitch(x, sr, 500, 3500) < 400 && near(pitch(x, sr, 30000, 42000), 440, 0.01f), "GLIDE works from the last note (poly)"); } }

    puts("--- voices, stealing, note priority");
    const int ns[9] = {48, 50, 53, 55, 58, 60, 63, 65, 68};
    auto hz = [](int n) { return 440.0f * std::pow(2.0f, (n - 69) / 12.0f); };
    auto present = [&](const Vec& x, int n) { return tone(x, hz(n), sr) > 2e-3f; };
    { Synth s; s.clean(25); for (int i = 0; i < 8; ++i) s.on(ns[i], 100); s.run(0.2); auto x = s.run(1.0); int c = 0; for (int i = 0; i < 8; ++i) c += present(x, ns[i]); check(c == 8, "8 notes all sound", juce::String(c)); s.on(ns[8]); s.run(0.1); auto y = s.run(1.0); check(!present(y, ns[0]) && present(y, ns[8]) && present(y, ns[1]), "9th note steals the oldest"); }
    { Synth s; s.clean(25); s.set("voices", 2); s.on(48); s.on(53); s.on(60); s.run(0.2); auto x = s.run(1.0); check(!present(x, 48) && present(x, 53) && present(x, 60), "VOICES 2 limits polyphony"); }
    { Synth s; s.clean(25); s.set("voices", 1); s.on(48); s.run(0.1); s.on(60); s.run(0.1); s.off(60); s.run(0.5); s.off(48); s.run(0.5); check(rms(s.run(0.2)) < 1e-5f, "VOICES 1 leaves nothing hanging"); }
    { Synth s; s.clean(25); s.opt("voice_mode", "MONO"); s.on(48); s.on(60); s.run(0.2); auto x = s.run(1.0); check(!present(x, 48) && present(x, 60), "MONO: last note priority"); s.off(60); s.run(0.2); auto y = s.run(1.0); check(present(y, 48), "MONO: falls back to the held note"); s.off(48); s.run(0.5); check(rms(s.run(0.2)) < 1e-5f, "MONO: silent after the last key"); }
    { Synth s; s.clean(25); s.opt("voice_mode", "MONO"); s.on(48); s.on(60); s.run(0.2); s.off(48); s.run(0.2); auto x = s.run(0.5); check(present(x, 60), "MONO: lifting a lower note keeps the top one"); }
    { auto env = [&](const char* mode) { Synth s; s.clean(30); s.opt("voice_mode", mode); s.set("sustain", 0); s.set("decay", 40); s.on(60); s.run(0.5); s.on(64); return rms(s.run(0.03)); }; check(env("MONO") > 1e-3f, "MONO: overlapping note retriggers the envelope", num(env("MONO"), 4)); check(env("LEGATO") < 1e-3f, "LEGATO: overlapping note keeps the envelope", num(env("LEGATO"), 4)); }
    { Synth s; s.clean(30); s.opt("voice_mode", "LEGATO"); s.set("vel_amp", 100); s.on(60, 127); s.run(0.3); const float a = rms(s.run(0.05)); s.on(64, 30); s.run(0.02); const float b = rms(s.run(0.05)); check(near(a, b, 0.1f), "LEGATO: a softer overlapping note does not jump the level", num(a, 4) + " -> " + num(b, 4)); }

    puts("--- stuck notes");
    auto silent = [&](Synth& s) { s.run(3.0); return rms(s.run(0.3)) < 1e-5f; };
    { Synth s; s.clean(25); s.set("decay", 10); for (int i = 0; i < 6; ++i) s.on(ns[i]); s.run(0.1); s.set("voices", 2); s.run(0.1); for (int i = 0; i < 6; ++i) s.off(ns[i]); check(silent(s), "lowering VOICES while holding notes"); }
    { Synth s; s.clean(25); s.set("decay", 10); s.on(48); s.on(55); s.on(60); s.run(0.1); s.opt("voice_mode", "MONO"); s.run(0.1); s.off(48); s.off(55); s.off(60); check(silent(s), "POLY -> MONO while holding notes"); }
    { Synth s; s.clean(25); s.set("decay", 10); s.on(48); s.on(55); s.on(60); s.run(0.1); s.opt("voice_mode", "LEGATO"); s.run(0.1); s.off(48); s.off(55); s.off(60); check(silent(s), "POLY -> LEGATO while holding notes"); }
    { Synth s; s.clean(25); s.set("decay", 10); s.opt("voice_mode", "MONO"); s.on(48); s.on(55); s.run(0.1); s.opt("voice_mode", "POLY"); s.run(0.1); s.off(48); s.off(55); check(silent(s), "MONO -> POLY while holding notes"); }
    { Synth s; s.clean(30); s.set("decay", 10); s.set("sustain", 0); s.opt("voice_mode", "MONO"); s.on(48); s.on(55); s.run(0.1); s.opt("voice_mode", "POLY"); s.off(48); s.off(55); s.run(0.5); s.opt("voice_mode", "LEGATO"); s.run(0.1); s.on(60); auto x = s.run(0.03); check(rms(x) > 1e-3f, "mode changes leave no stale held notes", "first LEGATO note rms " + num(rms(x), 4)); }
    { Synth s; s.clean(25); s.set("decay", 10); s.opt("voice_mode", "MONO"); s.on(48); s.on(55); s.run(0.1); s.opt("voice_mode", "POLY"); s.off(48); s.off(55); s.run(0.3); s.opt("voice_mode", "MONO"); s.on(60); s.run(0.1); s.off(60); check(silent(s), "no stale note returns after a mode change"); }
    { Synth s; s.clean(25); s.set("decay", 10); s.on(60); s.run(0.1); s.off(60); s.run(0.0); check(silent(s), "plain note on/off"); }
    { Synth s; s.clean(25); s.set("decay", 10); s.on(60); s.on(60, 0); check(silent(s), "note on velocity 0 counts as note off"); }
    { Synth s; s.clean(25); s.set("decay", 10); s.on(60); s.on(60); s.off(60); check(silent(s), "repeated note on, single note off"); }
    { Synth s; s.clean(25); s.set("decay", 10); s.on(60); s.run(0.1); s.run(0.05, true); s.off(60); s.run(0.05, true); check(silent(s), "note off during plugin bypass"); }
    { Synth s; s.clean(25); s.set("decay", 10); s.on(60); s.run(0.1); s.off(60); juce::MidiBuffer none; s.p->prepareToPlay(sr, 256); s.m.clear(); check(silent(s), "host re-prepare drops the voices"); }

    { Synth s; s.clean(25); s.set("o1_engine", 15); s.opt("o2_on", "ON"); s.opt("vca_mode", "ON"); s.on(60); s.run(0.2); s.off(60); check(silent(s), "VCA ON: percussive + free-running oscillators ends at key up"); }
    { Synth s; s.clean(25); s.set("o1_engine", 15); s.set("noise", 30); s.opt("vca_mode", "ON"); s.on(60); s.run(0.2); s.off(60); check(silent(s), "VCA ON: percussive oscillator + noise ends at key up"); }
    { Synth s; s.clean(25); s.set("noise", 40); s.opt("vca_mode", "LPG"); s.on(60); s.run(0.2); s.off(60); check(silent(s), "VCA LPG: noise ends at key up"); }
    { Synth s; s.clean(25); s.set("o1_engine", 13); s.opt("vca_mode", "ON"); s.on(60); s.run(0.02); s.off(60); auto tail = s.run(0.2); check(rms(tail) > 1e-3f, "VCA ON: a lone percussive model still rings past key up", num(rms(tail), 4)); check(silent(s), "...and ends"); }

    puts("--- sustain pedal and channel messages");
    { Synth s; s.clean(25); s.set("decay", 10); s.cc(64, 127); s.on(60); s.run(0.1); s.off(60); s.run(0.5); check(rms(s.run(0.2)) > 1e-3f, "pedal holds a released note"); s.cc(64, 0); check(silent(s), "pedal up releases it"); }
    { Synth s; s.clean(25); s.set("decay", 10); s.cc(64, 127); s.on(60); s.run(0.05); s.off(60); s.run(0.05); s.on(60); s.run(0.05); s.cc(64, 0); s.run(0.2); check(rms(s.run(0.2)) > 1e-3f, "pedal up keeps a re-pressed key sounding"); s.off(60); check(silent(s), "...and the key then ends it"); }
    { Synth s; s.clean(25); s.set("decay", 10); s.opt("voice_mode", "MONO"); s.cc(64, 127); s.on(60); s.run(0.05); s.off(60); s.run(0.5); check(rms(s.run(0.2)) > 1e-3f, "MONO: pedal holds"); s.cc(64, 0); check(silent(s), "MONO: pedal up releases"); }
    { Synth s; s.clean(25); s.set("release", 80); s.on(60); s.run(0.3); s.cc(123, 0); s.run(0.1); const float t = rms(s.run(0.05)); check(t > 0, "CC123 All Notes Off releases (with the release time)", num(t, 4)); s.set("release", 10); check(silent(s), "...and ends"); }
    { Synth s; s.clean(25); s.set("release", 90); s.on(60); s.run(0.3); s.cc(120, 0); s.run(0.03); check(rms(s.run(0.05)) < 1e-4f, "CC120 All Sound Off cuts immediately", num(rms(s.run(0.01)), 5)); }
    { Synth s; s.clean(25); s.set("decay", 10); s.cc(64, 127); s.run(0.01); s.cc(121, 0); s.on(60); s.run(0.05); s.off(60); check(silent(s), "CC121 Reset Controllers clears the pedal"); }
    { Synth s; s.clean(25); s.set("bend_range", 12); s.bend(16383); s.cc(121, 0); s.on(69); s.run(0.2); check(near(pitch(s.run(0.4), sr), 440, 0.01f), "CC121 recentres the bend"); }
    { Synth s; s.clean(25); s.set("decay", 10); s.cc(64, 127); s.on(60); s.run(0.05); s.off(60); s.cc(123, 0); check(silent(s), "CC123 under the pedal"); }

    puts("--- patch bay");
    { const auto src = [](Synth& s, const char* n) { return s.p->apvts.getParameter("c1_src")->getAllValueStrings().indexOf(n); };
      const auto dst = [](Synth& s, const char* n) { return s.p->apvts.getParameter("c1_dst")->getAllValueStrings().indexOf(n); };
      const auto cord = [&](Synth& s, const char* from, const char* to, float amount) { return s.p->connectCord(src(s, from), dst(s, to), amount); };
      const auto sine = [](Synth& s, int osc) { const juce::String k = "o" + juce::String(osc) + "_"; s.opt((k + "on").toRawUTF8(), "ON"); s.set((k + "engine").toRawUTF8(), 2); s.set((k + "timbre").toRawUTF8(), 0); };
      const auto treble = [](const Vec& x, size_t a, size_t b) { double d = 0, e = 0; for (size_t i = a + 2; i < b; ++i) { const double v = x[i] - 2 * x[i - 1] + x[i - 2]; d += v * v; e += x[i] * x[i]; } return (float)std::sqrt(d / juce::jmax(1e-12, e)); };

      { Synth a, b; a.clean(); b.clean(); cord(b, "LFO", "O1 PITCH", 0); cord(b, "NOISE", "CUTOFF", 0); a.on(60); b.on(60); auto x = a.run(0.5), y = b.run(0.5); double diff = 0; for (size_t i = 0; i < x.size(); ++i) diff = juce::jmax(diff, (double)std::abs(x[i] - y[i])); check(diff < 1e-5, "unplugged and amount-0 cords change nothing", num(diff, 7)); }
      { Synth s; s.clean(); cord(s, "LFO", "O1 PITCH", 50); s.opt("lfo_shape", "SQR"); s.set("lfo_rate", 45); s.on(69); auto x = s.run(1.2); const float period = 1.0f / (0.01f * std::pow(35000.0f, 0.45f)); const float a = pitch(x, sr, 500, 8000), b = pitch(x, sr, (size_t)((period * 0.5f + 0.02f) * sr), (size_t)((period * 0.5f + 0.2f) * sr)); check(near(a / juce::jmax(1.f, b), 4.0f, 0.04f), "LFO > O1 PITCH at 50: +-1 octave", num(a) + " / " + num(b)); }
      { Synth s; s.clean(); cord(s, "LFO", "O1 PITCH", 25); cord(s, "LFO", "ALL PITCH", 25); s.opt("lfo_shape", "SQR"); s.set("lfo_rate", 45); s.on(69); auto x = s.run(1.2); const float period = 1.0f / (0.01f * std::pow(35000.0f, 0.45f)); const float a = pitch(x, sr, 500, 8000), b = pitch(x, sr, (size_t)((period * 0.5f + 0.02f) * sr), (size_t)((period * 0.5f + 0.2f) * sr)); check(near(a / juce::jmax(1.f, b), 4.0f, 0.04f), "cords into one destination add up", num(a) + " / " + num(b)); }
      { Synth s; s.clean(30); s.set("cutoff", 30); cord(s, "MOD ENV", "CUTOFF", 100); s.set("m_sustain", 0); s.set("m_decay", 75); s.opt("vca_mode", "ON"); s.on(60); auto x = s.run(2.0); check(treble(x, 300, 2000) > 2 * treble(x, 70000, 80000), "MOD ENV > CUTOFF sweeps", num(db(treble(x, 300, 2000), treble(x, 70000, 80000))) + " dB"); }
      { Synth s; s.clean(30); s.set("cutoff", 30); cord(s, "VCA ENV", "CUTOFF", 100); s.set("sustain", 30); s.set("decay", 65); s.on(60); auto x = s.run(1.5); check(treble(x, 300, 2000) > 2 * treble(x, 50000, 60000), "VCA ENV > CUTOFF follows the level", num(db(treble(x, 300, 2000), treble(x, 50000, 60000))) + " dB"); }
      { auto vel = [&](int v) { Synth s; s.clean(30); s.set("cutoff", 30); cord(s, "VELOCITY", "CUTOFF", 100); s.on(60, v); auto x = s.run(0.5); return treble(x, 2000, 20000); }; check(vel(127) > 2 * vel(20), "VELOCITY > CUTOFF", num(db(vel(127), vel(20))) + " dB"); }
      { auto w = [&](int v) { Synth s; s.clean(30); s.set("cutoff", 30); cord(s, "WHEEL", "CUTOFF", 100); s.cc(1, v); s.on(60); auto x = s.run(0.5); return treble(x, 2000, 20000); }; check(w(127) > 2 * w(0), "WHEEL > CUTOFF", num(db(w(127), w(0))) + " dB"); }
      { auto a = [&](int v, bool poly) { Synth s; s.clean(30); s.set("cutoff", 30); cord(s, "AFTERTOUCH", "CUTOFF", 100); s.on(60); if (poly) s.polyAt(60, v); else s.at(v); auto x = s.run(0.5); return treble(x, 2000, 20000); }; check(a(127, false) > 2 * a(0, false), "AFTERTOUCH > CUTOFF, channel", num(db(a(127, false), a(0, false))) + " dB"); check(a(127, true) > 2 * a(0, true), "AFTERTOUCH > CUTOFF, poly", num(db(a(127, true), a(0, true))) + " dB"); }
      { auto k = [&](int n, bool patched) { Synth s; s.clean(30); s.noiseOnly(3); s.set("cutoff", 50); s.set("reso", 100); if (patched) cord(s, "KEY", "CUTOFF", 100); s.on(n); s.run(0.5); return pitch(s.run(0.6), sr); }; const float r = k(84, true) / juce::jmax(1.f, k(60, true)); check(r > 2.5f, "KEY > CUTOFF: higher notes open it", num(r, 2) + "x"); }
      { Synth s; s.clean(); cord(s, "GATE", "VCA", -100); s.on(60); check(rms(s.run(0.3), 2000) < 1e-4f, "GATE > VCA at -100 closes it while held"); }
      { auto n = [&](bool on) { Synth s; s.clean(); sine(s, 1); if (on) cord(s, "NOISE", "VCA", 100); s.on(69); auto x = s.run(1.0); return treble(x, 5000, 40000); }; check(n(true) > 3 * n(false), "NOISE > VCA, audio rate: broadband sidebands", num(db(n(true), n(false))) + " dB"); }
      { auto ring = [&](bool on) { Synth s; s.clean(); sine(s, 1); sine(s, 2); s.set("o2_semi", 7); s.set("o2_level", 0); if (on) cord(s, "OSC 2", "VCA", 100); s.on(69); auto x = s.run(1.0); return std::vector<float>{ tone(x, 1099.26f, sr, 5000), tone(x, 219.26f, sr, 5000), tone(x, 659.26f, sr, 5000), tone(x, 440.0f, sr, 5000) }; }; const auto off = ring(false), on = ring(true); check(on[0] > 10 * off[0] && on[1] > 10 * off[1], "OSC 2 > VCA: ring mod sum and difference tones", num(db(on[0], off[0])) + " / " + num(db(on[1], off[1])) + " dB"); check(off[2] < 0.05f * off[3], "OSC 2 at LEVEL 0 is silent by itself", num(db(off[2], off[3])) + " dB"); }
      { auto fm = [&](bool on) { Synth s; s.clean(); sine(s, 1); sine(s, 3); s.set("o3_semi", 6); s.set("o3_level", 0); if (on) cord(s, "OSC 3", "O1 PITCH", 40); s.on(57); auto x = s.run(1.0); return hannTone(x, 311.13f - 220.0f, sr, 5000) + hannTone(x, 311.13f + 220.0f, sr, 5000); }; check(fm(true) > 10 * fm(false), "OSC 3 > O1 PITCH: FM sidebands", num(db(fm(true), fm(false))) + " dB"); }
      { auto f = [&](bool on) { Synth s; s.clean(); s.set("cutoff", 45); s.set("reso", 40); s.set("o2_semi", 5); s.set("o2_level", 0); s.set("o2_engine", 2); s.set("o2_timbre", 0); s.opt("o2_on", "ON"); sine(s, 1); if (on) cord(s, "OSC 2", "CUTOFF", 100); s.on(69); auto x = s.run(1.0); return tone(x, 440.0f + 587.33f, sr, 5000) + tone(x, 587.33f - 440.0f, sr, 5000); }; check(f(true) > 5 * f(false), "OSC 2 > CUTOFF, audio rate: filter FM sidebands", num(db(f(true), f(false))) + " dB"); }
      { auto r = [&](bool on) { Synth s; s.clean(); s.set("cutoff", 45); s.set("reso", 60); s.set("o2_semi", 5); s.set("o2_level", 0); s.set("o2_engine", 2); s.set("o2_timbre", 0); s.opt("o2_on", "ON"); if (on) cord(s, "OSC 2", "RESO", 100); s.on(69); auto x = s.run(1.0); return treble(x, 5000, 40000); }; check(std::abs(db(r(true), r(false))) > 0.5f, "OSC 2 > RESO changes the tone", num(db(r(true), r(false))) + " dB"); }
      { Synth s; s.clean(30); s.set("cutoff", 45); cord(s, "WHEEL", "RESO", 100); s.cc(1, 127); s.on(60); auto x = s.run(0.5); Synth n; n.clean(30); n.set("cutoff", 45); n.on(60); auto y = n.run(0.5); check(std::abs(db(treble(x, 2000, 20000), treble(y, 2000, 20000))) > 1, "WHEEL > RESO", num(db(treble(x, 2000, 20000), treble(y, 2000, 20000))) + " dB"); }
      { Synth s; s.clean(); s.noiseOnly(0); cord(s, "WHEEL", "NOISE", 100); s.cc(1, 127); s.on(60); check(rms(s.run(0.3), 2000) > 1e-3f, "WHEEL > NOISE opens the noise with its knob at 0"); }
      for (int osc = 1; osc <= 3; ++osc) {
        const juce::String O = "O" + juce::String(osc) + " ", k = "o" + juce::String(osc) + "_";
        const auto solo = [&](Synth& s) { s.clean(); s.opt("o1_on", "OFF"); s.opt((k + "on").toRawUTF8(), "ON"); s.set((k + "engine").toRawUTF8(), 0); };
        { Synth s; solo(s); s.set((k + "level").toRawUTF8(), 0); s.p->connectCord(src(s, "WHEEL"), dst(s, (O + "LEVEL").toRawUTF8()), 100); s.cc(1, 127); s.on(69); check(rms(s.run(0.3), 3000) > 1e-3f, (O + "LEVEL input").toRawUTF8()); }
        { Synth s; solo(s); s.p->connectCord(src(s, "WHEEL"), dst(s, (O + "PITCH").toRawUTF8()), 50); s.cc(1, 127); s.on(69); s.run(0.15); check(near(pitch(s.run(0.5), sr), 880.0f, 0.02f), (O + "PITCH input: 50 is an octave").toRawUTF8()); }
        for (const char* what : {"HARM", "TIMBRE", "MORPH"}) {
            const auto measure = [&](bool on) { Synth s; solo(s); s.set((k + "harm").toRawUTF8(), 50); if (on) s.p->connectCord(src(s, "WHEEL"), dst(s, (O + what).toRawUTF8()), -50); s.cc(1, 127); s.on(57); auto x = s.run(0.6); return treble(x, 3000, 25000); };
            check(std::abs(db(measure(true), measure(false))) > 0.3f, (O + what + " input").toRawUTF8(), num(db(measure(true), measure(false))) + " dB");
        }
      }
      { Synth s; s.clean(); s.opt("o2_on", "OFF"); s.set("o2_level", 0); cord(s, "OSC 2", "VCA", -100); s.on(69); const float r = rms(s.run(0.3), 3000); check(r > 1e-3f, "a switched-off oscillator patches silence", num(r, 4)); }
      { Synth s; s.clean(); for (int c = 1; c <= 12; ++c) s.p->connectCord(c, c + 2, 50); check(s.p->connectCord(1, 20) == -1, "a 13th cord is refused"); check(s.p->connectCord(1, 3) == 0, "patching the same two jacks reuses the cord"); s.p->disconnectCord(4); check(s.p->connectCord(1, 20) == 4, "unplugging frees that cord"); }
      { Synth a; a.p->connectCord(1, 2, -33); juce::MemoryBlock mb; a.p->getStateInformation(mb); Synth b; b.p->setStateInformation(mb.getData(), (int)mb.getSize()); check(b.p->cordSource(0) == 1 && b.p->cordDest(0) == 2 && near(b.p->cordAmount(0), -33.0f, 0.01f), "cords survive save and reload"); b.p->setCurrentProgram(5); check(b.p->cordSource(0) == 0 && b.p->cordDest(0) == 0, "loading a preset clears the patch"); }
      { int bad = 0; for (int shape = 0; shape < 4; ++shape) { Synth s; s.opt("o2_on", "ON"); s.opt("o3_on", "ON"); s.set("reso", 100); s.set("drive", 100); s.set("volume", 100); s.set("noise", 50); for (int c = 0; c < 12; ++c) s.p->connectCord(1 + (c + shape * 3) % 24, 1 + (c * 7 + shape * 5) % 42, c % 2 ? 100 : -100); s.set("o1_engine", (float)(shape * 7)); for (int i = 0; i < 8; ++i) s.on(ns[i], 127); auto x = s.run(1.5); for (float v : x) if (!std::isfinite(v) || std::abs(v) > 1.001f) { ++bad; break; } for (int i = 0; i < 8; ++i) s.off(ns[i]); s.set("release", 10); s.run(4.0); if (rms(s.run(0.3)) > 1e-3f) ++bad; } check(bad == 0, "12 wild cords with feedback: finite, within +-1, and it ends", juce::String(bad)); }
      { Synth s; s.clean(25); s.set("o2_level", 0); s.opt("o2_on", "ON"); s.set("o1_engine", 15); s.opt("vca_mode", "ON"); cord(s, "OSC 2", "VCA", 50); s.on(60); s.run(0.2); s.off(60); check(silent(s), "VCA ON: a level-0 modulator does not hang the note"); }
    }

    puts("--- Tides");
    { const auto src = [](Synth& s, const char* n) { return s.p->apvts.getParameter("c1_src")->getAllValueStrings().indexOf(n); };
      const auto dst = [](Synth& s, const char* n) { return s.p->apvts.getParameter("c1_dst")->getAllValueStrings().indexOf(n); };
      const auto hzKnob = [](float hz) { return 100.0f * std::log(hz / 0.01f) / std::log(35000.0f); };
      const auto gatePatch = [&](Synth& s, const char* mode, const char* trigger) { s.clean(30); s.opt("t_mode", mode); s.opt("t_out", "GATES"); s.opt("t_trig", trigger); s.opt("vca_mode", "ON"); s.p->connectCord(src(s, "TIDES 3"), dst(s, "VCA"), -100); };   // in GATES mode, TIDES 3 and 4 carry the gates
      const auto ramp = [](const Vec& x, size_t a, size_t b) { return rms(x, a, b); };
      { Synth a, b; a.clean(); b.clean(); b.opt("t_mode", "AD"); b.set("t_rate", 90); a.on(60); b.on(60); auto x = a.run(0.4), y = b.run(0.4); double diff = 0; for (size_t i = 0; i < x.size(); ++i) diff = juce::jmax(diff, (double)std::abs(x[i] - y[i])); check(diff < 1e-5, "unpatched, Tides changes nothing", num(diff, 7)); }
      { Synth s; gatePatch(s, "LOOP", "FREE"); s.set("t_rate", hzKnob(2)); s.on(60); int c = crossings(s.run(4.0), sr); check(c >= 14 && c <= 18, "FREE, LOOP: runs at RATE, 2 Hz", juce::String(c)); }
      { Synth s; gatePatch(s, "AD", "FREE"); s.set("t_rate", hzKnob(2)); s.on(60); int c = crossings(s.run(4.0), sr); check(c >= 10, "FREE, AD: keeps firing by itself instead of one cycle and stopping", juce::String(c)); }
      { Synth s; s.clean(30); s.opt("t_mode", "AD"); s.opt("t_out", "PHASE"); s.opt("t_trig", "NOTE"); s.set("t_rate", hzKnob(3)); s.set("cutoff", 25); s.p->connectCord(src(s, "TIDES 1"), dst(s, "CUTOFF"), 100); s.on(60); auto x = s.run(1.0); const float early = rms(x, 1000, 5000), late = rms(x, 30000, 40000); check(early > 1.5f * late, "NOTE, AD: a note fires one shot that then rests", num(db(early, late)) + " dB"); s.off(60); s.run(1.0); s.on(60); auto y = s.run(1.0); check(rms(y, 1000, 5000) > 1.5f * rms(y, 30000, 40000), "...and the next note fires it again"); }
      { const auto firstHalfLevel = [&](const char* trigger, double gap) { Synth s; gatePatch(s, "LOOP", trigger); s.set("t_rate", hzKnob(0.7f)); s.on(60); s.run(0.02); s.off(60); s.run(gap); s.on(60); return ramp(s.run(0.05), 200, 1800); }; const float a = firstHalfLevel("NOTE", 0.33), b = firstHalfLevel("NOTE", 0.91); check(near(a, b, 0.15f), "NOTE: every note starts the cycle from the same place", num(a, 4) + " vs " + num(b, 4)); const float f = firstHalfLevel("FREE", 0.81), g = firstHalfLevel("FREE", 0.09); check(std::abs(db(f, g)) > 6, "FREE: a note lands wherever the cycle has got to", num(f, 4) + " vs " + num(g, 4)); }
      for (const auto& [div, want] : std::vector<std::pair<const char*, int>>{ {"1/4", 16}, {"1/8", 32}, {"1/4 T", 24}, {"1/8 T", 48}, {"1 BAR", 4} }) { Synth s; gatePatch(s, "LOOP", "CLOCK"); s.opt("t_div", div); s.head.bpm = 120; s.on(60); s.run(0.01); int c = crossings(s.run(4.0), sr, 5); check(std::abs(c - want) <= 3, (juce::String("CLOCK ") + div + " at 120 BPM").toRawUTF8(), juce::String(c) + ", want " + juce::String(want)); }
      { Synth s; gatePatch(s, "AD", "CLOCK"); s.opt("t_div", "1/4"); s.head.bpm = 60; s.on(60); s.run(0.01); int c = crossings(s.run(4.0), sr); check(c >= 6 && c <= 9, "CLOCK follows the tempo, and one-shots fire on it", juce::String(c)); }
      { const auto after = [&](double ppq) { Synth s; gatePatch(s, "LOOP", "CLOCK"); s.opt("t_div", "1 BAR"); s.head.bpm = 120; s.on(60); s.run(0.05); s.head.playing = true; s.head.ppq = ppq; s.run(0.01); return ramp(s.run(0.05), 200, 1800); }; const float a = after(0.5), b = after(0.5), c = after(2.5); check(near(a, b, 0.1f) && std::abs(db(a, c)) > 6, "CLOCK locks to the song position", num(a, 4) + " " + num(b, 4) + " vs " + num(c, 4)); }
      { const auto output = [&](const char* jack, const char* out, float shift) { Synth s; s.clean(30); s.opt("t_mode", "LOOP"); s.opt("t_out", out); s.opt("t_trig", "FREE"); s.set("t_rate", hzKnob(1)); s.set("t_shift", shift); s.set("cutoff", 28); s.p->connectCord(src(s, jack), dst(s, "CUTOFF"), 80); s.on(60); return s.run(1.5); }; const auto diff = [](const Vec& a, const Vec& b) { double d = 0; for (size_t i = 0; i < a.size(); ++i) d += std::abs(a[i] - b[i]); return d / (double)a.size(); }; const auto one = output("TIDES 1", "PHASE", 80); for (const char* jack : {"TIDES 2", "TIDES 3", "TIDES 4"}) check(diff(one, output(jack, "PHASE", 80)) > 1e-4, (juce::String(jack) + " differs from TIDES 1").toRawUTF8()); }
      for (const char* jack : {"T SHAPE", "T SLOPE", "T SMOOTH", "T RATE"}) { const auto play = [&](bool on) { Synth s; s.clean(30); s.opt("t_mode", "LOOP"); s.opt("t_out", "PHASE"); s.opt("t_trig", "FREE"); s.set("t_rate", hzKnob(2)); s.set("t_shape", 20); s.set("t_slope", 20); s.set("t_smooth", 20); s.set("cutoff", 28); s.p->connectCord(src(s, "TIDES 1"), dst(s, "CUTOFF"), 100); if (on) s.p->connectCord(src(s, "WHEEL"), dst(s, jack), 80); s.cc(1, 127); s.on(60); return s.run(1.5); }; const auto a = play(true), b = play(false); double d = 0; for (size_t i = 0; i < a.size(); ++i) d += std::abs(a[i] - b[i]); check(d / (double)a.size() > 1e-4, (juce::String(jack) + " input").toRawUTF8(), num(d / (double)a.size(), 5)); }
      { const auto play = [&](bool on) { Synth s; s.clean(30); s.opt("t_mode", "LOOP"); s.opt("t_out", "PHASE"); s.opt("t_trig", "FREE"); s.set("t_rate", hzKnob(1)); s.set("t_shift", 70); s.set("cutoff", 28); s.p->connectCord(src(s, "TIDES 2"), dst(s, "CUTOFF"), 100); if (on) s.p->connectCord(src(s, "WHEEL"), dst(s, "T SHIFT"), -60); s.cc(1, 127); s.on(60); return s.run(1.5); }; const auto a = play(true), b = play(false); double d = 0; for (size_t i = 0; i < a.size(); ++i) d += std::abs(a[i] - b[i]); check(d / (double)a.size() > 1e-4, "T SHIFT input, on TIDES 2", num(d / (double)a.size(), 5)); }
      { Synth s; s.clean(25); s.opt("t_trig", "FREE"); s.opt("t_out", "GATES"); s.p->connectCord(src(s, "TIDES 3"), dst(s, "VCA"), 100); s.on(60); s.run(0.3); s.off(60); check(silent(s), "a running Tides does not hold a note open"); }
      { int bad = 0; for (const char* trigger : {"NOTE", "FREE", "CLOCK"}) for (const char* mode : {"AD", "LOOP", "AR"}) for (const char* out : {"GATES", "AMP", "PHASE", "FREQ"}) { Synth s; s.clean(30); s.opt("t_trig", trigger); s.opt("t_mode", mode); s.opt("t_out", out); s.set("t_rate", 100); s.set("t_smooth", 100); s.set("t_shift", 100); s.p->connectCord(src(s, "TIDES 1"), dst(s, "T RATE"), 100); s.p->connectCord(src(s, "TIDES 2"), dst(s, "T SHAPE"), -100); s.p->connectCord(src(s, "TIDES 3"), dst(s, "CUTOFF"), 100); s.p->connectCord(src(s, "TIDES 4"), dst(s, "O1 PITCH"), 100); s.on(60); auto x = s.run(0.2); for (float v : x) if (!std::isfinite(v) || std::abs(v) > 1.001f) { ++bad; break; } } check(bad == 0, "every Tides mode with feedback stays finite and in range", juce::String(bad)); }
    }

    puts("--- clock");
    { const auto src = [](Synth& s, const char* n) { return s.p->apvts.getParameter("c1_src")->getAllValueStrings().indexOf(n); };
      const auto dst = [](Synth& s, const char* n) { return s.p->apvts.getParameter("c1_dst")->getAllValueStrings().indexOf(n); };
      // A gate on VCA at -100 turns the note off for each pulse, so edges in the audio are edges of the clock.
      const auto patch = [&](Synth& s, int channel = 1) { s.clean(30); s.opt("vca_mode", "ON"); s.head.bpm = 120; s.p->connectCord(src(s, ("CLOCK " + juce::String(channel)).toRawUTF8()), dst(s, "VCA"), -100); };
      const auto opt = [](Synth& s, int ch, const char* field, const char* value) { s.opt(("k" + juce::String(ch) + "_" + field).toRawUTF8(), value); };
      const auto set = [](Synth& s, int ch, const char* field, float value) { s.set(("k" + juce::String(ch) + "_" + field).toRawUTF8(), value); };
      { Synth a, b; a.clean(); b.clean(); opt(b, 1, "div", "x8"); b.set("ck_bpm", 300); a.on(60); b.on(60); auto x = a.run(0.4), y = b.run(0.4); double d = 0; for (size_t i = 0; i < x.size(); ++i) d = juce::jmax(d, (double)std::abs(x[i] - y[i])); check(d < 1e-5, "unpatched, the clock changes nothing", num(d, 7)); }
      for (const auto& [rate, want] : std::vector<std::pair<const char*, int>>{ {"x1", 16}, {"x2", 32}, {"x3", 48}, {"/2", 8}, {"/3", 5}, {"/4", 4} }) { Synth s; patch(s); opt(s, 1, "div", rate); s.on(60); s.run(0.01); int c = crossings(s.run(4.0), sr, 5); check(std::abs(c - want) <= 2, (juce::String("rate ") + rate + " at 120 BPM").toRawUTF8(), juce::String(c) + ", want " + juce::String(want)); }
      { Synth s; patch(s); s.head.bpm = 60; s.on(60); s.run(0.01); int c = crossings(s.run(4.0), sr, 5); check(c >= 7 && c <= 9, "SONG follows the host tempo", juce::String(c)); }
      { Synth s; patch(s); s.opt("ck_src", "FREE"); s.set("ck_bpm", 240); s.on(60); s.run(0.01); int c = crossings(s.run(2.0), sr, 5); check(c >= 14 && c <= 18, "FREE runs at its own BPM", juce::String(c)); }
      { const auto after = [&](double ppq) { Synth s; patch(s); opt(s, 1, "div", "/4"); s.on(60); s.run(0.05); s.head.playing = true; s.head.ppq = ppq; s.run(0.01); return rms(s.run(0.05), 200, 1800); }; const float a = after(0.5), b = after(2.5); check(std::abs(db(a, b)) > 6, "SONG locks to the song position", num(a, 4) + " vs " + num(b, 4)); }
      { const auto duty = [&](float width) { Synth s; patch(s); set(s, 1, "width", width); s.on(60); s.run(0.02); auto x = s.run(2.0); int off = 0, n = 0; for (size_t i = 0; i + 200 < x.size(); i += 200, ++n) if (rms(x, i, i + 200) < 1e-3f) ++off; return (float)off / (float)n; }; check(near(duty(25), 0.25f, 0.2f) && near(duty(75), 0.75f, 0.1f), "WIDTH sets the gate length", num(duty(25)) + ", " + num(duty(75))); }
      { Synth s; patch(s); opt(s, 1, "shape", "TRIG"); s.on(60); s.run(0.02); auto x = s.run(2.0); int off = 0, n = 0; for (size_t i = 0; i + 100 < x.size(); i += 100, ++n) if (rms(x, i, i + 100) < 1e-3f) ++off; float top = 0; for (float v : x) top = juce::jmax(top, std::abs(v)); int dips = 0; n = 0; for (size_t i = 0; i + 100 < x.size(); i += 100, ++n) if (rms(x, i, i + 100) < 0.35f * top) ++dips; check(dips > 0 && (float)dips / (float)n < 0.08f, "TRIG is a short pulse", juce::String(dips) + " of " + juce::String(n)); }
      { const auto hits = [&](int steps, int fill, int rotate) { Synth s; patch(s); set(s, 1, "steps", (float)steps); set(s, 1, "fill", (float)fill); set(s, 1, "rot", (float)rotate); s.head.playing = true; s.head.ppq = 0; s.on(60); return (crossings(s.run(4.0), sr, 5) + 1) / 2; }; check(hits(8, 3, 0) == 3, "Euclid 3 of 8 fires 3 times in 8 steps", juce::String(hits(8, 3, 0))); check(hits(8, 5, 0) == 5, "Euclid 5 of 8", juce::String(hits(8, 5, 0))); check(hits(8, 0, 0) == 0, "fill 0 is silence for the clock"); check(hits(0, 3, 0) >= 7, "steps 0 turns the pattern off", juce::String(hits(0, 3, 0))); using C = ms::WorkoutClock; int same = 0; for (int i = 0; i < 8; ++i) same += C::euclid(8, 3, 2, i) == C::euclid(8, 3, 0, i + 2); check(same == 8, "ROTATE moves the pattern"); int count = 0; for (int i = 0; i < 16; ++i) count += C::euclid(16, 5, 0, i); check(count == 5, "pattern arithmetic: 5 of 16"); }
      { const auto hits = [&](float p) { Synth s; patch(s); set(s, 1, "prob", p); s.on(60); s.run(0.01); return crossings(s.run(20.0), sr, 5) / 2; }; check(hits(0) == 0, "PROB 0 never fires", juce::String(hits(0))); check(hits(100) >= 38, "PROB 100 always fires", juce::String(hits(100))); const int half = hits(50); check(half > 8 && half < 32, "PROB 50 fires some of the time", juce::String(half)); }
      { using C = ms::WorkoutClock; ms::ClockChannelSettings set8[8]; set8[0].swing = 1.0f; C clock; clock.process(set8, 1.10, 0.5f); check(clock.out[0] == 0.0f, "SWING: an odd step waits"); clock.process(set8, 1.60, 0.5f); check(clock.out[0] == 1.0f, "...then lands late"); clock.process(set8, 0.10, 0.5f); check(clock.out[0] == 1.0f, "even steps are on time"); }
      { using C = ms::WorkoutClock; ms::ClockChannelSettings set8[8]; set8[0].phase = 0.75f; C clock; clock.process(set8, 0.0, 0.5f); check(clock.out[0] == 0.0f, "PHASE offsets the cycle"); clock.process(set8, 0.3, 0.5f); check(clock.out[0] == 1.0f, "...into the next step"); set8[0].phase = 0; set8[0].shape = C::SAW; C saw; saw.process(set8, 0.25, 0.5f); check(near(saw.out[0], 0.5f, 0.01f), "SAW falls"); set8[0].shape = C::RAMP; C ramp; ramp.process(set8, 0.25, 0.5f); check(near(ramp.out[0], -0.5f, 0.01f), "RAMP rises"); set8[0].shape = C::TRI; C tri; tri.process(set8, 0.25, 0.5f); check(std::abs(tri.out[0]) < 0.01f, "TRI"); set8[0].shape = C::SINE; C sine; sine.process(set8, 0.25, 0.5f); check(near(sine.out[0], 1.0f, 0.01f), "SINE"); set8[0].shape = C::SAMPLE_HOLD; C sh; sh.process(set8, 0.1, 0.5f); const float h = sh.out[0]; sh.process(set8, 0.9, 0.5f); check(sh.out[0] == h, "S/H holds through a step"); sh.process(set8, 1.1, 0.5f); check(sh.out[0] != h, "...and moves on the next"); set8[0].shape = C::GATE; set8[0].level = 0.5f; set8[0].offset = 0.25f; C lvl; lvl.process(set8, 0.1, 0.5f); check(near(lvl.out[0], 0.75f, 0.01f), "LEVEL and OFFSET"); set8[0].level = 1; set8[0].offset = 1; C top; top.process(set8, 0.1, 0.5f); check(top.out[0] == 1.0f, "output stays within +-1"); }
      for (int c = 1; c <= 8; ++c) { Synth s; patch(s, c); opt(s, c, "div", "x2"); s.on(60); s.run(0.01); int n = crossings(s.run(2.0), sr, 5); check(n >= 14 && n <= 18, ("CLOCK " + juce::String(c) + " output").toRawUTF8(), juce::String(n)); }
      { Synth s; s.clean(30); s.head.bpm = 120; s.set("sustain", 0); s.set("decay", 30); s.p->connectCord(src(s, "CLOCK 1"), dst(s, "ENV TRIG"), 100); s.on(60); s.run(0.02); auto x = s.run(2.0); int peaks = crossings(x, sr, 5) / 2; check(peaks >= 3, "ENV TRIG restarts the VCA envelope on each pulse", juce::String(peaks)); }
      { const auto treble = [&](bool patched) { Synth s; s.clean(30); s.head.bpm = 120; s.set("cutoff", 28); s.set("f_mod", 100); s.set("m_sustain", 0); s.set("m_decay", 45); s.opt("vca_mode", "ON"); if (patched) s.p->connectCord(src(s, "CLOCK 1"), dst(s, "MOD TRIG"), 100); s.on(60); s.run(1.0); auto x = s.run(2.0); double d = 0; for (size_t i = 2; i < x.size(); ++i) { const double v = x[i] - 2 * x[i - 1] + x[i - 2]; d += v * v; } return (float)std::sqrt(d / (double)x.size()); }; check(treble(true) > 1.5f * treble(false), "MOD TRIG restarts the MOD envelope", num(db(treble(true), treble(false))) + " dB"); }
      { const auto strikes = [&](bool patched) { Synth s; s.clean(30); s.head.bpm = 120; s.set("o1_engine", 13); s.opt("vca_mode", "ON"); if (patched) { s.p->connectCord(src(s, "CLOCK 1"), dst(s, "O1 TRIG"), 100); opt(s, 1, "shape", "TRIG"); } s.on(48); s.run(1.0); auto x = s.run(3.0); return rms(x); }; check(strikes(true) > 3 * strikes(false), "O1 TRIG strikes a drum model again and again while a key is held", num(db(strikes(true), strikes(false))) + " dB"); }
      { Synth s; s.clean(30); s.head.bpm = 120; s.set("o1_engine", 13); s.opt("vca_mode", "ON"); s.p->connectCord(src(s, "CLOCK 1"), dst(s, "O1 TRIG"), 100); s.on(48); s.run(0.3); s.off(48); s.run(2.0); check(rms(s.run(0.3)) < 1e-3f, "a clock does not keep a released drum going"); }
      { const auto tides = [&](bool patched) { Synth s; s.clean(30); s.head.bpm = 120; s.opt("t_mode", "AD"); s.opt("t_out", "PHASE"); s.set("t_rate", 54.5f); s.set("cutoff", 25); s.p->connectCord(src(s, "TIDES 1"), dst(s, "CUTOFF"), 100); if (patched) s.p->connectCord(src(s, "CLOCK 1"), dst(s, "T TRIG"), 100); s.on(60); s.run(1.0); return rms(s.run(2.0)); }; check(tides(true) > 1.2f * tides(false), "T TRIG fires Tides, even during a held note", num(db(tides(true), tides(false))) + " dB"); }
      { // PATCH: only what is patched into T TRIG fires Tides. Not a note, and not by itself.
        const auto play = [&](const char* mode, bool patched) { Synth s; s.clean(30); s.head.bpm = 120; s.opt("t_trig", "PATCH"); s.opt("t_mode", mode); s.opt("t_out", "PHASE"); s.set("t_rate", 54.5f); s.set("cutoff", 25); s.p->connectCord(src(s, "TIDES 1"), dst(s, "CUTOFF"), 100); if (patched) { s.p->connectCord(src(s, "CLOCK 2"), dst(s, "T TRIG"), 100); opt(s, 2, "shape", juce::String(mode) == "AR" ? "GATE" : "TRIG"); opt(s, 2, "div", juce::String(mode) == "AR" ? "x1" : "x2"); } s.on(60); s.run(0.5); return s.run(2.0); };
        const auto wobble = [](const Vec& x) { float lo = 1e9f, hi = 0; for (size_t i = 0; i + 1000 < x.size(); i += 1000) { const float r = rms(x, i, i + 1000); lo = juce::jmin(lo, r); hi = juce::jmax(hi, r); } return db(hi, juce::jmax(lo, 1e-6f)); };
        check(wobble(play("AD", true)) > 3, "PATCH, AD: our clock fires Tides", num(wobble(play("AD", true))) + " dB of movement");
        check(wobble(play("AD", false)) < 1, "PATCH, AD: with no clock it stays at rest, even with a note held", num(wobble(play("AD", false))) + " dB");
        check(wobble(play("AR", true)) > 3, "PATCH, AR: our clock fires Tides", num(wobble(play("AR", true))) + " dB");
        const auto a = play("LOOP", true), b = play("LOOP", false); double d = 0; for (size_t i = 0; i < a.size(); ++i) d += std::abs(a[i] - b[i]); check(d / (double)a.size() > 1e-4, "PATCH, LOOP: our clock restarts the cycle", num(d / (double)a.size(), 5));
        for (const char* trig : {"NOTE", "FREE", "CLOCK"}) { const auto play2 = [&](bool patched) { Synth s; s.clean(30); s.head.bpm = 120; s.opt("t_trig", trig); s.opt("t_mode", "AD"); s.opt("t_out", "PHASE"); s.set("t_rate", 54.5f); s.opt("t_div", "1 BAR"); s.set("cutoff", 25); s.p->connectCord(src(s, "TIDES 1"), dst(s, "CUTOFF"), 100); if (patched) { s.p->connectCord(src(s, "CLOCK 1"), dst(s, "T TRIG"), 100); opt(s, 1, "shape", "TRIG"); opt(s, 1, "div", "x3"); } s.on(60); s.run(0.5); return s.run(2.0); }; const auto x = play2(true), y = play2(false); double e = 0; for (size_t i = 0; i < x.size(); ++i) e += std::abs(x[i] - y[i]); check(e / (double)x.size() > 1e-4, (juce::String(trig) + " mode: our clock can fire Tides too").toRawUTF8(), num(e / (double)x.size(), 5)); }
      }
      { Synth s; s.clean(30); s.head.bpm = 120; s.opt("vca_mode", "ON"); s.opt("lfo_shape", "SQR"); s.set("lfo_rate", 20); s.set("cutoff", 30); s.set("f_mod", 100); s.opt("f_src", "LFO"); s.p->connectCord(src(s, "CLOCK 1"), dst(s, "LFO RESET"), 100); s.on(60); s.run(0.01); int n = crossings(s.run(4.0), sr, 5); check(n == 0, "LFO RST keeps restarting a slow LFO, so it never reaches its second half", juce::String(n)); }
      { int bad = 0; Synth s; s.opt("o2_on", "ON"); s.opt("o3_on", "ON"); s.opt("ck_src", "FREE"); s.set("ck_bpm", 300); for (int c = 1; c <= 8; ++c) { opt(s, c, "div", "x16"); opt(s, c, "shape", c % 2 ? "SAW" : "TRIG"); set(s, c, "swing", 100); set(s, c, "offset", 100); } for (int c = 0; c < 12; ++c) s.p->connectCord(17 + c % 8, 1 + (c * 5) % 32, c % 2 ? 100 : -100); for (int i = 0; i < 8; ++i) s.on(ns[i], 127); auto x = s.run(1.0); for (float v : x) if (!std::isfinite(v) || std::abs(v) > 1.001f) ++bad; check(bad == 0, "12 clock cords at maximum rate into everything: finite and in range", juce::String(bad)); for (int i = 0; i < 8; ++i) s.off(ns[i]); s.set("release", 10); s.run(4.0); check(rms(s.run(0.3)) < 1e-3f, "...and it ends"); }
    }

    puts("--- Elements");
    { const auto src = [](Synth& s, const char* n) { return s.p->apvts.getParameter("c1_src")->getAllValueStrings().indexOf(n); };
      const auto dst = [](Synth& s, const char* n) { return s.p->apvts.getParameter("c1_dst")->getAllValueStrings().indexOf(n); };
      const auto el = [](Synth& s, float vol = 30) { s.clean(vol); s.opt("o1_type", "ELEMENTS"); s.opt("vca_mode", "ON"); s.set("e_space", 0); };
      const auto silentExciter = [](Synth& s) { s.set("e_strike", 0); s.set("e_bow", 0); s.set("e_blow", 0); };
      const auto different = [](const Vec& a, const Vec& b) { double d = 0; for (size_t i = 0; i < a.size(); ++i) d += std::abs(a[i] - b[i]); return d / (double)a.size(); };
      { Synth s; el(s); s.on(69); const float r = rms(s.run(0.5)); check(r > 1e-3f, "ELEMENTS sounds on OSC 1", num(r, 4)); }
      { Synth a, b; a.clean(); b.clean(); b.set("e_strike", 100); b.set("e_geom", 90); a.on(60); b.on(60); check(different(a.run(0.4), b.run(0.4)) < 1e-6, "as PLAITS, its controls do nothing"); }
      // Tuning, read from the spectrum with a steady bow. Struck, inharmonic sounds fool an autocorrelation, and at high
      // GEOMETRY the STRING model is deliberately stiff, and STRINGS adds a sub-octave, so those are not tuning errors.
      { const auto strongest = [&](const Vec& x, float want, double rate) { float best = 0, at = 0; for (float f = want * 0.92f; f < want * 1.08f; f *= 1.0005f) { const float v = hannTone(x, f, rate, 0); if (v > best) { best = v; at = f; } } return at; };
        for (const auto& [model, note, want, rate] : std::vector<std::tuple<const char*, int, float, double>>{ {"MODAL", 69, 440.f, 44100}, {"MODAL", 57, 220.f, 44100}, {"STRING", 69, 440.f, 44100}, {"STRING", 57, 220.f, 44100}, {"MODAL", 69, 440.f, 48000}, {"MODAL", 69, 440.f, 96000} }) { Synth s(rate); el(s); s.opt("e_model", model); s.set("e_geom", 20); s.set("e_strike", 0); s.set("e_bow", 80); s.set("e_damp", 70); s.on(note); s.run(0.6); const float f = strongest(s.run(0.8), want, rate); const float cents = 1200.0f * std::log2(f / want); check(std::abs(cents) < 15, (juce::String(model) + " note " + juce::String(note) + " at " + juce::String(rate / 1000, 0) + " kHz").toRawUTF8(), num(f) + " Hz, " + num(cents, 1) + " cents"); }
        { Synth s; el(s); s.set("o1_oct", 1); s.set("e_geom", 20); s.set("e_strike", 0); s.set("e_bow", 80); s.set("e_damp", 70); s.on(57); s.run(0.6); const float f = strongest(s.run(0.8), 440.f, sr); check(std::abs(1200.0f * std::log2(f / 440.f)) < 15, "OCTAVE reaches it", num(f) + " Hz"); }
        { Synth s; el(s); s.set("bend_range", 12); s.bend(16383); s.set("e_geom", 20); s.set("e_strike", 0); s.set("e_bow", 80); s.set("e_damp", 70); s.on(57); s.run(0.6); const float f = strongest(s.run(0.8), 440.f, sr); check(std::abs(1200.0f * std::log2(f / 440.f)) < 15, "pitch bend reaches it", num(f) + " Hz"); }
      }
      { const auto m = [&](const char* name) { Synth s; el(s); s.opt("e_model", name); s.on(60); return s.run(0.5); }; const auto a = m("MODAL"), b = m("STRING"), c = m("STRINGS"); check(different(a, b) > 1e-4 && different(b, c) > 1e-4, "the three resonator models differ"); }
      { Synth s; el(s); silentExciter(s); s.on(60); check(rms(s.run(0.5)) < 1e-5f, "with no exciter it is silent"); }
      { const auto level = [&](const char* k) { Synth s; el(s); silentExciter(s); s.set(k, 80); s.set("e_damp", 50); s.on(60); auto x = s.run(2.0); return std::vector<float>{ rmsAt(x, 0.02, 0.2, sr), rmsAt(x, 1.5, 0.3, sr) }; }; const auto st = level("e_strike"), bw = level("e_bow"), bl = level("e_blow"); check(st[0] > 1e-3f && st[1] < 0.5f * st[0], "STRIKE: hits, then rings down while held", num(st[0], 4) + " -> " + num(st[1], 4)); check(bw[1] > 1e-3f, "BOW sustains while held", num(bw[1], 4)); check(bl[1] > 1e-3f, "BLOW sustains while held", num(bl[1], 4)); }
      { Synth s; el(s); silentExciter(s); s.set("e_bow", 80); s.on(60); s.run(0.5); s.off(60); s.run(3.0); check(rms(s.run(0.3)) < 1e-3f, "BOW stops after key up"); }
      for (const char* k : {"e_contour", "e_bow_t", "e_flow", "e_blow_t", "e_mallet", "e_strike_t", "e_geom", "e_bright", "e_damp", "e_pos"}) { const auto play = [&](float v) { Synth s; el(s); s.set("e_strike", 80); s.set("e_bow", 40); s.set("e_blow", 40); s.set(k, v); s.on(60); return s.run(0.5); }; const auto a = play(10), b = play(90); check(different(a, b) > 1e-4, (juce::String(k) + " changes the sound").toRawUTF8(), num(different(a, b), 5)); }
      { const auto tail = [&](float space) { Synth s; el(s); s.set("e_space", space); s.set("e_damp", 10); s.set("e_strike", 100); s.on(60); s.run(0.02); s.off(60); auto x = s.run(1.5); return rmsAt(x, 1.0, 0.4, sr); }; check(tail(100) > 2 * tail(0), "SPACE adds a reverb tail", num(tail(0), 5) + " -> " + num(tail(100), 5)); }
      { Synth s; el(s); silentExciter(s); s.opt("o2_on", "ON"); s.set("o2_level", 0); s.set("e_damp", 60); s.p->connectCord(src(s, "OSC 2"), dst(s, "E STK IN"), 100); s.on(60); const float r = rms(s.run(0.8), 8000); check(r > 1e-3f, "OSC 2 > E STK IN excites the resonator with audio", num(r, 4)); }
      { Synth s; el(s); silentExciter(s); s.set("noise", 0); s.p->connectCord(src(s, "NOISE"), dst(s, "E BLW IN"), 100); s.on(60); const float r = rms(s.run(0.8), 8000); check(r > 1e-3f, "NOISE > E BLW IN excites it", num(r, 4)); }
      { Synth s; el(s); silentExciter(s); s.p->connectCord(src(s, "WHEEL"), dst(s, "E STRIKE"), 100); s.cc(1, 127); s.on(60); check(rms(s.run(0.5)) > 1e-3f, "WHEEL > E STRIKE"); }
      for (const char* jack : {"E GEOM", "E BRIGHT", "E DAMP", "E POS", "E SPACE", "E BOW", "E BLOW"}) { const auto play = [&](bool on) { Synth s; el(s); s.set("e_space", 30); s.set("e_strike", 80); s.set("e_geom", 30); s.set("e_bright", 30); s.set("e_damp", 30); s.set("e_pos", 30); if (on) s.p->connectCord(src(s, "WHEEL"), dst(s, jack), 60); s.cc(1, 127); s.on(60); return s.run(0.6); }; const auto a = play(true), b = play(false); check(different(a, b) > 1e-4, (juce::String(jack) + " input").toRawUTF8(), num(different(a, b), 5)); }
      { const auto strikes = [&](bool patched) { Synth s; el(s); s.head.bpm = 120; s.set("e_damp", 10); s.p->connectCord(src(s, "LFO"), dst(s, "NOISE"), 0); if (patched) s.p->connectCord(src(s, "CLOCK 1"), dst(s, "O1 TRIG"), 100); s.opt("k1_shape", "TRIG"); s.on(60); s.run(1.0); return rms(s.run(3.0)); }; check(strikes(true) > 3 * strikes(false), "CLOCK > O1 TRIG strikes it again while a key is held", num(db(strikes(true), strikes(false))) + " dB"); }
      { Synth s; el(s); s.opt("o2_on", "OFF"); s.p->connectCord(src(s, "OSC 1"), dst(s, "CUTOFF"), 50); s.set("cutoff", 40); s.on(60); check(rms(s.run(0.5)) > 1e-3f, "its output patches like any oscillator"); }
      { Synth s; el(s); s.set("decay", 10); s.set("e_space", 100); s.set("e_damp", 0); s.set("e_strike", 100); s.opt("vca_mode", "ENV"); s.set("voices", 1); s.on(60); s.run(0.3); s.off(60); s.run(0.5); silentExciter(s); s.on(60); const float r = rms(s.run(0.2)); check(r < 1e-4f, "a new note hears nothing of the note before", num(r, 6)); }
      { Synth s; el(s); s.set("e_damp", 50); s.on(60); s.run(0.3); s.off(60); s.run(4.0); check(rms(s.run(0.3)) < 1e-4f, "it ends after key up in VCA ON"); }
      for (const char* input : {"E STK IN", "E BLW IN"}) for (const char* source : {"OSC 2", "NOISE"}) { Synth s; el(s); silentExciter(s); s.set("e_damp", 60); s.opt("o2_on", "ON"); s.set("o2_level", 0); s.p->connectCord(src(s, source), dst(s, input), 90); s.on(60); const float r = rms(s.run(0.5), 5000); s.off(60); s.run(6.0); const float after = rms(s.run(0.3)); check(r > 1e-3f && after < 1e-4f, (juce::String(source) + " > " + input + " stops with the key").toRawUTF8(), num(r, 4) + " -> " + num(after, 6)); }
      for (const char* mode : {"ENV", "LPG"}) { Synth s; el(s); s.opt("vca_mode", mode); s.set("e_damp", 50); s.on(60); const float r = rms(s.run(0.3)); s.off(60); s.run(5.0); check(r > 1e-3f && rms(s.run(0.3)) < 1e-4f, (juce::String("it sounds and ends in VCA ") + mode).toRawUTF8(), num(r, 4)); }
      { Synth s; el(s); for (int i = 0; i < 8; ++i) s.on(ns[i], 110); s.run(0.05); auto x = s.run(0.5); int c = 0; for (int i = 0; i < 8; ++i) c += tone(x, hz(ns[i]), sr) > 1e-4f; check(c >= 7, "8 voices play at once", juce::String(c)); }
      { int bad = 0; for (const char* model : {"MODAL", "STRING", "STRINGS"}) { Synth s; s.opt("o1_type", "ELEMENTS"); s.opt("e_model", model); s.set("e_strike", 100); s.set("e_bow", 100); s.set("e_blow", 100); s.set("e_damp", 100); s.set("e_space", 100); s.set("volume", 100); s.set("reso", 100); s.opt("o2_on", "ON"); s.opt("o3_on", "ON"); s.p->connectCord(src(s, "OSC 1"), dst(s, "E STK IN"), 100); s.p->connectCord(src(s, "OSC 1"), dst(s, "E BLW IN"), 100); s.p->connectCord(src(s, "OSC 2"), dst(s, "E STK IN"), 100); s.p->connectCord(src(s, "NOISE"), dst(s, "E BLW IN"), 100); s.p->connectCord(src(s, "OSC 1"), dst(s, "O1 PITCH"), 100); s.p->connectCord(src(s, "OSC 1"), dst(s, "E GEOM"), 100); s.set("noise", 100); for (int i = 0; i < 8; ++i) s.on(ns[i], 127); auto x = s.run(1.5); for (float v : x) if (!std::isfinite(v) || std::abs(v) > 1.001f) { ++bad; break; } for (int i = 0; i < 8; ++i) s.off(ns[i]); s.set("release", 10); s.set("e_damp", 30); s.run(6.0); if (rms(s.run(0.3)) > 1e-3f) ++bad; } check(bad == 0, "feedback into its own exciter: finite, in range, and it ends", juce::String(bad)); }
    }

    puts("--- voices that could ring forever");
    { const auto src = [](Synth& s, const char* n) { return s.p->apvts.getParameter("c1_src")->getAllValueStrings().indexOf(n); }; (void)src;
      // With its trigger patched, Plaits' gate does the job, not the VCA, so anything it cannot reach must be stopped another way.
      for (const char* mode : {"LPG", "ON"}) for (float model : {11.0f, 12.0f}) { Synth s; s.clean(30); s.set("o1_engine", model); s.set("o1_morph", 100); s.opt("vca_mode", mode); s.on(60); s.run(0.2); s.off(60); auto early = s.run(5.0); const float ringing = rmsAt(early, 4.5, 0.4, sr); s.run(34.0); const float gone = rms(s.run(0.5)); check(ringing > 1e-4f && gone < 1e-5f, (juce::String(model == 11 ? "String" : "Modal") + " at full MORPH in " + mode + " rings on, then ends").toRawUTF8(), num(ringing, 5) + " -> " + num(gone, 6)); }
      for (const char* mode : {"LPG", "ON"}) { Synth s; s.clean(30); s.set("o1_engine", 13); s.set("reso", 100); s.set("noise", 2); s.opt("vca_mode", mode); s.on(60); s.run(0.2); s.off(60); s.run(2.0); check(rms(s.run(0.3)) < 1e-4f, (juce::String("a self-oscillating filter does not hang VCA ") + mode).toRawUTF8()); }
      { Synth s; s.clean(30); s.set("reso", 100); s.noiseOnly(3); s.on(60); s.run(0.5); const float held = rms(s.run(0.5)); check(held > 1e-3f, "a held self-oscillating filter keeps singing", num(held, 4)); s.off(60); s.run(3.0); check(rms(s.run(0.3)) < 1e-4f, "...until the key is released"); }
    }

    puts("--- per-oscillator amp");
    { const auto setUp = [](Synth& s) { s.clean(30); s.set("o1_engine", 2); s.set("o1_timbre", 0); s.set("o2_engine", 2); s.set("o2_timbre", 0); s.opt("o2_on", "ON"); s.set("o2_semi", 7); s.set("lpg_decay", 35); s.set("sustain", 100); s.set("release", 20); };
      const float f1 = 440.0f, f2 = 659.26f;
      const auto level = [&](const Vec& x, float f, double t, double w) { return hannTone(Vec(x.begin() + (long)(t * sr), x.begin() + (long)((t + w) * sr)), f, sr, 0); };
      { Synth a, b; setUp(a); setUp(b); a.on(69); b.on(69); auto x = a.run(0.5), y = b.run(0.5); double d = 0; for (size_t i = 0; i < x.size(); ++i) d += std::abs(x[i] - y[i]); check(d == 0, "AUTO on all three leaves the old path alone"); }
      { Synth s; setUp(s); s.opt("o1_amp", "LPG"); s.opt("o2_amp", "ENV"); s.on(69); auto x = s.run(2.0); check(level(x, f1, 0.02, 0.1) > 3 * level(x, f1, 1.6, 0.3), "OSC 1 on LPG dies away while held", num(db(level(x, f1, 0.02, 0.1), level(x, f1, 1.6, 0.3))) + " dB"); check(near(level(x, f2, 1.6, 0.3), level(x, f2, 0.3, 0.3), 0.15f), "OSC 2 on ENV sustains beside it", num(db(level(x, f2, 0.3, 0.3), level(x, f2, 1.6, 0.3))) + " dB"); }
      { Synth s; setUp(s); s.opt("o1_amp", "LPG"); s.opt("o2_amp", "ENV"); s.set("lpg_decay", 75); s.on(69); s.run(0.2); s.off(69); auto x = s.run(1.0); check(level(x, f1, 0.1, 0.2) > 1e-4f, "after the key, LPG rings on past a short release", num(level(x, f1, 0.1, 0.2), 5)); check(level(x, f1, 0.1, 0.2) > 5 * level(x, f2, 0.1, 0.2), "...while the ENV oscillator is cut by it", num(db(level(x, f1, 0.1, 0.2), level(x, f2, 0.1, 0.2))) + " dB"); }
      { const auto after = [&](float decay) { Synth s; setUp(s); s.opt("o1_amp", "LPG"); s.opt("o2_on", "OFF"); s.set("lpg_decay", decay); s.on(69); return rmsAt(s.run(0.8), 0.5, 0.2, sr); }; check(after(70) > 3 * after(30), "LPG DECAY shapes it", num(db(after(70), after(30))) + " dB"); }
      { const auto attack = [&](const char* amp) { Synth s; setUp(s); s.opt("o1_amp", amp); s.opt("o2_on", "OFF"); s.set("attack", 70); s.on(69); return rms(s.run(0.05)); }; check(attack("ENV") < 0.3f * attack("LPG"), "the VCA envelope shapes ENV, not LPG", num(db(attack("LPG"), attack("ENV"))) + " dB"); }
      { const auto late = [&](const char* amp) { Synth s; s.clean(30); s.set("o1_engine", 13); s.opt("o1_amp", amp); s.on(48); s.run(1.5); return rms(s.run(0.5)); }; check(late("ENV") > 10 * late("AUTO"), "ENV unpatches the trigger: a drum model becomes a plain, sustained oscillator", num(db(late("ENV"), late("AUTO"))) + " dB"); }
      for (const char* vca : {"ENV", "LPG", "ON"}) for (const char* one : {"AUTO", "ENV", "LPG"}) for (const char* two : {"AUTO", "ENV", "LPG"}) { Synth s; setUp(s); s.opt("vca_mode", vca); s.opt("o1_amp", one); s.opt("o2_amp", two); s.set("noise", 10); s.on(69); const float r = rms(s.run(0.3)); s.off(69); s.run(3.0); check(r > 1e-3f && rms(s.run(0.3)) < 1e-4f, (juce::String("VCA ") + vca + ", OSC 1 " + one + ", OSC 2 " + two + ": sounds and ends").toRawUTF8(), num(r, 4)); }
      { Synth s; setUp(s); s.opt("o1_amp", "ENV"); s.opt("o2_on", "OFF"); s.on(69); s.run(0.2); s.off(69); s.run(0.3); s.on(69); auto x = s.run(0.05); check(rms(x) > 1e-3f, "a voice can be played again afterwards"); }
    }

    puts("--- level inputs do not click");
    { const auto src = [](Synth& s, const char* n) { return s.p->apvts.getParameter("c1_src")->getAllValueStrings().indexOf(n); };
      const auto dst = [](Synth& s, const char* n) { return s.p->apvts.getParameter("c1_dst")->getAllValueStrings().indexOf(n); };
      // A clean sine: the largest step between samples is the click, if there is one. An ungated sine at this note steps about 4% of peak.
      const auto play = [&](const char* from, const char* into, float amount, const std::function<void(Synth&)>& patch) { Synth s; s.clean(50); s.set("o1_engine", 2); s.set("o1_timbre", 0); s.opt("vca_mode", "ON"); s.head.bpm = 120; patch(s); s.p->connectCord(src(s, from), dst(s, into), amount); s.on(57); s.run(0.2); return s.run(2.0); };
      const auto worstStep = [](const Vec& x) { float step = 0, peak = 0; for (size_t i = 1; i < x.size(); ++i) { step = juce::jmax(step, std::abs(x[i] - x[i - 1])); peak = juce::jmax(peak, std::abs(x[i])); } return step / juce::jmax(1e-6f, peak); };
      const auto gate = [&](Synth& s) { s.opt("k1_div", "x4"); s.opt("k1_shape", "GATE"); s.set("k1_width", 45); };
      { const auto x = play("CLOCK 1", "VCA", -100, gate); check(worstStep(x) < 0.15f, "CLOCK gate > VCA has no click", num(100 * worstStep(x), 1) + "% of peak per sample"); check(crossings(x, sr, 5) >= 28, "...and still gates in time", juce::String(crossings(x, sr, 5))); }
      { const auto x = play("CLOCK 1", "O1 LEVEL", 100, [&](Synth& s) { gate(s); s.set("o1_level", 0); }); check(worstStep(x) < 0.15f, "CLOCK gate > O1 LEVEL has no click", num(100 * worstStep(x), 1) + "% of peak per sample"); check(crossings(x, sr, 5) >= 28, "...and still gates in time", juce::String(crossings(x, sr, 5))); }
      { const auto x = play("TIDES 3", "VCA", -100, [&](Synth& s) { s.opt("t_trig", "FREE"); s.opt("t_mode", "LOOP"); s.opt("t_out", "GATES"); s.set("t_rate", 52); }); check(worstStep(x) < 0.15f, "TIDES gate > VCA has no click, as in T Gate Chop", num(100 * worstStep(x), 1) + "% of peak per sample"); }
      { const auto x = play("LFO", "VCA", -100, [&](Synth& s) { s.opt("lfo_shape", "SQR"); s.set("lfo_rate", 62); }); check(worstStep(x) < 0.15f, "square LFO > VCA has no click", num(100 * worstStep(x), 1) + "% of peak per sample"); }
      { Synth s; s.clean(50); s.noiseOnly(0); s.head.bpm = 120; s.set("cutoff", 20); s.opt("vca_mode", "ON"); gate(s); s.p->connectCord(src(s, "CLOCK 1"), dst(s, "NOISE"), 100); s.on(57); s.run(0.2); auto x = s.run(2.0); float step = 0, peak = 0; for (size_t i = 1; i < x.size(); ++i) { step = juce::jmax(step, std::abs(x[i] - x[i - 1])); peak = juce::jmax(peak, std::abs(x[i])); } check(peak > 1e-3f && crossings(x, sr, 5) >= 20, "CLOCK gate > NOISE gates the noise, and goes fully quiet between", juce::String(crossings(x, sr, 5))); }
      { Synth s; s.clean(50); s.set("o1_engine", 2); s.set("o1_timbre", 0); s.opt("vca_mode", "ON"); s.head.bpm = 120; gate(s); s.p->connectCord(src(s, "CLOCK 1"), dst(s, "VCA"), -100); s.on(57); s.run(0.2); auto x = s.run(1.0); std::vector<float> level; for (size_t i = 0; i + 44 < x.size(); i += 22) level.push_back(rms(x, i, i + 44)); float high = 0; for (float v : level) high = juce::jmax(high, v); int fall = 1000; for (size_t i = 0; i < level.size(); ++i) if (level[i] > 0.8f * high) for (size_t k = i + 1; k < level.size() && (int)(k - i) / 2 < fall; ++k) if (level[k] < 0.1f * high) { fall = (int)(k - i) / 2; break; } check(fall < 12, "the gate's edge is still sharp", "falls in about " + juce::String(fall) + " ms"); }
    }

    puts("--- noise, models, extremes, state");
    { Synth s; s.clean(); s.noiseOnly(0); s.on(60); check(rms(s.run(0.3)) < 1e-5f, "all oscillators off, noise 0: silent"); Synth n; n.clean(); n.noiseOnly(50); n.on(60); check(rms(n.run(0.3), 500) > 1e-3f, "NOISE alone sounds"); }
    { int bad = 0; juce::String names; for (int model = 0; model < 24; ++model) { Synth s; s.clean(); s.set("o1_engine", (float)model); s.on(60); const float r = rms(s.run(0.4)); s.off(60); s.run(3.0); bool ok = r > 1e-4f; if (!ok) { ++bad; names << model << " "; } } check(bad == 0, "every model sounds", names); }
    { int bad = 0; for (int shape = 0; shape < 5; ++shape) { Synth s; s.set("o1_engine", (float)(shape * 4)); s.opt("o2_on", "ON"); s.opt("o3_on", "ON"); s.set("reso", 100); s.set("drive", 100); s.set("volume", 100); s.set("noise", 100); s.set("cutoff", 100 - shape * 20.0f); s.set("o1_fm", 100); s.set("vibrato", 100); s.set("lfo_rate", 100); s.opt("lfo_shape", juce::StringArray({"TRI","SQR","SAW","RAMP","S/H"})[shape].toRawUTF8()); for (int i = 0; i < 8; ++i) s.on(ns[i], 127); auto x = s.run(1.0); for (float v : x) if (!std::isfinite(v) || std::abs(v) > 1.001f) { ++bad; break; } } check(bad == 0, "worst-case patches: finite and within +-1"); }
    { auto rendered = [&](int bs) { Synth s(44100, bs); s.clean(); s.on(69); s.run(0.2); return pitch(s.run(0.5), 44100); }; for (int bs : {1, 7, 64, 128, 1024, 4096}) check(near(rendered(bs), 440, 0.01f), (juce::String("block size ") + juce::String(bs)).toRawUTF8()); }
    { Synth a; a.set("cutoff", 33); a.opt("f_mode", "HP"); a.opt("o2_on", "ON"); a.opt("voice_mode", "LEGATO"); a.set("o3_engine", 17); a.set("voices", 3); a.set("bend_range", 7); a.opt("lfo_div", "1/16"); a.set("o2_semi", -5);
      juce::MemoryBlock mb; a.p->getStateInformation(mb); Synth b; b.p->setStateInformation(mb.getData(), (int)mb.getSize()); int diff = 0; juce::String which; for (auto* q : a.p->getParameters()) { auto* r = dynamic_cast<juce::RangedAudioParameter*>(q); if (r == nullptr) continue; if (std::abs(r->getValue() - b.p->apvts.getParameter(r->getParameterID())->getValue()) > 1e-5f) { ++diff; which << r->getParameterID() << " "; } } check(diff == 0, "save and reload restores every parameter", which);
      for (auto* q : b.p->getParameters()) (void)q; a.on(60); b.on(60); a.run(0.1); b.run(0.1); }
    { MillstoneProcessor p; int helper = 0; for (auto* q : p.getParameters()) if (q->getName(64).containsIgnoreCase("Closed") || dynamic_cast<juce::RangedAudioParameter*>(q)->getParameterID().contains("__open")) ++helper; check(helper == 0, "no internal helper parameters shown to the host", juce::String(helper) + " exposed"); }
    { MillstoneProcessor p; check(p.getTailLengthSeconds() >= 10.0, "host told about the release tail", num(p.getTailLengthSeconds()) + " s"); }

    puts("--- effects");
    {   // Init is dry, and what a preset says it uses is what it has on.
        const auto& library = MillstoneProcessor::factoryPresets();
        const auto value = [](const MillstoneProcessor::Preset& q, const char* key) { for (const auto& [k, v] : q.values) if (k == key) return v; return -1.0f; };
        check(value(library[0], "fx_dl_mix") == 0 && value(library[0], "fx_rv_mix") == 0 && value(library[0], "fx_ph_mix") == 0 && value(library[0], "fx_ch_mode") == 0, "Init has every effect off");
        int wrong = 0, withFx = 0, first = 0;
        for (const auto& q : library) {
            const bool delay = value(q, "fx_dl_mix") > 0, reverb = value(q, "fx_rv_mix") > 0;
            wrong += delay != q.uses.contains("Delay") || reverb != q.uses.contains("Reverb");
            if (q.pack == "Factory 1") { withFx += delay || reverb || q.uses.contains("Colour FX"); ++first; }
        }
        check(wrong == 0, "a preset's tags say which effects it has on");
        check(withFx > 20 && withFx < first * 2 / 3, "the first library has a sprinkle, not a coat", juce::String(withFx));
    }
    {   // The echo of a short note lands on the song's tempo, and moves with it.
        const auto echoAt = [&](double bpm) {
            Synth s; s.head.bpm = bpm; s.clean(); s.set("release", 0); s.set("decay", 5); s.set("sustain", 0);
            s.opt("fx_dl_sync", "SYNC"); s.opt("fx_dl_div", "1/4"); s.set("fx_dl_mix", 50); s.set("fx_dl_fb", 0); s.set("fx_dl_wow", 0);
            s.run(0.05); s.on(60); auto a = s.run(0.08); s.off(60); auto tail = s.run(1.5);
            float top = 0; for (float x : a) top = std::max(top, std::abs(x));
            size_t quiet = 0; while (quiet < tail.size() && std::abs(tail[quiet]) > top * 0.03f) ++quiet;   // the note's own end
            for (size_t i = quiet + 2000; i < tail.size(); ++i) if (std::abs(tail[i]) > top * 0.1f) return (double)(i + a.size()) / 44100.0;
            return -1.0;
        };
        const double at120 = echoAt(120), at60 = echoAt(60);
        check(std::abs(at120 - 0.5) < 0.03, "a synced 1/4 at 120 BPM echoes after half a second", num(at120) + " s");
        check(std::abs(at60 - 1.0) < 0.03, "and at 60 BPM after a whole second", num(at60) + " s");
    }
    {   // Stereo, bounded, and a whole chain at full tilt on a loud chord.
        Synth s; s.opt("fx_ch_mode", "I+II"); s.set("fx_ph_mix", 100); s.set("fx_dl_mix", 100); s.set("fx_dl_fb", 120); s.set("fx_dl_drive", 100); s.opt("fx_dl_sync", "FREE"); s.set("fx_dl_time", 40);
        s.opt("fx_dl_heads", "1+2+3"); s.opt("fx_rv_type", "SPRING"); s.set("fx_rv_mix", 100); s.set("fx_rv_decay", 100);
        for (int n : { 36, 48, 55, 60, 64, 67, 72, 79 }) s.on(n, 127);
        float top = 0; bool bad = false; auto out = s.run(3.0); for (float x : out) { top = std::max(top, std::abs(x)); bad = bad || !std::isfinite(x); }
        check(!bad && top <= 1.0f, "every effect flat out on a full chord stays within full scale", num(top));
    }
    {   // The host's tail knows about the echoes, and an endless one is capped so a bounce still finishes.
        Synth s; const double plain = s.p->getTailLengthSeconds();
        s.set("fx_dl_mix", 50); s.set("fx_dl_fb", 50); s.run(0.02); const double some = s.p->getTailLengthSeconds();
        s.set("fx_dl_fb", 120); s.run(0.02); const double endless = s.p->getTailLengthSeconds();
        check(some > plain && endless > some && endless < 100.0, "the tail reported follows the delay", num(some) + " " + num(endless) + " s");
    }
    {   // Saved and restored with the set.
        Synth a; a.set("fx_dl_fb", 77); a.opt("fx_rv_type", "SPRING"); juce::MemoryBlock mb; a.p->getStateInformation(mb); Synth b; b.p->setStateInformation(mb.getData(), (int)mb.getSize());
        check(std::abs(b.p->apvts.getRawParameterValue("fx_dl_fb")->load() - 77.0f) < 0.01f && b.p->apvts.getRawParameterValue("fx_rv_type")->load() > 0.5f, "effect settings come back with the set");
    }

    printf("\n%s (%d failed)\n", failures == 0 ? "PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
