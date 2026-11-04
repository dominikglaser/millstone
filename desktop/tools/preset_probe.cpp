// Developer tool: what a setting actually produces. Plays a note through the whole plugin and prints its harmonics and movement.
// Usage: millstone_preset_probe <note> [preset=NAME] key=value ...   (a value is an option name or a number; default patch otherwise)
#include "PluginProcessor.h"
#include <algorithm>
#include <complex>
#include <cstdio>
#include <vector>
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    const double sr = 44100; const int block = 256;
    MillstoneProcessor p; p.setRateAndBufferSizeDetails(sr, block); p.prepareToPlay(sr, block);
    const int note = juce::String(argv[1]).getIntValue();
    for (int i = 2; i < argc; ++i) {
        const juce::String arg(argv[i]); const auto key = arg.upToFirstOccurrenceOf("=", false, false), value = arg.fromFirstOccurrenceOf("=", false, false);
        if (key == "preset") { const auto& l = MillstoneProcessor::factoryPresets(); for (int n = 0; n < (int)l.size(); ++n) if (l[(size_t)n].name == value) p.setCurrentProgram(n); continue; }
        auto* q = p.apvts.getParameter(key); if (q == nullptr) { printf("no %s\n", key.toRawUTF8()); return 1; }
        float raw = value.getFloatValue(); if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(q)) raw = (float)c->choices.indexOf(value);
        q->setValueNotifyingHost(q->convertTo0to1(raw));
    }
    juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)100), 0);
    juce::AudioBuffer<float> b(2, block); std::vector<float> x;
    while (x.size() < (size_t)(sr * 4)) { b.clear(); p.processBlock(b, m); m.clear(); x.insert(x.end(), b.getReadPointer(0), b.getReadPointer(0) + block); }
    const double f0 = 440.0 * std::pow(2.0, (note - 69) / 12.0);
    const auto bin = [&](double hz, size_t from, size_t n) { std::complex<double> s(0); for (size_t i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * std::cos(6.283185307 * (double)i / (double)n); s += (double)x[from + i] * w * std::polar(1.0, -6.283185307 * hz * (double)i / sr); } return 2.0 * std::abs(s) / ((double)n * 0.5); };
    const size_t n = (size_t)(sr * 1.0), from = (size_t)(sr * 1.5);
    double h1 = 0; printf("harmonics dB rel 1st:");
    for (int k = 1; k <= 12; ++k) { const double a = bin(f0 * k, from, n); if (k == 1) h1 = a; printf(" %5.1f", 20 * std::log10(a / (h1 + 1e-9) + 1e-6)); }
    // Movement: how strongly the level of the band around the 8th harmonic beats, and at what rate.
    std::vector<double> env; const size_t hop = 512;
    { double lp = 0, hp = 0; const double a = 1 - std::exp(-6.283 * f0 * 16 / sr), c = 1 - std::exp(-6.283 * f0 * 4 / sr); double e = 0; for (size_t i = 0; i < x.size(); ++i) { lp += (x[i] - lp) * a; hp += (lp - hp) * c; const double v = lp - hp; e += v * v; if ((i + 1) % hop == 0) { env.push_back(std::sqrt(e / hop)); e = 0; } } }
    double mean = 0, dev = 0; size_t skip = env.size() / 4; for (size_t i = skip; i < env.size(); ++i) mean += env[i]; mean /= (double)(env.size() - skip); int cross = 0; for (size_t i = skip + 1; i < env.size(); ++i) { dev += std::abs(env[i] - mean); cross += (env[i - 1] < mean) != (env[i] < mean); }
    double rms = 0, peak = 0; for (size_t i = 0; i < x.size(); ++i) { rms += x[i] * x[i]; peak = std::max(peak, (double)std::abs(x[i])); }
    printf("\nrms %.1f dB  peak %.1f dB  beating depth %.0f%%  rate %.2f Hz\n", 10 * std::log10(rms / (double)x.size() + 1e-12), 20 * std::log10(peak + 1e-9), 100 * dev / (double)(env.size() - skip) / (mean + 1e-9), (double)cross / 2.0 / ((double)(env.size() - skip) * (double)hop / sr));
    // For hits: the strongest two tones in the first 100 ms, and how long the level takes to fall 30 dB from its peak.
    {
        const size_t size = (size_t)(sr * 0.1); std::vector<std::pair<double, double>> tones;
        for (double hz = 60; hz < 12000; hz *= 1.01) tones.push_back({ bin(hz, 0, size), hz });
        std::sort(tones.rbegin(), tones.rend());
        const double first = tones[0].second; double second = 0;
        for (const auto& t : tones) if (t.second > first * 1.18 || t.second < first / 1.18) { second = t.second; break; }
        double top = 0; size_t at = 0; const size_t w = (size_t)(0.002 * sr); std::vector<double> levels;
        for (size_t i = 0; i + w < x.size(); i += w) { double e = 0; for (size_t k = i; k < i + w; ++k) e += x[k] * x[k]; levels.push_back(e); if (e > top) { top = e; at = levels.size() - 1; } }
        size_t end = at; for (size_t i = at; i < levels.size(); ++i) if (levels[i] > top * 0.001) end = i;
        printf("tones %.0f Hz and %.0f Hz (%+.1f st)  falls 30 dB in %.0f ms\n", first, second, 12.0 * std::log2(second / first), (double)(end - at) * 2.0);
    }
    return 0;
}
