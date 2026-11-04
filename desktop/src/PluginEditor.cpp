#include "PluginEditor.h"
#include "BinaryData.h"
#include <map>

namespace {

// layout.conf uses the MPC's coordinates, whose top 92 px belong to the MPC's own header. Our tab bar is
// shorter, so everything moves up by yShift.
constexpr int designW = 1280, designH = 680, yShift = 40;

// Colours layout.conf has no theme key for, sampled from the Key 37 Gen 2: pale button caps and the black screen.
const juce::Colour boxLine(0xff77756d), capFill(0xffefede6), capInk(0xff1c1c1b), screenFill(0xff131312),
                   screenInk(0xfff2f1ec), toggleOff(0xff6b6a63);

using Attrs = std::map<juce::String, juce::String>;
using Theme = std::map<juce::String, juce::Colour>;

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

struct Tab {
    juce::String name;
    std::vector<Item> items;
};

struct Layout {
    Theme theme;
    std::vector<Tab> tabs;
};

// Parses: `kind a=1 b="two words"`, `[tab NAME]`, `theme_x=rrggbb`. Unknown kinds (qlinks) are ignored later.
Layout parseLayout() {
    Layout out;
    juce::StringArray lines;
    lines.addLines(juce::String::fromUTF8(BinaryData::layout_conf, BinaryData::layout_confSize));
    for (auto line : lines) {
        line = line.trim();
        if (line.isEmpty() || line.startsWithChar('#')) continue;
        if (line.startsWith("theme_")) {
            out.theme[line.upToFirstOccurrenceOf("=", false, false)] =
                juce::Colour::fromString("ff" + line.fromFirstOccurrenceOf("=", false, false));
        } else if (line.startsWith("[tab ")) {
            out.tabs.push_back({ line.fromFirstOccurrenceOf("[tab ", false, false).upToLastOccurrenceOf("]", false, false), {} });
        } else if (!out.tabs.empty()) {
            juce::StringArray tokens;
            tokens.addTokens(line, " ", "\"");
            Item item;
            item.kind = tokens[0];
            for (int i = 1; i < tokens.size(); ++i)
                if (tokens[i].containsChar('='))
                    item.attrs[tokens[i].upToFirstOccurrenceOf("=", false, false)] =
                        tokens[i].fromFirstOccurrenceOf("=", false, false).unquoted();
            out.tabs.back().items.push_back(std::move(item));
        }
    }
    return out;
}

const param_t* findEngineParam(const juce::String& key) {
    for (int i = 0; i < NPARAMS; ++i)
        if (key == PARAMS[i].key) return &PARAMS[i];
    return nullptr;
}

juce::Font uiFont(float size, bool bold = false, float tracking = 0.0f) {
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain))
        .withExtraKerningFactor(tracking);
}

// A single segment or the +/- button: flat, amber when selected.
void drawFlatButton(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, juce::Colour text,
                    const juce::String& label, juce::Font font) {
    g.setColour(fill);
    g.fillRect(r);
    g.setColour(text);
    g.setFont(font);
    g.drawFittedText(label, r.toNearestInt().reduced(2, 0), juce::Justification::centred, 1, 0.8f);
}

class Segment : public juce::Button {
public:
    Segment(const juce::String& text, const Theme& t) : juce::Button(text), theme(t) {}
    void paintButton(juce::Graphics& g, bool over, bool) override {
        const bool on = getToggleState();
        auto fill = on ? theme.at("theme_accent") : (over ? capFill.darker(0.08f) : capFill);
        auto ink = on ? theme.at("theme_btn_text") : capInk;
        drawFlatButton(g, getLocalBounds().toFloat(), fill, ink, getButtonText(), uiFont(11.5f, on, 0.08f));
    }
private:
    const Theme& theme;
};

// Base for everything bound to one parameter. Size and position come from the layout item.
class Control : public juce::Component {
public:
    juce::String whenKey, whenValue;   // layout's `when=key:VALUE`
};

class KnobControl : public Control {
public:
    KnobControl(const Item& item, juce::AudioProcessorValueTreeState& apvts, const Theme& t) {
        const auto key = item.str("key");
        setInterceptsMouseClicks(false, true);
        const int r = item.num("r", 34);
        const int boxW = item.num("bw", 130);
        const int w = juce::jmax(2 * r, boxW);
        setBounds(item.num("cx") - w / 2, item.num("cy") - r - yShift, w, 2 * r + 57);

        slider.setComponentID(key);
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setMouseDragSensitivity(220);
        slider.setBounds(w / 2 - r, 0, 2 * r, 2 * r);
        addAndMakeVisible(slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, key, slider);

        if (auto* p = apvts.getParameter(key))
            slider.setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));

        name.setText(item.str("label"), juce::dontSendNotification);
        name.setFont(uiFont(15.5f));
        name.setJustificationType(juce::Justification::centred);
        name.setMinimumHorizontalScale(0.75f);
        name.setBorderSize({});
        name.setColour(juce::Label::textColourId, t.at("theme_ink"));
        name.setInterceptsMouseClicks(false, false);
        name.setBounds(0, 2 * r + 3, w, 20);
        addAndMakeVisible(name);

        const auto* ep = findEngineParam(key);
        isInt = ep != nullptr && ep->int_display;
        value.setEditable(false, true);   // double-click to type a value
        value.setFont(uiFont(14.0f));
        value.setJustificationType(juce::Justification::centred);
        value.setColour(juce::Label::textColourId, t.at("theme_ink"));
        value.setColour(juce::Label::outlineColourId, boxLine);
        value.setColour(juce::Label::textWhenEditingColourId, capInk);
        value.setColour(juce::Label::backgroundWhenEditingColourId, capFill);
        value.setColour(juce::Label::outlineWhenEditingColourId, t.at("theme_accent"));
        value.setBounds((w - boxW) / 2, 2 * r + 27, boxW, 30);
        value.onTextChange = [this] {
            slider.setValue(value.getText().getDoubleValue(), juce::sendNotificationSync);
            refresh();   // shows the clamped value if the typed one was out of range
        };
        addAndMakeVisible(value);

        slider.onValueChange = [this] { refresh(); };
        refresh();
    }

private:
    void refresh() {
        const auto v = slider.getValue();
        value.setText(isInt ? juce::String(juce::roundToInt(v)) : juce::String(v, 1), juce::dontSendNotification);
    }

    juce::Slider slider;
    juce::Label name, value;
    bool isInt = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ENUM rows and columns, as one button per option.
class EnumControl : public Control {
public:
    EnumControl(const Item& item, bool vertical, juce::AudioProcessorValueTreeState& apvts, const Theme& t) : theme(t) {
        const auto key = item.str("key");
        auto* param = apvts.getParameter(key);
        jassert(param != nullptr);
        const auto choices = param->getAllValueStrings();
        const int n = choices.size();
        const int segW = vertical ? 120 : item.num("sw", 60), segH = vertical ? 30 : 32, gap = 2;
        const int groupW = vertical ? segW : n * segW + (n - 1) * gap;
        const int groupH = vertical ? n * segH + (n - 1) * gap : segH;
        const int labelH = 18;
        setBounds(item.num("cx") - groupW / 2, item.num("cy") - (vertical ? groupH / 2 : segH / 2) - labelH - yShift,
                  groupW, groupH + labelH);
        setInterceptsMouseClicks(false, true);

        title = item.str("label");
        attachment = std::make_unique<juce::ParameterAttachment>(*param, [this](float v) { select(juce::roundToInt(v)); });
        for (int i = 0; i < n; ++i) {
            auto* b = segments.add(new Segment(choices[i], theme));
            b->setComponentID(key + ":" + juce::String(i));
            b->setBounds(vertical ? 0 : i * (segW + gap), labelH + (vertical ? i * (segH + gap) : 0), segW, segH);
            b->onClick = [this, i] { attachment->setValueAsCompleteGesture((float)i); };
            addAndMakeVisible(b);
        }
        attachment->sendInitialUpdate();
    }

    void paint(juce::Graphics& g) override {
        g.setColour(theme.at("theme_ink_dim"));
        g.setFont(uiFont(10.5f, true, 0.25f));
        g.drawText(title, 0, 0, getWidth(), 14, juce::Justification::centred);
    }

private:
    void select(int index) {
        for (int i = 0; i < segments.size(); ++i)
            segments[i]->setToggleState(i == index, juce::dontSendNotification);
    }

    const Theme& theme;
    juce::String title;
    juce::OwnedArray<Segment> segments;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

class ToggleControl : public Control {
    struct Switch : juce::Button {
        Switch(const Theme& t) : juce::Button("switch"), theme(t) {}
        void paintButton(juce::Graphics& g, bool, bool) override {
            auto r = getLocalBounds().toFloat();
            g.setColour(screenFill);
            g.fillRect(r);
            const float d = r.getHeight() - 6.0f;
            const bool on = getToggleState();
            const auto knob = juce::Rectangle<float>(d, d).withCentre({ on ? r.getRight() - d / 2 - 3 : d / 2 + 3, r.getCentreY() });
            g.setColour(theme.at("theme_knob_ring"));
            g.fillEllipse(knob.expanded(1.5f));
            g.setColour(on ? juce::Colour(0xffe0452a) : toggleOff);
            g.fillEllipse(knob);
        }
        const Theme& theme;
    };

public:
    ToggleControl(const Item& item, juce::AudioProcessorValueTreeState& apvts, const Theme& t) : sw(t) {
        const auto key = item.str("key");
        auto* param = apvts.getParameter(key);
        jassert(param != nullptr);
        setBounds(item.num("cx") - 40, item.num("cy") - 13 - yShift, 80, 50);
        setInterceptsMouseClicks(false, true);
        sw.setComponentID(key);
        sw.setBounds(15, 0, 50, 26);
        sw.onClick = [this] { attachment->setValueAsCompleteGesture(sw.getToggleState() ? 0.0f : 1.0f); };
        addAndMakeVisible(sw);

        name.setText(item.str("label"), juce::dontSendNotification);
        name.setFont(uiFont(14.0f));
        name.setJustificationType(juce::Justification::centred);
        name.setBorderSize({});
        name.setColour(juce::Label::textColourId, t.at("theme_ink"));
        name.setBounds(0, 30, 80, 18);
        addAndMakeVisible(name);

        attachment = std::make_unique<juce::ParameterAttachment>(*param, [this](float v) {
            sw.setToggleState(v >= 0.5f, juce::dontSendNotification);
        });
        attachment->sendInitialUpdate();
    }

private:
    Switch sw;
    juce::Label name;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// A model or division chooser: shows the current option and opens a menu.
class PopupControl : public Control {
public:
    PopupControl(const Item& item, juce::AudioProcessorValueTreeState& apvts, const Theme& t, juce::LookAndFeel& menuLook)
        : theme(t), look(menuLook) {
        const auto key = item.str("key");
        param = apvts.getParameter(key);
        jassert(param != nullptr);
        setComponentID(key);
        cols = juce::jmax(1, item.num("cols", 1));
        title = item.str("label");
        const int w = item.num("w", 200), h = item.num("h", 48);
        setBounds(item.num("cx") - w / 2, item.num("cy") - h / 2 - titleH - yShift, w, h + titleH);
        attachment = std::make_unique<juce::ParameterAttachment>(*param, [this](float v) {
            current = juce::roundToInt(v);
            repaint();
        });
        attachment->sendInitialUpdate();
    }

    void paint(juce::Graphics& g) override {
        g.setColour(theme.at("theme_ink_dim"));
        g.setFont(uiFont(10.5f, true, 0.25f));
        g.drawText(title, 2, 0, getWidth() - 2, 14, juce::Justification::centredLeft);

        auto box = getLocalBounds().withTrimmedTop(titleH).toFloat();
        g.setColour(screenFill);
        g.fillRect(box);
        g.setColour(boxLine);
        g.drawRect(box);

        auto arrow = box.removeFromRight(36.0f);
        g.drawVerticalLine((int)arrow.getX(), box.getY(), box.getBottom());
        juce::Path tri;
        tri.addTriangle(arrow.getCentreX() - 8, arrow.getCentreY() - 4, arrow.getCentreX() + 8, arrow.getCentreY() - 4,
                        arrow.getCentreX(), arrow.getCentreY() + 6);
        g.setColour(screenInk);
        g.fillPath(tri);

        g.setColour(screenInk);
        g.setFont(uiFont(17.0f));
        g.drawFittedText(param->getAllValueStrings()[current], box.reduced(12, 0).toNearestInt(), juce::Justification::centredLeft, 1);
    }

    void mouseDown(const juce::MouseEvent&) override {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&look);
        const auto choices = param->getAllValueStrings();
        for (int i = 0; i < choices.size(); ++i)
            menu.addItem(i + 1, choices[i], true, i == current);
        juce::Component::SafePointer<PopupControl> self(this);
        menu.showMenuAsync(juce::PopupMenu::Options()
                               .withTargetComponent(this)
                               .withMinimumNumColumns(cols)
                               .withMaximumNumColumns(cols)
                               .withStandardItemHeight(26),
                           [self](int picked) {
                               if (self != nullptr && picked > 0)
                                   self->attachment->setValueAsCompleteGesture((float)(picked - 1));
                           });
    }

private:
    static constexpr int titleH = 16;
    const Theme& theme;
    juce::LookAndFeel& look;
    juce::RangedAudioParameter* param = nullptr;
    juce::String title;
    int cols = 1, current = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// The - / + buttons. Same behaviour as the MPC build's step params: wraps around the list.
class StepControl : public Control {
public:
    StepControl(const Item& item, juce::AudioProcessorValueTreeState& apvts, const Theme& t) : button(item.str("label")), theme(t) {
        const auto* ep = findEngineParam(item.str("key"));
        jassert(ep != nullptr && ep->step_target >= 0);
        target = apvts.getParameter(PARAMS[ep->step_target].key);
        delta = juce::roundToInt(ep->step_delta);
        setBounds(item.num("cx") - 23, item.num("cy") - 19 - yShift, 46, 38);
        button.theme = &theme;
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
    struct Flat : juce::Button {
        explicit Flat(const juce::String& text) : juce::Button(text) {}
        void paintButton(juce::Graphics& g, bool over, bool down) override {
            auto fill = capFill;
            if (down) fill = fill.darker(0.2f);
            else if (over) fill = fill.darker(0.08f);
            drawFlatButton(g, getLocalBounds().toFloat(), fill, capInk, getButtonText(), uiFont(20.0f, true));
        }
        const Theme* theme = nullptr;
    };
    Flat button;
    const Theme& theme;
    juce::RangedAudioParameter* target = nullptr;
    int delta = 0;
};

} // namespace

struct MillstoneEditor::LookAndFeel : juce::LookAndFeel_V4 {
    explicit LookAndFeel(const Theme& t) : theme(t) {
        setColour(juce::PopupMenu::backgroundColourId, theme.at("theme_panel"));
        setColour(juce::PopupMenu::textColourId, theme.at("theme_ink"));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, theme.at("theme_accent"));
        setColour(juce::PopupMenu::highlightedTextColourId, theme.at("theme_btn_text"));
        setColour(juce::ComboBox::backgroundColourId, screenFill);
        setColour(juce::ComboBox::textColourId, screenInk);
        setColour(juce::ComboBox::outlineColourId, boxLine);
        setColour(juce::ComboBox::arrowColourId, screenInk);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider&) override {
        const auto area = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(0.5f);
        const float r = juce::jmin(area.getWidth(), area.getHeight()) / 2;
        const auto c = area.getCentre();
        g.setColour(theme.at("theme_knob_face"));
        g.fillEllipse(area.withSizeKeepingCentre(2 * r, 2 * r));
        // The mockup's dot, sweeping 7 o'clock to 5 o'clock.
        const float angle = juce::degreesToRadians(-135.0f + pos * 270.0f);
        const float dotR = r * 0.17f;
        const auto dot = c.getPointOnCircumference(r * 0.72f, angle);
        g.setColour(theme.at("theme_knob_dot"));
        g.fillEllipse(juce::Rectangle<float>(2 * dotR, 2 * dotR).withCentre(dot));
    }

    juce::Font getPopupMenuFont() override { return uiFont(15.0f); }

    const Theme& theme;
};

struct MillstoneEditor::Page : juce::Component {
    Page(const Tab& tab, MillstoneProcessor& proc, const Theme& t, juce::LookAndFeel& look) : theme(t) {
        setInterceptsMouseClicks(false, true);
        for (const auto& item : tab.items) {
            std::unique_ptr<Control> c;
            const auto key = item.str("key");
            if (item.kind == "frame" || item.kind == "text") statics.push_back(item);
            else if (item.kind == "knob") c = std::make_unique<KnobControl>(item, proc.apvts, theme);
            else if (item.kind == "enum_v") c = std::make_unique<EnumControl>(item, true, proc.apvts, theme);
            else if (item.kind == "enum_h") c = std::make_unique<EnumControl>(item, false, proc.apvts, theme);
            else if (item.kind == "toggle") c = std::make_unique<ToggleControl>(item, proc.apvts, theme);
            else if (item.kind == "popup") c = std::make_unique<PopupControl>(item, proc.apvts, theme, look);
            else if (item.kind == "button") c = std::make_unique<StepControl>(item, proc.apvts, theme);
            if (c == nullptr) continue;
            const auto when = item.str("when");
            c->whenKey = when.upToFirstOccurrenceOf(":", false, false);
            c->whenValue = when.fromFirstOccurrenceOf(":", false, false);
            addAndMakeVisible(*c);
            controls.push_back(std::move(c));
        }
    }

    void paint(juce::Graphics& g) override {
        for (const auto& s : statics) {
            if (s.kind == "frame") {
                const auto r = juce::Rectangle<int>(s.num("x"), s.num("y") - yShift, s.num("w"), s.num("h"));
                g.setColour(theme.at("theme_line"));
                g.drawRect(r);
                g.setColour(theme.at("theme_ink"));
                g.setFont(uiFont(11.0f, true, 0.3f));
                g.drawText(s.str("title"), r.getX() + 20, r.getY() + 12, r.getWidth() - 40, 16, juce::Justification::centredLeft);
                g.setColour(boxLine);
                g.fillRect(r.getX() + 18, r.getY() + 36, r.getWidth() - 36, 1);
            } else {
                const float size = (float)s.str("size", "1").getDoubleValue();
                const bool big = size > 2.0f;
                g.setColour(juce::Colour::fromString("ff" + s.str("color", "ece9e2")));
                g.setFont(uiFont(juce::jmax(10.5f, size * 8.0f), big, big ? 0.55f : 0.14f));
                g.drawText(s.str("label"), s.num("cx") - 130, s.num("cy") - yShift - 15, 260, 30, juce::Justification::centred);
            }
        }
    }

    // Applies `when=` conditions, e.g. the LFO's RATE knob swapping with DIVISION when SYNC is on.
    void updateVisibility(MillstoneProcessor& proc) {
        for (auto& c : controls) {
            if (c->whenKey.isEmpty()) continue;
            auto* p = proc.apvts.getParameter(c->whenKey);
            c->setVisible(p != nullptr && p->getCurrentValueAsText() == c->whenValue);
        }
    }

    const Theme& theme;
    std::vector<Item> statics;
    std::vector<std::unique_ptr<Control>> controls;
};

namespace {
const Layout& layout() {
    static const Layout parsed = parseLayout();
    return parsed;
}
}

// Category filter, preset list, previous/next, Save and Delete. Sits to the right of the tabs.
struct MillstoneEditor::PresetBar : juce::Component {
    explicit PresetBar(MillstoneProcessor& p) : processor(p) {
        for (auto* b : {&previous, &next, &save, &remove}) {
            b->setColour(juce::TextButton::buttonColourId, capFill);
            b->setColour(juce::TextButton::textColourOffId, capInk);
            addAndMakeVisible(*b);
        }
        category.setComponentID("preset_category");
        presetList.setComponentID("preset_list");
        previous.setComponentID("preset_prev");
        next.setComponentID("preset_next");
        presetList.setTextWhenNothingSelected("Custom");
        addAndMakeVisible(category);
        addAndMakeVisible(presetList);
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
        auto r = getLocalBounds();
        category.setBounds(r.removeFromLeft(112)); r.removeFromLeft(4);
        remove.setBounds(r.removeFromRight(52)); r.removeFromRight(2);
        save.setBounds(r.removeFromRight(46)); r.removeFromRight(4);
        next.setBounds(r.removeFromRight(26)); r.removeFromRight(2);
        previous.setBounds(r.removeFromRight(26)); r.removeFromRight(4);
        presetList.setBounds(r);
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

    const Entry* find(const juce::String& name) const {
        for (const auto& e : entries) if (e.name == name) return &e;
        return nullptr;
    }

    void rebuild() {
        const auto shown = category.getSelectedId() > 1 ? category.getText() : juce::String();
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
        const int restored = seen.indexOf(shown);
        category.setSelectedId(restored >= 0 ? restored + 2 : 1, juce::dontSendNotification);
    }

    void fillList() {
        presetList.clear(juce::dontSendNotification);
        const bool all = category.getSelectedId() <= 1;
        const auto shown = category.getText();
        for (int i = 0; i < (int)entries.size(); ++i)
            if (all || entries[(size_t)i].category == shown)
                presetList.addItem(entries[(size_t)i].name, i + 1);
        const auto name = processor.getCurrentPresetName();
        for (int i = 0; i < (int)entries.size(); ++i)
            if (entries[(size_t)i].name == name && (all || entries[(size_t)i].category == shown))
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
    juce::TextButton previous { "<" }, next { ">" }, save { "Save" }, remove { "Delete" };
    std::vector<Entry> entries;
    juce::String lastName;
    std::unique_ptr<juce::AlertWindow> dialog;
};

MillstoneEditor::MillstoneEditor(MillstoneProcessor& p)
    : juce::AudioProcessorEditor(p), processor(p), lnf(std::make_unique<LookAndFeel>(layout().theme)) {
    setLookAndFeel(lnf.get());
    content.setBounds(0, 0, designW, designH);
    addAndMakeVisible(content);

    int x = 10;
    int index = 0;
    for (const auto& tab : layout().tabs) {
        auto* b = tabButtons.add(new juce::TextButton(tab.name));
        b->setBounds(x, 10, 132, 32);
        b->setColour(juce::TextButton::buttonColourId, capFill);
        b->setColour(juce::TextButton::buttonOnColourId, layout().theme.at("theme_accent"));
        b->setColour(juce::TextButton::textColourOffId, capInk);
        b->setColour(juce::TextButton::textColourOnId, layout().theme.at("theme_btn_text"));
        b->setConnectedEdges(0);
        b->onClick = [this, index] { showTab(index); };
        content.addAndMakeVisible(b);
        x += 134;

        auto page = std::make_unique<Page>(tab, p, layout().theme, *lnf);
        page->setBounds(0, 0, designW, designH);
        content.addChildComponent(*page);
        pages.push_back(std::move(page));
        ++index;
    }

    presetBar = std::make_unique<PresetBar>(processor);
    presetBar->setBounds(820, 10, 450, 32);
    content.addAndMakeVisible(*presetBar);

    showTab(processor.uiTab);
    timerCallback();
    startTimerHz(15);

    setResizable(true, true);
    setResizeLimits(designW / 2, designH / 2, designW * 2, designH * 2);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio((double)designW / designH);
    setSize(designW, designH);
}

MillstoneEditor::~MillstoneEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void MillstoneEditor::showTab(int index) {
    index = juce::jlimit(0, (int)pages.size() - 1, index);
    processor.uiTab = index;
    for (int i = 0; i < (int)pages.size(); ++i) {
        pages[(size_t)i]->setVisible(i == index);
        tabButtons[i]->setToggleState(i == index, juce::dontSendNotification);
    }
}

void MillstoneEditor::timerCallback() {
    for (auto& page : pages) page->updateVisibility(processor);
    presetBar->sync();
}

void MillstoneEditor::paint(juce::Graphics& g) {
    g.fillAll(layout().theme.at("theme_bg"));
}

void MillstoneEditor::resized() {
    content.setTransform(juce::AffineTransform::scale((float)getWidth() / designW, (float)getHeight() / designH));
}
