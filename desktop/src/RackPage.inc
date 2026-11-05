// The second page: a road-case rack holding the effects and the clock, each its own black unit with its own accent. Included by
// PluginEditor.cpp, inside its namespace rules: the first half is art and tables, the second half is the components.

// ---- what the units' screens offer ----------------------------------------------------------------------------------
struct FxValue { const char* key; const char* value; };
struct FxPreset { const char* name; std::vector<FxValue> values; };
struct RackSetting { const char* key; const char* label; };
struct RackLcdSpec {
    const char* id;
    juce::Rectangle<int> screen;
    juce::Point<int> keys;                  // the preset < > keys
    juce::Point<int> pot;                   // the VALUE knob, if it has settings to edit
    int potR;
    juce::uint32 accent;
    const char* mixKey;                     // a preset brings a unit that is off up to this...
    const char* mixValue;                   // ...this much. It never moves a mix that is already set.
    std::vector<RackSetting> settings;
    std::vector<FxPreset> presets;
};

const std::vector<RackLcdSpec>& rackLcds() {
    static const std::vector<RackLcdSpec> table {
        { "delay", { 830, 432, 440, 150 }, { 830, 592 }, { 1336, 505 }, 34, 0xff4fd07a, "fx_dl_mix", "35",
          { {"fx_dl_drive", "DRIVE"}, {"fx_dl_wow", "WOW"}, {"fx_dl_duck", "DUCK"}, {"fx_dl_width", "WIDTH"} },
          {
            { "STEPPERS",     { {"fx_dl_sync","SYNC"},{"fx_dl_div","1/8 D"},{"fx_dl_fb","55"},{"fx_dl_heads","1"},{"fx_dl_pp","OFF"},{"fx_dl_low","30"},{"fx_dl_high","62"},{"fx_dl_drive","30"},{"fx_dl_wow","20"},{"fx_dl_duck","0"} } },
            { "SKANK ANSWER", { {"fx_dl_sync","SYNC"},{"fx_dl_div","1/8 D"},{"fx_dl_fb","45"},{"fx_dl_heads","1"},{"fx_dl_pp","OFF"},{"fx_dl_low","40"},{"fx_dl_high","66"},{"fx_dl_drive","25"},{"fx_dl_wow","15"},{"fx_dl_duck","70"} } },
            { "ROOTS TAPE",   { {"fx_dl_sync","SYNC"},{"fx_dl_div","1/4"},{"fx_dl_fb","78"},{"fx_dl_heads","1+3"},{"fx_dl_pp","OFF"},{"fx_dl_low","35"},{"fx_dl_high","52"},{"fx_dl_drive","45"},{"fx_dl_wow","35"},{"fx_dl_duck","0"} } },
            { "RUNAWAY",      { {"fx_dl_sync","SYNC"},{"fx_dl_div","1/4 T"},{"fx_dl_fb","108"},{"fx_dl_heads","1"},{"fx_dl_pp","OFF"},{"fx_dl_low","30"},{"fx_dl_high","55"},{"fx_dl_drive","55"},{"fx_dl_wow","30"},{"fx_dl_duck","0"} } },
            { "SLAPBACK",     { {"fx_dl_sync","FREE"},{"fx_dl_time","110"},{"fx_dl_fb","10"},{"fx_dl_heads","1"},{"fx_dl_pp","OFF"},{"fx_dl_low","25"},{"fx_dl_high","72"},{"fx_dl_drive","20"},{"fx_dl_wow","10"},{"fx_dl_duck","0"} } },
            { "PING HALL",    { {"fx_dl_sync","SYNC"},{"fx_dl_div","1/8"},{"fx_dl_fb","60"},{"fx_dl_heads","1"},{"fx_dl_pp","ON"},{"fx_dl_width","100"},{"fx_dl_low","30"},{"fx_dl_high","70"},{"fx_dl_drive","15"},{"fx_dl_wow","10"},{"fx_dl_duck","0"} } },
            { "WIDE DOTTED",  { {"fx_dl_sync","SYNC"},{"fx_dl_div","1/8 D"},{"fx_dl_fb","50"},{"fx_dl_heads","1"},{"fx_dl_pp","ON"},{"fx_dl_width","70"},{"fx_dl_low","30"},{"fx_dl_high","64"},{"fx_dl_drive","20"},{"fx_dl_wow","15"},{"fx_dl_duck","20"} } },
            { "DUB TECHNO",   { {"fx_dl_sync","SYNC"},{"fx_dl_div","1/4 D"},{"fx_dl_fb","70"},{"fx_dl_heads","1"},{"fx_dl_pp","ON"},{"fx_dl_width","60"},{"fx_dl_low","45"},{"fx_dl_high","58"},{"fx_dl_drive","30"},{"fx_dl_wow","10"},{"fx_dl_duck","40"} } },
          } },
        { "reverb", { 100, 722, 620, 124 }, { 744, 785 }, { 0, 0 }, 0, 0xff58a8ff, "fx_rv_mix", "30", {},
          {
            { "DUB SPRING",   { {"fx_rv_type","SPRING"},{"fx_rv_decay","55"},{"fx_rv_diff","70"},{"fx_rv_damp","30"} } },
            { "TIGHT SPRING", { {"fx_rv_type","SPRING"},{"fx_rv_decay","25"},{"fx_rv_diff","50"},{"fx_rv_damp","45"} } },
            { "WET TANK",     { {"fx_rv_type","SPRING"},{"fx_rv_decay","85"},{"fx_rv_diff","85"},{"fx_rv_damp","20"} } },
            { "SMALL ROOM",   { {"fx_rv_type","NORMAL"},{"fx_rv_decay","15"},{"fx_rv_diff","50"},{"fx_rv_damp","50"} } },
            { "STUDIO PLATE", { {"fx_rv_type","NORMAL"},{"fx_rv_decay","45"},{"fx_rv_diff","65"},{"fx_rv_damp","35"} } },
            { "BRIGHT PLATE", { {"fx_rv_type","NORMAL"},{"fx_rv_decay","55"},{"fx_rv_diff","70"},{"fx_rv_damp","10"} } },
            { "DARK HALL",    { {"fx_rv_type","NORMAL"},{"fx_rv_decay","80"},{"fx_rv_diff","75"},{"fx_rv_damp","65"} } },
            { "LONG CLOUD",   { {"fx_rv_type","NORMAL"},{"fx_rv_decay","100"},{"fx_rv_diff","85"},{"fx_rv_damp","40"} } },
          } },
        { "chorus", { 810, 956, 420, 88 }, { 1245, 987 }, { 0, 0 }, 0, 0xffff9a3c, nullptr, nullptr, {},
          {
            { "CLASSIC I",    { {"fx_ch_mode","I"},{"fx_ch_mix","100"} } },
            { "CLASSIC II",   { {"fx_ch_mode","II"},{"fx_ch_mix","100"} } },
            { "NERVOUS",      { {"fx_ch_mode","I+II"},{"fx_ch_mix","100"} } },
            { "SUBTLE WIDTH", { {"fx_ch_mode","I"},{"fx_ch_mix","45"} } },
            { "THICK",        { {"fx_ch_mode","II"},{"fx_ch_mix","70"} } },
            { "BYPASS",       { {"fx_ch_mode","OFF"} } },
          } },
        { "phaser", { 810, 1076, 420, 88 }, { 1245, 1107 }, { 0, 0 }, 0, 0xffc77dff, "fx_ph_mix", "60", {},
          {
            { "SLOW SWEEP",   { {"fx_ph_rate","20"},{"fx_ph_depth","80"},{"fx_ph_fb","30"} } },
            { "STONE",        { {"fx_ph_rate","40"},{"fx_ph_depth","70"},{"fx_ph_fb","60"} } },
            { "SWIRL",        { {"fx_ph_rate","60"},{"fx_ph_depth","60"},{"fx_ph_fb","75"} } },
            { "RESONANT",     { {"fx_ph_rate","30"},{"fx_ph_depth","90"},{"fx_ph_fb","90"} } },
            { "SHIMMER",      { {"fx_ph_rate","72"},{"fx_ph_depth","40"},{"fx_ph_fb","20"} } },
          } },
        { "tides", { 830, 268, 440, 110 }, { 830, 384 }, { 1336, 322 }, 30, 0xff2fd6c8, nullptr, nullptr,
          { {"t_smooth", "SMOOTH"}, {"t_shift", "SHIFT"} },
          {
            { "NOTE ENVELOPE", { {"t_mode","AD"},{"t_out","PHASE"},{"t_trig","NOTE"},{"t_rate","45"},{"t_shape","35"},{"t_slope","15"},{"t_smooth","50"},{"t_shift","50"} } },
            { "NOTE SWELL",    { {"t_mode","AR"},{"t_out","PHASE"},{"t_trig","NOTE"},{"t_rate","35"},{"t_shape","60"},{"t_slope","70"},{"t_smooth","50"},{"t_shift","50"} } },
            { "SLOW DRIFT",    { {"t_mode","LOOP"},{"t_out","PHASE"},{"t_trig","FREE"},{"t_rate","20"},{"t_shape","50"},{"t_slope","50"},{"t_smooth","50"},{"t_shift","50"} } },
            { "FOUR PHASES",   { {"t_mode","LOOP"},{"t_out","PHASE"},{"t_trig","FREE"},{"t_rate","35"},{"t_shape","50"},{"t_slope","50"},{"t_smooth","50"},{"t_shift","75"} } },
            { "FAST WOBBLE",   { {"t_mode","LOOP"},{"t_out","PHASE"},{"t_trig","FREE"},{"t_rate","70"},{"t_shape","50"},{"t_slope","50"},{"t_smooth","50"},{"t_shift","50"} } },
            { "CLOCKED RAMP",  { {"t_mode","LOOP"},{"t_out","PHASE"},{"t_trig","CLOCK"},{"t_div","1/4"},{"t_shape","50"},{"t_slope","90"},{"t_smooth","50"},{"t_shift","50"} } },
            { "CLOCKED GATES", { {"t_mode","LOOP"},{"t_out","GATES"},{"t_trig","CLOCK"},{"t_div","1/8"},{"t_shape","50"},{"t_slope","50"},{"t_smooth","50"},{"t_shift","50"} } },
            { "SPREAD HARMS",  { {"t_mode","LOOP"},{"t_out","FREQ"},{"t_trig","FREE"},{"t_rate","30"},{"t_shape","50"},{"t_slope","50"},{"t_smooth","50"},{"t_shift","65"} } },
          } },
    };
    return table;
}

// ---- the art ---------------------------------------------------------------------------------------------------------
void paintBrushedBlack(juce::Graphics& g, juce::Rectangle<float> r, int seed) {
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff222224), 0, r.getY(), juce::Colour(0xff121213), 0, r.getBottom(), false));
    g.fillRect(r);
    juce::Random rng(seed);
    const int lines = (int)(r.getHeight() * 16.0f);
    for (int i = 0; i < lines; ++i) {
        const float y = r.getY() + rng.nextFloat() * r.getHeight(), x = r.getX() + rng.nextFloat() * r.getWidth(), length = 40.0f + rng.nextFloat() * 500.0f;
        g.setColour((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha(0.012f + rng.nextFloat() * 0.035f));
        g.fillRect(x, y, juce::jmin(length, r.getRight() - x), 0.5f);
    }
}

void paintRackScrew(juce::Graphics& g, float cx, float cy, float angle) {   // black oxide, over a washer
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(cx - 6.5f, cy - 5.5f, 13.0f, 13.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff77777a), cx - 3, cy - 5, juce::Colour(0xff1c1c1e), cx + 4, cy + 5, false));
    g.fillEllipse(cx - 6, cy - 6, 12, 12);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4c4c50), cx - 2, cy - 4, juce::Colour(0xff0d0d0e), cx + 3, cy + 4, false));
    g.fillEllipse(cx - 4.3f, cy - 4.3f, 8.6f, 8.6f);
    const juce::Point<float> c(cx, cy);
    g.setColour(juce::Colour(0xff050505));
    g.drawLine({ c.getPointOnCircumference(3.8f, angle), c.getPointOnCircumference(3.8f, angle + juce::MathConstants<float>::pi) }, 1.3f);
    g.drawLine({ c.getPointOnCircumference(3.8f, angle + 1.57f), c.getPointOnCircumference(3.8f, angle + 1.57f + juce::MathConstants<float>::pi) }, 1.3f);
}

void paintHandle(juce::Graphics& g, juce::Rectangle<float> r) {   // a pull handle, standing off the ear
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRoundedRectangle(r.translated(3.0f, 4.0f), 8.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3c3c3f), r.getX(), 0, juce::Colour(0xff09090a), r.getRight(), 0, false));
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.drawLine(r.getX() + 3.0f, r.getY() + 9.0f, r.getX() + 3.0f, r.getBottom() - 9.0f, 1.0f);
    const auto hollow = r.reduced(7.0f, 13.0f);
    g.setColour(juce::Colour(0xff050505));
    g.fillRoundedRectangle(hollow, 4.0f);
}

void paintRackUnit(juce::Graphics& g, const Item& item, int seed) {
    const juce::Rectangle<float> r((float)item.num("x"), (float)item.num("y"), (float)item.num("w"), (float)item.num("h"));
    const auto accent = itemAccentOf(item);
    g.setColour(juce::Colours::black);
    g.fillRect(r.translated(0.0f, 2.0f));
    paintBrushedBlack(g, r, seed);

    // Folded edge: light from above, as on the main panel.
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.20f), 0, r.getY(), juce::Colours::transparentWhite, 0, r.getY() + 3.5f, false));
    g.fillRect(r.withHeight(3.5f));
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, 0, r.getBottom() - 4.0f, juce::Colours::black.withAlpha(0.65f), 0, r.getBottom(), false));
    g.fillRect(r.withTop(r.getBottom() - 4.0f));

    // The ears, a little raised from the face.
    for (const float x : { r.getX() + 54.0f, r.getRight() - 54.0f }) {
        g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillRect(x - 0.5f, r.getY() + 1, 1.5f, r.getHeight() - 2);
        g.setColour(juce::Colours::white.withAlpha(0.07f)); g.fillRect(x + 1.0f, r.getY() + 1, 1.0f, r.getHeight() - 2);
    }
    juce::Random turns(seed + 3);
    const bool tall = r.getHeight() > 70.0f;
    for (const float x : { r.getX() + 15.0f, r.getRight() - 15.0f })
        for (const float y : tall ? std::vector<float>{ r.getY() + 16.0f, r.getBottom() - 16.0f } : std::vector<float>{ r.getCentreY() })
            paintRackScrew(g, x, y, turns.nextFloat() * 3.14f);
    if (item.num("handles") != 0)
        for (const float x : { r.getX() + 28.0f, r.getRight() - 50.0f })
            paintHandle(g, { x, r.getY() + 26.0f, 22.0f, r.getHeight() - 52.0f });

    // Maker's printing: the unit's own colour.
    const juce::Point<float> at((float)item.num("tx", (int)r.getX() + 68), (float)item.num("ty", (int)r.getY() + 9));
    const auto name = item.str("label"), model = item.str("model");
    g.setColour(accent);
    const auto nameFont = printFont(16.0f, true, 0.20f);
    g.setFont(nameFont);
    g.drawText(name, juce::Rectangle<float>(at.x, at.y, 320.0f, 20.0f), juce::Justification::centredLeft);
    g.setColour(cream.withAlpha(0.72f));
    g.setFont(printFont(9.5f, true, 0.20f));
    g.drawText(model, juce::Rectangle<float>(at.x, at.y + 19.0f, 320.0f, 12.0f), juce::Justification::centredLeft);

    if (item.num("rule") != 0) {
        const float x0 = at.x + juce::GlyphArrangement::getStringWidth(nameFont, name) + 14.0f, x1 = (float)item.num("rule");
        g.setColour(accent.withAlpha(0.55f));
        g.fillRect(x0, at.y + 9.5f, x1 - x0, 1.2f);
    }

    // A glowing badge, the way digital rack units light theirs.
    const auto badge = item.str("badge");
    if (badge.isNotEmpty()) {
        const juce::Rectangle<float> area((float)item.num("bx"), (float)item.num("by"), (float)item.num("bw", 240), 40.0f);
        g.setFont(printFont(30.0f, true, 0.10f));
        for (int ring = 3; ring >= 1; --ring) {
            g.setColour(accent.withAlpha(0.07f));
            for (const auto& d : { juce::Point<float>(-1, 0), juce::Point<float>(1, 0), juce::Point<float>(0, -1), juce::Point<float>(0, 1), juce::Point<float>(-1, -1), juce::Point<float>(1, 1), juce::Point<float>(-1, 1), juce::Point<float>(1, -1) })
                g.drawText(badge, area.translated(d.x * (float)ring, d.y * (float)ring), juce::Justification::centred);
        }
        g.setColour(accent.brighter(0.5f));
        g.drawText(badge, area, juce::Justification::centred);
    }
}

void paintRackVent(juce::Graphics& g, const Item& item, int seed) {
    const juce::Rectangle<float> r((float)item.num("x"), (float)item.num("y"), (float)item.num("w"), (float)item.num("h"));
    paintBrushedBlack(g, r, seed);
    g.setColour(juce::Colours::white.withAlpha(0.14f));
    g.fillRect(r.withHeight(1.5f));
    const int slots = 6;
    const float span = r.getWidth() - 220.0f, slotW = span / (float)slots - 18.0f;
    for (int i = 0; i < slots; ++i) {
        const juce::Rectangle<float> slot(r.getX() + 110.0f + (float)i * (slotW + 18.0f) + 9.0f, r.getCentreY() - 5.5f, slotW, 11.0f);
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.fillRoundedRectangle(slot.translated(0.0f, 1.0f), 5.5f);
        g.setColour(juce::Colour(0xff030303));
        g.fillRoundedRectangle(slot, 5.5f);
    }
    juce::Random turns(seed);
    for (const float x : { r.getX() + 15.0f, r.getRight() - 15.0f }) paintRackScrew(g, x, r.getCentreY(), turns.nextFloat() * 3.14f);
}

juce::Image makeRackArt() {
    juce::Image img(juce::Image::RGB, designW * artScale, rackH * artScale, true, juce::SoftwareImageType());
    juce::Graphics g(img);
    g.addTransform(juce::AffineTransform::scale((float)artScale));
    const juce::Rectangle<float> all(0, 0, (float)designW, (float)rackH);

    // The road case: textured rubber, with the rack recessed into it.
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2b2b2d), 0, 0, juce::Colour(0xff161617), 0, (float)rackH, false));
    g.fillRect(all);
    juce::Random grain(41);
    for (int i = 0; i < 36000; ++i) {
        g.setColour((grain.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha(0.03f + grain.nextFloat() * 0.07f));
        const float d = 0.5f + grain.nextFloat() * 0.9f;
        g.fillEllipse(grain.nextFloat() * (float)designW, grain.nextFloat() * (float)rackH, d, d);
    }
    const juce::Rectangle<float> well(22, 20, (float)designW - 44, (float)rackH - 42);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(well.expanded(5.0f), 8.0f, 1.2f);
    g.setColour(juce::Colour(0xff050505));
    g.fillRoundedRectangle(well, 4.0f);

    // Rails either side of the units, with their holes.
    for (const float x : { 25.0f, (float)designW - 31.0f }) {
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4a4d), x, 0, juce::Colour(0xff1c1c1e), x + 6.0f, 0, false));
        g.fillRect(x, well.getY(), 6.0f, well.getHeight());
    }

    int seed = 300;
    for (const auto& item : panelItems()) {
        if (item.kind == "unit") paintRackUnit(g, item, seed += 7);
        else if (item.kind == "vent") paintRackVent(g, item, seed += 7);
        else if (item.kind == "toggle" && item.str("page") == "rack") {
            g.setColour(cream);
            g.setFont(printFont(10.5f));
            g.drawText(item.str("label"), juce::Rectangle<float>((float)item.num("cx") - 40, (float)item.num("cy") + 20, 80, 13), juce::Justification::centred);
        }
    }

    // Light falling across the case from above.
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.05f), 0, 0, juce::Colours::transparentWhite, 0, 240, false));
    g.fillRect(well);
    return img;
}

const juce::Image& sharedRackArt() {   // drawn the first time anyone opens the rack
    static const juce::Image image = makeRackArt();
    return image;
}

void drawGlowIn(juce::Graphics& g, juce::Colour colour, const juce::String& text, juce::Rectangle<int> r, juce::Justification just) {
    g.setColour(colour.withAlpha(0.15f));
    for (auto d : { juce::Point<int>(-1, 0), juce::Point<int>(1, 0), juce::Point<int>(0, -1), juce::Point<int>(0, 1) })
        g.drawFittedText(text, r + d, just, 1, 0.8f);
    g.setColour(colour);
    g.drawFittedText(text, r, just, 1, 0.8f);
}

void paintRackScreen(juce::Graphics& g, juce::Rectangle<float> screen) {
    g.setColour(juce::Colours::white.withAlpha(0.13f));
    g.fillRoundedRectangle(screen.expanded(2.5f).translated(0.3f, 1.0f), 4.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff000000), 0, screen.getY(), juce::Colour(0xff2b2b2d), 0, screen.getBottom(), false));
    g.fillRoundedRectangle(screen.expanded(2.0f), 4.0f);
    g.setColour(juce::Colour(0xff06080b));
    g.fillRoundedRectangle(screen, 2.5f);
}

void paintRackGlass(juce::Graphics& g, juce::Rectangle<float> screen) {
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.07f), 0, screen.getY(), juce::Colours::transparentWhite, 0, screen.getCentreY(), false));
    g.fillRoundedRectangle(screen.withHeight(screen.getHeight() * 0.5f), 2.5f);
}

// ---- a value on a screen, worked with the mouse like any knob -----------------------------------------------------------
// Up and down over the same 220 pixels as a knob, double click for the default, and the wheel. A plain click changes nothing.
struct LcdDrag {
    static constexpr float fullTravel = 220.0f, wheelTravel = 0.15f;
    juce::RangedAudioParameter* held = nullptr;
    float start = 0.0f;

    void down(juce::RangedAudioParameter* q) {
        up();
        held = q;
        if (held == nullptr) return;
        start = held->getValue();
        held->beginChangeGesture();
    }
    void drag(const juce::MouseEvent& e) {
        if (held != nullptr) held->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, start - (float)e.getDistanceFromDragStartY() / fullTravel));
    }
    void up() {
        if (held != nullptr) held->endChangeGesture();
        held = nullptr;
    }
    void reset(juce::RangedAudioParameter* q) {
        if (q == nullptr) return;
        if (held != q) q->beginChangeGesture();
        q->setValueNotifyingHost(q->getDefaultValue());
        if (held == q) start = q->getValue(); else q->endChangeGesture();
    }
    static void wheel(juce::RangedAudioParameter* q, const juce::MouseWheelDetails& w) {
        q->beginChangeGesture();
        const float delta = (w.deltaX != 0.0f ? -w.deltaX : w.deltaY) * (w.isReversed ? -wheelTravel : wheelTravel);
        q->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, q->getValue() + delta));
        q->endChangeGesture();
    }
};

// ---- a unit's screen: its presets, and the deeper settings on a VALUE knob ----------------------------------------------
class RackLcd : public juce::Component {
public:
    RackLcd(MillstoneProcessor& p, const RackLcdSpec& s, int index) : processor(p), spec(s), slot(index), accent(s.accent),
        previous("<"), next(">") {
        setComponentID(juce::String("lcd_") + spec.id);
        setBounds(0, 0, designW, rackH);
        for (auto* key : { &previous, &next }) { key->dark = true; key->lamp = false; key->lampOn = accent; addAndMakeVisible(*key); }
        previous.setComponentID(juce::String("preset_prev_") + spec.id);
        next.setComponentID(juce::String("preset_next_") + spec.id);
        previous.setBounds(spec.keys.x, spec.keys.y, 36, 26);
        next.setBounds(spec.keys.x + 40, spec.keys.y, 36, 26);
        previous.onClick = [this] { step(-1); };
        next.onClick = [this] { step(1); };
        if (!spec.settings.empty()) {
            value.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            value.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            value.setMouseDragSensitivity(220);
            value.setComponentID(juce::String("lcd_value_") + spec.id);
            value.getProperties().set("rackAccent", juce::String::toHexString((int)(accent.getARGB() & 0xffffff)));
            value.setBounds(spec.pot.x - spec.potR - knobPad, spec.pot.y - spec.potR - knobPad, 2 * (spec.potR + knobPad), 2 * (spec.potR + knobPad));
            value.onDragStart = [this] { if (auto* q = current()) q->beginChangeGesture(); };
            value.onDragEnd = [this] { if (auto* q = current()) q->endChangeGesture(); };
            value.onValueChange = [this] { if (!binding) if (auto* q = current()) { q->setValueNotifyingHost(q->convertTo0to1((float)value.getValue())); repaint(); } };
            addAndMakeVisible(value);
            bindValue();
        }
    }

    bool hitTest(int x, int y) override {
        return spec.screen.contains(x, y) || previous.getBounds().contains(x, y) || next.getBounds().contains(x, y) || (value.isVisible() && value.getBounds().contains(x, y));
    }

    void mouseDown(const juce::MouseEvent& e) override {
        for (int i = 0; i < (int)spec.settings.size(); ++i)
            if (settingCell(i).contains(e.position)) selectSetting(i);
        lcdDrag.down(paramAt(e.position));
    }
    void mouseDrag(const juce::MouseEvent& e) override { lcdDrag.drag(e); refresh(); }
    void mouseUp(const juce::MouseEvent&) override { lcdDrag.up(); }
    void mouseDoubleClick(const juce::MouseEvent& e) override { lcdDrag.reset(paramAt(e.position)); refresh(); }
    void mouseMove(const juce::MouseEvent& e) override { setMouseCursor(paramAt(e.position) != nullptr ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor); }
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override {
        if (auto* q = paramAt(e.position)) { LcdDrag::wheel(q, w); refresh(); }
        else juce::Component::mouseWheelMove(e, w);
    }

    void selectSetting(int i) { selected = i; bindValue(); repaint(); }

    void step(int direction) {
        const int count = (int)spec.presets.size();
        const int now = processor.fxPreset[(size_t)slot];
        apply(((now < 0 ? (direction > 0 ? -1 : 0) : now) + direction + count) % count);
    }

    void apply(int index) {
        processor.fxPreset[(size_t)slot] = index;
        for (const auto& v : spec.presets[(size_t)index].values) writeValue(v.key, v.value);
        if (spec.mixKey != nullptr && processor.apvts.getRawParameterValue(spec.mixKey)->load() <= 0.0f) writeValue(spec.mixKey, spec.mixValue);
        bindValue();
        refresh();
        repaint();
    }

    // Called by the timer, so automation, presets and the knobs all show.
    void refresh() {
        if (auto* q = current()) if (!value.isMouseButtonDown()) { binding = true; value.setValue(q->convertFrom0to1(q->getValue()), juce::dontSendNotification); binding = false; }
        const auto stamp = stateText();
        if (stamp != lastStamp) { lastStamp = stamp; getProperties().set("preset", presetText()); repaint(spec.screen); }
    }

    void paint(juce::Graphics& g) override {
        const auto screen = spec.screen.toFloat();
        paintRackScreen(g, screen);
        const auto dim = accent.withMultipliedSaturation(0.75f).withMultipliedBrightness(0.72f);
        if (juce::String(spec.id) == "delay") paintDelay(g, screen, dim);
        else if (juce::String(spec.id) == "reverb") paintReverb(g, screen, dim);
        else if (juce::String(spec.id) == "tides") paintTides(g, screen, dim);
        else paintSmall(g, screen, dim);
        paintRackGlass(g, screen);
        if (!spec.settings.empty()) {
            Item item;
            item.kind = "knob";
            item.attrs["cx"] = juce::String(spec.pot.x);
            item.attrs["cy"] = juce::String(spec.pot.y);
            item.attrs["r"] = juce::String(spec.potR);
            item.attrs["label"] = "VALUE";
            item.attrs["page"] = "rack";
            paintKnobPrinting(g, item);
        }
    }

private:
    juce::RangedAudioParameter* current() const {
        if (spec.settings.empty()) return nullptr;
        return processor.apvts.getParameter(spec.settings[(size_t)selected].key);
    }

    // The value drawn at a point on the screen, if there is one.
    juce::RangedAudioParameter* paramAt(juce::Point<float> at) const {
        const auto param = [this](const juce::String& key) { return processor.apvts.getParameter(key); };
        for (int i = 0; i < (int)spec.settings.size(); ++i)
            if (settingCell(i).contains(at)) return param(spec.settings[(size_t)i].key);
        const juce::String id(spec.id);
        const auto screen = spec.screen.toFloat();
        if (id == "delay" && delayTimeArea(screen).contains(at)) return param(text("fx_dl_sync") == "SYNC" ? "fx_dl_div" : "fx_dl_time");
        if (id == "reverb" && reverbTypeArea(screen).contains(at)) return param("fx_rv_type");
        if (id == "reverb" && reverbTailArea(screen).contains(at)) return param("fx_rv_decay");
        if (id == "tides") {
            if (tidesHeaderArea(screen, 0).contains(at)) return param("t_mode");
            if (tidesHeaderArea(screen, 1).contains(at)) return param("t_out");
            if (tidesHeaderArea(screen, 2).contains(at)) return param(text("t_trig") == "CLOCK" ? "t_div" : "t_trig");
        }
        return nullptr;
    }

    static juce::Rectangle<float> delayTimeArea(juce::Rectangle<float> s) { return { s.getRight() - 170, s.getY() + 6, 160, 14 }; }
    static juce::Rectangle<float> reverbTypeArea(juce::Rectangle<float> s) { return { s.getRight() - 180, s.getY() + 6, 170, 14 }; }
    static juce::Rectangle<float> reverbTailArea(juce::Rectangle<float> s) { return s.withTrimmedTop(36).withWidth(170).reduced(10, 2); }
    static juce::Rectangle<float> tidesHeaderArea(juce::Rectangle<float> s, int column) {   // MODE, OUTPUT, and TRIGGER or its division
        static constexpr float widths[] = { 40, 56, 68 };
        float right = s.getRight() - 10;
        for (int i = 2; i > column; --i) right -= widths[i];
        return { right - widths[column], s.getY() + 6, widths[column], 14 };
    }

    void bindValue() {
        auto* q = current();
        if (q == nullptr) return;
        binding = true;
        const auto range = q->getNormalisableRange();
        value.setRange(range.start, range.end, 0.0);
        value.setDoubleClickReturnValue(true, q->convertFrom0to1(q->getDefaultValue()));
        value.setValue(q->convertFrom0to1(q->getValue()), juce::dontSendNotification);
        binding = false;
    }

    void writeValue(const juce::String& key, const juce::String& text) {
        auto* q = processor.apvts.getParameter(key);
        if (q == nullptr) { jassertfalse; return; }
        float raw = text.getFloatValue();
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(q)) raw = (float)choice->choices.indexOf(text);
        q->beginChangeGesture();
        q->setValueNotifyingHost(q->convertTo0to1(raw));
        q->endChangeGesture();
    }

    bool presetMatches(int index) const {
        for (const auto& v : spec.presets[(size_t)index].values) {
            auto* q = processor.apvts.getParameter(v.key);
            if (q == nullptr) return false;
            const float now = q->convertFrom0to1(q->getValue());
            float want = juce::String(v.value).getFloatValue();
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(q)) want = (float)choice->choices.indexOf(v.value);
            if (std::abs(now - want) > 0.26f) return false;
        }
        return true;
    }

    juce::String presetText() const {
        const int index = processor.fxPreset[(size_t)slot];
        if (index < 0) return "CUSTOM";
        return juce::String(spec.presets[(size_t)index].name) + (presetMatches(index) ? "" : " *");
    }

    juce::String text(const char* key) const { auto* q = processor.apvts.getParameter(key); return q != nullptr ? q->getCurrentValueAsText() : juce::String(); }
    float raw(const char* key) const { return processor.apvts.getRawParameterValue(key)->load(); }

    juce::String stateText() const {
        juce::String s = presetText() + juce::String(selected);
        for (const auto& preset : { spec.presets[0] }) for (const auto& v : preset.values) s << text(v.key) << "/";
        if (juce::String(spec.id) == "tides") s << text("t_div") << text("t_rate");
        if (juce::String(spec.id) == "delay") s << text("fx_dl_sync") << text("fx_dl_time") << text("fx_dl_div") << text("fx_dl_pp") << text("fx_dl_width") << juce::String(processor.delaySecondsNow.load());
        return s;
    }

    juce::Rectangle<float> settingCell(int i) const {
        const auto s = spec.screen.toFloat();
        const float rowH = (s.getHeight() - 40.0f) * 0.5f;
        return { s.getX() + 10.0f + (float)(i % 2) * 86.0f, s.getY() + 34.0f + (float)(i / 2) * rowH, 82.0f, rowH - 3.0f };
    }

    void paintPresetLine(juce::Graphics& g, juce::Rectangle<float> screen, juce::Colour dim) const {
        g.setColour(dim);
        g.setFont(displayFont(10.0f));
        g.drawText("PRESET", juce::Rectangle<float>(screen.getX() + 10, screen.getY() + 6, 56, 14), juce::Justification::centredLeft);
        g.setFont(displayFont(16.0f));
        drawGlowIn(g, accent, presetText(), juce::Rectangle<float>(screen.getX() + 66, screen.getY() + 3, 220, 20).toNearestInt(), juce::Justification::centredLeft);
        g.setColour(dim.withAlpha(0.5f));
        g.fillRect(screen.getX() + 8, screen.getY() + 25, screen.getWidth() - 16, 1.0f);
    }

    void paintCells(juce::Graphics& g, juce::Colour dim) const {
        // The deep settings: pick one here, turn it with VALUE.
        for (int i = 0; i < (int)spec.settings.size(); ++i) {
            const auto cell = settingCell(i);
            const bool on = i == selected;
            if (on) { g.setColour(accent); g.fillRoundedRectangle(cell, 1.5f); }
            g.setColour(on ? juce::Colour(0xff06080b) : dim);
            g.setFont(displayFont(10.5f));
            g.drawText(spec.settings[(size_t)i].label, cell.withHeight(15).translated(0, 3), juce::Justification::centred);
            g.setFont(displayFont(19.0f));
            g.setColour(on ? juce::Colour(0xff06080b) : accent);
            g.drawText(juce::String(juce::roundToInt(raw(spec.settings[(size_t)i].key))), cell.withTrimmedTop(16), juce::Justification::centred);
        }
    }

    void paintDelay(juce::Graphics& g, juce::Rectangle<float> screen, juce::Colour dim) const {
        paintPresetLine(g, screen, dim);
        // The time, in both languages.
        const juce::String time = text("fx_dl_sync") == "SYNC" ? text("fx_dl_div") : juce::String("FREE");
        g.setColour(dim);
        g.setFont(displayFont(11.0f));
        g.drawText(time + "   " + juce::String(juce::roundToInt(processor.delaySecondsNow.load() * 1000.0f)) + " ms", delayTimeArea(screen), juce::Justification::centredRight);

        paintCells(g, dim);

        // The pattern on the tape: arrivals in multiples of TIME, worked through the heads and the loop.
        const auto map = screen.withTrimmedLeft(192).withTrimmedTop(32).reduced(8, 4);
        static constexpr int masks[] = { 1, 2, 4, 3, 5, 6, 7 };
        const int mask = masks[juce::jlimit(0, 6, juce::roundToInt(raw("fx_dl_heads")))], heads = (mask & 1) + ((mask >> 1) & 1) + ((mask >> 2) & 1);
        const float loop = raw("fx_dl_fb") * 0.01f / (float)heads;
        std::array<float, 25> level {}; std::array<int, 25> hops {};
        for (int h = 0; h < 3; ++h) if (mask & (1 << h)) { level[(size_t)h + 1] = 1.0f; hops[(size_t)h + 1] = 1; }
        for (int k = 1; k < 25; ++k) for (int h = 0; h < 3; ++h) if ((mask & (1 << h)) && k + h + 1 < 25) {
            level[(size_t)(k + h + 1)] += level[(size_t)k] * loop; hops[(size_t)(k + h + 1)] = hops[(size_t)k] + 1;
        }
        const bool pong = text("fx_dl_pp") == "ON";
        const float width = pong ? raw("fx_dl_width") * 0.01f : 0.0f;
        const float baseline = pong ? map.getCentreY() : map.getBottom();
        const float tall = pong ? map.getHeight() * 0.5f - 1.0f : map.getHeight() - 1.0f;
        g.setColour(dim.withAlpha(0.6f));
        g.fillRect(map.getX(), baseline, map.getWidth(), 1.0f);
        for (int k = 1; k < 25; ++k) {
            if (level[(size_t)k] < 0.02f) continue;
            const float x = map.getX() + map.getWidth() * (float)k / 25.0f, v = juce::jmin(1.0f, level[(size_t)k]);
            g.setColour(accent);
            if (pong) {
                // A hop goes left then right. WIDTH brings each one back towards the middle, which shows as a bit of both.
                const bool right = hops[(size_t)k] % 2 == 0;
                const float main = v * (0.5f + 0.5f * width), other = v * (0.5f - 0.5f * width);
                const float up = right ? other : main, down = right ? main : other;
                g.fillRect(x, baseline - up * tall, 3.5f, up * tall);
                g.fillRect(x, baseline + 1.0f, 3.5f, down * tall);
            } else {
                g.fillRect(x, baseline - v * tall, 3.5f, v * tall);
            }
        }
    }

    void paintReverb(juce::Graphics& g, juce::Rectangle<float> screen, juce::Colour dim) const {
        paintPresetLine(g, screen, dim);
        const bool spring = text("fx_rv_type") == "SPRING";
        const float decay = raw("fx_rv_decay") * 0.01f;
        const float seconds = spring ? springTailSeconds(decay) : reverbTailSeconds(decay);
        g.setColour(dim);
        g.setFont(displayFont(11.0f));
        g.drawText(text("fx_rv_type"), reverbTypeArea(screen), juce::Justification::centredRight);
        const auto left = reverbTailArea(screen);
        g.setFont(displayFont(34.0f));
        drawGlowIn(g, accent, juce::String(seconds, 1) + " s", left.withHeight(46).toNearestInt(), juce::Justification::centredLeft);
        g.setColour(dim);
        g.setFont(displayFont(11.0f));
        g.drawText("TAIL", left.withTrimmedTop(48).withHeight(16), juce::Justification::centredLeft);

        // The tail itself, against a 10 second scale. A spring also draws its echoes.
        const auto curve = screen.withTrimmedLeft(180).withTrimmedTop(32).reduced(8, 6);
        g.setColour(dim.withAlpha(0.6f));
        g.fillRect(curve.getX(), curve.getBottom(), curve.getWidth(), 1.0f);
        juce::Path tail;
        tail.startNewSubPath(curve.getBottomLeft());
        const float scale = 10.0f;
        const int points = 80;
        for (int i = 0; i <= points; ++i) {
            const float t = (float)i / (float)points, v = std::pow(10.0f, -3.0f * t * scale / juce::jmax(0.1f, seconds));
            const float ripple = spring ? 0.72f + 0.28f * std::cos(t * 60.0f) : 1.0f;
            tail.lineTo(curve.getX() + t * curve.getWidth(), curve.getBottom() - v * ripple * curve.getHeight());
        }
        tail.lineTo(curve.getBottomRight());
        tail.closeSubPath();
        g.setColour(accent.withAlpha(0.35f));
        g.fillPath(tail);
        g.setColour(accent);
        g.strokePath(tail, juce::PathStrokeType(1.3f));
    }

    // One cycle of the slope, drawn from its settings: a picture of the shape, not a measurement of the output.
    void paintTides(juce::Graphics& g, juce::Rectangle<float> screen, juce::Colour dim) const {
        paintPresetLine(g, screen, dim);
        const bool clocked = text("t_trig") == "CLOCK";
        g.setColour(dim);
        g.setFont(displayFont(11.0f));
        g.drawText(text("t_mode"), tidesHeaderArea(screen, 0), juce::Justification::centredRight);
        g.drawText(text("t_out"), tidesHeaderArea(screen, 1), juce::Justification::centredRight);
        g.drawText(clocked ? text("t_div") : text("t_trig"), tidesHeaderArea(screen, 2), juce::Justification::centredRight);
        paintCells(g, dim);

        const auto curve = screen.withTrimmedLeft(192).withTrimmedTop(32).reduced(8, 6);
        const float slope = juce::jlimit(0.02f, 0.98f, raw("t_slope") * 0.01f), bend = std::pow(2.0f, (raw("t_shape") * 0.01f - 0.5f) * 4.0f);
        const auto mode = text("t_mode");
        const float hold = mode == "AR" ? 0.4f : 0.0f;
        const float active = mode == "AD" ? 0.75f : 1.0f;
        const float rise = (active - hold) * slope, fall = (active - hold) * (1.0f - slope);
        juce::Path path;
        g.setColour(dim.withAlpha(0.6f));
        g.fillRect(curve.getX(), curve.getBottom(), curve.getWidth(), 1.0f);
        for (int i = 0; i <= 160; ++i) {
            const float t = (float)i / 160.0f;
            float v = 0.0f;
            if (t < rise) v = std::pow(t / rise, bend);
            else if (t < rise + hold) v = 1.0f;
            else if (t < active) v = std::pow(1.0f - (t - rise - hold) / fall, bend);
            const juce::Point<float> at(curve.getX() + t * curve.getWidth(), curve.getBottom() - v * curve.getHeight());
            if (i == 0) path.startNewSubPath(at); else path.lineTo(at);
        }
        g.setColour(accent);
        g.strokePath(path, juce::PathStrokeType(1.6f));
    }

    void paintSmall(juce::Graphics& g, juce::Rectangle<float> screen, juce::Colour dim) const {
        g.setColour(dim);
        g.setFont(displayFont(11.0f));
        g.drawText("PRESET", juce::Rectangle<float>(screen.getX() + 14, screen.getY() + 10, 80, 14), juce::Justification::centredLeft);
        g.setFont(displayFont(30.0f));
        drawGlowIn(g, accent, presetText(), juce::Rectangle<float>(screen.getX() + 14, screen.getY() + 30, screen.getWidth() - 28, 50).toNearestInt(), juce::Justification::centredLeft);
    }

    MillstoneProcessor& processor;
    const RackLcdSpec& spec;
    int slot;
    juce::Colour accent;
    Segment previous, next;
    ParamSlider value;
    LcdDrag lcdDrag;
    int selected = 0;
    bool binding = false;
    juce::String lastStamp;
};
