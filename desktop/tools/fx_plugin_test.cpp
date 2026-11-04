// The effects as you meet them: through the whole plugin, playing real presets, turning the real parameters.
#include "PluginProcessor.h"
#include <complex>
#include <cstdio>
#include <memory>
#include <vector>
using Vec = std::vector<float>;
struct Head : juce::AudioPlayHead {
    double bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setBpm(bpm); i.setPpqPosition(0.0); i.setIsPlaying(false); return i; }
};
struct Synth {
    std::unique_ptr<MillstoneProcessor> p = std::make_unique<MillstoneProcessor>();
    double sr = 44100; int block = 256; juce::MidiBuffer m; Head head;
    Synth() { p->setRateAndBufferSizeDetails(sr, block); p->prepareToPlay(sr, block); p->setPlayHead(&head); }
    void preset(const juce::String& name) { const auto& l = MillstoneProcessor::factoryPresets(); for (int i = 0; i < (int)l.size(); ++i) if (l[(size_t)i].name == name) p->setCurrentProgram(i); set("fx_dl_mix", 0); set("fx_rv_mix", 0); set("fx_ph_mix", 0); opt("fx_ch_mode", "OFF"); }   // these test one effect at a time
    void set(const char* k, float v) { auto* q = p->apvts.getParameter(k); jassert(q); q->setValueNotifyingHost(q->convertTo0to1(v)); }
    void opt(const char* k, const char* name) { auto* q = p->apvts.getParameter(k); const int i = q->getAllValueStrings().indexOf(name); jassert(i >= 0); q->setValueNotifyingHost(q->convertTo0to1((float)i)); }
    void on(int n, int v = 100) { m.addEvent(juce::MidiMessage::noteOn(1, n, (juce::uint8)v), 0); }
    void off(int n) { m.addEvent(juce::MidiMessage::noteOff(1, n), 0); }
    void run(double seconds, Vec& l, Vec& r) {
        const int total = (int)(seconds * sr); juce::AudioBuffer<float> b(2, block); l.clear(); r.clear();
        for (int done = 0; done < total; done += block) { b.clear(); p->processBlock(b, m); m.clear(); l.insert(l.end(), b.getReadPointer(0), b.getReadPointer(0) + block); r.insert(r.end(), b.getReadPointer(1), b.getReadPointer(1) + block); }
    }
};
static double db(double x) { return 20.0 * std::log10(std::max(x, 1e-7)); }
static double rms(const Vec& v, size_t a = 0, size_t b = (size_t)-1) { double s = 0; b = std::min(b, v.size()); for (size_t i = a; i < b; ++i) s += v[i] * v[i]; return std::sqrt(s / std::max<size_t>(1, b - a)); }
static double top(const Vec& v) { double s = 0; for (float x : v) s = std::max(s, (double)std::abs(x)); return s; }

static int failures = 0, checks = 0;
static void check(bool ok, const juce::String& what, const juce::String& info = {}) {
    ++checks; printf("%s %-62s %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8(), info.toRawUTF8()); if (!ok) ++failures;
}
static juce::String num(double x) { return juce::String(x, 2); }

struct Bands { double low, mid, high; };
// Power below 200 Hz, 200 Hz to 2 kHz, and above, from Hann-windowed FFTs, so that strong bass cannot leak into the top band.
static Bands spectrumBands(const Vec& v, size_t from, double sr) {
    constexpr int size = 8192; std::vector<std::complex<double>> x((size_t)size);
    double low = 0, mid = 0, high = 0; int frames = 0;
    for (size_t at = from; at + size <= v.size(); at += size / 2, ++frames) {
        for (int i = 0; i < size; ++i) x[(size_t)i] = v[at + (size_t)i] * (0.5 - 0.5 * std::cos(6.283185307 * i / size));
        for (int i = 1, j = 0; i < size; ++i) { int bit = size >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(x[(size_t)i], x[(size_t)j]); }
        for (int len = 2; len <= size; len <<= 1) { const std::complex<double> w = std::polar(1.0, -6.283185307 / len); for (int i = 0; i < size; i += len) { std::complex<double> u(1.0); for (int k = 0; k < len / 2; ++k, u *= w) { const auto a = x[(size_t)(i + k)], b = x[(size_t)(i + k + len / 2)] * u; x[(size_t)(i + k)] = a + b; x[(size_t)(i + k + len / 2)] = a - b; } } }
        for (int bin = 1; bin < size / 2; ++bin) { const double hz = bin * sr / size, p = std::norm(x[(size_t)bin]); if (hz < 200) low += p; else if (hz < 2000) mid += p; else high += p; }
    }
    return { 10 * std::log10(low + 1e-12), 10 * std::log10(mid + 1e-12), 10 * std::log10(high + 1e-12) };
}

// A short note with the delay fully wet, so that all that comes out is echoes, and silence is silence.
struct Echoes {
    Vec l, r; double sr = 44100;
    double firstAbove(double level, double from = 0.0) const { for (size_t i = (size_t)(from * sr); i < l.size(); ++i) if (std::abs(l[i]) > level || std::abs(r[i]) > level) return (double)i / sr; return -1; }
    double energy(double a, double b) const { Vec w(l.begin() + (long)(a * sr), l.begin() + (long)(std::min(b * sr, (double)l.size()))); Vec x(r.begin() + (long)(a * sr), r.begin() + (long)(std::min(b * sr, (double)r.size()))); return std::max(top(w), top(x)); }
};
static Echoes echoes(std::function<void(Synth&)> setup, double seconds = 2.0, std::function<void(Synth&)> whileHeld = {}) {
    Synth s; s.set("attack", 0); s.set("decay", 5); s.set("sustain", 100); s.set("release", 0); s.set("vel_amp", 0); s.set("cutoff", 100); s.set("reso", 0); s.opt("o2_on", "OFF"); s.opt("o3_on", "OFF"); s.set("volume", 50);
    s.opt("fx_dl_sync", "FREE"); s.set("fx_dl_time", 300); s.set("fx_dl_mix", 100); s.set("fx_dl_fb", 0); s.set("fx_dl_wow", 0); s.set("fx_dl_drive", 0); s.set("fx_dl_low", 0); s.set("fx_dl_high", 100);
    setup(s);
    Vec a, b; s.run(0.1, a, b);
    if (whileHeld) whileHeld(s);
    s.run(0.03, a, b);   // settled: the switches have faded in
    s.on(60); s.run(0.02, a, b);
    Echoes e; e.l = a; e.r = b;
    s.off(60); s.run(seconds, a, b);
    e.l.insert(e.l.end(), a.begin(), a.end()); e.r.insert(e.r.end(), b.begin(), b.end());
    return e;
}

int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    const std::vector<juce::String> names = []{ std::vector<juce::String> n; const auto& l = MillstoneProcessor::factoryPresets(); for (size_t i = 0; i < l.size(); i += 18) n.push_back(l[i].name); return n; }();

    puts("--- reverb level, chord held 3 s and released 2 s, against the same preset dry");
    double worstPeak = 0, worstRise = -100, quietest = 100, worstCrowded = 0, sustainedRise = -100;
    for (const auto& name : names) {
        const auto render = [&](int type, float mix, float decay, Vec& l) { Synth s; s.preset(name); s.opt("fx_rv_type", type ? "SPRING" : "NORMAL"); s.set("fx_rv_mix", mix); s.set("fx_rv_decay", decay); Vec a, b; s.run(0.05, a, b); for (int n : {48, 55, 60, 64}) s.on(n); s.run(3, a, b); l = a; for (int n : {48, 55, 60, 64}) s.off(n); s.run(2, a, b); l.insert(l.end(), a.begin(), a.end()); };
        Vec dry; render(0, 0, 50, dry);
        for (int type : {0, 1}) for (float mix : {30.f, 50.f, 100.f}) for (float decay : {0.f, 50.f, 100.f}) {
            Vec w; render(type, mix, decay, w);
            const double rise = db(rms(w)) - db(rms(dry));
            size_t over = 0; for (float x : w) over += std::abs(x) > 0.85f; worstCrowded = std::max(worstCrowded, 100.0 * (double)over / (double)w.size());
            if (name == "Init" && mix <= 50.f) sustainedRise = std::max(sustainedRise, rise);
            worstPeak = std::max(worstPeak, top(w)); worstRise = std::max(worstRise, rise); quietest = std::min(quietest, rise);
            if (rise > 9.0) printf("  loud: %s %s mix %.0f decay %.0f: %+.1f dB\n", name.toRawUTF8(), type ? "spring" : "normal", mix, decay, rise);
        }
    }
    check(worstPeak <= 0.945, "no reverb setting on any preset reaches the ceiling", num(db(worstPeak)) + " dB");
    check(worstRise <= 9.0, "and none is far louder than the dry preset", "worst " + num(worstRise) + " dB, quietest " + num(quietest) + " dB");
    check(worstCrowded < 1.0, "the ceiling is rarely touched", num(worstCrowded) + "% of samples");
    check(sustainedRise <= 4.0, "a held Init chord at MIX 50 stays near its dry level", num(sustainedRise) + " dB");
    check(quietest > -12.0, "but it is still there", num(quietest) + " dB");

    puts("--- delay, each control through the plugin");
    { // MIX
        const auto render = [&](float mix, Vec& out) { Synth s; s.set("fx_dl_mix", mix); s.opt("fx_dl_sync", "FREE"); s.set("fx_dl_time", 800); Vec a, b; s.run(0.05, a, b); s.on(60); s.run(0.4, a, b); out = a; };
        Vec off, half, full; render(0, off); render(50, half); render(100, full);
        check(db(rms(half)) - db(rms(off)) < 0.2, "MIX 50 keeps the dry whole", num(db(rms(half)) - db(rms(off))));
        check(top(full) < 0.01, "MIX 100 has no dry at all", num(top(full)));
    }
    { // TIME and every division
        for (float ms : {100.f, 500.f, 1500.f}) { const auto e = echoes([ms](Synth& s) { s.set("fx_dl_time", ms); }); const double at = e.firstAbove(0.01); check(std::abs(at - ms / 1000.0) < 0.012, "FREE " + juce::String((int)ms) + " ms", num(at) + " s"); }
        const std::vector<std::pair<const char*, double>> divisions { {"1/32", .125}, {"1/16 T", 1. / 6}, {"1/16", .25}, {"1/8 T", 1. / 3}, {"1/16 D", .375}, {"1/8", .5}, {"1/4 T", 2. / 3}, {"1/8 D", .75}, {"1/4", 1}, {"1/2 T", 4. / 3}, {"1/4 D", 1.5}, {"1/2", 2}, {"1/2 D", 3}, {"1 BAR", 4} };
        int wrong = 0; juce::String list;
        for (double bpm : {80.0, 120.0, 174.0}) for (const auto& d : divisions) {
            const double want = d.second * 60.0 / bpm;
            const auto e = echoes([&](Synth& s) { s.head.bpm = bpm; s.opt("fx_dl_sync", "SYNC"); s.opt("fx_dl_div", d.first); }, want + 0.3);
            const double at = e.firstAbove(0.01);
            if (std::abs(at - want) > 0.012) { ++wrong; list << d.first << "@" << bpm << "=" << num(at) << " "; }
        }
        check(wrong == 0, "all 14 divisions land on the beat, at three tempos", list);
    }
    { // REPEATS
        const auto ratio = [&](float fb) { const auto e = echoes([fb](Synth& s) { s.set("fx_dl_fb", fb); }); const auto power = [&](double a, double b) { return rms(e.l, (size_t)(a * 44100), (size_t)(b * 44100)); }; return power(.59, .75) / power(.29, .45); };
        check(ratio(0) < 0.01, "REPEATS 0: one echo only", num(ratio(0)));
        check(std::abs(ratio(50) - 0.5) < 0.12, "REPEATS 50: the next is half", num(ratio(50)));
        check(std::abs(ratio(100) - 1.0) < 0.2, "REPEATS 100: the next is as loud", num(ratio(100)));
        const auto e = echoes([](Synth& s) { s.set("fx_dl_fb", 120); s.set("fx_dl_time", 100); s.set("fx_dl_high", 70); s.set("fx_dl_low", 20); }, 15.0);
        check(e.energy(12, 15) > 0.02, "REPEATS 120 is still going 12 s after the note", num(e.energy(12, 15)));
    }
    { // INPUT and THROW
        const auto shut = echoes([](Synth& s) { s.opt("fx_dl_in", "CLOSED"); });
        check(shut.energy(.3, 1) < 0.0005, "INPUT CLOSED: a note stays out of the echo", num(shut.energy(.3, 1)));
        const auto thrown = echoes([](Synth& s) { s.opt("fx_dl_in", "CLOSED"); }, 2.0, [](Synth& s) { s.opt("fx_dl_throw", "ON"); });
        check(thrown.energy(.3, 1) > 0.02, "and holding THROW lets it in", num(thrown.energy(.3, 1)));
        const auto open = echoes([](Synth& s) { s.opt("fx_dl_in", "OPEN"); });
        check(open.energy(.3, 1) > 0.02, "INPUT OPEN: every note goes in", num(open.energy(.3, 1)));
    }
    { // HEADS
        const std::vector<std::tuple<const char*, bool, bool, bool>> heads { {"1", 1, 0, 0}, {"2", 0, 1, 0}, {"3", 0, 0, 1}, {"1+2", 1, 1, 0}, {"1+3", 1, 0, 1}, {"2+3", 0, 1, 1}, {"1+2+3", 1, 1, 1} };
        for (const auto& [name, a, b, c] : heads) {
            const auto e = echoes([n = name](Synth& s) { s.opt("fx_dl_heads", n); });
            const double one = e.energy(.29, .45), two = e.energy(.59, .75), three = e.energy(.89, 1.05);
            const auto right = [](double x, bool on) { return on ? x > 0.02 : x < 0.002; };
            check(right(one, a) && right(two, b) && right(three, c), juce::String("HEADS ") + name, num(one) + " " + num(two) + " " + num(three));
        }
    }
    { // PING PONG and WIDTH
        const auto e = echoes([](Synth& s) { s.opt("fx_dl_pp", "ON"); s.set("fx_dl_fb", 70); });
        const auto side = [&](const Vec& v, double a, double b) { Vec w(v.begin() + (long)(a * 44100), v.begin() + (long)(b * 44100)); return top(w); };
        check(side(e.l, .29, .45) > 0.02 && side(e.r, .29, .45) < 0.001, "PING PONG: first left", num(side(e.r, .29, .45)));
        check(side(e.r, .59, .75) > 0.01 && side(e.l, .59, .75) < 0.001, "then right", num(side(e.l, .59, .75)));
        const auto balance = [&](float width) { const auto w = echoes([width](Synth& s) { s.opt("fx_dl_pp", "ON"); s.set("fx_dl_width", width); }); return side(w.r, .29, .45) / side(w.l, .29, .45); };
        check(balance(100) < 0.01, "WIDTH 100 is hard left", num(balance(100)));
        check(std::abs(balance(50) - 1.0 / 3.0) < 0.05, "WIDTH 50 is between", num(balance(50)));
        check(std::abs(balance(0) - 1.0) < 0.01, "WIDTH 0 is centred", num(balance(0)));
    }
    { // HIGH CUT and LOW CUT must be plainly audible on a first echo, on bass-heavy and bright sounds alike
        for (const juce::String preset : { names[1], names[3] }) {
            const auto bands = [&](float highCut, float lowCut) {
                Synth s; s.preset(preset); s.opt("fx_dl_sync", "FREE"); s.set("fx_dl_time", 300); s.set("fx_dl_mix", 100); s.set("fx_dl_fb", 0); s.set("fx_dl_wow", 0); s.set("fx_dl_high", highCut); s.set("fx_dl_low", lowCut);
                Vec l, r; s.run(0.05, l, r); s.on(36); s.on(60); s.on(72); s.run(1.5, l, r);
                return spectrumBands(l, (size_t)(0.5 * s.sr), s.sr);
            };
            const auto open = bands(100, 0), dark = bands(0, 0), thin = bands(100, 100);
            check(open.high - dark.high > 30 && open.mid - dark.mid > 12, preset + ": HIGH CUT takes out highs and mids", "high -" + num(open.high - dark.high) + " mid -" + num(open.mid - dark.mid) + " low -" + num(open.low - dark.low));
            check(open.low - thin.low > 30 && open.mid - thin.mid > 8, preset + ": LOW CUT takes out lows and mids", "low -" + num(open.low - thin.low) + " mid -" + num(open.mid - thin.mid) + " high -" + num(open.high - thin.high));
            const auto halfway = bands(50, 0), quarter = bands(100, 25);
            check(open.high - halfway.high > 8, preset + ": HIGH CUT is already plain at 50", num(open.high - halfway.high));
            check(open.low - quarter.low > 1, preset + ": LOW CUT at 25 has started", num(open.low - quarter.low));
        }
    }
    { // DRIVE, WOW, DUCK through the plugin: they change the sound
        const auto differs = [&](const char* key, float value) {
            const auto render = [&](float v) { Vec a, b; Synth s; s.preset(names[1]); s.opt("fx_dl_sync", "FREE"); s.set("fx_dl_time", 200); s.set("fx_dl_mix", 100); s.set("fx_dl_fb", 40); s.set("fx_dl_drive", 0); s.set("fx_dl_wow", 0); s.set("fx_dl_duck", 0); s.set(key, v); s.run(0.05, a, b); s.on(48); s.run(2, a, b); return a; };
            const auto x = render(0), y = render(value); double d = 0; for (size_t i = 0; i < x.size(); ++i) d += (x[i] - y[i]) * (x[i] - y[i]);
            return db(std::sqrt(d / (double)x.size())) - db(rms(x));
        };
        for (const char* key : {"fx_dl_drive", "fx_dl_wow", "fx_dl_duck"}) { const double d = differs(key, 100); check(d > -20, juce::String(key) + " changes the sound", num(d) + " dB of difference"); }
    }
    printf("\n%s: %d checks, %d failed\n", failures ? "FAILED" : "fx plugin ok", checks, failures);
    return failures ? 1 : 0;
}
