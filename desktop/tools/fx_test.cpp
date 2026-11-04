// The effects measured directly, with no synth in front of them: is each one what it claims to be?
#include "FxChain.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

static int failures = 0, checks = 0;
static void check(bool ok, const char* what, double value = 0.0) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s (%g)\n", what, value); }
}

using Buffer = std::vector<float>;
static double peak(const Buffer& b, size_t from = 0, size_t to = (size_t)-1) {
    double p = 0; for (size_t i = from; i < b.size() && i < to; ++i) p = std::max(p, (double)std::abs(b[i])); return p;
}
static double rms(const Buffer& b, size_t from, size_t to) {
    double s = 0; to = std::min(to, b.size()); for (size_t i = from; i < to; ++i) s += b[i] * b[i]; return std::sqrt(s / std::max<size_t>(1, to - from));
}
static size_t firstAbove(const Buffer& b, double level, size_t from = 0) {
    for (size_t i = from; i < b.size(); ++i) if (std::abs(b[i]) > level) return i; return b.size();
}

struct Rendered { Buffer l, r; };
template <typename Input>
static Rendered run(FxChain& fx, int seconds, double sr, Input input, int block = 256) {
    Rendered out; const size_t n = (size_t)(seconds * sr);
    out.l.resize(n); out.r.resize(n);
    for (size_t i = 0; i < n; ++i) out.l[i] = out.r[i] = input(i);
    for (size_t i = 0; i < n; i += (size_t)block) { const int m = (int)std::min<size_t>((size_t)block, n - i); fx.process(out.l.data() + i, out.r.data() + i, m); }
    return out;
}
static std::unique_ptr<FxChain> chain(const FxChain::Settings& s, double sr) {
    auto fx = std::make_unique<FxChain>(); fx->setSettings(s); fx->prepare(sr); fx->setSettings(s); return fx;
}
static float burst(size_t i, double sr, double hz = 800.0, double seconds = 0.02) {
    return (double)i / sr < seconds ? 0.5f * (float)std::sin(6.283185307 * hz * (double)i / sr) : 0.0f;
}
static FxChain::Settings cleanDelay(float mix = 0.5f) {
    FxChain::Settings s; s.delay.mix = mix; s.delay.wow = 0; s.delay.drive = 0; s.delay.lowCutHz = 20; s.delay.highCutHz = 20000; s.delay.timeSeconds = 0.25f; s.delay.repeats = 0.5f; return s;
}

int main() {
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        // Off is off: not one bit changed.
        {
            auto fx = chain(FxChain::Settings {}, sr);
            auto out = run(*fx, 1, sr, [&](size_t i) { return 0.4f * (float)std::sin(0.03 * (double)i); });
            bool same = true; for (size_t i = 0; i < out.l.size(); ++i) same = same && out.l[i] == 0.4f * (float)std::sin(0.03 * (double)i);
            check(same, "with every effect off the signal is untouched");
        }
        // The echo lands where TIME says, at the level it should, and repeats fall by REPEATS.
        {
            auto s = cleanDelay(); auto fx = chain(s, sr);
            auto out = run(*fx, 2, sr, [&](size_t i) { return burst(i, sr); });
            const size_t gap = (size_t)(sr * 0.1), at = firstAbove(out.l, 0.02, (size_t)(sr * 0.05));
            check(std::abs((double)at / sr - 0.25) < 0.003, "first echo arrives at TIME", (double)at / sr);
            const double one = peak(out.l, at - gap / 4, at + gap), two = peak(out.l, at + (size_t)(sr * .25) - gap / 4, at + (size_t)(sr * .25) + gap);
            check(one > 0.25 && one < 0.5, "first echo level", one);
            check(std::abs(two / one - 0.5) < 0.1, "second echo falls by REPEATS", two / one);
        }
        // Three heads: on 1+3 the echoes come at T and 3T, with nothing at 2T except what the loop feeds.
        {
            auto s = cleanDelay(); s.delay.heads = 5; s.delay.repeats = 0; auto fx = chain(s, sr);
            auto out = run(*fx, 2, sr, [&](size_t i) { return burst(i, sr); });
            const auto win = [&](double t) { return peak(out.l, (size_t)((t - .01) * sr), (size_t)((t + .05) * sr)); };
            check(win(.25) > .1 && win(.75) > .1, "heads 1+3 play at T and 3T", win(.75));
            check(win(.5) < .001, "heads 1+3 stay out of 2T", win(.5));
        }
        // Ping-pong goes left, right, left, from a centred input.
        {
            auto s = cleanDelay(); s.delay.pingPong = true; auto fx = chain(s, sr);
            auto out = run(*fx, 2, sr, [&](size_t i) { return burst(i, sr); });
            const auto L = [&](double t) { return peak(out.l, (size_t)((t - .01) * sr), (size_t)((t + .05) * sr)); };
            const auto R = [&](double t) { return peak(out.r, (size_t)((t - .01) * sr), (size_t)((t + .05) * sr)); };
            check(L(.25) > .1 && R(.25) < .001, "first bounce is left only", R(.25));
            check(R(.5) > .05 && L(.5) < .001, "second bounce is right only", L(.5));
            check(L(.75) > .02 && R(.75) < .001, "third bounce is left again", R(.75));
        }
        // SEND at zero records nothing; THROW opens it.
        {
            auto s = cleanDelay(); s.delay.send = 0; auto fx = chain(s, sr);
            auto out = run(*fx, 1, sr, [&](size_t i) { return burst(i, sr); });
            check(peak(out.l, (size_t)(sr * .1)) < 0.002, "SEND 0 keeps the note off the tape", peak(out.l, (size_t)(sr * .1)));
            s.delay.throwOn = true; auto thrown = chain(s, sr);
            out = run(*thrown, 1, sr, [&](size_t i) { return burst(i, sr); });
            check(peak(out.l, (size_t)(sr * .1)) > 0.1, "THROW sends it anyway", peak(out.l, (size_t)(sr * .1)));
        }
        // The loop filters: repeats get darker, and bass does not build.
        {
            auto s = cleanDelay(); s.delay.highCutHz = 1500; s.delay.repeats = 0.8f; auto fx = chain(s, sr);
            auto bright = run(*fx, 2, sr, [&](size_t i) { return burst(i, sr, 5000.0); });
            auto mid = cleanDelay(); mid.delay.repeats = 0.8f; auto open = chain(mid, sr);
            auto unfiltered = run(*open, 2, sr, [&](size_t i) { return burst(i, sr, 5000.0); });
            check(peak(bright.l, (size_t)(sr * .45)) < 0.1 * peak(unfiltered.l, (size_t)(sr * .45)), "HIGH CUT darkens each repeat");
            s = cleanDelay(); s.delay.lowCutHz = 500; s.delay.repeats = 0.8f; auto thin = chain(s, sr);
            auto low = run(*thin, 2, sr, [&](size_t i) { return burst(i, sr, 80.0, .1); });
            auto full = chain(mid, sr);
            auto lowOpen = run(*full, 2, sr, [&](size_t i) { return burst(i, sr, 80.0, .1); });
            check(peak(low.l, (size_t)(sr * .45)) < 0.2 * peak(lowOpen.l, (size_t)(sr * .45)), "LOW CUT thins each repeat");
        }
        // Past 100% it runs away by itself, and stays bounded and alive. At every drive.
        for (float drive : { 0.0f, 0.5f, 1.0f }) {
            auto s = cleanDelay(); s.delay.repeats = 1.2f; s.delay.drive = drive; s.delay.wow = 0.5f; s.delay.timeSeconds = 0.1f; s.delay.highCutHz = 6000; s.delay.lowCutHz = 60;
            auto fx = chain(s, sr);
            auto out = run(*fx, 12, sr, [&](size_t i) { return burst(i, sr); });
            const double late = rms(out.l, (size_t)(sr * 10), (size_t)(sr * 12));
            check(late > 0.03, "repeats past 100% keep going alone", late);
            check(peak(out.l) <= 0.95 && peak(out.r) <= 0.95, "and the output stays under the ceiling", peak(out.l));
        }
        // WIDTH narrows the bounces, all the way to the centre.
        for (float width : { 0.0f, 0.5f, 1.0f }) {
            auto s = cleanDelay(); s.delay.pingPong = true; s.delay.width = width; auto fx = chain(s, sr);
            auto out = run(*fx, 1, sr, [&](size_t i) { return burst(i, sr); });
            const size_t a = (size_t)(sr * .24), b = (size_t)(sr * .3);
            const double l = peak(out.l, a, b), r = peak(out.r, a, b), expected = (1.0 - width) / (1.0 + width);
            check(std::abs(r / l - expected) < 0.03, "WIDTH sets the balance of a bounce", r / l);
        }
        // DRIVE colours rather than gets louder: harmonics rise, level stays near.
        {
            const auto measure = [&](float drive, double& level, double& third) {
                auto s = cleanDelay(); s.delay.repeats = 0; s.delay.mix = 1; s.delay.drive = drive; auto fx = chain(s, sr);
                auto out = run(*fx, 2, sr, [&](size_t i) { return 0.3f * (float)std::sin(6.283185307 * 220.0 * (double)i / sr); });
                double re = 0, im = 0, sum = 0; const size_t from = (size_t)sr, to = (size_t)(2 * sr);
                for (size_t i = from; i < to; ++i) { const double t = 6.283185307 * 660.0 * (double)i / sr; re += out.l[i] * std::cos(t); im += out.l[i] * std::sin(t); sum += out.l[i] * out.l[i]; }
                level = std::sqrt(sum / (double)(to - from)); third = 2.0 * std::sqrt(re * re + im * im) / (double)(to - from) / (level * 1.414);
            };
            double l0, t0, l1, t1; measure(0, l0, t0); measure(1, l1, t1);
            check(t1 > 8.0 * t0 && t1 > 0.02, "DRIVE adds harmonics", t1 / t0);
            check(std::abs(20.0 * std::log10(l1 / l0)) < 3.0, "and does not turn into a volume knob", 20.0 * std::log10(l1 / l0));
        }
        // Below 100% it does die away.
        {
            auto s = cleanDelay(); s.delay.repeats = 0.85f; s.delay.timeSeconds = 0.1f; auto fx = chain(s, sr);
            auto out = run(*fx, 8, sr, [&](size_t i) { return burst(i, sr); });
            check(rms(out.l, (size_t)(sr * 7), (size_t)(sr * 8)) < 0.001, "repeats under 100% die away");
        }
        // Moving TIME bends the pitch of what is on the tape. It glides, with no jump and no reversal.
        {
            auto s = cleanDelay(); s.delay.repeats = 0; s.delay.mix = 1.0f; auto fx = chain(s, sr);
            const size_t n = (size_t)(sr * 2); Buffer l(n), r(n);
            for (size_t i = 0; i < n; ++i) l[i] = r[i] = 0.5f * (float)std::sin(6.283185307 * 440.0 * (double)i / sr);
            double biggest = 0;
            for (size_t i = 0; i < n; i += 256) {
                if (i >= (size_t)sr) { s.delay.timeSeconds = 0.5f; fx->setSettings(s); }
                fx->process(l.data() + i, r.data() + i, (int)std::min<size_t>(256, n - i));
            }
            for (size_t i = (size_t)(sr * 1.0); i < n; ++i) biggest = std::max(biggest, (double)std::abs(l[i] - l[i - 1]));
            check(biggest < 0.2, "a change of TIME glides, without a click", biggest);
            size_t crossings = 0; for (size_t i = (size_t)(sr * 1.1) + 1; i < (size_t)(sr * 1.5); ++i) crossings += (l[i - 1] < 0) != (l[i] < 0);
            const double hz = (double)crossings / 2.0 / 0.4;
            check(hz < 400.0 && hz > 40.0, "and pitches the tape down while it does", hz);
        }
        // Wow moves the pitch; with none it is steady.
        {
            auto s = cleanDelay(); s.delay.repeats = 0; s.delay.mix = 1.0f; s.delay.wow = 1.0f; auto fx = chain(s, sr);
            auto wobbled = run(*fx, 4, sr, [&](size_t i) { return 0.5f * (float)std::sin(6.283185307 * 1000.0 * (double)i / sr); });
            auto s0 = cleanDelay(); s0.delay.repeats = 0; s0.delay.mix = 1.0f; auto fx0 = chain(s0, sr);
            auto steady = run(*fx0, 4, sr, [&](size_t i) { return 0.5f * (float)std::sin(6.283185307 * 1000.0 * (double)i / sr); });
            const auto spread = [&](const Buffer& b) { // distance between zero crossings in ms
                std::vector<double> crossing; for (size_t i = (size_t)sr; i < b.size(); ++i) if (b[i - 1] < 0 && b[i] >= 0) crossing.push_back((double)i - b[i] / (b[i] - b[i - 1]));
                double lo = 1e9, hi = 0; for (size_t i = 1; i < crossing.size(); ++i) { lo = std::min(lo, crossing[i] - crossing[i - 1]); hi = std::max(hi, crossing[i] - crossing[i - 1]); }
                return (hi - lo) / lo; };
            check(spread(wobbled.l) > 0.008 && spread(wobbled.l) < 0.15, "WOW moves the pitch by a tape-like amount", spread(wobbled.l));
            check(spread(steady.l) < 0.003, "with WOW at 0 the pitch is steady", spread(steady.l));
        }
        // DUCK: echoes are held down while the input plays.
        {
            auto s = cleanDelay(); s.delay.duck = 1.0f; s.delay.repeats = 0.8f; s.delay.mix = 1.0f; auto ducked = chain(s, sr);
            const auto play = [&](size_t i) { return (double)i / sr < 1.0 ? 0.5f * (float)std::sin(6.283185307 * 300.0 * (double)i / sr) : 0.0f; };
            auto out = run(*ducked, 3, sr, play);
            check(rms(out.l, (size_t)(sr * .6), (size_t)(sr * 1.0)) < 0.5 * rms(out.l, (size_t)(sr * 1.6), (size_t)(sr * 2.0)), "DUCK holds echoes down while playing, and lets them up after");
        }
        // Mix down and up: no old echo comes back.
        {
            auto s = cleanDelay(); s.delay.repeats = 0.95f; auto fx = chain(s, sr);
            auto first = run(*fx, 1, sr, [&](size_t i) { return burst(i, sr); });
            s.delay.mix = 0; fx->setSettings(s);
            auto gone = run(*fx, 1, sr, [](size_t) { return 0.0f; });
            s.delay.mix = 0.5f; fx->setSettings(s);
            auto back = run(*fx, 1, sr, [](size_t) { return 0.0f; });
            check(peak(back.l) < 0.0001, "an echo turned off does not come back when it is turned on", peak(back.l));
        }
        // Normal reverb: a tail, stereo, decaying, longer with DECAY.
        for (auto type : { FxChain::ReverbType::plate, FxChain::ReverbType::spring }) {
            const char* name = type == FxChain::ReverbType::plate ? "normal" : "spring";
            char what[120];
            const auto tailFor = [&](float decay) {
                FxChain::Settings s; s.reverbType = type; s.reverbMix = 0.5f; s.reverbDecay = decay; s.reverbDiffusion = 0.6f; s.reverbDamping = 0.2f;
                auto fx = chain(s, sr);
                auto out = run(*fx, 20, sr, [&](size_t i) { return (double)i / sr < .2 ? 0.5f * (float)std::sin(6.283185307 * 700.0 * (double)i / sr) : 0.0f; });
                const size_t blockSize = (size_t)(sr * 0.05);
                const double start = rms(out.l, (size_t)(sr * .22), (size_t)(sr * .32));
                size_t at = (size_t)(sr * .32); while (at + blockSize < out.l.size() && rms(out.l, at, at + blockSize) > start * 0.001) at += blockSize;
                bool wide = false; for (size_t i = (size_t)(sr * .3); i < (size_t)(sr * .8); ++i) wide = wide || std::abs(out.l[i] - out.r[i]) > 1e-3;
                std::snprintf(what, sizeof what, "%s reverb is stereo", name); check(wide, what);
                std::snprintf(what, sizeof what, "%s reverb is in bounds", name); check(peak(out.l) <= 1.0, what, peak(out.l));
                std::snprintf(what, sizeof what, "%s reverb has a level", name); check(start > 0.03, what, start);
                return (double)at / sr;
            };
            const double shortTail = tailFor(0.1f), longTail = tailFor(0.9f);
            std::printf("  %s at %g: tail %.2f s at decay 10, %.2f s at decay 90\n", name, sr, shortTail, longTail);
            std::snprintf(what, sizeof what, "%s reverb: DECAY lengthens the tail", name); check(longTail > 1.6 * shortTail, what, longTail);
            const double reported = type == FxChain::ReverbType::plate ? reverbTailSeconds(0.9f) : springTailSeconds(0.9f);
            std::snprintf(what, sizeof what, "%s reverb: the tail reported to the host covers it", name); check(reported + 0.7 >= longTail, what, reported);
        }
        // Spring: the defining chirp. An impulse's highs arrive after its lows.
        {
            FxChain::Settings s; s.reverbType = FxChain::ReverbType::spring; s.reverbMix = 1.0f; s.reverbDecay = 0.3f; s.reverbDiffusion = 0.8f; s.reverbDamping = 0.0f;
            auto fx = chain(s, sr);
            auto out = run(*fx, 1, sr, [](size_t i) { return i == 0 ? 0.9f : 0.0f; });
            const auto arrival = [&](double hz) {   // when a narrow band of the first 100 ms is at its strongest
                const int window = (int)(sr * 0.008); double best = 0, when = 0;
                for (int t = 0; t + window < (int)(sr * 0.1); t += window / 4) {
                    double re = 0, im = 0;
                    for (int i = 0; i < window; ++i) { double w = std::sin(3.14159265 * i / window); w *= w; re += out.l[(size_t)(t + i)] * w * std::cos(6.28318531 * hz * i / sr); im += out.l[(size_t)(t + i)] * w * std::sin(6.28318531 * hz * i / sr); }
                    const double e = re * re + im * im; if (e > best) { best = e; when = t / sr; }
                }
                return when; };
            const double lows = arrival(500.0), highs = arrival(3400.0);
            std::printf("  spring at %g: 500 Hz arrives at %.1f ms, 3.4 kHz at %.1f ms\n", sr, lows * 1000, highs * 1000);
            check(highs > lows + 0.006, "a spring chirps: highs arrive after lows", highs - lows);
        }
        // KICK sounds with nothing playing, rings, and stops.
        {
            FxChain::Settings s; s.reverbType = FxChain::ReverbType::spring; s.reverbMix = 0.5f; s.reverbDecay = 0.5f; auto fx = chain(s, sr);
            auto quiet = run(*fx, 1, sr, [](size_t) { return 0.0f; });
            check(peak(quiet.l) == 0.0, "the spring is silent with no input", peak(quiet.l));
            s.springKick = true; fx->setSettings(s);
            auto kicked = run(*fx, 8, sr, [](size_t) { return 0.0f; });
            check(peak(kicked.l) > 0.1 && peak(kicked.l) <= 1.0, "KICK bangs the tank", peak(kicked.l));
            check(rms(kicked.l, (size_t)(sr * 7), (size_t)(sr * 8)) < 0.002, "and the bang dies away", rms(kicked.l, (size_t)(sr * 7), (size_t)(sr * 8)));
        }
        // Switching reverb type with a note ringing: no jump.
        {
            FxChain::Settings s; s.reverbMix = 0.5f; auto fx = chain(s, sr);
            const size_t n = (size_t)(sr * 2); Buffer l(n, 0.0f), r(n, 0.0f); double biggest = 0;
            for (size_t i = 0; i < n; ++i) l[i] = r[i] = 0.25f * (float)std::sin(6.283185307 * 200.0 * (double)i / sr);
            for (size_t i = 0; i < n; i += 128) {
                if (i >= n / 2) { s.reverbType = FxChain::ReverbType::spring; fx->setSettings(s); }
                fx->process(l.data() + i, r.data() + i, (int)std::min<size_t>(128, n - i));
            }
            for (size_t i = n / 2 - 100; i < n / 2 + (size_t)(sr * .05); ++i) biggest = std::max(biggest, (double)std::abs(l[i] - l[i - 1]));
            check(biggest < 0.2, "switching reverb type fades rather than clicks", biggest);
        }
        // Everything at once, flat out, with a loud signal.
        {
            FxChain::Settings s; s.phaserMix = 1; s.phaserFeedback = .9f; s.phaserRateHz = 2; s.chorusMode = ChorusMode::oneAndTwo; s.delay.mix = 1; s.delay.repeats = 1.2f; s.delay.drive = 1; s.delay.heads = 7; s.delay.timeSeconds = .05f; s.delay.wow = 1;
            s.delay.highCutHz = 20000; s.delay.lowCutHz = 20; s.reverbType = FxChain::ReverbType::spring; s.reverbMix = 1; s.reverbDecay = 1; s.reverbDiffusion = 1; s.reverbDamping = 0;
            auto fx = chain(s, sr);
            auto out = run(*fx, 6, sr, [&](size_t i) { return (float)std::sin(6.283185307 * 110.0 * (double)i / sr) > 0 ? 1.0f : -1.0f; });
            bool finite = true; for (float x : out.l) finite = finite && std::isfinite(x);
            check(finite && peak(out.l) <= 0.95 && peak(out.r) <= 0.95, "everything at maximum stays finite and in bounds", peak(out.l));
        }
    }
    std::printf("%s: %d checks, %d failed\n", failures ? "FAILED" : "fx ok", checks, failures);
    return failures ? 1 : 0;
}
