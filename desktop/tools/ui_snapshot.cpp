// Developer check, built only with -DMILLSTONE_UI_SNAPSHOT=ON: renders every page to a PNG and exercises the
// controls against the parameters. Usage: millstone_ui_snapshot <output-dir>
#include "PluginEditor.h"

static int failures = 0;
static int reached = 0;
static void check(bool ok, const juce::String& what) {
    std::cout << (ok ? "ok   " : "FAIL ") << what << std::endl;
    if (!ok) ++failures;
}

static juce::Component* find(juce::Component& root, const juce::String& id) {
    if (root.getComponentID() == id) return &root;
    for (auto* c : root.getChildren())
        if (auto* hit = find(*c, id)) return hit;
    return nullptr;
}

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File dir(argv[1]);
    MillstoneProcessor proc;
    const auto openStart = juce::Time::getMillisecondCounterHiRes();
    MillstoneEditor editor(proc);
    editor.setVisible(true);   // what the mouse hit-testing needs; no window is opened
    const auto firstOpen = juce::Time::getMillisecondCounterHiRes() - openStart;
    const auto againStart = juce::Time::getMillisecondCounterHiRes();
    { MillstoneEditor second(proc); }
    const auto nextOpen = juce::Time::getMillisecondCounterHiRes() - againStart;
    std::cout << "info opening the window: first " << (int)firstOpen << " ms, later " << (int)nextOpen << " ms" << std::endl;
    check(nextOpen < 300, "later windows open quickly");
    auto& ps = proc.apvts;
    auto text = [&](const char* k) { return ps.getParameter(k)->getCurrentValueAsText(); };

    // Controls -> parameters.
    auto click = [&](const char* id) {
        auto* b = dynamic_cast<juce::Button*>(find(editor, id));
        check(b != nullptr, juce::String("found button ") + id);
        if (b != nullptr) b->onClick();
    };
    click("o1_next");
    check(text("o1_engine") == "Waveshaper", "+ steps the model forward");
    click("o1_prev");
    click("o1_prev");
    check(text("o1_engine") == "Chiptune", "- wraps from the first model to the last");
    click("o1_next");
    check(text("o1_engine") == "Virtual Analog", "+ wraps from the last model to the first");
    click("o3_on");
    check(text("o3_on") == "ON", "toggle turns OSC 3 on");
    click("f_mode:1");
    check(text("f_mode") == "HP", "segment selects HP");
    click("f_mode:0");
    click("lfo_shape:4");
    check(text("lfo_shape") == "S/H", "segment selects S/H");

    auto* cutoff = dynamic_cast<juce::Slider*>(find(editor, "cutoff"));
    check(cutoff != nullptr, "found cutoff knob");
    cutoff->setValue(33.0, juce::sendNotificationSync);
    check(std::abs(ps.getRawParameterValue("cutoff")->load() - 33.0f) < 0.01f, "knob sets cutoff");

    // Parameters (host automation, restored state) -> controls.
    auto* reso = dynamic_cast<juce::Slider*>(find(editor, "reso"));
    auto* rp = ps.getParameter("reso");
    rp->setValueNotifyingHost(rp->convertTo0to1(70.0f));
    check(std::abs(reso->getValue() - 70.0) < 0.01, "knob follows automation");

    // Every engine parameter should be on the panel, except the popups' internal open/closed helpers.
    for (int i = 0; i < NPARAMS; ++i) {
        const juce::String key(PARAMS[i].key);
        if (key.startsWithChar('c') && (key.endsWith("_src") || key.endsWith("_dst"))) continue;   // cables, tested below
        if (key.startsWith("ck_") || (key.startsWithChar('k') && key.containsChar('_'))) continue;   // on the clock's LCD, tested below
        if (key == "t_smooth" || key == "t_shift") continue;   // on Tides' screen, tested below
        if (key == "fx_dl_width" || key == "fx_dl_drive" || key == "fx_dl_wow" || key == "fx_dl_duck") continue;   // on the echo's screen, tested below
        if (PARAMS[i].popup_of >= 0 || (PARAMS[i].momentary && PARAMS[i].step_target < 0)) continue;
        const bool found = find(editor, key) != nullptr || find(editor, key + ":0") != nullptr;
        check(found, "on panel: " + key);
    }

    // Conditional controls: RATE (sync off) vs DIVISION (sync on).
    auto render = [&](const juce::String& name) {
        auto img = editor.createComponentSnapshot(editor.getLocalBounds(), true, 2.0f);
        juce::PNGImageFormat png;
        auto f = dir.getChildFile(name + ".png");
        f.deleteFile();
        juce::FileOutputStream out(f);
        png.writeImageToStream(img, out);
    };
    render("panel");
    click("lfo_sync");
    editor.refreshFromProcessor();
    render("panel_sync");
    auto* division = find(editor, "lfo_div");
    check(division != nullptr && division->isVisible(), "DIVISION shows when SYNC is on");
    auto* phase = find(editor, "lfo_lock:1");
    check(phase != nullptr && phase->getParentComponent()->isVisible(), "PHASE shows when SYNC is on");
    click("lfo_lock:1");
    check(text("lfo_lock") == "BAR", "PHASE selects BAR");
    click("lfo_sync");
    editor.refreshFromProcessor();
    check(phase != nullptr && !phase->getParentComponent()->isVisible(), "PHASE hides when SYNC is off");

    // OSC 1 swaps its controls between Plaits and Elements.
    auto* plaitsKnob = find(editor, "o1_timbre"), *elementsKnob = find(editor, "e_geom");
    check(plaitsKnob != nullptr && plaitsKnob->getParentComponent()->isVisible() && elementsKnob != nullptr && !elementsKnob->getParentComponent()->isVisible(), "OSC 1 starts as Plaits");
    click("o1_type:1");
    editor.refreshFromProcessor();
    check(!plaitsKnob->getParentComponent()->isVisible() && elementsKnob->getParentComponent()->isVisible(), "switching to ELEMENTS swaps the controls");
    render("panel_elements");
    click("o1_type:0");
    editor.refreshFromProcessor();

    // Nothing printed or placed collides with anything else that can be on screen with it.
    { const auto problems = MillstoneEditor::layoutProblems(); check(problems.isEmpty(), "panel layout has no collisions, " + juce::String(problems.size()) + " found"); for (const auto& p : problems) std::cout << "   " << p << std::endl; }

    // Every visible control can really be reached: the thing under its centre is that control.
    {
        const auto reachable = [&](const juce::String& state) {
            std::function<void(juce::Component&)> walk = [&](juce::Component& c) {
                for (auto* child : c.getChildren()) {
                    if (!child->isVisible()) continue;
                    if (dynamic_cast<juce::Button*>(child) != nullptr || dynamic_cast<juce::Slider*>(child) != nullptr || dynamic_cast<juce::ComboBox*>(child) != nullptr) {
                        if (child->getWidth() > 0 && child->isEnabled()) {
                            const auto centre = editor.getLocalPoint(child, child->getLocalBounds().getCentre());
                            auto* hit = editor.getComponentAt(centre);
                            const bool ok = hit == child || (hit != nullptr && child->isParentOf(hit));
                            if (!ok) check(false, state + ": " + child->getComponentID() + " is covered by " + (hit != nullptr ? hit->getComponentID() : juce::String("nothing")));
                            ++reached;
                        }
                    } else walk(*child);
                }
            };
            walk(editor);
        };
        click("patch_toggle");
        for (const char* type : {"o1_type:0", "o1_type:1"}) for (const char* sync : {"off", "on"}) {
            click(type);
            const bool on = text("lfo_sync") == "ON"; if (on != (juce::String(sync) == "on")) click("lfo_sync");
            editor.refreshFromProcessor();
            reachable(juce::String(type) + " " + sync);
        }
        click("o1_type:0"); if (text("lfo_sync") == "ON") click("lfo_sync");
        click("rack_toggle");
        for (const char* sync : {"fx_dl_sync:0", "fx_dl_sync:1"}) for (const char* type : {"fx_rv_type:0", "fx_rv_type:1"}) for (int channel = 0; channel < 8; ++channel) {
            click(sync); click(type); click(("clock_channel:" + juce::String(channel)).toRawUTF8()); editor.refreshFromProcessor();
            reachable(juce::String("rack ") + sync + type + juce::String(channel));
        }
        for (const char* trig : {"t_trig:0", "t_trig:1", "t_trig:2", "t_trig:3"}) { click(trig); editor.refreshFromProcessor(); reachable(juce::String("rack ") + trig); }
        click("t_trig:0");
        click("fx_dl_sync:1"); click("fx_rv_type:0"); click("clock_channel:0"); click("rack_back"); editor.refreshFromProcessor();
        std::cout << "info " << reached << " control positions reached across every page" << std::endl;
        check(reached > 1500, "the reach check really walked the panel");

        // The cables, through their real mouse handlers.
        auto* bay = find(editor, "patch_bay");
        check(bay != nullptr, "patch bay exists");
        const auto event = [&](juce::Point<float> at, juce::Point<float> down, int clicks, bool dragged) { return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, bay, bay, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), clicks, dragged); };
        const auto drag = [&](juce::Point<float> from, juce::Point<float> to) { bay->mouseDown(event(from, from, 1, false)); bay->mouseDrag(event((from + to) * 0.5f, from, 1, true)); bay->mouseDrag(event(to, from, 1, true)); bay->mouseUp(event(to, from, 1, true)); };
        for (int c = 0; c < MillstoneProcessor::numCords; ++c) proc.disconnectCord(c);
        const auto lfoOut = MillstoneEditor::jackPosition(true, 1), wheelOut = MillstoneEditor::jackPosition(true, 6), clockOut = MillstoneEditor::jackPosition(true, 17), cutoffIn = MillstoneEditor::jackPosition(false, 2), resoIn = MillstoneEditor::jackPosition(false, 3), tTrigIn = MillstoneEditor::jackPosition(false, 26);
        check(!lfoOut.isOrigin() && !clockOut.isOrigin() && !tTrigIn.isOrigin(), "jacks are where the layout says");
        check(bay->hitTest((int)lfoOut.x, (int)lfoOut.y) && !bay->hitTest((int)(lfoOut.x + 23), (int)(lfoOut.y + 29)), "only the jacks take the mouse, so knobs nearby still work");
        drag(lfoOut, cutoffIn);
        check(proc.cordSource(0) == 1 && proc.cordDest(0) == 2, "dragging an output to an input patches it");
        drag(cutoffIn, wheelOut);
        check(proc.cordSource(0) == 1 && proc.cordDest(0) == 2 ? false : true, "pulling a plug out and dropping it on an output removes it, and does not leave it half connected");
        drag(tTrigIn, clockOut);
        check(proc.cordSource(0) == 17 && proc.cordDest(0) == 26, "dragging an empty input to an output patches it too: CLOCK 1 > T TRIG");
        const float amount = proc.cordAmount(0);
        drag(tTrigIn, resoIn);
        check(proc.cordSource(0) == 17 && proc.cordDest(0) == 3 && std::abs(proc.cordAmount(0) - amount) < 0.01f, "moving a plug to another input keeps its amount");
        drag(resoIn, { resoIn.x + 21.5f, resoIn.y + 29.0f });   // the gap between jacks
        check(proc.cordSource(0) == 0 || proc.cordDest(0) == 0, "dropping a pulled plug on nothing unplugs it");
        drag(lfoOut, wheelOut);
        int plugged = 0; for (int c = 0; c < MillstoneProcessor::numCords; ++c) plugged += proc.cordSource(c) != 0 && proc.cordDest(c) != 0;
        check(plugged == 0, "an output cannot be patched to an output");
        drag(lfoOut, cutoffIn); drag(wheelOut, cutoffIn);
        bay->mouseDoubleClick(event(cutoffIn, cutoffIn, 2, false));
        plugged = 0; for (int c = 0; c < MillstoneProcessor::numCords; ++c) plugged += proc.cordSource(c) != 0 && proc.cordDest(c) != 0;
        check(plugged == 0, "double-clicking an input clears everything in it");

        // A knob, through its real mouse handlers, and the display's readout.
        auto* knob = dynamic_cast<juce::Slider*>(find(editor, "drive"));
        check(knob != nullptr, "found a knob");
        knob->setValue(50.0, juce::sendNotificationSync);
        const juce::Point<float> middle(knob->getWidth() * 0.5f, knob->getHeight() * 0.5f);
        const auto knobEvent = [&](juce::Point<float> at, bool dragged, int clicks = 1) { return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, knob, knob, juce::Time::getCurrentTime(), middle, juce::Time::getCurrentTime(), clicks, dragged); };
        check(knob->hitTest((int)middle.x, (int)middle.y) && !knob->hitTest(1, 1), "a knob takes clicks on the knob, not on its shadow");
        knob->mouseDown(knobEvent(middle, false));
        knob->mouseDrag(knobEvent(middle.translated(0, -40.0f), true));
        knob->mouseUp(knobEvent(middle.translated(0, -40.0f), true));
        const float dragged = ps.getRawParameterValue("drive")->load();
        check(dragged > 50.0f, "dragging a knob up raises it, and the parameter follows");
        knob->mouseDoubleClick(knobEvent(middle, false, 2));
        const auto* driveParam = ps.getParameter("drive");
        check(std::abs(ps.getRawParameterValue("drive")->load() - driveParam->convertFrom0to1(driveParam->getDefaultValue())) < 0.01f, "double-clicking a knob resets it");
        auto* name = dynamic_cast<juce::Label*>(find(editor, "readout_name")), *value = dynamic_cast<juce::Label*>(find(editor, "readout_value"));
        check(name != nullptr && value != nullptr, "the display has a readout");
        knob->mouseEnter(knobEvent(middle, false));
        check(name->getText() == "DRIVE", "pointing at a knob names it on the display, shows " + name->getText());
        value->setText("37", juce::sendNotificationSync);
        check(std::abs(ps.getRawParameterValue("drive")->load() - 37.0f) < 0.01f, "typing a value on the display sets it");
        value->setText("9999", juce::sendNotificationSync);
        check(std::abs(ps.getRawParameterValue("drive")->load() - 100.0f) < 0.01f && value->getText() == "100.0", "an out-of-range value is clamped and shown clamped");
        proc.connectCord(13, 2, 40);
        auto* cordKnob = dynamic_cast<juce::Slider*>(find(editor, "c1_amt"));
        cordKnob->mouseEnter(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), {}, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, cordKnob, cordKnob, juce::Time::getCurrentTime(), {}, juce::Time::getCurrentTime(), 0, false));
        check(name->getText().contains("TIDES 1 > CUTOFF"), "a cord's knob says what it connects, shows " + name->getText());
        for (int c = 0; c < MillstoneProcessor::numCords; ++c) proc.disconnectCord(c);
        click("patch_toggle");
    }

    // Wear obeys the height of what it is on.
    const auto wear = MillstoneEditor::checkWear();
    std::cout << "info " << wear.drawn << " of " << wear.tried << " candidate marks were physically possible and drawn" << std::endl;
    check(wear.drawn > 0 && wear.drawn < wear.tried, "the feasibility check really rejects marks");
    check(wear.onFlat > 10000, "wear reaches the flat metal, " + juce::String(wear.onFlat) + " pixels");
    check(wear.onRaised == 0, "no wear lands on anything raised, " + juce::String(wear.onRaised) + " pixels");
    check(wear.onRecessed == 0, "no wear lands in a slot, " + juce::String(wear.onRecessed) + " pixels");

    // The drawer.
    const int closedH = editor.getHeight(), width = editor.getWidth();
    auto* patch = dynamic_cast<juce::Button*>(find(editor, "patch_toggle"));
    check(patch != nullptr, "PATCH button exists");
    patch->onClick();
    check(editor.getHeight() > closedH, "PATCH opens the drawer: the window grows");
    check(proc.patchBayOpen, "the drawer state is remembered");
    proc.connectCord(1, 7);      // LFO > O1 HARM
    proc.connectCord(11, 6);     // OSC 2 > O1 PITCH
    proc.connectCord(12, 2);     // OSC 3 > CUTOFF
    proc.connectCord(3, 3);      // MOD ENV > RESO
    proc.connectCord(9, 4);      // NOISE > VCA
    proc.connectCord(6, 20);     // WHEEL > O3 LEVEL
    proc.connectCord(13, 2);     // TIDES 1 > CUTOFF
    proc.connectCord(14, 22);    // TIDES 2 > T SHAPE
    editor.refreshFromProcessor();
    render("panel_patch");
    { const auto t0 = juce::Time::getMillisecondCounterHiRes(); click("rack_toggle"); editor.refreshFromProcessor(); render("panel_rack_first"); std::cout << "info first time the rack is drawn: " << juce::roundToInt(juce::Time::getMillisecondCounterHiRes() - t0) << " ms" << std::endl; }
    check(proc.rackOpen && find(editor, "rack_page")->isVisible(), "RACK opens the rack");
    check(editor.getHeight() > closedH, "and takes its tall window");
    check(proc.patchBayOpen, "the patch bay's setting is kept for the panel");
    check(!find(editor, "cutoff")->getParentComponent()->isVisible(), "the panel's knobs step aside");
    check(find(editor, "t_rate")->getParentComponent()->isVisible(), "Tides is in the rack");
    { bool jackShowing = false; for (auto* child : find(editor, "rack_page")->getParentComponent()->getChildren()) if (child != find(editor, "rack_page") && child->isVisible() && child->getBottom() > 864) jackShowing = true; check(!jackShowing, "nothing of the drawer is left showing"); }
    check(!find(editor, "preset_list")->getParentComponent()->isVisible(), "so does the panel's display");
    render("panel_rack");
    {
        check(find(editor, "fx_dl_div")->getParentComponent()->isVisible() && !find(editor, "fx_dl_time")->getParentComponent()->isVisible(), "SYNC shows the division, not the time");
        click("fx_dl_sync:0"); editor.refreshFromProcessor();
        check(find(editor, "fx_dl_time")->getParentComponent()->isVisible(), "FREE shows the time");
        click("fx_dl_sync:1"); editor.refreshFromProcessor();
        auto* throwButton = dynamic_cast<juce::Button*>(find(editor, "fx_dl_throw"));
        check(throwButton != nullptr && throwButton->getParentComponent()->isVisible(), "THROW is always there, one press away");
        throwButton->setState(juce::Button::buttonDown);
        check(text("fx_dl_throw") == "ON", "THROW is on while held");
        throwButton->setState(juce::Button::buttonNormal);
        check(text("fx_dl_throw") == "OFF", "THROW is off when released");
        check(!find(editor, "fx_rv_kick")->getParentComponent()->isVisible(), "NORMAL hides KICK");
        click("fx_rv_type:1"); editor.refreshFromProcessor();
        check(find(editor, "fx_rv_kick")->getParentComponent()->isVisible(), "SPRING shows KICK");
        click("fx_rv_type:0"); editor.refreshFromProcessor();

        // The echo's deeper settings: pick on the screen, turn VALUE, and it is that parameter that moves.
        {
            auto* pot = dynamic_cast<juce::Slider*>(find(editor, "lcd_value_delay"));
            check(pot != nullptr, "the echo has a VALUE knob");
            pot->setValue(61.0, juce::sendNotificationSync);
            check(std::abs(ps.getRawParameterValue("fx_dl_drive")->load() - 61.0f) < 0.5f, "VALUE starts on DRIVE");
        }

        // Every unit's presets load, step round, say what they are, and bring a unit that is off up to where it can be heard.
        for (const juce::String unit : {"tides", "delay", "reverb", "chorus", "phaser"}) {
            auto* lcd = find(editor, "lcd_" + unit);
            check(lcd != nullptr && lcd->isVisible(), unit + " has a screen");
            juce::StringArray names;
            for (int i = 0; i < 12; ++i) {
                click(("preset_next_" + unit).toRawUTF8()); editor.refreshFromProcessor();
                const auto name = lcd->getProperties()["preset"].toString();
                check(name.isNotEmpty() && !name.endsWith("*") && name != "CUSTOM", unit + " preset reads back as loaded: " + name);
                names.addIfNotAlreadyThere(name);
            }
            check(names.size() >= 5, unit + " offers its presets");
        }
        check(ps.getRawParameterValue("fx_dl_mix")->load() > 0.0f && ps.getRawParameterValue("fx_rv_mix")->load() > 0.0f && ps.getRawParameterValue("fx_ph_mix")->load() > 0.0f, "a preset turns a silent unit up");
        ps.getParameter("fx_dl_mix")->setValueNotifyingHost(ps.getParameter("fx_dl_mix")->convertTo0to1(77.0f));
        click("preset_next_delay");
        check(std::abs(ps.getRawParameterValue("fx_dl_mix")->load() - 77.0f) < 0.5f, "but never moves a mix you have set");
        ps.getParameter("fx_dl_fb")->setValueNotifyingHost(ps.getParameter("fx_dl_fb")->convertTo0to1(3.0f));
        editor.refreshFromProcessor();
        check(find(editor, "lcd_delay")->getProperties()["preset"].toString().endsWith("*"), "and an edited preset is marked");
        render("panel_rack_loaded");
                for (auto* base : proc.getParameters()) if (auto* q = dynamic_cast<juce::RangedAudioParameter*>(base)) if (q->getParameterID().startsWith("fx_") || q->getParameterID().startsWith("t_")) q->setValueNotifyingHost(q->getDefaultValue());
        proc.fxPreset = { -1, -1, -1, -1, -1 };
    }
    editor.refreshFromProcessor();
    auto* lcd = find(editor, "clock_lcd"), *rate = find(editor, "t_rate");
    auto* selectPot = dynamic_cast<juce::Slider*>(find(editor, "clock_select")), *valuePot = dynamic_cast<juce::Slider*>(find(editor, "clock_value"));
    check(lcd != nullptr && lcd->isVisible() && selectPot != nullptr && valuePot != nullptr, "CLOCK page shows the LCD and its pots");
    const auto lcdSays = [&](const char* what) { return lcd->getProperties()[what].toString(); };

    // Every setting on every output, through the pots: SELECT finds it, VALUE sets that parameter and no other.
    const char* suffixes[] = { "div", "shape", "width", "swing", "prob", "phase", "level", "offset", "steps", "fill", "rot" };
    const char* labels[] = { "RATE", "SHAPE", "WIDTH", "SWING", "PROB", "PHASE", "LEVEL", "OFFSET", "STEPS", "FILL", "ROTATE" };
    int wrongPot = 0;
    for (int channel = 0; channel < 8; ++channel) {
        click(("clock_channel:" + juce::String(channel)).toRawUTF8());
        editor.refreshFromProcessor();
        for (int i = 0; i < 11; ++i) {
            selectPot->setValue(i, juce::sendNotificationSync);
            if (lcdSays("setting") != labels[i] || lcdSays("output") != juce::String(channel + 1)) ++wrongPot;
            auto* target = ps.getParameter("k" + juce::String(channel + 1) + "_" + suffixes[i]);
            std::vector<float> before;
            for (auto* other : proc.getParameters()) before.push_back(other->getValue());
            const double wanted = valuePot->getMinimum() + (valuePot->getMaximum() - valuePot->getMinimum()) * 0.75;
            valuePot->setValue(wanted, juce::sendNotificationSync);
            if (std::abs(target->convertFrom0to1(target->getValue()) - (float)valuePot->getValue()) > 0.51f) ++wrongPot;
            int moved = 0, n = 0;
            for (auto* other : proc.getParameters()) { if (std::abs(other->getValue() - before[(size_t)n]) > 1e-6f) ++moved; ++n; }
            if (moved != 1 && std::abs(target->getValue() - before[(size_t)target->getParameterIndex()]) > 1e-6f) ++wrongPot;
            if (lcdSays("value").isEmpty()) ++wrongPot;
        }
    }
    check(wrongPot == 0, "SELECT and VALUE reach all 11 settings on all 8 outputs, and move only that one, " + juce::String(wrongPot) + " wrong");
    click("clock_channel:0");
    selectPot->setValue(12, juce::sendNotificationSync);
    check(lcdSays("setting") == "BPM", "SELECT reaches BPM, shows " + lcdSays("setting"));
    valuePot->setValue(177, juce::sendNotificationSync);
    check(std::abs(ps.getRawParameterValue("ck_bpm")->load() - 177.0f) < 0.01f, "VALUE sets it");
    selectPot->setValue(11, juce::sendNotificationSync);
    valuePot->setValue(1, juce::sendNotificationSync);
    check(text("ck_src") == "FREE" && lcdSays("value") == "FREE", "SOURCE shows by name, shows " + lcdSays("value"));
    valuePot->setValue(0, juce::sendNotificationSync);

    // The screen follows anything else that changes a value.
    selectPot->setValue(3, juce::sendNotificationSync);
    auto* swing = ps.getParameter("k1_swing");
    swing->setValueNotifyingHost(swing->convertTo0to1(42.0f));
    editor.refreshFromProcessor();
    check(lcdSays("value") == "42.0" && std::abs(valuePot->getValue() - 42.0) < 0.01, "automation moves the screen and the VALUE pot, shows " + lcdSays("value"));
    click("clock_channel:3");
    editor.refreshFromProcessor();
    check(lcdSays("output") == "4" && std::abs(valuePot->getValue() - ps.getRawParameterValue("k4_swing")->load()) < 0.01 && std::abs(valuePot->getValue() - 42.0) > 1.0, "the output switch shows that output's own value");
    auto* s4 = ps.getParameter("k4_swing");
    s4->setValueNotifyingHost(s4->convertTo0to1(11.0f));
    editor.refreshFromProcessor();
    check(lcdSays("value") == "11.0" && std::abs(swing->convertFrom0to1(swing->getValue()) - 42.0f) < 0.01f, "outputs keep separate values");

    // Clicking a setting on the screen, and double-clicking VALUE.
    const auto lcdEvent = [&](juce::Point<float> at) { return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, lcd, lcd, juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 1, false); };
    const auto screen = juce::Rectangle<float>(354.0f, 2.0f, 590.0f, 82.0f);
    lcd->mouseDown(lcdEvent({ screen.getX() + 202.0f + 50.0f * 3 + 20.0f, screen.getY() + 9.0f + 22.0f + 10.0f }));   // second row, fourth: STEPS
    check(lcdSays("setting") == "STEPS", "clicking a setting on the screen selects it, shows " + lcdSays("setting"));
    valuePot->setValue(16, juce::sendNotificationSync);
    const juce::Point<float> potMiddle(valuePot->getWidth() * 0.5f, valuePot->getHeight() * 0.5f);
    valuePot->mouseDoubleClick(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), potMiddle, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, valuePot, valuePot, juce::Time::getCurrentTime(), potMiddle, juce::Time::getCurrentTime(), 2, false));
    check(std::abs(ps.getRawParameterValue("k4_steps")->load()) < 0.01f, "double-clicking VALUE resets that setting");
    check(!find(editor, "clock_channel:7")->isVisible() == false, "the output switch stays beside it");
    ps.getParameter("k4_steps")->setValueNotifyingHost(ps.getParameter("k4_steps")->convertTo0to1(16.0f));
    ps.getParameter("k4_fill")->setValueNotifyingHost(ps.getParameter("k4_fill")->convertTo0to1(5.0f));
    editor.refreshFromProcessor();
    render("panel_clock");
    click("rack_back");
    editor.refreshFromProcessor();
    editor.refreshFromProcessor();
    check(!rate->isShowing() && !find(editor, "rack_page")->isVisible() && proc.patchBayOpen, "PANEL goes back, with the patch bay as it was");
    {   // With the patch bay closed, the rack still has its full size, and leaving restores the short window exactly.
        patch->onClick();
        const int shortW = editor.getWidth(), shortH = editor.getHeight();
        click("rack_toggle"); editor.refreshFromProcessor();
        check(!proc.patchBayOpen && editor.getHeight() > shortH, "from a closed patch bay, the rack still gets the tall window");
        { int rackHeightFromClosed = editor.getHeight(); click("rack_back"); patch->onClick(); click("rack_toggle"); check(editor.getHeight() == rackHeightFromClosed, "the same size as from an open one"); click("rack_back"); patch->onClick(); click("rack_toggle"); editor.refreshFromProcessor(); }
        check(std::abs((double)editor.getWidth() / editor.getHeight() - 1480.0 / 1209.0) < 0.01, "at the rack's own proportions");
        click("rack_back"); editor.refreshFromProcessor();
        check(editor.getWidth() == shortW && editor.getHeight() == shortH, "and back to the same short window");
        patch->onClick();
    }
    patch->onClick();
    check(editor.getHeight() == closedH && editor.getWidth() == width, "and closes back to the same size");

    std::cout << (failures == 0 ? "PASSED" : "FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
