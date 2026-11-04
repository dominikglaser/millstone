// Developer check, built only with -DMILLSTONE_UI_SNAPSHOT=ON: renders every page to a PNG and exercises the
// controls against the parameters. Usage: millstone_ui_snapshot <output-dir>
#include "PluginEditor.h"

static int failures = 0;
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
    check(editor.getWidth() == width && editor.getHeight() > closedH, "PATCH opens the drawer: the window grows and the panel keeps its size");
    check(proc.patchBayOpen, "the drawer state is remembered");
    proc.connectCord(1, 7);      // LFO > O1 HARM
    proc.connectCord(11, 6);     // OSC 2 > O1 PITCH
    proc.connectCord(12, 2);     // OSC 3 > CUTOFF
    proc.connectCord(3, 3);      // MOD ENV > RESO
    proc.connectCord(9, 4);      // NOISE > VCA
    proc.connectCord(6, 20);     // WHEEL > O3 LEVEL
    editor.refreshFromProcessor();
    render("panel_patch");
    patch->onClick();
    check(editor.getHeight() == closedH, "and closes back to the same size");

    std::cout << (failures == 0 ? "PASSED" : "FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
