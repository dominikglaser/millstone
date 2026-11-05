// Small screens on the main panel, for what a knob cannot say: the actual curve and the actual numbers. The formulas are the engine's own
// (src/dsp_bits.h), so what is drawn is what is heard. Included by PluginEditor.cpp.

class ScreenControl : public Control {
public:
    ScreenControl(const Item& item, MillstoneProcessor& p) : processor(p), mode(item.str("mode")), prefix(item.str("prefix")) {
        setBounds(item.num("x"), item.num("y"), item.num("w"), item.num("h"));
        setInterceptsMouseClicks(false, false);
        setComponentID("screen_" + (prefix.isNotEmpty() ? mode + "_" + prefix : mode));
    }

    void refresh() override {
        juce::String stamp;
        if (mode == "env") stamp << raw("attack") << "/" << raw("decay") << "/" << raw("sustain") << "/" << raw("release");
        else if (mode == "filter") stamp << raw("cutoff") << "/" << raw("reso") << "/" << raw("f_mode") << "/" << raw("drive");
        else stamp << raw("lfo_page") << "/" << raw(lfoKey("shape")) << "/" << raw(lfoKey("rate")) << "/" << raw(lfoKey("sync")) << "/" << raw(lfoKey("div")) << "/" << raw(lfoKey("lock")) << "/" << raw(lfoKey("retrig"));
        if (stamp != lastStamp) { lastStamp = stamp; getProperties().set("text", caption()); repaint(); }
    }

    void paint(juce::Graphics& g) override {
        const auto screen = getLocalBounds().toFloat().reduced(2.5f);
        paintLcdWell(g, screen, 2.0f);
        if (mode == "env") paintEnvelope(g, screen);
        else if (mode == "filter") paintFilter(g, screen);
        else paintLfo(g, screen);
        paintLcdGlass(g, screen, 2.0f);
    }

private:
    float raw(const juce::String& key) const { return processor.apvts.getRawParameterValue(prefix + key)->load(); }
    juce::String text(const juce::String& key) const { return processor.apvts.getParameter(key)->getCurrentValueAsText(); }

    static juce::String timeText(float seconds) {
        if (seconds < 1.0f) return juce::String(juce::roundToInt(seconds * 1000.0f)) + " ms";
        return juce::String(seconds, seconds < 10.0f ? 2 : 1) + " s";
    }
    static juce::String hertzText(float hz) {
        if (hz >= 1000.0f) return juce::String(hz * 0.001f, 2) + " kHz";
        if (hz >= 100.0f) return juce::String(juce::roundToInt(hz)) + " Hz";
        return juce::String(hz, hz < 1.0f ? 3 : 2) + " Hz";
    }
    float attackSeconds() const { return 0.002f * std::pow(1500.0f, raw("attack") * 0.01f); }
    float decaySeconds() const { return 0.004f * std::pow(2500.0f, raw("decay") * 0.01f); }
    float releaseSeconds() const { return 0.004f * std::pow(2500.0f, raw("release") * 0.01f); }
    float cutoffHz() const { return 20.0f * std::pow(2.0f, 10.0f * raw("cutoff") * 0.01f); }
    juce::String lfoKey(const char* suffix) const { return juce::String(raw("lfo_page") > 0.5f ? "lfo2_" : "lfo_") + suffix; }
    float lfoHz() const { return 0.01f * std::pow(35000.0f, raw(lfoKey("rate")) * 0.01f); }

    juce::String caption() const {
        if (mode == "env") return timeText(attackSeconds()) + " / " + timeText(decaySeconds()) + " / " + juce::String(juce::roundToInt(raw("sustain"))) + "% / " + timeText(releaseSeconds());
        if (mode == "filter") return text("f_mode") + " " + hertzText(cutoffHz());
        return raw(lfoKey("sync")) > 0.5f ? text(lfoKey("div")) : hertzText(lfoHz());
    }

    // Time is drawn compressed: a 2 ms attack and a 10 s release have to be readable on one small screen.
    void paintEnvelope(juce::Graphics& g, juce::Rectangle<float> s) const {
        g.setColour(amberDim.brighter(0.5f));
        g.setFont(displayFont(10.0f));
        g.drawFittedText(caption(), s.reduced(6.0f, 2.0f).withHeight(13.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
        const auto r = s.withTrimmedTop(15.0f).reduced(6.0f, 2.0f);
        const auto width = [](float seconds) { return std::pow(seconds, 0.33f); };
        const float a = width(attackSeconds()), d = width(decaySeconds()), hold = 0.55f, rel = width(releaseSeconds());
        const float scale = r.getWidth() / (a + d + hold + rel), sus = raw("sustain") * 0.01f;
        juce::Path p;
        p.startNewSubPath(r.getBottomLeft());
        const auto Y = [&](float v) { return r.getBottom() - v * r.getHeight(); };
        float x = r.getX();
        for (int i = 1; i <= 16; ++i) { const float t = (float)i / 16.0f; p.lineTo(x + a * scale * t, Y((1.0f - std::pow(1.0f - t, 2.2f)))); }
        x += a * scale;
        for (int i = 1; i <= 24; ++i) { const float t = (float)i / 24.0f; p.lineTo(x + d * scale * t, Y(sus + (1.0f - sus) * std::pow(0.001f, t))); }
        x += d * scale;
        p.lineTo(x + hold * scale, Y(sus));
        x += hold * scale;
        for (int i = 1; i <= 24; ++i) { const float t = (float)i / 24.0f; p.lineTo(x + rel * scale * t, Y(sus * std::pow(0.001f, t))); }
        auto fill = p;
        fill.lineTo(r.getBottomRight());
        fill.closeSubPath();
        g.setColour(amber.withAlpha(0.22f));
        g.fillPath(fill);
        g.setColour(amber);
        g.strokePath(p, juce::PathStrokeType(1.3f));
    }

    // The ladder's own response: four one-pole stages in a feedback loop.
    void paintFilter(juce::Graphics& g, juce::Rectangle<float> s) const {
        g.setColour(amberDim.brighter(0.5f));
        g.setFont(displayFont(11.0f));
        g.drawText(caption(), s.reduced(8.0f, 2.0f).withHeight(14.0f), juce::Justification::centredLeft);
        const auto r = s.withTrimmedTop(3.0f).withTrimmedBottom(3.0f).withTrimmedLeft(s.getWidth() * 0.22f).reduced(4.0f, 0.0f);
        const float fc = cutoffHz(), k = std::pow(raw("reso") * 0.01f, 1.3f) * 4.7f;
        const bool highpass = raw("f_mode") > 0.5f;
        const float comp = highpass ? 1.0f : std::sqrt(1.0f + k);
        const float floorDb = -36.0f, topDb = 18.0f;
        juce::Path p;
        g.setColour(amberDim.withAlpha(0.45f));
        for (const float hz : { 100.0f, 1000.0f, 10000.0f }) {
            const float x = r.getX() + r.getWidth() * std::log(hz / 20.0f) / std::log(1000.0f);
            g.fillRect(x, r.getY(), 1.0f, r.getHeight());
        }
        const float zero = r.getY() + r.getHeight() * (topDb / (topDb - floorDb));
        g.fillRect(r.getX(), zero, r.getWidth(), 1.0f);
        const int points = 120;
        for (int i = 0; i <= points; ++i) {
            const float t = (float)i / (float)points, hz = 20.0f * std::pow(1000.0f, t), w = hz / fc;
            const std::complex<float> stage(1.0f, w), loop = stage * stage * stage * stage + k;
            const std::complex<float> top = highpass ? std::complex<float>(w * w * w * w, 0.0f) : std::complex<float>(comp, 0.0f);
            const float db = juce::jlimit(floorDb, topDb, 20.0f * std::log10(std::abs(top / loop) + 1e-6f));
            const juce::Point<float> at(r.getX() + t * r.getWidth(), r.getY() + r.getHeight() * (topDb - db) / (topDb - floorDb));
            if (i == 0) p.startNewSubPath(at); else p.lineTo(at);
        }
        g.setColour(amber);
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }

    void paintLfo(juce::Graphics& g, juce::Rectangle<float> s) const {
        const bool sync = raw(lfoKey("sync")) > 0.5f;
        g.setFont(displayFont(10.5f));
        g.setColour(amberDim.brighter(0.5f));
        const auto head = s.reduced(8.0f, 4.0f).withHeight(14.0f);
        g.drawText(text(lfoKey("shape")), head, juce::Justification::centredLeft);
        g.drawText(sync ? "SONG" : (raw(lfoKey("retrig")) > 0.5f ? "RETRIG" : "FREE"), head, juce::Justification::centredRight);
        g.setFont(displayFont(22.0f));
        drawGlowText(g, caption(), s.reduced(8.0f, 4.0f).withTrimmedTop(14.0f).withHeight(28.0f).toNearestInt(), juce::Justification::centredLeft);

        const auto r = s.withTrimmedTop(48.0f).reduced(8.0f, 6.0f);
        const int shape = juce::roundToInt(raw(lfoKey("shape")));
        g.setColour(amberDim.withAlpha(0.55f));
        g.fillRect(r.getX(), r.getCentreY(), r.getWidth(), 1.0f);
        juce::Path p;
        const int points = 200, cycles = 2;
        static const float held[8] = { 0.5f, -0.7f, 0.15f, 0.9f, -0.35f, 0.7f, -0.9f, 0.3f };
        for (int i = 0; i <= points; ++i) {
            const float t = (float)i / (float)points, c = t * (float)cycles, phase = c - std::floor(c);
            float v = 0.0f;
            switch (shape) {
                case 0: v = phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase; break;
                case 1: v = phase < 0.5f ? 1.0f : -1.0f; break;
                case 2: v = 1.0f - 2.0f * phase; break;
                case 3: v = 2.0f * phase - 1.0f; break;
                default: v = held[juce::jmin(7, (int)(t * 8.0f))]; break;
            }
            const juce::Point<float> at(r.getX() + t * r.getWidth(), r.getCentreY() - v * (r.getHeight() * 0.5f - 1.0f));
            if (i == 0) p.startNewSubPath(at); else p.lineTo(at);
        }
        g.setColour(amber);
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }

    MillstoneProcessor& processor;
    juce::String mode, prefix, lastStamp;
};
