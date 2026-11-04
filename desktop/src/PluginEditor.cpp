#include "PluginEditor.h"
#include "BinaryData.h"
#include <map>

namespace {

constexpr int designW = 1480, designH = 864;      // the window scales from this
constexpr int woodW = 36;
constexpr int artScale = 2;                       // the static art is rendered at twice the design size, for Retina
constexpr int knobPad = 8;                        // room around a knob for its shadow, or the control's edge cuts it off
constexpr bool showWear = false;                  // scratches, gouges, scuffs and rubbed printing: off for now

const juce::Colour ink(0xff1b1a18), cream(0xffe9e4d4), capFill(0xffd9d3c2), capInk(0xff20201d), screenFill(0xff0d0b08),
                   amber(0xffffb347), amberDim(0xff6e4a1c), ledOn(0xffff4a2a), ledOff(0xff4a1d14);
constexpr juce::uint8 metalR = 168, metalG = 165, metalB = 154;

using Attrs = std::map<juce::String, juce::String>;

struct Item {
    juce::String kind;
    Attrs attrs;
    juce::String str(const char* k, const juce::String& fallback = {}) const {
        auto it = attrs.find(k);
        return it == attrs.end() ? fallback : it->second;
    }
    int num(const char* k, int fallback = 0) const {
        auto it = attrs.find(k);
        return it == attrs.end() ? fallback : it->second.getIntValue();
    }
};

std::vector<Item> parsePanel() {
    std::vector<Item> items;
    juce::StringArray lines;
    lines.addLines(juce::String::fromUTF8(BinaryData::panel_conf, BinaryData::panel_confSize));
    for (auto line : lines) {
        line = line.trim();
        if (line.isEmpty() || line.startsWithChar('#')) continue;
        juce::StringArray tokens;
        tokens.addTokens(line, " ", "\"");
        Item item;
        item.kind = tokens[0];
        for (int i = 1; i < tokens.size(); ++i)
            if (tokens[i].containsChar('='))
                item.attrs[tokens[i].upToFirstOccurrenceOf("=", false, false)] =
                    tokens[i].fromFirstOccurrenceOf("=", false, false).unquoted();
        items.push_back(std::move(item));
    }
    return items;
}

const std::vector<Item>& panelItems() {
    static const std::vector<Item> parsed = parsePanel();
    return parsed;
}

const param_t* findEngineParam(const juce::String& key) {
    for (int i = 0; i < NPARAMS; ++i)
        if (key == PARAMS[i].key) return &PARAMS[i];
    return nullptr;
}

juce::Font printFont(float size, bool bold = true, float tracking = 0.06f) {
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor(tracking);
}

juce::Font displayFont(float size) {
    return juce::Font(juce::FontOptions("Menlo", size, juce::Font::bold));
}

// A lit display: the text with a soft glow around it.
void drawGlowText(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, juce::Justification just) {
    g.setColour(amber.withAlpha(0.14f));
    for (auto d : {juce::Point<int>(-1, 0), juce::Point<int>(1, 0), juce::Point<int>(0, -1), juce::Point<int>(0, 1)})
        g.drawFittedText(text, r + d, just, 1, 0.8f);
    g.setColour(amber);
    g.drawFittedText(text, r, just, 1, 0.8f);
}

// ---- static panel art ----------------------------------------------------------------------------------------------
void paintBrushedMetal(juce::Image& img) {
    juce::Random rng(1977);
    juce::Image::BitmapData px(img, juce::Image::BitmapData::writeOnly);
    const float h = (float)img.getHeight();
    for (int y = 0; y < img.getHeight(); ++y) {
        const float row = (rng.nextFloat() - 0.5f) * 7.0f;
        const float light = 1.04f - 0.10f * (float)y / h;   // lit from above
        float streak = (rng.nextFloat() - 0.5f) * 6.0f;
        auto* line = px.getLinePointer(y);
        for (int x = 0; x < img.getWidth(); ++x) {
            streak = streak * 0.992f + (rng.nextFloat() - 0.5f) * 0.55f;   // long horizontal brush marks
            const float v = row + streak * 1.6f + (rng.nextFloat() - 0.5f) * 2.5f;
            const auto channel = [&](float base) { return (juce::uint8)juce::jlimit(0.0f, 255.0f, base * light + v); };
            reinterpret_cast<juce::PixelRGB*>(line + x * px.pixelStride)->setARGB(255, channel(metalR), channel(metalG), channel(metalB));
        }
    }
}

// Large soft stains, from a few rough noise layers stretched over the panel.
void paintPatina(juce::Graphics& g) {
    juce::Random rng(84);
    for (const auto& [cells, colour, strength] : std::vector<std::tuple<int, juce::Colour, float>>{
             {14, juce::Colour(0xff3a2a18), 0.14f}, {44, juce::Colour(0xff2a2319), 0.12f}, {120, juce::Colour(0xff4a3620), 0.10f}, {30, juce::Colour(0xffffffff), 0.05f}}) {
        const int rows = juce::jmax(2, cells * designH / designW);
        juce::Image stain(juce::Image::ARGB, cells, rows, true, juce::SoftwareImageType());
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < cells; ++x) {
                const float n = rng.nextFloat();
                stain.setPixelAt(x, y, colour.withAlpha(n * n * strength));
            }
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(stain, juce::Rectangle<float>(0, 0, (float)designW, (float)designH));
    }
}

void paintWood(juce::Image& img, int fromX, int toX, int seed) {
    juce::Random rng(seed);
    juce::Image::BitmapData px(img, juce::Image::BitmapData::writeOnly);
    std::vector<float> pores((size_t)(toX - fromX));
    for (auto& p : pores) p = rng.nextFloat();
    const float phase = rng.nextFloat() * 6.28f;
    for (int y = 0; y < img.getHeight(); ++y) {
        const float fy = (float)y;
        auto* line = px.getLinePointer(y);
        for (int x = fromX; x < toX; ++x) {
            const float t = (float)(x - fromX) / (float)(toX - fromX);
            auto& pore = pores[(size_t)(x - fromX)];
            pore += (rng.nextFloat() - 0.5f) * 0.03f;                         // pores run along the grain
            pore = juce::jlimit(0.0f, 1.0f, pore);
            const float warp = 2.6f * std::sin(fy * 0.0031f + phase) + 0.9f * std::sin(fy * 0.0127f + phase * 2.0f);
            const float ring = 0.5f + 0.5f * std::sin((t * 16.0f + warp) * 2.1f);
            const float fine = 0.5f + 0.5f * std::sin((t * 61.0f + warp * 3.0f));
            float v = 0.42f + 0.34f * ring + 0.10f * fine - (pore > 0.86f ? 0.22f : 0.0f);
            const float edge = std::abs(2.0f * t - 1.0f);
            v *= 1.0f - 0.55f * edge * edge * edge * edge;                     // rounded cheek
            reinterpret_cast<juce::PixelRGB*>(line + x * px.pixelStride)->setARGB(255,
                (juce::uint8)juce::jlimit(0.0f, 255.0f, 44.0f + v * 118.0f),
                (juce::uint8)juce::jlimit(0.0f, 255.0f, 24.0f + v * 68.0f),
                (juce::uint8)juce::jlimit(0.0f, 255.0f, 12.0f + v * 34.0f));
        }
    }
}

void paintScrew(juce::Graphics& g, float cx, float cy, float r, float angle) {
    const juce::Rectangle<float> head(cx - r, cy - r, 2 * r, 2 * r);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(head.expanded(1.2f).translated(0.5f, 1.0f));
    juce::ColourGradient steel(juce::Colour(0xffd6d3c9), cx - r * 0.5f, cy - r * 0.7f, juce::Colour(0xff55524a), cx + r * 0.6f, cy + r * 0.8f, false);
    g.setGradientFill(steel);
    g.fillEllipse(head);
    g.setColour(juce::Colour(0xff4d3214).withAlpha(0.35f));   // a little rust
    g.drawEllipse(head.reduced(0.4f), 0.9f);
    const juce::Point<float> c(cx, cy);
    const auto a = c.getPointOnCircumference(r * 0.86f, angle), b = c.getPointOnCircumference(r * 0.86f, angle + juce::MathConstants<float>::pi);
    g.setColour(juce::Colour(0xf0141310));
    g.drawLine({ a, b }, juce::jmax(1.1f, r * 0.26f));
}

void paintScratches(juce::Graphics& g, juce::Random& rng, int count, juce::Rectangle<float> area, float maxLength, float maxAlpha) {
    if (!showWear) return;
    for (int i = 0; i < count; ++i) {
        const juce::Point<float> a(area.getX() + rng.nextFloat() * area.getWidth(), area.getY() + rng.nextFloat() * area.getHeight());
        const float angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        const float length = 3.0f + rng.nextFloat() * rng.nextFloat() * maxLength;
        const auto b = a.getPointOnCircumference(length, angle);
        const auto mid = (a + b) * 0.5f + juce::Point<float>((rng.nextFloat() - 0.5f) * length * 0.10f, (rng.nextFloat() - 0.5f) * length * 0.10f);
        juce::Path p;
        p.startNewSubPath(a);
        p.quadraticTo(mid, b);
        const float width = 0.25f + rng.nextFloat() * 0.45f, alpha = maxAlpha * (0.3f + 0.7f * rng.nextFloat());
        g.setColour(juce::Colour(0xff2a241c).withAlpha(alpha * 0.8f));
        g.strokePath(p, juce::PathStrokeType(width), juce::AffineTransform::translation(0.4f, 0.5f));
        g.setColour(juce::Colour(0xfff6f3e8).withAlpha(alpha));
        g.strokePath(p, juce::PathStrokeType(width));
    }
}

// Printed scale, label and everything else that belongs to a control but never moves.
void paintKnobPrinting(juce::Graphics& g, const Item& item, float offsetX = 0, float offsetY = 0) {
    const float cx = (float)item.num("cx") + offsetX, cy = (float)item.num("cy") + offsetY, r = (float)item.num("r", 20);
    g.setColour(ink.withAlpha(0.82f));
    const int ticks = r > 30 ? 21 : 11;
    for (int i = 0; i < ticks; ++i) {
        const float angle = juce::degreesToRadians(-135.0f + 270.0f * (float)i / (float)(ticks - 1));
        const bool major = r <= 30 || i % 2 == 0;
        const juce::Point<float> c(cx, cy);
        g.drawLine({ c.getPointOnCircumference(r + 3.5f, angle), c.getPointOnCircumference(r + (major ? 8.5f : 6.0f), angle) }, major ? 1.1f : 0.8f);
    }
    g.setColour(ink);
    g.setFont(printFont(r >= 30 ? 13.0f : 11.0f));
    g.drawText(item.str("label"), juce::Rectangle<float>(cx - 62, cy + r + 9, 124, 15), juce::Justification::centred);
}

void paintFaderPrinting(juce::Graphics& g, const Item& item) {
    const float cx = (float)item.num("cx"), cy = (float)item.num("cy"), h = (float)item.num("h", 160);
    g.setColour(ink.withAlpha(0.8f));
    for (int i = 0; i <= 10; ++i) {
        const float y = cy - h * 0.5f + 12.0f + (h - 24.0f) * (float)i / 10.0f;
        const float w = i % 5 == 0 ? 7.0f : 4.0f;
        g.drawLine(cx - 15 - w, y, cx - 15, y, 0.9f);
        g.drawLine(cx + 15, y, cx + 15 + w, y, 0.9f);
    }
    g.setColour(ink);
    g.setFont(printFont(14.0f));
    g.drawText(item.str("label"), juce::Rectangle<float>(cx - 24, cy + h * 0.5f + 5, 48, 17), juce::Justification::centred);
    // the slot in the metal
    const juce::Rectangle<float> slot(cx - 4.0f, cy - h * 0.5f + 4, 8.0f, h - 8);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.fillRoundedRectangle(slot.expanded(1.1f).translated(0.4f, 0.8f), 3.0f);
    g.setColour(juce::Colour(0xff070706));
    g.fillRoundedRectangle(slot, 2.5f);
}

juce::Image makePanelArt() {
    juce::Image img(juce::Image::RGB, designW * artScale, designH * artScale, true, juce::SoftwareImageType());
    paintBrushedMetal(img);
    paintWood(img, 0, woodW * artScale, 7);
    paintWood(img, (designW - woodW) * artScale, designW * artScale, 19);
    juce::Graphics g(img);
    g.addTransform(juce::AffineTransform::scale((float)artScale));
    paintPatina(g);

    const float left = (float)woodW, right = (float)(designW - woodW);

    for (const auto& item : panelItems()) {
        if (item.kind == "frame") {
            const auto r = juce::Rectangle<float>((float)item.num("x"), (float)item.num("y"), (float)item.num("w"), (float)item.num("h"));
            g.setColour(juce::Colours::black.withAlpha(0.045f));   // a very slightly darker printed field
            g.fillRoundedRectangle(r, 3.0f);
            g.setColour(ink.withAlpha(0.75f));
            g.drawRoundedRectangle(r, 3.0f, 1.2f);
            const auto title = item.str("title");
            const auto font = printFont(11.5f, true, 0.20f);
            const float titleW = juce::GlyphArrangement::getStringWidth(font, title) + 22.0f;
            const auto strip = juce::Rectangle<float>(r.getX(), r.getY(), titleW, 19.0f);
            g.setColour(ink);
            g.fillRoundedRectangle(strip, 3.0f);
            g.setColour(cream);
            g.setFont(font);
            g.drawText(title, strip, juce::Justification::centred);
        } else if (item.kind == "knob") {
            if (item.str("when").isEmpty()) paintKnobPrinting(g, item);
        } else if (item.kind == "fader") {
            paintFaderPrinting(g, item);
        } else if (item.kind == "enum_v" || item.kind == "enum_h") {
            if (item.str("when").isNotEmpty()) continue;
            const bool vertical = item.kind == "enum_v";
            const auto* def = findEngineParam(item.str("key"));
            const int n = def != nullptr ? def->nopts : 1;
            const int sh = item.num("sh", 26);
            const int groupH = vertical ? n * sh + (n - 1) * 3 : sh;
            g.setColour(ink);
            g.setFont(printFont(10.5f));
            g.drawText(item.str("label"), juce::Rectangle<float>((float)item.num("cx") - 60, (float)item.num("cy") - (float)groupH * 0.5f - 15, 120, 13), juce::Justification::centred);
        } else if (item.kind == "toggle") {
            g.setColour(ink);
            g.setFont(printFont(10.5f));
            g.drawText(item.str("label"), juce::Rectangle<float>((float)item.num("cx") - 40, (float)item.num("cy") + 20, 80, 13), juce::Justification::centred);
        }
    }

    // Maker's name and model, as printed on the casing.
    g.setColour(ink);
    g.setFont(printFont(34.0f, true, 0.26f));
    g.drawText("MILLSTONE", juce::Rectangle<float>(left + 18, 14, 352, 38), juce::Justification::centredLeft);
    g.setFont(printFont(11.5f, true, 0.30f));
    g.drawText("HYBRID POLYPHONIC SYNTHESIZER", juce::Rectangle<float>(left + 20, 52, 352, 16), juce::Justification::centredLeft);

    // Wear: fine scuffing everywhere, heavier at the edges and where hands rest, and a few long gouges.
    juce::Random rng(2023);
    const juce::Rectangle<float> face(left, 0, right - left, (float)designH);
    paintScratches(g, rng, 1100, face, 34.0f, 0.16f);
    paintScratches(g, rng, 280, face, 110.0f, 0.30f);
    paintScratches(g, rng, 46, face, 340.0f, 0.50f);
    for (int i = 0; showWear && i < 7; ++i) {   // real gouges, where something heavy slid across it
        const juce::Point<float> a(left + rng.nextFloat() * (right - left), rng.nextFloat() * designH);
        const auto b = a.getPointOnCircumference(60.0f + rng.nextFloat() * 230.0f, juce::MathConstants<float>::halfPi + (rng.nextFloat() - 0.5f) * 1.5f);
        juce::Path gouge;
        gouge.startNewSubPath(a);
        gouge.quadraticTo((a + b) * 0.5f + juce::Point<float>(rng.nextFloat() * 26.0f - 13.0f, rng.nextFloat() * 26.0f - 13.0f), b);
        const float w = 0.6f + rng.nextFloat() * 0.7f;
        g.setColour(juce::Colour(0xff19140e).withAlpha(0.55f));
        g.strokePath(gouge, juce::PathStrokeType(w + 0.5f), juce::AffineTransform::translation(0.6f, 0.9f));
        g.setColour(juce::Colour(0xfffbf9f0).withAlpha(0.46f));
        g.strokePath(gouge, juce::PathStrokeType(w));
    }
    paintScratches(g, rng, 200, juce::Rectangle<float>(left, 800, right - left, 64), 60.0f, 0.28f);   // the front edge
    paintScratches(g, rng, 90, juce::Rectangle<float>(left, 0, 70, (float)designH), 40.0f, 0.24f);
    paintScratches(g, rng, 90, juce::Rectangle<float>(right - 70, 0, 70, (float)designH), 40.0f, 0.24f);
    for (const auto& item : panelItems()) {                                                            // a ring worn around well-used knobs
        if (!showWear || item.kind != "knob" || item.num("r") < 26) continue;
        const float r = (float)item.num("r") + 11.0f;
        for (int i = 0; i < 26; ++i) {
            const float start = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            juce::Path arc;
            const float radius = r + rng.nextFloat() * 5.0f;
            arc.addCentredArc((float)item.num("cx"), (float)item.num("cy"), radius, radius, 0, start, start + 0.2f + rng.nextFloat() * 0.8f, true);
            g.setColour(juce::Colour(0xfff6f3e8).withAlpha(0.05f + rng.nextFloat() * 0.13f));
            g.strokePath(arc, juce::PathStrokeType(0.5f));
        }
    }
    // Rubbed-off printing: small flecks of bare metal.
    const juce::Colour bare(metalR, metalG, metalB);
    for (int i = 0; showWear && i < 9000; ++i) {
        const float d = 0.25f + rng.nextFloat() * rng.nextFloat() * 1.3f;
        g.setColour(bare.withAlpha(0.25f + rng.nextFloat() * 0.5f));
        g.fillEllipse(left + rng.nextFloat() * (right - left), rng.nextFloat() * designH, d, d * (0.5f + rng.nextFloat()));
    }
    // Dings: bright nicks with a shadow, mostly near corners and the front edge.
    for (int i = 0; showWear && i < 46; ++i) {
        const bool edge = rng.nextBool();
        const float x = left + rng.nextFloat() * (right - left);
        const float y = edge ? (rng.nextBool() ? rng.nextFloat() * 10.0f : designH - rng.nextFloat() * 12.0f) : rng.nextFloat() * designH;
        const float w = 1.0f + rng.nextFloat() * 3.5f, h = 0.6f + rng.nextFloat() * 1.8f;
        g.setColour(juce::Colours::black.withAlpha(0.32f));
        g.fillEllipse(x + 0.5f, y + 0.7f, w, h);
        g.setColour(juce::Colour(0xfff1eee3).withAlpha(0.55f));
        g.fillEllipse(x, y, w, h);
    }

    // The display's recess.
    const juce::Rectangle<float> bezel(440, 13, 610, 62);
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.fillRoundedRectangle(bezel.expanded(2.0f).translated(0.5f, 1.2f), 5.0f);
    juce::ColourGradient rim(juce::Colour(0xff2b2a27), 0, bezel.getY(), juce::Colour(0xff5b5951), 0, bezel.getBottom(), false);
    g.setGradientFill(rim);
    g.fillRoundedRectangle(bezel.expanded(2.0f), 5.0f);
    g.setColour(screenFill);
    g.fillRoundedRectangle(bezel, 3.0f);

    // The model plate: a small riveted badge, itself a bit knocked about.
    const juce::Rectangle<float> plate(1262, 22, 160, 44);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(plate.translated(0.8f, 1.6f), 2.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2b2925), 0, plate.getY(), juce::Colour(0xff14130f), 0, plate.getBottom(), false));
    g.fillRoundedRectangle(plate, 2.0f);
    g.setColour(cream.withAlpha(0.28f));
    g.drawRoundedRectangle(plate.reduced(2.5f), 1.5f, 0.6f);
    g.setColour(cream);
    g.setFont(printFont(13.0f, true, 0.18f));
    g.drawText("3 OSC  8 VOICE", plate.withHeight(27).translated(0, 3), juce::Justification::centred);
    g.setFont(printFont(8.5f, true, 0.24f));
    g.drawText("24 dB LADDER FILTER", plate.withTrimmedTop(25), juce::Justification::centred);
    { juce::Random plateWear(5); paintScratches(g, plateWear, 14, plate, 34.0f, 0.34f); }
    for (const auto& p : std::vector<juce::Point<float>>{ {plate.getX() - 7, plate.getCentreY()}, {plate.getRight() + 7, plate.getCentreY()} })
        paintScrew(g, p.x, p.y, 3.4f, p.x * 0.7f);

    // Casing bevels against the top and bottom.
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.28f), 0, 0, juce::Colours::transparentWhite, 0, 7, false));
    g.fillRect(face.withHeight(7));
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, 0, designH - 10.0f, juce::Colours::black.withAlpha(0.42f), 0, (float)designH, false));
    g.fillRect(face.withTop(designH - 10.0f));

    juce::Random turns(11);
    for (const auto& p : std::vector<juce::Point<float>>{ {left + 12, 12}, {right - 12, 12}, {left + 12, designH - 12.0f}, {right - 12, designH - 12.0f}, {740, designH - 12.0f}, {right - 12, 44} })
        paintScrew(g, p.x, p.y, 5.4f, turns.nextFloat() * 3.14f);

    // The wood itself went in first; here it gets its shadow, a worn edge and rubbed corners.
    for (const auto& cheek : { juce::Rectangle<float>(0, 0, left, (float)designH), juce::Rectangle<float>(right, 0, left, (float)designH) }) {
        paintScratches(g, rng, 90, cheek, 46.0f, 0.15f);
        for (const float y : { 0.0f, designH - 56.0f }) {   // corners rubbed pale
            if (!showWear) break;
            juce::ColourGradient rub(juce::Colour(0x38e0b98a), cheek.getCentreX(), y + 28, juce::Colour(0x00e0b98a), cheek.getCentreX(), y + (y == 0.0f ? 66.0f : -10.0f), true);
            g.setGradientFill(rub);
            g.fillRect(cheek.getX(), y - 10, cheek.getWidth(), 76.0f);
        }
    }
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.60f), left, 0, juce::Colours::transparentBlack, left + 7, 0, false));
    g.fillRect(left, 0.0f, 7.0f, (float)designH);
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.60f), right, 0, juce::Colours::transparentBlack, right - 7, 0, false));
    g.fillRect(right - 7, 0.0f, 7.0f, (float)designH);
    return img;
}

// ---- controls -------------------------------------------------------------------------------------------------------
// The display shows whichever control was last touched or pointed at.
using Readout = std::function<void(juce::Slider*, juce::RangedAudioParameter*)>;

juce::String sliderText(const juce::Slider& s, bool isInt) {
    return isInt ? juce::String(juce::roundToInt(s.getValue())) : juce::String(s.getValue(), 1);
}

class ParamSlider : public juce::Slider {
public:
    std::function<void()> onTouched;
    void mouseEnter(const juce::MouseEvent& e) override { juce::Slider::mouseEnter(e); if (onTouched) onTouched(); }
    void valueChanged() override { if (isMouseOverOrDragging() && onTouched) onTouched(); }
    bool hitTest(int x, int y) override {
        if (getSliderStyle() != juce::Slider::RotaryVerticalDrag) return juce::Slider::hitTest(x, y);
        // The bounds include room for the shadow; only the knob itself should take the mouse.
        return getLocalBounds().toFloat().getCentre().getDistanceFrom({ (float)x, (float)y }) <= (float)getWidth() * 0.5f - (float)knobPad + 2.0f;
    }
};

class Control : public juce::Component {
public:
    juce::String whenKey, whenValue;   // panel.conf's `when=key:VALUE`
};

class SliderControl : public Control {
public:
    SliderControl(const Item& item, bool fader, juce::AudioProcessorValueTreeState& apvts, Readout& readout) {
        const auto key = item.str("key");
        setInterceptsMouseClicks(false, true);
        slider.setComponentID(key);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        if (fader) {
            const int h = item.num("h", 160);
            setBounds(item.num("cx") - 16, item.num("cy") - h / 2 - 4, 32, h + 8);   // the cap's shadow needs room at both ends
            slider.setSliderStyle(juce::Slider::LinearVertical);
            slider.setSliderSnapsToMousePosition(false);
            slider.setBounds(getLocalBounds());
        } else {
            const int r = item.num("r", 20);
            const int margin = item.str("when").isNotEmpty() ? 62 : knobPad;   // a conditional knob brings its own printing
            setBounds(item.num("cx") - r - margin, item.num("cy") - r - margin, 2 * (r + margin), 2 * (r + margin));
            slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            slider.setMouseDragSensitivity(220);
            slider.setBounds(margin - knobPad, margin - knobPad, 2 * (r + knobPad), 2 * (r + knobPad));
            if (item.str("when").isNotEmpty()) printing = std::make_unique<Item>(item);
        }
        addAndMakeVisible(slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, key, slider);
        auto* param = apvts.getParameter(key);
        if (param != nullptr) slider.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
        slider.onTouched = [this, &readout, param] { readout(&slider, param); };
    }

    void paint(juce::Graphics& g) override {
        if (printing != nullptr) paintKnobPrinting(g, *printing, -(float)getX(), -(float)getY());
    }

private:
    ParamSlider slider;
    std::unique_ptr<Item> printing;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// A push button with a lamp: the lit one is the option in use.
class Segment : public juce::Button {
public:
    explicit Segment(const juce::String& text) : juce::Button(text) {}
    void paintButton(juce::Graphics& g, bool over, bool down) override {
        const bool on = getToggleState();
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(r.translated(0, 1.2f), 2.5f);
        const float press = (on || down) ? 0.9f : 0.0f;
        const auto cap = r.withTrimmedBottom(1.2f - press * 0.6f).translated(0, press * 0.6f);
        auto top = capFill.brighter(over ? 0.1f : 0.04f), bottom = capFill.darker(0.28f);
        if (on) { top = top.darker(0.10f); bottom = bottom.darker(0.05f); }
        g.setGradientFill(juce::ColourGradient(top, 0, cap.getY(), bottom, 0, cap.getBottom(), false));
        g.fillRoundedRectangle(cap, 2.5f);
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.drawLine(cap.getX() + 2, cap.getY() + 0.6f, cap.getRight() - 2, cap.getY() + 0.6f, 0.8f);
        // lamp
        const auto lamp = juce::Rectangle<float>(5.0f, 5.0f).withCentre({ cap.getX() + 8.5f, cap.getCentreY() });
        if (on) { g.setColour(ledOn.withAlpha(0.28f)); g.fillEllipse(lamp.expanded(2.5f)); }
        g.setColour(on ? ledOn : ledOff);
        g.fillEllipse(lamp);
        g.setColour(capInk);
        g.setFont(printFont(10.5f, true, 0.04f));
        g.drawFittedText(getButtonText(), cap.withTrimmedLeft(13).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
};

class EnumControl : public Control {
public:
    EnumControl(const Item& item, bool vertical, juce::AudioProcessorValueTreeState& apvts) {
        const auto key = item.str("key");
        auto* param = apvts.getParameter(key);
        jassert(param != nullptr);
        const auto choices = param->getAllValueStrings();
        const int n = choices.size(), gap = 3;
        const int segW = item.num("sw", 60), segH = item.num("sh", 26);
        const int groupW = vertical ? segW : n * segW + (n - 1) * gap;
        const int groupH = vertical ? n * segH + (n - 1) * gap : segH;
        ownLabel = item.str("when").isNotEmpty();
        title = item.str("label");
        const int labelH = ownLabel ? 17 : 0;
        setBounds(item.num("cx") - groupW / 2 - (ownLabel ? 30 : 0), item.num("cy") - groupH / 2 - labelH, groupW + (ownLabel ? 60 : 0), groupH + labelH);
        setInterceptsMouseClicks(false, true);
        attachment = std::make_unique<juce::ParameterAttachment>(*param, [this](float v) {
            for (int i = 0; i < segments.size(); ++i) segments[i]->setToggleState(i == juce::roundToInt(v), juce::dontSendNotification);
        });
        for (int i = 0; i < n; ++i) {
            auto* b = segments.add(new Segment(choices[i]));
            b->setComponentID(key + ":" + juce::String(i));
            b->setBounds((ownLabel ? 30 : 0) + (vertical ? 0 : i * (segW + gap)), labelH + (vertical ? i * (segH + gap) : 0), segW, segH);
            b->onClick = [this, i] { attachment->setValueAsCompleteGesture((float)i); };
            addAndMakeVisible(b);
        }
        attachment->sendInitialUpdate();
    }
    void paint(juce::Graphics& g) override {
        if (!ownLabel) return;
        g.setColour(ink);
        g.setFont(printFont(10.5f));
        g.drawText(title, 0, 0, getWidth(), 13, juce::Justification::centred);
    }
private:
    juce::String title;
    bool ownLabel = false;
    juce::OwnedArray<Segment> segments;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// A metal toggle switch with a lamp beside it.
class ToggleControl : public Control {
    struct Lever : juce::Button {
        Lever() : juce::Button("switch") {}
        void paintButton(juce::Graphics& g, bool, bool) override {
            const bool on = getToggleState();
            const float cx = 15.0f, cy = (float)getHeight() * 0.5f;
            juce::ColourGradient nut(juce::Colour(0xffdedbd1), cx - 6, cy - 7, juce::Colour(0xff4d4b45), cx + 6, cy + 7, false);
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.fillEllipse(cx - 10.5f, cy - 9.5f, 21, 21);
            g.setGradientFill(nut);
            g.fillEllipse(cx - 10, cy - 10, 20, 20);
            g.setColour(juce::Colour(0xff111110));
            g.fillEllipse(cx - 4.5f, cy - 4.5f, 9, 9);
            const float dir = on ? -1.0f : 1.0f, tip = cy + dir * 13.0f;
            juce::Path bat;
            bat.startNewSubPath(cx - 2.6f, cy);
            bat.lineTo(cx + 2.6f, cy);
            bat.lineTo(cx + 3.9f, tip);
            bat.lineTo(cx - 3.9f, tip);
            bat.closeSubPath();
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff1eee6), cx - 4, 0, juce::Colour(0xff6c6a63), cx + 4, 0, false));
            g.fillPath(bat);
            g.fillEllipse(cx - 3.9f, tip - 2.2f, 7.8f, 4.4f);
            const auto lamp = juce::Rectangle<float>(8.0f, 8.0f).withCentre({ 34.0f, cy });
            g.setColour(juce::Colours::black.withAlpha(0.65f));
            g.fillEllipse(lamp.expanded(1.5f));
            if (on) { g.setColour(ledOn.withAlpha(0.3f)); g.fillEllipse(lamp.expanded(3.5f)); }
            g.setColour(on ? ledOn : ledOff);
            g.fillEllipse(lamp);
            g.setColour(juce::Colours::white.withAlpha(on ? 0.65f : 0.2f));
            g.fillEllipse(lamp.getX() + 1.8f, lamp.getY() + 1.4f, 2.4f, 1.8f);
        }
    };
public:
    ToggleControl(const Item& item, juce::AudioProcessorValueTreeState& apvts) {
        const auto key = item.str("key");
        auto* param = apvts.getParameter(key);
        jassert(param != nullptr);
        setBounds(item.num("cx") - 15, item.num("cy") - 18, 44, 36);
        setInterceptsMouseClicks(false, true);
        lever.setComponentID(key);
        lever.setBounds(getLocalBounds());
        lever.onClick = [this] { attachment->setValueAsCompleteGesture(lever.getToggleState() ? 0.0f : 1.0f); };
        addAndMakeVisible(lever);
        attachment = std::make_unique<juce::ParameterAttachment>(*param, [this](float v) {
            lever.setToggleState(v >= 0.5f, juce::dontSendNotification);
            lever.repaint();
        });
        attachment->sendInitialUpdate();
    }
private:
    Lever lever;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// A small lit window: shows the current option and opens a menu.
class PopupControl : public Control {
public:
    PopupControl(const Item& item, juce::AudioProcessorValueTreeState& apvts, juce::LookAndFeel& menuLook) : look(menuLook) {
        const auto key = item.str("key");
        param = apvts.getParameter(key);
        jassert(param != nullptr);
        setComponentID(key);
        cols = juce::jmax(1, item.num("cols", 1));
        title = item.str("label");
        titleH = title.isNotEmpty() ? 15 : 0;
        const int w = item.num("w", 200), h = item.num("h", 38);
        setBounds(item.num("cx") - w / 2, item.num("cy") - h / 2 - titleH, w, h + titleH);
        attachment = std::make_unique<juce::ParameterAttachment>(*param, [this](float v) { current = juce::roundToInt(v); repaint(); });
        attachment->sendInitialUpdate();
    }
    void paint(juce::Graphics& g) override {
        if (titleH > 0) { g.setColour(ink); g.setFont(printFont(10.5f)); g.drawText(title, 0, 0, getWidth(), 13, juce::Justification::centred); }
        auto box = getLocalBounds().withTrimmedTop(titleH).toFloat().reduced(1.5f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.fillRoundedRectangle(box.expanded(1.5f).translated(0.3f, 0.8f), 3.5f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff26251f), 0, box.getY(), juce::Colour(0xff55534b), 0, box.getBottom(), false));
        g.fillRoundedRectangle(box.expanded(1.5f), 3.5f);
        g.setColour(screenFill);
        g.fillRoundedRectangle(box, 2.5f);
        auto arrow = box.removeFromRight(22.0f);
        juce::Path tri;
        tri.addTriangle(arrow.getCentreX() - 5, arrow.getCentreY() - 3, arrow.getCentreX() + 5, arrow.getCentreY() - 3, arrow.getCentreX(), arrow.getCentreY() + 4);
        g.setColour(amberDim);
        g.fillPath(tri);
        g.setFont(displayFont(16.0f));
        drawGlowText(g, param->getAllValueStrings()[current], box.withTrimmedLeft(9).toNearestInt(), juce::Justification::centredLeft);
        const auto glass = getLocalBounds().withTrimmedTop(titleH).toFloat().reduced(1.5f);
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.11f), 0, glass.getY(), juce::Colours::transparentWhite, 0, glass.getCentreY(), false));
        g.fillRoundedRectangle(glass.withHeight(glass.getHeight() * 0.5f), 2.5f);
    }
    void mouseDown(const juce::MouseEvent&) override {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&look);
        const auto choices = param->getAllValueStrings();
        for (int i = 0; i < choices.size(); ++i) menu.addItem(i + 1, choices[i], true, i == current);
        juce::Component::SafePointer<PopupControl> self(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMinimumNumColumns(cols).withMaximumNumColumns(cols).withStandardItemHeight(26),
                           [self](int picked) { if (self != nullptr && picked > 0) self->attachment->setValueAsCompleteGesture((float)(picked - 1)); });
    }
private:
    juce::LookAndFeel& look;
    juce::RangedAudioParameter* param = nullptr;
    juce::String title;
    int cols = 1, current = 0, titleH = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// The - / + model buttons, small rubber caps. Same behaviour as the MPC build: wraps around the list.
class StepControl : public Control {
public:
    StepControl(const Item& item, juce::AudioProcessorValueTreeState& apvts) : button(item.str("label")) {
        const auto* ep = findEngineParam(item.str("key"));
        jassert(ep != nullptr && ep->step_target >= 0);
        target = apvts.getParameter(PARAMS[ep->step_target].key);
        delta = juce::roundToInt(ep->step_delta);
        setBounds(item.num("cx") - 16, item.num("cy") - 16, 32, 32);
        button.setComponentID(item.str("key"));
        button.setBounds(getLocalBounds());
        button.onClick = [this] {
            const int count = target->getNumSteps();
            const int index = juce::roundToInt(target->convertFrom0to1(target->getValue()));
            const int next = ((index + delta) % count + count) % count;
            target->beginChangeGesture();
            target->setValueNotifyingHost(target->convertTo0to1((float)next));
            target->endChangeGesture();
        };
        addAndMakeVisible(button);
    }
private:
    struct Cap : juce::Button {
        explicit Cap(const juce::String& text) : juce::Button(text) {}
        void paintButton(juce::Graphics& g, bool over, bool down) override {
            auto r = getLocalBounds().toFloat().reduced(2.0f);
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.fillRoundedRectangle(r.translated(0, 1.5f), 4.0f);
            if (down) r = r.translated(0, 1.0f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(over ? 0xff46453f : 0xff3a3934), 0, r.getY(), juce::Colour(0xff15140f), 0, r.getBottom(), false));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(juce::Colours::white.withAlpha(0.2f));
            g.drawLine(r.getX() + 3, r.getY() + 0.7f, r.getRight() - 3, r.getY() + 0.7f, 0.8f);
            g.setColour(cream);
            g.setFont(printFont(18.0f));
            g.drawText(getButtonText(), r, juce::Justification::centred);
        }
    };
    Cap button;
    juce::RangedAudioParameter* target = nullptr;
    int delta = 0;
};

} // namespace

struct MillstoneEditor::LookAndFeel : juce::LookAndFeel_V4 {
    LookAndFeel() {
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff181612));
        setColour(juce::PopupMenu::textColourId, amber);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, amber);
        setColour(juce::PopupMenu::highlightedTextColourId, screenFill);
        setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::ComboBox::textColourId, amber);
        setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::ComboBox::arrowColourId, amberDim);
        setColour(juce::TextButton::textColourOffId, cream);
    }

    // The knob body never changes with its value, so it is drawn once per size.
    const juce::Image& knobBody(int r) {
        auto& image = bodies[r];
        if (image.isNull()) {
            const int pad = knobPad, size = 2 * (r + pad);
            image = juce::Image(juce::Image::ARGB, size * artScale, size * artScale, true, juce::SoftwareImageType());
            juce::Graphics g(image);
            g.addTransform(juce::AffineTransform::scale((float)artScale));
            const juce::Point<float> c((float)(r + pad), (float)(r + pad));
            const float R = (float)r;
            const auto circle = [&](float radius) { return juce::Rectangle<float>(2 * radius, 2 * radius).withCentre(c); };
            for (int i = 6; i > 0; --i) {   // contact shadow, falling down and right
                g.setColour(juce::Colours::black.withAlpha(0.075f));
                g.fillEllipse(circle(R + (float)i * 0.8f).translated(1.0f, 2.2f));
            }
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4945), c.x - R * 0.6f, c.y - R, juce::Colour(0xff060605), c.x + R * 0.5f, c.y + R, false));
            g.fillEllipse(circle(R));
            // fluted grip
            const int flutes = juce::jmax(20, r * 2 / 1);
            for (int i = 0; i < flutes; ++i) {
                const float a = juce::MathConstants<float>::twoPi * (float)i / (float)flutes;
                const float lit = 0.5f + 0.5f * std::cos(a + 0.7f);
                g.setColour(juce::Colours::white.withAlpha(0.04f + 0.20f * lit));
                g.drawLine({ c.getPointOnCircumference(R * 0.76f, a), c.getPointOnCircumference(R * 0.985f, a) }, 0.9f);
                g.setColour(juce::Colours::black.withAlpha(0.7f));
                const float b = a + juce::MathConstants<float>::twoPi * 0.5f / (float)flutes;
                g.drawLine({ c.getPointOnCircumference(R * 0.76f, b), c.getPointOnCircumference(R * 0.985f, b) }, 1.0f);
            }
            // spun metal cap: light rolls around it in a wedge pattern
            const float capR = R * 0.70f;
            for (int i = 0; i < 180; ++i) {
                const float a0 = juce::MathConstants<float>::twoPi * (float)i / 180.0f, a1 = a0 + juce::MathConstants<float>::twoPi / 180.0f + 0.004f;
                const float v = 0.56f + 0.30f * std::cos(2.0f * (a0 - 0.65f)) + 0.06f * std::cos(6.0f * a0 + 1.0f);
                juce::Path wedge;
                wedge.addPieSegment(circle(capR), a0, a1, 0.0f);
                g.setColour(juce::Colour::fromFloatRGBA(v, v * 0.985f, v * 0.94f, 1.0f));
                g.fillPath(wedge);
            }
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.drawEllipse(circle(capR), 1.0f);
            g.setColour(juce::Colours::white.withAlpha(0.22f));
            g.drawEllipse(circle(capR - 1.0f), 0.6f);
            // it has been used: scuffs on the cap
            juce::Random rng(r * 37);
            g.reduceClipRegion([&] { juce::Path p; p.addEllipse(circle(capR)); return p; }());
            paintScratches(g, rng, 7, circle(capR), R * 0.8f, 0.30f);
        }
        return image;
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider&) override {
        const int pad = knobPad, r = juce::jmin(w, h) / 2 - pad;
        const juce::Point<float> c((float)x + (float)w * 0.5f, (float)y + (float)h * 0.5f);
        g.drawImage(knobBody(r), juce::Rectangle<float>((float)(2 * (r + pad)), (float)(2 * (r + pad))).withCentre(c));
        const float angle = juce::degreesToRadians(-135.0f + pos * 270.0f), R = (float)r;
        const float thick = juce::jmax(1.6f, R * 0.085f);
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.drawLine({ c.getPointOnCircumference(R * 0.30f, angle), c.getPointOnCircumference(R * 0.985f, angle) }, thick + 1.3f);
        g.setColour(cream);
        g.drawLine({ c.getPointOnCircumference(R * 0.30f, angle), c.getPointOnCircumference(R * 0.985f, angle) }, thick);
    }

    int getSliderThumbRadius(juce::Slider&) override { return 16; }   // track inset: 4 of margin plus the 12 the printed scale expects

    void drawLinearSlider(juce::Graphics& g, int x, int, int w, int, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override {
        const float cx = (float)x + (float)w * 0.5f, cap = 28.0f, tall = 22.0f;
        const auto r = juce::Rectangle<float>(cap, tall).withCentre({ cx, pos });
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(r.translated(1.0f, 2.8f).expanded(0.8f), 3.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff5a5852), 0, r.getY(), juce::Colour(0xff0e0d0b), 0, r.getBottom(), false));
        g.fillRoundedRectangle(r, 2.5f);
        for (int i = 1; i < 5; ++i) {   // grip ridges, either side of the line
            const float d = (float)i * 2.2f;
            for (const float yy : { pos - d - 1.0f, pos + d + 1.0f }) {
                g.setColour(juce::Colours::black.withAlpha(0.7f));
                g.drawLine(r.getX() + 2, yy, r.getRight() - 2, yy, 0.9f);
                g.setColour(juce::Colours::white.withAlpha(0.14f));
                g.drawLine(r.getX() + 2, yy + 0.8f, r.getRight() - 2, yy + 0.8f, 0.5f);
            }
        }
        g.setColour(cream);
        g.fillRect(r.getX() + 1.5f, pos - 1.0f, cap - 3.0f, 2.0f);
    }

    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override {
        juce::Path tri;
        const float cx = (float)w - 11.0f, cy = (float)h * 0.5f;
        tri.addTriangle(cx - 4.5f, cy - 2.5f, cx + 4.5f, cy - 2.5f, cx, cy + 3.5f);
        g.setColour(amberDim);
        g.fillPath(tri);
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return displayFont(15.0f); }
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override {
        label.setBounds(2, 0, box.getWidth() - 22, box.getHeight());
        label.setFont(getComboBoxFont(box));
    }
    juce::Font getPopupMenuFont() override { return displayFont(14.0f); }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override {
        auto r = b.getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(r.translated(0, 1.4f), 3.0f);
        if (down) r = r.translated(0, 1.0f);
        const float alpha = b.isEnabled() ? 1.0f : 0.55f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(over ? 0xff46453f : 0xff3a3934).withMultipliedAlpha(alpha), 0, r.getY(), juce::Colour(0xff15140f).withMultipliedAlpha(alpha), 0, r.getBottom(), false));
        g.fillRoundedRectangle(r, 3.0f);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return printFont(11.5f); }

private:
    std::map<int, juce::Image> bodies;
};

// The display: category and preset selectors, and a readout of the control last touched. Double-click the value to type one.
struct MillstoneEditor::PresetBar : juce::Component {
    explicit PresetBar(MillstoneProcessor& p) : processor(p) {
        for (auto* b : {&previous, &next, &save, &remove}) addAndMakeVisible(*b);
        category.setComponentID("preset_category");
        presetList.setComponentID("preset_list");
        previous.setComponentID("preset_prev");
        next.setComponentID("preset_next");
        presetList.setTextWhenNothingSelected("Custom");
        addAndMakeVisible(category);
        addAndMakeVisible(presetList);
        readoutName.setFont(displayFont(13.0f));
        readoutName.setColour(juce::Label::textColourId, amberDim.brighter(0.5f));
        readoutName.setJustificationType(juce::Justification::centredLeft);
        readoutName.setInterceptsMouseClicks(false, false);
        readoutName.setText("MILLSTONE READY", juce::dontSendNotification);
        addAndMakeVisible(readoutName);
        readoutValue.setFont(displayFont(16.0f));
        readoutValue.setColour(juce::Label::textColourId, amber);
        readoutValue.setColour(juce::Label::backgroundWhenEditingColourId, amberDim);
        readoutValue.setColour(juce::Label::textWhenEditingColourId, screenFill);
        readoutValue.setJustificationType(juce::Justification::centredRight);
        readoutValue.setEditable(false, true);
        readoutValue.onTextChange = [this] {
            if (shown != nullptr) { shown->setValue(readoutValue.getText().getDoubleValue(), juce::sendNotificationSync); showValue(); }
        };
        addAndMakeVisible(readoutValue);
        category.onChange = [this] { categoryChanged(); };
        presetList.onChange = [this] { if (presetList.getSelectedId() > 0) load(entries[(size_t)presetList.getSelectedId() - 1]); };
        previous.onClick = [this] { step(-1); };
        next.onClick = [this] { step(1); };
        save.onClick = [this] { askToSave(); };
        remove.onClick = [this] { askToDelete(); };
        rebuild();
        sync(true);
    }

    void resized() override {
        // Inside the display; the buttons sit on the metal to its right.
        category.setBounds(8, 2, 150, 28);
        presetList.setBounds(166, 2, 300, 28);
        readoutName.setBounds(8, 32, 330, 24);
        readoutValue.setBounds(336, 32, 266, 24);
        previous.setBounds(626, 4, 34, 26);
        next.setBounds(664, 4, 34, 26);
        save.setBounds(626, 36, 72, 26);
        remove.setBounds(706, 36, 72, 26);
    }

    void paint(juce::Graphics& g) override {   // the divider in the display
        g.setColour(amberDim.withAlpha(0.45f));
        g.fillRect(8.0f, 31.0f, 594.0f, 1.0f);
    }

    void paintOverChildren(juce::Graphics& g) override {   // glass over the display
        const juce::Rectangle<float> glass(0, 0, 610, 62);
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.09f), 0, 0, juce::Colours::transparentWhite, 0, 26, false));
        g.fillRoundedRectangle(glass.withHeight(26), 3.0f);
    }

    void show(juce::Slider* slider, juce::RangedAudioParameter* param) {
        shown = slider;
        shownInt = false;
        if (param != nullptr) {
            readoutName.setText(param->getName(32), juce::dontSendNotification);
            if (const auto* def = findEngineParam(param->getParameterID())) shownInt = def->int_display != 0;
        }
        showValue();
    }

    // Follows the host by comparing the name, not the selector: a filter that hides the current preset leaves the
    // selector empty on purpose, and treating that as out of sync would keep resetting the filter.
    void sync(bool force = false) {
        const auto name = processor.getCurrentPresetName();
        if (!force && name == lastName) return;
        lastName = name;
        const auto* current = find(name);
        if (current != nullptr && category.getSelectedId() > 1 && category.getText() != current->category)
            category.setSelectedId(1, juce::dontSendNotification);
        fillList();
    }

private:
    struct Entry { juce::String name, category; int program; };   // program < 0: a user preset

    void showValue() { if (shown != nullptr) readoutValue.setText(sliderText(*shown, shownInt), juce::dontSendNotification); }

    const Entry* find(const juce::String& name) const {
        for (const auto& e : entries) if (e.name == name) return &e;
        return nullptr;
    }

    void rebuild() {
        const auto shownCategory = category.getSelectedId() > 1 ? category.getText() : juce::String();
        entries.clear();
        const auto& library = MillstoneProcessor::factoryPresets();
        for (int i = 0; i < (int)library.size(); ++i)
            entries.push_back({ library[(size_t)i].name, library[(size_t)i].category, i });
        for (const auto& name : processor.userPresetNames())
            entries.push_back({ name, "User", -1 });
        category.clear(juce::dontSendNotification);
        category.addItem("All", 1);
        juce::StringArray seen;
        for (const auto& e : entries)
            if (seen.addIfNotAlreadyThere(e.category))
                category.addItem(e.category, seen.size() + 1);
        const int restored = seen.indexOf(shownCategory);
        category.setSelectedId(restored >= 0 ? restored + 2 : 1, juce::dontSendNotification);
    }

    void fillList() {
        presetList.clear(juce::dontSendNotification);
        const bool all = category.getSelectedId() <= 1;
        const auto shownCategory = category.getText();
        for (int i = 0; i < (int)entries.size(); ++i)
            if (all || entries[(size_t)i].category == shownCategory)
                presetList.addItem(entries[(size_t)i].name, i + 1);
        const auto name = processor.getCurrentPresetName();
        for (int i = 0; i < (int)entries.size(); ++i)
            if (entries[(size_t)i].name == name && (all || entries[(size_t)i].category == shownCategory))
                presetList.setSelectedId(i + 1, juce::dontSendNotification);
        const auto* current = find(name);
        remove.setEnabled(current != nullptr && current->program < 0);
    }

    void load(const Entry& e) {
        if (e.program >= 0) processor.setCurrentProgram(e.program);
        else processor.loadUserPreset(e.name);
        sync(true);
    }

    void categoryChanged() {
        fillList();
        // Land on something playable, unless what is playing already belongs here.
        if (presetList.getSelectedId() == 0 && presetList.getNumItems() > 0)
            load(entries[(size_t)presetList.getItemId(0) - 1]);
    }

    void step(int delta) {
        const int count = presetList.getNumItems();
        if (count == 0) return;
        const int at = presetList.indexOfItemId(presetList.getSelectedId());
        const int to = at < 0 ? 0 : ((at + delta) % count + count) % count;
        load(entries[(size_t)presetList.getItemId(to) - 1]);
    }

    void saved(const juce::String& name) {
        if (!processor.saveUserPreset(name)) {
            juce::NativeMessageBox::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Could not save").withMessage("\"" + name + "\" is empty or belongs to a factory preset. Pick another name.")
                .withButton("OK").withAssociatedComponent(this), nullptr);
            return;
        }
        rebuild();
        category.setSelectedId(1, juce::dontSendNotification);
        sync(true);
    }

    void askToSave() {
        dialog = std::make_unique<juce::AlertWindow>("Save preset", "Name:", juce::MessageBoxIconType::NoIcon, this);
        const auto* current = find(processor.getCurrentPresetName());
        dialog->addTextEditor("name", current != nullptr && current->program < 0 ? current->name : juce::String());
        dialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetBar> self(this);
        dialog->enterModalState(true, juce::ModalCallbackFunction::create([self](int result) {
            if (self == nullptr || result != 1) return;
            const auto name = self->dialog->getTextEditorContents("name").trim();
            if (!self->processor.userPresetNames().contains(name)) { self->saved(name); return; }
            juce::NativeMessageBox::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Replace preset",
                "Replace your preset \"" + name + "\"?", self.getComponent(),
                juce::ModalCallbackFunction::create([self, name](int ok) { if (self != nullptr && ok != 0) self->saved(name); }));
        }), false);
    }

    void askToDelete() {
        const auto name = processor.getCurrentPresetName();
        juce::Component::SafePointer<PresetBar> self(this);
        juce::NativeMessageBox::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Delete preset",
            "Delete your preset \"" + name + "\"?", this,
            juce::ModalCallbackFunction::create([self, name](int ok) {
                if (self == nullptr || ok == 0) return;
                self->processor.deleteUserPreset(name);
                self->rebuild();
                self->sync(true);
            }));
    }

    MillstoneProcessor& processor;
    juce::ComboBox category, presetList;
    juce::TextButton previous { "<" }, next { ">" }, save { "SAVE" }, remove { "DELETE" };
    juce::Label readoutName, readoutValue;
    juce::Slider* shown = nullptr;
    bool shownInt = false;
    std::vector<Entry> entries;
    juce::String lastName;
    std::unique_ptr<juce::AlertWindow> dialog;
};

MillstoneEditor::MillstoneEditor(MillstoneProcessor& p)
    : juce::AudioProcessorEditor(p), processor(p), lnf(std::make_unique<LookAndFeel>()), art(makePanelArt()) {
    setLookAndFeel(lnf.get());
    content.setBounds(0, 0, designW, designH);
    content.setInterceptsMouseClicks(false, true);
    addAndMakeVisible(content);

    presetBar = std::make_unique<PresetBar>(processor);
    readoutTarget = [this](juce::Slider* s, juce::RangedAudioParameter* q) { presetBar->show(s, q); };

    for (const auto& item : panelItems()) {
        std::unique_ptr<Control> c;
        if (item.kind == "knob") c = std::make_unique<SliderControl>(item, false, processor.apvts, readoutTarget);
        else if (item.kind == "fader") c = std::make_unique<SliderControl>(item, true, processor.apvts, readoutTarget);
        else if (item.kind == "enum_v") c = std::make_unique<EnumControl>(item, true, processor.apvts);
        else if (item.kind == "enum_h") c = std::make_unique<EnumControl>(item, false, processor.apvts);
        else if (item.kind == "toggle") c = std::make_unique<ToggleControl>(item, processor.apvts);
        else if (item.kind == "popup") c = std::make_unique<PopupControl>(item, processor.apvts, *lnf);
        else if (item.kind == "button") c = std::make_unique<StepControl>(item, processor.apvts);
        if (c == nullptr) continue;
        const auto when = item.str("when");
        c->whenKey = when.upToFirstOccurrenceOf(":", false, false);
        c->whenValue = when.fromFirstOccurrenceOf(":", false, false);
        content.addAndMakeVisible(*c);
        controls.push_back(std::move(c));
    }

    presetBar->setBounds(440, 13, 790, 66);
    presetBar->setInterceptsMouseClicks(false, true);
    content.addAndMakeVisible(*presetBar);

    timerCallback();
    startTimerHz(15);

    setResizable(true, true);
    setResizeLimits(designW / 2, designH / 2, designW * 3 / 2, designH * 3 / 2);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio((double)designW / designH);
    setSize(designW * 85 / 100, designH * 85 / 100);
}

MillstoneEditor::~MillstoneEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void MillstoneEditor::timerCallback() {
    for (auto& base : controls) {   // RATE swaps with DIVISION, and PHASE appears, when SYNC is on
        auto* c = static_cast<Control*>(base.get());
        if (c->whenKey.isEmpty()) continue;
        auto* p = processor.apvts.getParameter(c->whenKey);
        c->setVisible(p != nullptr && p->getCurrentValueAsText() == c->whenValue);
    }
    presetBar->sync();
}

void MillstoneEditor::paint(juce::Graphics& g) {
    g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
    g.drawImage(art, getLocalBounds().toFloat());
}

void MillstoneEditor::resized() {
    content.setTransform(juce::AffineTransform::scale((float)getWidth() / designW, (float)getHeight() / designH));
}
