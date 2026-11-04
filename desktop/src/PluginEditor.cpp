#include "PluginEditor.h"
#include "BinaryData.h"
#include "workout_clock.h"
#include <array>
#include <map>

namespace {

constexpr int designW = 1480, designH = 864;      // the window scales from this
constexpr int drawerH = 345, totalH = designH + drawerH;   // the patch bay drawer hangs below the panel
constexpr int woodW = 36;
constexpr int artScale = 2;                       // the static art is rendered at twice the design size, for Retina
constexpr int knobPad = 8;                        // room around a knob for its shadow, or the control's edge cuts it off
constexpr bool showMetalWear = true;              // height-aware scratches on the metal face and its printing
constexpr bool showWear = false;                  // wear on the other materials (wood, plastic, knob caps): not done yet

const juce::Colour ink(0xff1b1a18), cream(0xffe9e4d4), capFill(0xffd9d3c2), capInk(0xff20201d), screenFill(0xff0d0b08),
                   amber(0xffffb347), amberDim(0xff6e4a1c), ledOn(0xffff4a2a), ledOff(0xff4a1d14);
constexpr juce::uint8 metalR = 168, metalG = 165, metalB = 154;
const juce::Colour cordColours[12] = { juce::Colour(0xffc8352b), juce::Colour(0xffe2b13c), juce::Colour(0xff3274b5), juce::Colour(0xff4c9a4a),
    juce::Colour(0xffdd7a2a), juce::Colour(0xff8b56a8), juce::Colour(0xffd9d6cc), juce::Colour(0xff2f9c9a),
    juce::Colour(0xffd76f93), juce::Colour(0xff9bb83a), juce::Colour(0xff57bcd8), juce::Colour(0xff9a6a40) };

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

// A control that comes and goes draws its own printing; the rest is part of the static art.
bool changesWithPage(const Item& item) { return item.str("when").isNotEmpty() || item.str("page").isNotEmpty(); }

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
    const juce::Colour ink = item.num("cy") >= designH ? cream : ::ink;   // the drawer is dark, so its printing is light
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

// ---- wear on the metal ----------------------------------------------------------------------------------------------
// What each spot on the face is, in design pixels. A scratch has to obey this: it ends where something stands proud of the
// metal, and it jumps a slot in the metal, because whatever made it was sliding along the surface.
enum Surface : juce::uint8 { flat, raised, recessed };

struct HeightMap {
    std::vector<juce::uint8> cells = std::vector<juce::uint8>((size_t)designW * totalH, (juce::uint8)flat);
    Surface at(float x, float y) const {
        const int ix = (int)x, iy = (int)y;
        if (ix < 0 || iy < 0 || ix >= designW || iy >= totalH) return raised;
        return (Surface)cells[(size_t)iy * designW + (size_t)ix];
    }
    void rect(float x, float y, float w, float h, Surface s) {
        for (int iy = juce::jmax(0, (int)y); iy < juce::jmin(totalH, (int)std::ceil(y + h)); ++iy)
            for (int ix = juce::jmax(0, (int)x); ix < juce::jmin(designW, (int)std::ceil(x + w)); ++ix)
                cells[(size_t)iy * designW + (size_t)ix] = s;
    }
    void circle(float cx, float cy, float r, Surface s) {
        for (int iy = juce::jmax(0, (int)(cy - r)); iy < juce::jmin(totalH, (int)std::ceil(cy + r) + 1); ++iy)
            for (int ix = juce::jmax(0, (int)(cx - r)); ix < juce::jmin(designW, (int)std::ceil(cx + r) + 1); ++ix)
                if (juce::square((float)ix + 0.5f - cx) + juce::square((float)iy + 0.5f - cy) <= r * r)
                    cells[(size_t)iy * designW + (size_t)ix] = s;
    }
};

std::vector<juce::Point<float>> screwPositions() {
    const float left = (float)woodW, right = (float)(designW - woodW);
    return { {left + 12, 12}, {right - 12, 12}, {left + 12, designH - 12.0f}, {right - 12, designH - 12.0f}, {740, designH - 12.0f}, {right - 12, 44},
             {1255, 44}, {1429, 44}, {left + 12, totalH - 10.0f}, {right - 12, totalH - 10.0f}, {left + 12, designH + 11.0f}, {right - 12, designH + 11.0f} };
}

// pad grows raised parts, so a scratch stops short of them rather than running under their shadow.
HeightMap buildHeightMap(float pad) {
    HeightMap map;
    for (const auto& item : panelItems()) {
        const float cx = (float)item.num("cx"), cy = (float)item.num("cy");
        if (item.kind == "knob") {
            map.circle(cx, cy, (float)item.num("r", 20) + pad, raised);
        } else if (item.kind == "fader") {
            const float h = (float)item.num("h", 160);
            map.rect(cx - 5 - pad * 0.3f, cy - h * 0.5f + 3, 10 + pad * 0.6f, h - 6, recessed);
        } else if (item.kind == "enum_v" || item.kind == "enum_h") {
            const auto* def = findEngineParam(item.str("key"));
            const int n = def != nullptr ? def->nopts : 1, sw = item.num("sw", 60), sh = item.num("sh", 26);
            const float w = item.kind == "enum_v" ? (float)sw : (float)(n * sw + (n - 1) * 3), h = item.kind == "enum_v" ? (float)(n * sh + (n - 1) * 3) : (float)sh;
            map.rect(cx - w * 0.5f - pad, cy - h * 0.5f - pad, w + 2 * pad, h + 2 * pad, raised);
        } else if (item.kind == "toggle") {
            map.rect(cx - 15 - pad, cy - 18 - pad, 44 + 2 * pad, 36 + 2 * pad, raised);
        } else if (item.kind == "popup") {
            const float w = (float)item.num("w", 200), h = (float)item.num("h", 38);
            map.rect(cx - w * 0.5f - pad, cy - h * 0.5f - pad, w + 2 * pad, h + 2 * pad, raised);
        } else if (item.kind == "button") {
            map.rect(cx - 16 - pad, cy - 16 - pad, 32 + 2 * pad, 32 + 2 * pad, raised);
        } else if (item.kind == "jack") {
            map.circle(cx, cy, 12.5f + pad, raised);
        }
    }
    map.rect(58 - pad, 914 - pad, 74 + 2 * pad, 55 + 2 * pad, raised);        // drawer page buttons
    map.rect(150 - pad, 930 - pad, 246 + 2 * pad, 26 + 2 * pad, raised);      // clock output buttons
    map.rect(412 - pad, 884 - pad, 590 + 2 * pad, 86 + 2 * pad, raised);      // clock LCD
    map.circle(58 + 354 + 590 + 58, 884 + 30, 22 + pad, raised);              // clock pots
    map.circle(58 + 354 + 590 + 154, 884 + 30, 22 + pad, raised);
    map.rect(438 - pad, 11 - pad, 614 + 2 * pad, 66 + 2 * pad, raised);       // display
    map.rect(1066 - pad, 17 - pad, 152 + 2 * pad, 58 + 2 * pad, raised);      // preset buttons
    map.rect(1262 - pad, 22 - pad, 160 + 2 * pad, 46 + 2 * pad, raised);      // model plate
    for (const auto& p : screwPositions()) map.circle(p.x, p.y, 6.0f + pad, raised);
    map.rect(0, 0, (float)woodW + pad, (float)totalH, raised);                // wood stands proud of the face
    map.rect((float)(designW - woodW) - pad, 0, (float)woodW + pad, (float)totalH, raised);
    map.rect(0, 0, (float)designW, 2 + pad * 0.3f, raised);                   // the casing's lip
    map.rect(0, (float)designH - 2 - pad * 0.3f, (float)designW, 6 + pad * 0.6f, raised);
    return map;
}

// Light comes from the top left. In a groove, the wall nearest the light is in shadow and the far wall catches it, so a
// scratch's look depends on its direction: one running along the light hardly shows.
void paintGrooveSegment(juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, float width, float alpha, bool onInk, bool anodised) {
    const auto d = b - a;
    const float length = std::sqrt(d.x * d.x + d.y * d.y);
    if (length < 0.01f) return;
    juce::Point<float> toLight(-d.y / length, d.x / length);
    float facing = toLight.x * -0.6f + toLight.y * -0.8f;
    if (facing < 0) { toLight = -toLight; facing = -facing; }
    const float wall = width * 0.5f;
    if (onInk) {   // the ink is gone along here, down to the metal
        g.setColour(juce::Colour(metalR, metalG, metalB).withAlpha(juce::jmin(1.0f, alpha * 2.0f)));
        g.drawLine({ a, b }, width + 0.7f);
    }
    if (anodised) {   // through the dark coat: the aluminium shows pale, with little to see of the walls
        g.setColour(juce::Colour(0xffb9b6ab).withAlpha(alpha * 0.75f));
        g.drawLine({ a, b }, width);
        return;
    }
    g.setColour(juce::Colour(0xff17120b).withAlpha(alpha * (0.25f + 0.75f * facing)));
    g.drawLine({ a + toLight * wall, b + toLight * wall }, width * 0.8f);
    g.setColour(juce::Colour(0xfffbf8ee).withAlpha(alpha * (0.45f + 0.55f * facing)));
    g.drawLine({ a - toLight * wall, b - toLight * wall }, width * 0.8f);
}

struct InkMap {   // where the printing is, read back from the art
    std::vector<bool> cells = std::vector<bool>((size_t)designW * designH, false);
    bool at(float x, float y) const {
        const int ix = (int)x, iy = (int)y;
        return ix >= 0 && iy >= 0 && ix < designW && iy < designH && cells[(size_t)iy * designW + (size_t)ix];
    }
};

// Could something really have made this mark? Every stroke has to pass as a whole, or it is not drawn at all. It must stay
// on one flat surface, with room around it for whatever made it, and follow a path a hand or a dragged object can follow.
bool scratchIsFeasible(const HeightMap& clearance, const std::vector<juce::Point<float>>& stroke, float maxTurnRadians) {
    if (stroke.size() < 2) return false;
    for (const auto& p : stroke)
        if (clearance.at(p.x, p.y) != flat) return false;
    float turned = 0.0f;
    for (size_t i = 2; i < stroke.size(); ++i) {
        const auto d0 = stroke[i - 1] - stroke[i - 2], d1 = stroke[i] - stroke[i - 1];
        float step = std::atan2(d1.y, d1.x) - std::atan2(d0.y, d0.x);
        while (step > juce::MathConstants<float>::pi) step -= juce::MathConstants<float>::twoPi;
        while (step < -juce::MathConstants<float>::pi) step += juce::MathConstants<float>::twoPi;
        turned += std::abs(step);
    }
    return turned <= maxTurnRadians;
}

void drawScratch(juce::Graphics& g, const InkMap& ink, const std::vector<juce::Point<float>>& stroke, float width, float alpha) {
    const float count = (float)stroke.size();
    for (size_t i = 1; i < stroke.size(); ++i) {
        // pressure builds and fades: scratches taper out rather than ending square
        const float t = (float)i / count, taper = juce::jmin(1.0f, juce::jmin(t, 1.0f - t) * 6.0f + 0.1f);
        const bool tray = stroke[i].y >= designH;
        paintGrooveSegment(g, stroke[i - 1], stroke[i], width * (0.65f + 0.35f * taper), alpha * taper, !tray && ink.at(stroke[i].x, stroke[i].y), tray);
    }
}

std::vector<juce::Point<float>> scratchStroke(juce::Random& rng, juce::Point<float> from, float angle, float length, float wander) {
    const auto to = from.getPointOnCircumference(length, angle);
    const juce::Point<float> bend = (from + to) * 0.5f + juce::Point<float>((rng.nextFloat() - 0.5f) * length * wander, (rng.nextFloat() - 0.5f) * length * wander);
    std::vector<juce::Point<float>> points;
    const int steps = juce::jmax(2, (int)(length / 2.5f));
    for (int i = 0; i <= steps; ++i) {
        const float t = (float)i / (float)steps, u = 1.0f - t;
        points.push_back(from * (u * u) + bend * (2 * u * t) + to * (t * t));
    }
    return points;
}

struct WearCounts { int tried = 0, drawn = 0; };

void paintMetalWear(juce::Graphics& g, const juce::Image& printed, WearCounts* counts) {
    // The bigger the thing that made a mark, the more open space it needs to have moved through.
    const HeightMap fingernail = buildHeightMap(3.0f), object = buildHeightMap(7.0f), heavy = buildHeightMap(14.0f);
    const HeightMap& map = fingernail;
    InkMap ink;
    {   // anything much darker than the metal at this stage is printing
        juce::Image::BitmapData px(printed, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < designH; ++y)
            for (int x = 0; x < designW; ++x)
                ink.cells[(size_t)y * designW + (size_t)x] = px.getPixelColour(x * artScale + 1, y * artScale + 1).getPerceivedBrightness() < 0.42f;
    }
    juce::Random rng(2023);
    const float left = (float)woodW, width = (float)(designW - 2 * woodW), twoPi = juce::MathConstants<float>::twoPi;
    const auto anywhere = [&] { return juce::Point<float>(left + rng.nextFloat() * width, rng.nextFloat() * totalH); };
    WearCounts local;
    // Keeps trying fresh strokes, but never bends the rule to reach the number: a wanted count is only an upper limit.
    const auto attempt = [&](const HeightMap& clearance, const std::vector<juce::Point<float>>& stroke, float maxTurn, float w, float alpha) {
        ++local.tried;
        if (!scratchIsFeasible(clearance, stroke, maxTurn)) return false;
        ++local.drawn;
        drawScratch(g, ink, stroke, w, alpha);
        return true;
    };

    for (int i = 0; i < 1400; ++i)   // fine scuffing from years of use
        attempt(fingernail, scratchStroke(rng, anywhere(), rng.nextFloat() * twoPi, 4.0f + rng.nextFloat() * rng.nextFloat() * 42.0f, 0.1f), 0.7f, 0.28f + rng.nextFloat() * 0.22f, 0.10f + rng.nextFloat() * 0.18f);
    for (int tries = 0, done = 0; tries < 1500 && done < 80; ++tries)   // proper scratches
        if (attempt(object, scratchStroke(rng, anywhere(), rng.nextFloat() * twoPi, 30.0f + rng.nextFloat() * 130.0f, 0.12f), 0.6f, 0.35f + rng.nextFloat() * 0.3f, 0.26f + rng.nextFloat() * 0.26f)) ++done;
    for (int tries = 0, done = 0; tries < 500 && done < 6; ++tries) {   // scrapes: parallel lines from one dragged object, all possible or none
        const auto origin = anywhere();
        const float angle = rng.nextFloat() * twoPi, length = 30.0f + rng.nextFloat() * 90.0f;
        const juce::Point<float> across(-std::sin(angle), std::cos(angle));
        const int lines = 3 + rng.nextInt(6);
        std::vector<std::vector<juce::Point<float>>> strokes;
        bool possible = true;
        for (int i = 0; i < lines && possible; ++i) {
            strokes.push_back(scratchStroke(rng, origin + across * ((float)i * 2.2f + rng.nextFloat() * 1.6f), angle, length * (0.6f + rng.nextFloat() * 0.5f), 0.02f));
            possible = scratchIsFeasible(heavy, strokes.back(), 0.15f);
        }
        ++local.tried;
        if (!possible) continue;
        ++local.drawn;
        ++done;
        for (const auto& stroke : strokes) drawScratch(g, ink, stroke, 0.3f + rng.nextFloat() * 0.22f, 0.28f + rng.nextFloat() * 0.26f);
    }
    for (int tries = 0, done = 0; tries < 800 && done < 3; ++tries)   // a few real gouges: only where something heavy could have slid
        if (attempt(heavy, scratchStroke(rng, anywhere(), rng.nextFloat() * twoPi, 50.0f + rng.nextFloat() * 110.0f, 0.1f), 0.45f, 0.9f + rng.nextFloat() * 0.5f, 0.55f)) ++done;

    // Hands: more scuffing along the front edge, and rings worn around the knobs that get used most.
    for (int i = 0; i < 300; ++i)
        attempt(fingernail, scratchStroke(rng, { left + rng.nextFloat() * width, designH - rng.nextFloat() * 42.0f }, rng.nextFloat() * twoPi, 5.0f + rng.nextFloat() * 36.0f, 0.1f), 0.7f, 0.3f, 0.12f + rng.nextFloat() * 0.2f);
    for (const auto& item : panelItems()) {
        if (item.kind != "knob" || item.num("r") < 26) continue;
        const juce::Point<float> c((float)item.num("cx"), (float)item.num("cy"));
        for (int i = 0; i < 26; ++i) {   // a fingertip following the knob: an arc is the path here, so it may turn with it
            const float radius = (float)item.num("r") + 8.0f + rng.nextFloat() * 8.0f, start = rng.nextFloat() * twoPi, sweep = 0.2f + rng.nextFloat() * 0.8f;
            std::vector<juce::Point<float>> arc;
            for (int k = 0; k <= 12; ++k) arc.push_back(c.getPointOnCircumference(radius, start + sweep * (float)k / 12.0f));
            attempt(fingernail, arc, sweep + 0.1f, 0.3f, 0.09f + rng.nextFloat() * 0.13f);
        }
    }
    if (counts != nullptr) *counts = local;

    // Printing wears thin in tiny flecks, evenly: nothing here says why any one spot would be rubbed harder.
    const juce::Colour bare(metalR, metalG, metalB);
    const auto fleck = [&](juce::Point<float> p) {
        if (!ink.at(p.x, p.y) || map.at(p.x, p.y) != flat) return;
        const float d = 0.3f + rng.nextFloat() * rng.nextFloat() * 1.5f;
        g.setColour(bare.withAlpha(0.35f + rng.nextFloat() * 0.55f));
        g.fillEllipse(p.x, p.y, d, d * (0.5f + rng.nextFloat()));
    };
    for (int i = 0; i < 14000; ++i) fleck({ left + rng.nextFloat() * width, rng.nextFloat() * designH });

    // Dings: small dents, shaded for the same light, mostly along the edges.
    for (int i = 0; i < 40; ++i) {
        const bool edge = rng.nextBool();
        const juce::Point<float> p(left + rng.nextFloat() * width, edge ? (rng.nextBool() ? 3.0f + rng.nextFloat() * 8.0f : designH - 3.0f - rng.nextFloat() * 8.0f) : rng.nextFloat() * designH);
        if (map.at(p.x, p.y) != flat || map.at(p.x + 5, p.y) != flat || map.at(p.x, p.y + 4) != flat || map.at(p.x + 5, p.y + 4) != flat) continue;
        const float w = 1.2f + rng.nextFloat() * 3.0f, h = 0.8f + rng.nextFloat() * 1.8f;
        g.setColour(juce::Colour(0xff17120b).withAlpha(0.5f));   // the wall nearest the light is in shadow
        g.fillEllipse(p.x - 0.4f, p.y - 0.5f, w, h);
        g.setColour(juce::Colour(0xfff6f3e8).withAlpha(0.6f));
        g.fillEllipse(p.x + 0.4f, p.y + 0.6f, w, h);
    }
}

juce::Image makePanelArt(bool wear = true, WearCounts* counts = nullptr) {
    juce::Image img(juce::Image::RGB, designW * artScale, totalH * artScale, true, juce::SoftwareImageType());
    paintBrushedMetal(img);
    paintWood(img, 0, woodW * artScale, 7);
    paintWood(img, (designW - woodW) * artScale, designW * artScale, 19);
    juce::Graphics g(img);
    g.addTransform(juce::AffineTransform::scale((float)artScale));
    paintPatina(g);

    const float left = (float)woodW, right = (float)(designW - woodW);

    // The drawer is the same brushed metal, anodised dark.
    g.setColour(juce::Colour(0xff0d0c0a).withAlpha(0.80f));
    g.fillRect(left, (float)designH, right - left, (float)drawerH);

    for (const auto& item : panelItems()) {
        if (item.kind == "frame") {
            const auto r = juce::Rectangle<float>((float)item.num("x"), (float)item.num("y"), (float)item.num("w"), (float)item.num("h"));
            const bool tray = item.num("y") >= designH;
            const juce::Colour ink = tray ? cream : ::ink, cream = tray ? ::ink : ::cream;
            g.setColour((tray ? juce::Colours::white : juce::Colours::black).withAlpha(tray ? 0.03f : 0.045f));   // a very slightly different printed field
            g.fillRoundedRectangle(r, 3.0f);
            g.setColour(ink.withAlpha(tray ? 0.45f : 0.75f));
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
            if (!changesWithPage(item)) paintKnobPrinting(g, item);
            if (item.str("cord").isNotEmpty()) {   // a colour dot, matching that cord's cable
                g.setColour(cordColours[item.num("cord") % 12]);
                g.fillEllipse((float)item.num("cx") - 3.5f + 15.0f, (float)item.num("cy") + (float)item.num("r") + 12.5f, 7.0f, 7.0f);
            }
        } else if (item.kind == "jack") {
            const juce::Point<float> c((float)item.num("cx"), (float)item.num("cy"));
            const auto round = [&](float radius) { return juce::Rectangle<float>(2 * radius, 2 * radius).withCentre(c); };
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillEllipse(round(12.5f).translated(0.8f, 1.6f));
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe2dfd5), c.x - 8, c.y - 10, juce::Colour(0xff55524a), c.x + 8, c.y + 10, false));
            g.fillEllipse(round(12.0f));                                                     // the nut
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.drawEllipse(round(8.2f), 1.0f);
            g.setColour(juce::Colour(0xff070706));
            g.fillEllipse(round(6.6f));                                                      // the socket
            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.drawEllipse(round(6.6f), 0.6f);
            g.setColour(cream);
            g.setFont(printFont(9.5f, true, 0.04f));
            g.drawFittedText(item.str("label"), juce::Rectangle<float>(c.x - 21, c.y + 15, 42, 13).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        } else if (item.kind == "fader") {
            paintFaderPrinting(g, item);
        } else if (item.kind == "enum_v" || item.kind == "enum_h") {
            if (changesWithPage(item)) continue;
            const bool vertical = item.kind == "enum_v";
            const auto* def = findEngineParam(item.str("key"));
            const int n = def != nullptr ? def->nopts : 1;
            const int sh = item.num("sh", 26);
            const int groupH = vertical ? n * sh + (n - 1) * 3 : sh;
            g.setColour(item.num("cy") >= designH ? cream : ink);
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

    if (wear && showMetalWear) paintMetalWear(g, img, counts);
    juce::Random rng(2023);   // for the materials still behind showWear
    const juce::Rectangle<float> face(left, 0, right - left, (float)designH);

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
    g.fillRect(left, 0.0f, 7.0f, (float)totalH);
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.60f), right, 0, juce::Colours::transparentBlack, right - 7, 0, false));
    g.fillRect(right - 7, 0.0f, 7.0f, (float)totalH);
    // The panel's edge casts a shadow into the drawer.
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.75f), 0, (float)designH, juce::Colours::transparentBlack, 0, designH + 14.0f, false));
    g.fillRect(left, (float)designH, right - left, 14.0f);
    for (const auto& p : std::vector<juce::Point<float>>{ {left + 12, totalH - 10.0f}, {right - 12, totalH - 10.0f}, {left + 12, designH + 11.0f}, {right - 12, designH + 11.0f} })
        paintScrew(g, p.x, p.y, 4.6f, turns.nextFloat() * 3.14f);
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
    juce::String page;                 // which drawer page it is on, if any
    int channel = -1;                  // which clock output, if any
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
            const int margin = changesWithPage(item) ? 62 : knobPad;   // a conditional knob brings its own printing
            setBounds(item.num("cx") - r - margin, item.num("cy") - r - margin, 2 * (r + margin), 2 * (r + margin));
            slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            slider.setMouseDragSensitivity(220);
            slider.setBounds(margin - knobPad, margin - knobPad, 2 * (r + knobPad), 2 * (r + knobPad));
            if (changesWithPage(item)) printing = std::make_unique<Item>(item);
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
        ownLabel = changesWithPage(item);
        title = item.str("label");
        titleInk = item.num("cy") >= designH ? cream : ink;
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
        g.setColour(titleInk);
        g.setFont(printFont(10.5f));
        g.drawText(title, 0, 0, getWidth(), 13, juce::Justification::centred);
    }
private:
    juce::String title;
    juce::Colour titleInk;
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
        titleInk = item.num("cy") >= designH ? cream : ink;
        const int w = item.num("w", 200), h = item.num("h", 38);
        setBounds(item.num("cx") - w / 2, item.num("cy") - h / 2 - titleH, w, h + titleH);
        attachment = std::make_unique<juce::ParameterAttachment>(*param, [this](float v) { current = juce::roundToInt(v); repaint(); });
        attachment->sendInitialUpdate();
    }
    void paint(juce::Graphics& g) override {
        if (titleH > 0) { g.setColour(titleInk); g.setFont(printFont(10.5f)); g.drawText(title, 0, 0, getWidth(), 13, juce::Justification::centred); }
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
    juce::Colour titleInk;
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

namespace {
const juce::Image& sharedPanelArt() {   // drawn once, then every window of every instance shares it
    static const juce::Image image = makePanelArt();
    return image;
}
}

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
        for (auto* b : {&previous, &next, &patch, &save, &remove}) addAndMakeVisible(*b);
        patch.setComponentID("patch_toggle");
        patch.onClick = [this] { if (onPatch) onPatch(); };
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
        readoutValue.setComponentID("readout_value");
        readoutName.setComponentID("readout_name");
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
        patch.setBounds(706, 4, 72, 26);
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

    std::function<void()> onPatch;
    void setPatchLit(bool lit) { patch.setColour(juce::TextButton::textColourOffId, lit ? amber : cream); patch.repaint(); }

    void show(juce::Slider* slider, juce::RangedAudioParameter* param) {
        shown = slider;
        shownInt = false;
        if (param != nullptr) {
            const auto id = param->getParameterID();
            if (id.startsWithChar('c') && id.endsWith("_amt")) {   // a cord: say what it connects
                const auto stem = id.upToLastOccurrenceOf("_", false, false);
                const auto from = processor.apvts.getParameter(stem + "_src")->getCurrentValueAsText(), to = processor.apvts.getParameter(stem + "_dst")->getCurrentValueAsText();
                readoutName.setText("CORD " + stem.substring(1) + ":  " + (from == "OFF" || to == "OFF" ? juce::String("UNPLUGGED") : from + " > " + to), juce::dontSendNotification);
            } else {
                readoutName.setText(param->getName(32), juce::dontSendNotification);
            }
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
    juce::TextButton previous { "<" }, next { ">" }, patch { "PATCH" }, save { "SAVE" }, remove { "DELETE" };
    juce::Label readoutName, readoutValue;
    juce::Slider* shown = nullptr;
    bool shownInt = false;
    std::vector<Entry> entries;
    juce::String lastName;
    std::unique_ptr<juce::AlertWindow> dialog;
};

// The cables and the jacks' mouse handling. Drag between an output and an input to patch; pull a plug out of an input to
// move or remove that cord; double-click an input to unplug everything in it.
struct MillstoneEditor::PatchBay : juce::Component {
    explicit PatchBay(MillstoneProcessor& p) : processor(p) {
        setComponentID("patch_bay");
        for (const auto& item : panelItems())
            if (item.kind == "jack")
                jacks.push_back({ juce::Point<float>((float)item.num("cx"), (float)(item.num("cy") - designH)), item.str("role") == "src", item.num("index") });
    }

    bool hitTest(int x, int y) override { return jackAt({ (float)x, (float)y }) != nullptr; }

    void sync() {   // presets, automation and undo can all move cords
        std::array<int, MillstoneProcessor::numCords * 2> now;
        for (int c = 0; c < MillstoneProcessor::numCords; ++c) { now[(size_t)c * 2] = processor.cordSource(c); now[(size_t)c * 2 + 1] = processor.cordDest(c); }
        if (now != seen) { seen = now; repaint(); }
    }

    void paint(juce::Graphics& g) override {
        for (int c = 0; c < MillstoneProcessor::numCords; ++c) {
            const auto* from = find(true, processor.cordSource(c)), *to = find(false, processor.cordDest(c));
            if (from != nullptr && to != nullptr) drawCable(g, from->pos, to->pos, cordColours[c], c);
        }
        if (dragging != nullptr) {
            const int cord = dragColour >= 0 ? dragColour : firstFree();
            drawCable(g, dragging->pos, dragPos, cordColours[juce::jmax(0, cord)], 99);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto* jack = jackAt(e.position);
        if (jack == nullptr) return;
        dragAmount = 50.0f;
        dragColour = -1;
        dragging = jack;
        dragPos = e.position;
        if (!jack->isSource) {   // pulling a plug out of an input: the newest cord in it comes away
            for (int c = MillstoneProcessor::numCords - 1; c >= 0; --c)
                if (processor.cordDest(c) == jack->index && processor.cordSource(c) != 0) {
                    dragging = find(true, processor.cordSource(c));
                    dragAmount = processor.cordAmount(c);
                    dragColour = c;
                    processor.disconnectCord(c);
                    break;
                }
        }
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override { dragPos = e.position; repaint(); }

    void mouseUp(const juce::MouseEvent& e) override {
        if (dragging == nullptr) return;
        const auto* target = jackAt(e.position);
        if (target != nullptr && target->isSource != dragging->isSource) {
            const auto* source = dragging->isSource ? dragging : target, *dest = dragging->isSource ? target : dragging;
            processor.connectCord(source->index, dest->index, dragAmount);
        }
        dragging = nullptr;
        sync();
        repaint();
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override {
        const auto* jack = jackAt(e.position);
        if (jack == nullptr || jack->isSource) return;
        for (int c = 0; c < MillstoneProcessor::numCords; ++c)
            if (processor.cordDest(c) == jack->index) processor.disconnectCord(c);
        sync();
    }

private:
    struct Jack { juce::Point<float> pos; bool isSource; int index; };

    const Jack* jackAt(juce::Point<float> p) const {
        for (const auto& j : jacks) if (j.pos.getDistanceFrom(p) <= 14.0f) return &j;
        return nullptr;
    }
    const Jack* find(bool source, int index) const {
        for (const auto& j : jacks) if (j.isSource == source && j.index == index) return &j;
        return nullptr;
    }
    int firstFree() const {
        for (int c = 0; c < MillstoneProcessor::numCords; ++c) if (processor.cordSource(c) == 0 || processor.cordDest(c) == 0) return c;
        return -1;
    }

    static void drawPlug(juce::Graphics& g, juce::Point<float> c, juce::Colour colour) {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillEllipse(juce::Rectangle<float>(19, 19).withCentre(c).translated(1.0f, 2.2f));
        g.setGradientFill(juce::ColourGradient(colour.brighter(0.6f), c.x - 5, c.y - 7, colour.darker(0.7f), c.x + 5, c.y + 7, false));
        g.fillEllipse(juce::Rectangle<float>(18, 18).withCentre(c));
        g.setColour(colour.darker(0.9f));
        g.drawEllipse(juce::Rectangle<float>(9, 9).withCentre(c), 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.28f));
        g.fillEllipse(c.x - 5.5f, c.y - 6.5f, 5, 3);
    }

    void drawCable(juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour colour, int seed) const {
        // Slack hangs under gravity, kept inside the drawer.
        const float length = a.getDistanceFrom(b);
        const float sag = juce::jmin((float)getHeight() - 8.0f - juce::jmax(a.y, b.y), 26.0f + length * 0.22f + (float)(seed % 4) * 5.0f);
        juce::Path cable;
        cable.startNewSubPath(a);
        cable.cubicTo(a + juce::Point<float>(length * 0.12f * (b.x > a.x ? 1.0f : -1.0f), sag * 1.3f), b + juce::Point<float>(length * 0.12f * (b.x > a.x ? -1.0f : 1.0f), sag * 1.3f), b);
        const juce::PathStrokeType stroke(4.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour(juce::Colours::black.withAlpha(0.42f));
        g.strokePath(cable, stroke, juce::AffineTransform::translation(1.8f, 3.2f));
        g.setColour(colour.darker(0.35f));
        g.strokePath(cable, stroke);
        g.setColour(colour.brighter(0.25f));
        g.strokePath(cable, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation(-0.5f, -0.8f));
        drawPlug(g, a, colour);
        drawPlug(g, b, colour);
    }

    MillstoneProcessor& processor;
    std::vector<Jack> jacks;
    std::array<int, MillstoneProcessor::numCords * 2> seen {};
    const Jack* dragging = nullptr;
    juce::Point<float> dragPos;
    float dragAmount = 50.0f;
    int dragColour = -1;
};

// The drawer's page buttons, and the clock's output buttons.
struct MillstoneEditor::DrawerTabs : juce::Component {
    DrawerTabs(MillstoneProcessor& p, std::function<void()> changed) : processor(p), onChanged(std::move(changed)) {
        setInterceptsMouseClicks(false, true);
        const auto add = [this](const juce::String& text, const juce::String& id, juce::Rectangle<int> bounds, std::function<void()> click) {
            auto* b = buttons.add(new Segment(text));
            b->setComponentID(id);
            b->setBounds(bounds);
            b->onClick = [this, click] { click(); refresh(); onChanged(); };
            addAndMakeVisible(b);
            return b;
        };
        add("TIDES", "drawer_tides", { 0, 24, 74, 26 }, [this] { processor.drawerPage = 0; });
        add("CLOCK", "drawer_clock", { 0, 53, 74, 26 }, [this] { processor.drawerPage = 1; });
        for (int c = 0; c < 8; ++c)
            add(juce::String(c + 1), "clock_channel:" + juce::String(c), { 92 + c * 31, 40, 29, 26 }, [this, c] { processor.clockChannel = c; });
        refresh();
    }
    void paint(juce::Graphics& g) override {
        if (processor.drawerPage != 1) return;
        g.setColour(cream);
        g.setFont(printFont(10.5f));
        g.drawText("OUTPUT", 92, 23, 246, 14, juce::Justification::centred);
    }
    void refresh() {
        buttons[0]->setToggleState(processor.drawerPage == 0, juce::dontSendNotification);
        buttons[1]->setToggleState(processor.drawerPage == 1, juce::dontSendNotification);
        for (int c = 0; c < 8; ++c) {
            buttons[c + 2]->setVisible(processor.drawerPage == 1);
            buttons[c + 2]->setToggleState(processor.clockChannel == c, juce::dontSendNotification);
        }
        repaint();
    }
    MillstoneProcessor& processor;
    std::function<void()> onChanged;
    juce::OwnedArray<Segment> buttons;
};

// The clock's screen and its two pots: SELECT scrolls through the settings, VALUE changes the one selected. The buttons beside
// it pick which output is being edited. The parameters are ordinary ones, so a host can automate them and the screen follows.
struct MillstoneEditor::ClockLcd : juce::Component {
    static constexpr int numSettings = 13;
    struct Setting { const char* suffix; const char* label; };
    static const Setting* settings() {
        static const Setting table[numSettings] = { {"div", "RATE"}, {"shape", "SHAPE"}, {"width", "WIDTH"}, {"swing", "SWING"}, {"prob", "PROB"}, {"phase", "PHASE"}, {"level", "LEVEL"}, {"offset", "OFFSET"}, {"steps", "STEPS"}, {"fill", "FILL"}, {"rot", "ROTATE"}, {"ck_src", "SOURCE"}, {"ck_bpm", "BPM"} };
        return table;
    }

    explicit ClockLcd(MillstoneProcessor& p) : processor(p) {
        setComponentID("clock_lcd");
        for (auto* pot : { &select, &value }) {
            pot->setSliderStyle(juce::Slider::RotaryVerticalDrag);
            pot->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addAndMakeVisible(*pot);
        }
        select.setComponentID("clock_select");
        value.setComponentID("clock_value");
        select.setRange(0, numSettings - 1, 1);
        select.setMouseDragSensitivity(200);
        select.setDoubleClickReturnValue(false, 0);
        select.onValueChange = [this] { processor.clockSetting = juce::roundToInt(select.getValue()); bindValue(); repaint(); };
        value.setMouseDragSensitivity(220);
        value.onDragStart = [this] { if (auto* q = current()) q->beginChangeGesture(); };
        value.onDragEnd = [this] { if (auto* q = current()) q->endChangeGesture(); };
        value.onValueChange = [this] { if (!binding) if (auto* q = current()) { q->setValueNotifyingHost(q->convertTo0to1((float)value.getValue())); refresh(); } };
        select.setValue(processor.clockSetting, juce::dontSendNotification);
        bindValue();
    }

    juce::RangedAudioParameter* current() const {
        const juce::String suffix(settings()[processor.clockSetting].suffix);
        return processor.apvts.getParameter(suffix.startsWith("ck_") ? suffix : "k" + juce::String(processor.clockChannel + 1) + "_" + suffix);
    }

    // Points the VALUE pot at whatever is selected.
    void bindValue() {
        auto* q = current();
        if (q == nullptr) return;
        binding = true;
        const auto range = q->getNormalisableRange();
        const bool stepped = q->getNumSteps() <= 64;
        value.setRange(range.start, range.end, stepped ? 1.0 : 0.0);
        value.setDoubleClickReturnValue(true, q->convertFrom0to1(q->getDefaultValue()));
        value.setValue(q->convertFrom0to1(q->getValue()), juce::dontSendNotification);
        binding = false;
        refresh();
    }

    // Called by the timer as well, so automation, presets and a changed output all show.
    void refresh() {
        auto* q = current();
        if (q == nullptr) return;
        if (!value.isMouseButtonDown()) { binding = true; value.setValue(q->convertFrom0to1(q->getValue()), juce::dontSendNotification); binding = false; }
        const auto text = valueText(*q);
        const auto stamp = text + juce::String(processor.clockChannel) + juce::String(processor.clockSetting) + previewStamp();
        getProperties().set("setting", settings()[processor.clockSetting].label);
        getProperties().set("value", text);
        getProperties().set("output", processor.clockChannel + 1);
        if (stamp != lastStamp) { lastStamp = stamp; repaint(); }
    }

    void channelChanged() { bindValue(); repaint(); }

    void resized() override {
        select.setBounds(potX(0) - potR - knobPad, potY - potR - knobPad, 2 * (potR + knobPad), 2 * (potR + knobPad));
        value.setBounds(potX(1) - potR - knobPad, potY - potR - knobPad, 2 * (potR + knobPad), 2 * (potR + knobPad));
    }

    void paint(juce::Graphics& g) override {
        const auto screen = screenBounds();
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.fillRoundedRectangle(screen.expanded(2.5f).translated(0.5f, 1.0f), 4.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2a2925), 0, screen.getY(), juce::Colour(0xff53514a), 0, screen.getBottom(), false));
        g.fillRoundedRectangle(screen.expanded(2.0f), 4.0f);
        g.setColour(screenFill);
        g.fillRoundedRectangle(screen, 2.5f);

        auto* q = current();
        // Left: what is being edited.
        auto left = screen.reduced(10, 6).withWidth(186);
        g.setFont(displayFont(11.0f));
        g.setColour(amberDim.brighter(0.4f));
        g.drawText("OUTPUT " + juce::String(processor.clockChannel + 1), left.withHeight(15), juce::Justification::centredLeft);
        g.setFont(displayFont(12.5f));
        g.drawText(settings()[processor.clockSetting].label, left.withTrimmedTop(16).withHeight(17), juce::Justification::centredLeft);
        g.setFont(displayFont(24.0f));
        if (q != nullptr) drawGlowText(g, valueText(*q), left.withTrimmedTop(35).toNearestInt(), juce::Justification::centredLeft);

        // Middle: every setting, with the selected one lit.
        for (int i = 0; i < numSettings; ++i) {
            const auto cell = settingCell(i);
            const bool on = i == processor.clockSetting;
            if (on) { g.setColour(amber); g.fillRoundedRectangle(cell, 1.5f); }
            g.setColour(on ? screenFill : amberDim.brighter(0.25f));
            g.setFont(displayFont(9.5f));
            g.drawText(settings()[i].label, cell, juce::Justification::centred);
        }

        // Right: the output's shape, or its Euclidean pattern.
        paintPreview(g, screen.withLeft(screen.getRight() - 136).reduced(10, 10));

        // Glass.
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.08f), 0, screen.getY(), juce::Colours::transparentWhite, 0, screen.getCentreY(), false));
        g.fillRoundedRectangle(screen.withHeight(screen.getHeight() * 0.5f), 2.5f);

        for (const auto& [index, label] : { std::pair<int, const char*>(0, "SELECT"), std::pair<int, const char*>(1, "VALUE") }) {
            Item item;
            item.kind = "knob";
            item.attrs["cx"] = juce::String(potX(index));
            item.attrs["cy"] = juce::String(potY + designH);   // the drawer's printing is light
            item.attrs["r"] = juce::String(potR);
            item.attrs["label"] = label;
            paintKnobPrinting(g, item, 0.0f, -(float)designH);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {   // the settings on the screen can be picked directly
        for (int i = 0; i < numSettings; ++i)
            if (settingCell(i).contains(e.position)) select.setValue(i, juce::sendNotificationSync);
    }

    bool hitTest(int x, int y) override {   // the screen and the pots: the rest of this area belongs to what is beneath it
        return screenBounds().contains((float)x, (float)y) || select.getBounds().contains(x, y) || value.getBounds().contains(x, y);
    }

    static constexpr int lcdX = 354, lcdY = 0, lcdW = 590, lcdH = 86;   // inside this component
    static constexpr int potY = 30, potR = 22;
    static int potX(int index) { return lcdX + lcdW + 58 + index * 96; }
    static juce::Rectangle<float> screenBounds() { return { (float)lcdX, (float)lcdY + 2, (float)lcdW, (float)lcdH - 4 }; }

private:
    juce::Rectangle<float> settingCell(int i) const {
        const auto s = screenBounds();
        const float w = 50.0f, h = 21.0f, x0 = s.getX() + 202.0f, y0 = s.getY() + 9.0f;
        return { x0 + (float)(i % 5) * (w + 0.0f), y0 + (float)(i / 5) * (h + 1.0f), w - 2.0f, h };
    }

    static juce::String valueText(juce::RangedAudioParameter& q) {
        if (q.getNumSteps() <= 64 && dynamic_cast<juce::AudioParameterChoice*>(&q) != nullptr) return q.getCurrentValueAsText();
        const float v = q.convertFrom0to1(q.getValue());
        return q.getNumSteps() <= 64 ? juce::String(juce::roundToInt(v)) : juce::String(v, 1);
    }

    float channelValue(const char* suffix) const {
        return processor.apvts.getRawParameterValue("k" + juce::String(processor.clockChannel + 1) + "_" + suffix)->load();
    }

    juce::String previewStamp() const {
        return juce::String(channelValue("shape")) + "/" + juce::String(channelValue("width")) + "/" + juce::String(channelValue("steps")) + "/" + juce::String(channelValue("fill")) + "/" + juce::String(channelValue("rot"));
    }

    void paintPreview(juce::Graphics& g, juce::Rectangle<float> r) const {
        const int steps = juce::roundToInt(channelValue("steps"));
        if (steps > 0) {   // the pattern, as dots
            const int fill = juce::roundToInt(channelValue("fill")), rotate = juce::roundToInt(channelValue("rot"));
            const int perRow = juce::jmin(steps, 8), rows = (steps + perRow - 1) / perRow;
            const float cell = juce::jmin(r.getWidth() / (float)perRow, r.getHeight() / (float)rows);
            for (int i = 0; i < steps; ++i) {
                const auto dot = juce::Rectangle<float>(cell * 0.7f, cell * 0.7f).withCentre({ r.getX() + cell * ((float)(i % perRow) + 0.5f), r.getY() + cell * ((float)(i / perRow) + 0.5f) });
                if (ms::WorkoutClock::euclid(steps, fill, rotate, i)) { g.setColour(amber); g.fillEllipse(dot); }
                else { g.setColour(amberDim); g.drawEllipse(dot, 1.0f); }
            }
            return;
        }
        // One cycle of the shape.
        const int shape = juce::roundToInt(channelValue("shape"));
        const float width = channelValue("width") * 0.01f;
        juce::Path path;
        const float mid = r.getCentreY(), half = r.getHeight() * 0.5f - 2.0f;
        for (int i = 0; i <= 100; ++i) {
            const float t = (float)i / 100.0f;
            float v = 0.0f;
            switch (shape) {
                case ms::WorkoutClock::GATE: v = t < width ? 1.0f : -1.0f; break;
                case ms::WorkoutClock::TRIG: v = t < 0.06f ? 1.0f : -1.0f; break;
                case ms::WorkoutClock::SAW: v = 1.0f - 2.0f * t; break;
                case ms::WorkoutClock::RAMP: v = 2.0f * t - 1.0f; break;
                case ms::WorkoutClock::TRI: v = t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t; break;
                case ms::WorkoutClock::SINE: v = std::sin(t * juce::MathConstants<float>::twoPi); break;
                default: { static const float held[5] = { 0.4f, -0.7f, 0.9f, -0.2f, 0.6f }; v = held[juce::jmin(4, (int)(t * 5.0f))]; break; }
            }
            const juce::Point<float> at(r.getX() + t * r.getWidth(), mid - v * half);
            if (i == 0) path.startNewSubPath(at); else path.lineTo(at);
        }
        g.setColour(amber);
        g.strokePath(path, juce::PathStrokeType(1.6f));
    }

    MillstoneProcessor& processor;
    ParamSlider select, value;
    bool binding = false;
    juce::String lastStamp;
};

MillstoneEditor::MillstoneEditor(MillstoneProcessor& p)
    : juce::AudioProcessorEditor(p), processor(p), lnf(std::make_unique<LookAndFeel>()), art(sharedPanelArt()) {
    setLookAndFeel(lnf.get());
    content.setBounds(0, 0, designW, totalH);
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
        c->page = item.str("page");
        c->channel = item.num("channel", -1);
        content.addAndMakeVisible(*c);
        controls.push_back(std::move(c));
    }

    presetBar->setBounds(440, 13, 790, 66);
    presetBar->setInterceptsMouseClicks(false, true);
    presetBar->onPatch = [this] { setPatchBayOpen(!processor.patchBayOpen); };
    content.addAndMakeVisible(*presetBar);

    drawerTabs = std::make_unique<DrawerTabs>(processor, [this] { clockLcd->channelChanged(); timerCallback(); });
    drawerTabs->setBounds(58, 890, 350, 84);
    content.addAndMakeVisible(*drawerTabs);

    clockLcd = std::make_unique<ClockLcd>(processor);
    clockLcd->setBounds(58, 884, 1360, 88);
    content.addChildComponent(*clockLcd);

    patchBay = std::make_unique<PatchBay>(processor);
    patchBay->setBounds(0, designH, designW, drawerH);
    content.addAndMakeVisible(*patchBay);

    timerCallback();
    startTimerHz(15);

    setResizable(true, true);
    setSize(designW * 85 / 100, designH * 85 / 100);
    setPatchBayOpen(processor.patchBayOpen);
}

void MillstoneEditor::setPatchBayOpen(bool open) {
    processor.patchBayOpen = open;
    presetBar->setPatchLit(open);
    const int height = open ? totalH : designH;
    setResizeLimits(designW / 2, height / 2, designW * 3 / 2, height * 3 / 2);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio((double)designW / height);
    // The panel keeps its size and the window grows, unless that would run off the screen: then it scales to fit,
    // and closing the drawer goes back to the size it had.
    int width = getWidth();
    if (open) {
        widthBeforeDrawer = width;
        if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds()))
            width = juce::jmin(width, (display->userArea.getHeight() - 40) * designW / height);
        width = juce::jmax(width, designW / 2);
    } else if (widthBeforeDrawer > 0) {
        width = widthBeforeDrawer;
    }
    setSize(width, juce::roundToInt((double)width * height / designW));
}

MillstoneEditor::~MillstoneEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void MillstoneEditor::timerCallback() {
    for (auto& base : controls) {   // RATE swaps with DIVISION, and PHASE appears, when SYNC is on
        auto* c = static_cast<Control*>(base.get());
        if (c->whenKey.isEmpty() && c->page.isEmpty()) continue;
        bool show = c->page.isEmpty() || c->page == (processor.drawerPage == 0 ? "tides" : "clock");
        if (c->channel >= 0 && c->channel != processor.clockChannel) show = false;
        if (show && c->whenKey.isNotEmpty()) {
            auto* p = processor.apvts.getParameter(c->whenKey);
            const bool negate = c->whenValue.startsWithChar('!');   // when=key:!VALUE shows it for anything else
            const bool same = p != nullptr && p->getCurrentValueAsText() == (negate ? c->whenValue.substring(1) : c->whenValue);
            show = p != nullptr && same != negate;
        }
        c->setVisible(show);
    }
    clockLcd->setVisible(processor.drawerPage == 1);
    clockLcd->refresh();
    presetBar->sync();
    patchBay->sync();
}

juce::Point<float> MillstoneEditor::jackPosition(bool isOutput, int index) {
    for (const auto& item : panelItems())
        if (item.kind == "jack" && (item.str("role") == "src") == isOutput && item.num("index") == index)
            return { (float)item.num("cx"), (float)(item.num("cy") - designH) };
    return {};
}

juce::StringArray MillstoneEditor::layoutProblems() {
    struct Part { juce::String name; juce::Rectangle<float> r; const Item* item; bool hasFrame; bool round = false; };
    std::vector<Part> parts;
    const auto textRect = [](const juce::String& text, const juce::Font& font, float cx, float top, float h, float maxW = 1e6f) {
        const float w = juce::jmin(maxW, juce::GlyphArrangement::getStringWidth(font, text));
        return juce::Rectangle<float>(cx - w * 0.5f, top, w, h);
    };
    std::vector<juce::Rectangle<float>> frames;
    for (const auto& item : panelItems()) {
        const float cx = (float)item.num("cx"), cy = (float)item.num("cy");
        const auto label = item.str("label"), name = item.kind + " " + (item.str("key").isNotEmpty() ? item.str("key") : label);
        if (item.kind == "frame") {
            frames.push_back({ (float)item.num("x"), (float)item.num("y"), (float)item.num("w"), (float)item.num("h") });
            parts.push_back({ "title " + item.str("title"), { (float)item.num("x"), (float)item.num("y"), juce::GlyphArrangement::getStringWidth(printFont(11.5f, true, 0.20f), item.str("title")) + 22.0f, 19.0f }, &item, false });
        } else if (item.kind == "knob") {
            const float r = (float)item.num("r", 20);
            parts.push_back({ name, juce::Rectangle<float>(2 * (r + 8.5f), 2 * (r + 8.5f)).withCentre({ cx, cy }), &item, true, true });
            parts.push_back({ name + " label", textRect(label, printFont(r >= 30 ? 13.0f : 11.0f), cx, cy + r + 10, 13), &item, true });
        } else if (item.kind == "fader") {
            const float h = (float)item.num("h", 160);
            parts.push_back({ name, { cx - 22, cy - h * 0.5f, 44, h }, &item, true });
            parts.push_back({ name + " label", textRect(label, printFont(14.0f), cx, cy + h * 0.5f + 5, 15), &item, true });
        } else if (item.kind == "enum_h" || item.kind == "enum_v") {
            const auto* def = findEngineParam(item.str("key"));
            const int n = def != nullptr ? def->nopts : 1;
            const float sw = (float)item.num("sw", 60), sh = (float)item.num("sh", 26), w = item.kind == "enum_v" ? sw : (float)n * sw + (float)(n - 1) * 3, h = item.kind == "enum_v" ? (float)n * sh + (float)(n - 1) * 3 : sh;
            parts.push_back({ name, { cx - w * 0.5f, cy - h * 0.5f, w, h }, &item, true });
            if (label.isNotEmpty()) parts.push_back({ name + " label", textRect(label, printFont(10.5f), cx, cy - h * 0.5f - 15, 12), &item, true });
        } else if (item.kind == "toggle") {
            parts.push_back({ name, { cx - 15, cy - 18, 44, 36 }, &item, true });
            parts.push_back({ name + " label", textRect(label, printFont(10.5f), cx, cy + 20, 12), &item, true });
        } else if (item.kind == "popup") {
            const float w = (float)item.num("w", 200), h = (float)item.num("h", 38);
            parts.push_back({ name, { cx - w * 0.5f, cy - h * 0.5f, w, h }, &item, true });
            if (label.isNotEmpty()) parts.push_back({ name + " label", textRect(label, printFont(10.5f), cx, cy - h * 0.5f - 15, 12), &item, true });
        } else if (item.kind == "button") {
            parts.push_back({ name, { cx - 16, cy - 16, 32, 32 }, &item, true });
        } else if (item.kind == "jack") {
            parts.push_back({ name + " " + label, { cx - 12.5f, cy - 12.5f, 25, 25 }, &item, true, true });
            parts.push_back({ name + " " + label + " label", textRect(label, printFont(9.5f, true, 0.04f), cx, cy + 15, 11, 42), &item, true });
        }
    }
    static const Item fixedItem;
    parts.push_back({ "display", { 438, 11, 614, 66 }, &fixedItem, false });
    parts.push_back({ "preset buttons", { 1066, 17, 152, 58 }, &fixedItem, false });
    parts.push_back({ "model plate", { 1262, 22, 160, 46 }, &fixedItem, false });
    parts.push_back({ "drawer page buttons", { 58, 914, 74, 55 }, &fixedItem, true });
    static Item clockButtons;
    clockButtons.attrs["page"] = "clock";
    parts.push_back({ "clock output buttons", { 150, 930, 246, 26 }, &clockButtons, true });
    parts.push_back({ "clock LCD", { 410, 884, 594, 86 }, &clockButtons, true });
    parts.push_back({ "clock SELECT", { 1060 - 30.5f, 914 - 30.5f, 61, 61 }, &clockButtons, true, true });
    parts.push_back({ "clock VALUE", { 1156 - 30.5f, 914 - 30.5f, 61, 61 }, &clockButtons, true, true });
    parts.push_back({ "clock SELECT label", { 1035, 946, 50, 13 }, &clockButtons, true });
    parts.push_back({ "clock VALUE label", { 1135, 946, 44, 13 }, &clockButtons, true });

    // Two things can only collide if they can be on screen together.
    const auto together = [](const Item& a, const Item& b) {
        if (a.str("page").isNotEmpty() && b.str("page").isNotEmpty() && a.str("page") != b.str("page")) return false;
        if (a.str("channel").isNotEmpty() && b.str("channel").isNotEmpty() && a.str("channel") != b.str("channel")) return false;
        const auto wa = a.str("when"), wb = b.str("when");
        if (wa.isNotEmpty() && wb.isNotEmpty() && wa.upToFirstOccurrenceOf(":", false, false) == wb.upToFirstOccurrenceOf(":", false, false)) {
            const auto va = wa.fromFirstOccurrenceOf(":", false, false), vb = wb.fromFirstOccurrenceOf(":", false, false);
            if (va != vb && !(va.startsWithChar('!') && va.substring(1) != vb) && !(vb.startsWithChar('!') && vb.substring(1) != va)) return false;
        }
        return true;
    };
    juce::StringArray problems;
    for (size_t i = 0; i < parts.size(); ++i) {
        for (size_t k = i + 1; k < parts.size(); ++k) {
            if (parts[i].item == parts[k].item && parts[i].item != &fixedItem) continue;   // a control and its own label
            if (!together(*parts[i].item, *parts[k].item)) continue;
            const auto clash = [](const Part& a, const Part& b) {   // a knob and a jack are round: their square corners are empty
                if (!a.r.reduced(0.5f).intersects(b.r.reduced(0.5f))) return false;
                if (a.round && b.round) return a.r.getCentre().getDistanceFrom(b.r.getCentre()) < (a.r.getWidth() + b.r.getWidth()) * 0.5f;
                if (!a.round && !b.round) return true;
                const auto& circle = a.round ? a : b; const auto& box = a.round ? b : a;
                const auto c = circle.r.getCentre();
                const juce::Point<float> nearest(juce::jlimit(box.r.getX(), box.r.getRight(), c.x), juce::jlimit(box.r.getY(), box.r.getBottom(), c.y));
                return c.getDistanceFrom(nearest) < circle.r.getWidth() * 0.5f;
            };
            if (clash(parts[i], parts[k])) problems.add(parts[i].name + " touches " + parts[k].name);
        }
        if (parts[i].hasFrame) {
            bool inside = false;
            for (const auto& f : frames) inside = inside || f.contains(parts[i].r);
            if (!inside) problems.add(parts[i].name + " leaves its frame");
        }
    }
    return problems;
}

MillstoneEditor::WearCheck MillstoneEditor::checkWear() {
    WearCounts counts;
    const auto clean = makePanelArt(false), worn = makePanelArt(true, &counts);
    const auto strict = buildHeightMap(0.0f);
    WearCheck out;
    out.tried = counts.tried;
    out.drawn = counts.drawn;
    juce::Image::BitmapData a(clean, juce::Image::BitmapData::readOnly), b(worn, juce::Image::BitmapData::readOnly);
    for (int y = 0; y < totalH * artScale; ++y)
        for (int x = 0; x < designW * artScale; ++x) {
            const auto s = strict.at((float)x / artScale, (float)y / artScale);
            const auto ca = a.getPixelColour(x, y), cb = b.getPixelColour(x, y);
            if (ca == cb) continue;
            if (s == raised) ++out.onRaised;
            else if (s == recessed) ++out.onRecessed;
            else ++out.onFlat;
        }
    return out;
}

void MillstoneEditor::paint(juce::Graphics& g) {
    g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
    const float scale = (float)getWidth() / (float)(designW * artScale);
    g.drawImageTransformed(art, juce::AffineTransform::scale(scale));
}

void MillstoneEditor::resized() {
    content.setTransform(juce::AffineTransform::scale((float)getWidth() / designW));
}
