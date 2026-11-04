// The preset browser: a large screen over the panel, opened from the display. Filters on the left, the list on the right, and what
// you think of the selected preset underneath. Included by PluginEditor.cpp, after PresetBar.

struct MillstoneEditor::PresetBrowser : juce::Component, private juce::ListBoxModel {
    PresetBrowser(MillstoneProcessor& p, PresetBar& b, std::function<void()> close) : processor(p), bar(b), onClose(std::move(close)) {
        setComponentID("preset_browser");
        setWantsKeyboardFocus(true);
        search.setComponentID("browser_search");
        search.setFont(displayFont(15.0f));
        search.setTextToShowWhenEmpty("Search name, sound, tag...", amberDim);
        search.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff050403));
        search.setColour(juce::TextEditor::textColourId, amber);
        search.setColour(juce::CaretComponent::caretColourId, amber);
        search.setColour(juce::TextEditor::outlineColourId, amberDim);
        search.setColour(juce::TextEditor::focusedOutlineColourId, amber);
        search.setColour(juce::TextEditor::highlightColourId, amberDim);
        search.setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::black);
        search.setIndents(8, 6);
        search.setText(processor.browserFilter.search, false);
        search.onTextChange = [this] { processor.browserFilter.search = search.getText().trim(); filterChanged(); };
        search.onEscapeKey = [this] { onClose(); };
        search.onReturnKey = [this] { list.grabKeyboardFocus(); };
        addAndMakeVisible(search);

        list.setModel(this);
        list.setComponentID("browser_list");
        list.setRowHeight(rowH);
        list.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff050403));
        list.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        list.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, amberDim);
        addAndMakeVisible(list);

        const auto addKey = [this](Segment& key, const char* id, std::function<void()> click) {
            key.dark = true; key.lamp = false; key.lampOn = amber;
            key.setComponentID(id);
            key.onClick = std::move(click);
            addAndMakeVisible(key);
        };
        addKey(closeKey, "browser_close", [this] { onClose(); });
        addKey(clearKey, "browser_clear", [this] { clearFilters(); });
        addKey(starKey, "browser_star", [this] { toggleStar(selectedEntry()); });
        addKey(flagKey, "browser_flag", [this] { askFlag(); });
        addKey(revealKey, "browser_reveal", [this] { revealFlags(); });
        starKey.lamp = flagKey.lamp = true;

        const auto& lib = MillstoneProcessor::factoryPresets();
        addGroup("TYPE", [](auto& f) -> juce::StringArray& { return f.types; }, { "Bass", "Lead", "Pad", "Keys", "Pluck", "Perc", "Sequence", "Riser/FX", "Texture", "Drone" });
        addGroup("STYLE", [](auto& f) -> juce::StringArray& { return f.styles; }, { "Classic", "Techno", "Dub", "House", "Rave", "Ambient", "Cinematic", "Experimental" });
        addGroup("CHARACTER", [](auto& f) -> juce::StringArray& { return f.character; }, { "Dark", "Bright", "Warm", "Metallic", "Evolving", "Aggressive", "Soft" });
        addGroup("USES", [](auto& f) -> juce::StringArray& { return f.uses; }, { "Plaits", "Elements", "Tides", "Clock", "Patch Bay", "LPG", "Delay", "Reverb", "Colour FX" });
        addGroup("PLAYS WITH", [](auto& f) -> juce::StringArray& { return f.plays; }, { "Hold key", "Song tempo", "Wheel", "Pressure" });
        juce::StringArray packs;
        for (const auto& preset : lib) packs.addIfNotAlreadyThere(preset.pack);
        packs.add("User");
        addGroup("PACK", [](auto& f) -> juce::StringArray& { return f.packs; }, packs);
        addShowKey(favouritesKey, "browser_favourites", [this] { processor.browserFilter.favouritesOnly = !processor.browserFilter.favouritesOnly; filterChanged(); });
        addShowKey(flaggedKey, "browser_flagged", [this] { processor.browserFilter.flaggedOnly = !processor.browserFilter.flaggedOnly; filterChanged(); });
    }

    void opened() { bar.reloadLibrary(); refreshAll(); showCurrent(true); }

    void sync() { showCurrent(false); }

    void resized() override {
        const auto P = panelArea();
        closeKey.setBounds((int)P.getRight() - 96, (int)P.getY() + 12, 80, 28);
        search.setBounds(60, (int)P.getY() + 50, 300, 32);
        clearKey.setBounds(366, (int)P.getY() + 50, 74, 32);

        int y = (int)P.getY() + 92;
        for (auto* group : groups) {
            group->titleY = y;
            y += 16;
            int x = 60;
            for (auto* chip : group->chips) {
                const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(printFont(10.5f, true, 0.05f), chip->getButtonText())) + 30;
                if (x + w > 440) { x = 60; y += 28; }
                chip->setBounds(x, y, w, 25);
                x += w + 4;
            }
            y += 33;
        }
        showTitleY = y;
        y += 16;
        favouritesKey.setBounds(60, y, 120, 25);
        flaggedKey.setBounds(184, y, 100, 25);

        const int listX = 460, listW = (int)P.getRight() - 18 - listX, detailH = 96;
        list.setBounds(listX, (int)P.getY() + 72, listW, (int)P.getBottom() - detailH - 16 - ((int)P.getY() + 72));
        const int by = (int)P.getBottom() - 46;
        starKey.setBounds(listX, by, 118, 30);
        flagKey.setBounds(listX + 124, by, 118, 30);
        revealKey.setBounds(listX + 248, by, 140, 30);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::black.withAlpha(0.72f));
        const auto P = panelArea();
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillRoundedRectangle(P.expanded(5.0f).translated(0.0f, 3.0f), 7.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff36342e), 0, P.getY(), juce::Colour(0xff15140f), 0, P.getBottom(), false));
        g.fillRoundedRectangle(P.expanded(3.0f), 6.0f);
        g.setColour(screenFill);
        g.fillRoundedRectangle(P, 4.0f);

        g.setFont(printFont(17.0f, true, 0.22f));
        g.setColour(amber);
        g.drawText("PRESETS", 60, (int)P.getY() + 12, 200, 28, juce::Justification::centredLeft);
        g.setFont(displayFont(13.0f));
        g.setColour(amberDim.brighter(0.5f));
        g.drawText(juce::String((int)bar.filteredEntries().size()) + " of " + juce::String((int)bar.allEntries().size()), 200, (int)P.getY() + 12, 240, 28, juce::Justification::centredRight);

        g.setFont(printFont(10.5f, true, 0.16f));
        g.setColour(amberDim.brighter(0.45f));
        for (auto* group : groups) g.drawText(group->title, 60, group->titleY, 300, 14, juce::Justification::centredLeft);
        g.drawText("SHOW", 60, showTitleY, 300, 14, juce::Justification::centredLeft);

        // Column heads.
        const auto heads = list.getBounds().toFloat();
        g.setColour(amberDim.brighter(0.45f));
        g.setFont(printFont(10.5f, true, 0.12f));
        const float y = heads.getY() - 19;
        g.drawText("NAME", heads.getX() + colName, y, 200, 15, juce::Justification::centredLeft);
        g.drawText("TYPE", heads.getX() + colType, y, 100, 15, juce::Justification::centredLeft);
        g.drawText("STYLE", heads.getX() + colStyle, y, 150, 15, juce::Justification::centredLeft);
        g.drawText("CHARACTER", heads.getX() + colChar, y, 150, 15, juce::Justification::centredLeft);
        g.setColour(amberDim.withAlpha(0.6f));
        g.fillRect(heads.getX(), heads.getY() - 3, heads.getWidth(), 1.0f);

        // About the selected preset.
        const auto text = list.getBounds().withY(list.getBottom() + 6).withHeight(44).toFloat();
        if (const auto* e = selectedEntry()) {
            const auto note = e->preset != nullptr ? e->preset->note : juce::String("Your preset");
            g.setFont(displayFont(13.0f));
            g.setColour(amber);
            g.drawFittedText(note, text.withHeight(19).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
            juce::StringArray about;
            if (e->preset != nullptr) {
                if (!e->preset->uses.isEmpty()) about.add("Uses " + e->preset->uses.joinIntoString(", "));
                if (!e->preset->plays.isEmpty()) about.add("Plays with " + e->preset->plays.joinIntoString(", "));
            }
            g.setFont(displayFont(11.0f));
            g.setColour(amberDim.brighter(0.4f));
            g.drawFittedText(about.joinIntoString("   "), text.withTrimmedTop(20).withHeight(18).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
            const auto flag = flags.find(e->name);
            if (flag != flags.end()) {
                g.setColour(flagColour);
                g.drawFittedText("FLAGGED: " + (flag->second.isEmpty() ? juce::String("no note") : flag->second), juce::Rectangle<int>(revealKey.getRight() + 12, revealKey.getY(), list.getRight() - revealKey.getRight() - 12, 30), juce::Justification::centredLeft, 2, 0.8f);
            }
        }
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::escapeKey) { onClose(); return true; }
        return false;
    }

    // For tests.
    void flagSelected(const juce::String& note) { if (const auto* e = selectedEntry()) { processor.setFlag(e->name, note); refreshMarks(); } }

private:
    struct Group {
        juce::String title;
        std::function<juce::StringArray&(MillstoneProcessor::BrowserFilter&)> field;
        juce::OwnedArray<Segment> chips;
        int titleY = 0;
    };

    juce::Rectangle<float> panelArea() const { return { 40.0f, 20.0f, (float)getWidth() - 80.0f, (float)getHeight() - 40.0f }; }

    void addGroup(const juce::String& title, std::function<juce::StringArray&(MillstoneProcessor::BrowserFilter&)> field, const juce::StringArray& words) {
        auto* group = groups.add(new Group());
        group->title = title;
        group->field = std::move(field);
        for (const auto& word : words) {
            auto* chip = group->chips.add(new Segment(word));
            chip->dark = true;
            chip->lampOn = amber;
            chip->setComponentID("chip:" + word);
            chip->onClick = [this, group, word] {
                auto& selected = group->field(processor.browserFilter);
                if (selected.contains(word)) selected.removeString(word); else selected.add(word);
                filterChanged();
            };
            addAndMakeVisible(chip);
        }
    }

    void addShowKey(Segment& key, const char* id, std::function<void()> click) {
        key.dark = true;
        key.lampOn = amber;
        key.setComponentID(id);
        key.onClick = std::move(click);
        addAndMakeVisible(key);
    }

    void clearFilters() {
        processor.browserFilter = {};
        search.setText({}, false);
        filterChanged();
    }

    void filterChanged() {
        bar.refilter();
        refreshAll();
        showCurrent(true);
    }

    // Reads the files once, and lights the chips. One that would leave nothing is dimmed, so dead ends show before you hit them.
    void refreshAll() {
        refreshMarks();
        auto& f = processor.browserFilter;
        const auto& entries = bar.allEntries();
        const auto favourites = processor.favourites();
        for (auto* group : groups) {
            auto& field = group->field(f);
            for (auto* chip : group->chips) {
                const auto word = chip->getButtonText();
                const bool on = field.contains(word);
                chip->setToggleState(on, juce::dontSendNotification);
                bool useful = on;
                if (!useful) {
                    auto probe = f;
                    group->field(probe) = juce::StringArray(word);
                    for (const auto& e : entries) if (PresetBar::matches(e, probe, favourites, flags)) { useful = true; break; }
                }
                chip->setAlpha(useful ? 1.0f : 0.35f);
            }
        }
        favouritesKey.setToggleState(f.favouritesOnly, juce::dontSendNotification);
        flaggedKey.setToggleState(f.flaggedOnly, juce::dontSendNotification);
        list.updateContent();
        repaint();
    }

    void refreshMarks() {
        flags = processor.flags();
        favouriteNames = processor.favourites();
        const auto* e = selectedEntry();
        starKey.setToggleState(e != nullptr && favouriteNames.contains(e->name), juce::dontSendNotification);
        flagKey.setToggleState(e != nullptr && flags.find(e->name) != flags.end(), juce::dontSendNotification);
        starKey.setEnabled(e != nullptr);
        flagKey.setEnabled(e != nullptr);
        flagKey.setButtonText(flagKey.getToggleState() ? "FLAGGED" : "FLAG");
        starKey.setButtonText(starKey.getToggleState() ? "FAVOURITE" : "FAVOURITE");
        list.repaint();
        repaint();
    }

    const PresetBar::Entry* selectedEntry() const {
        const int row = list.getSelectedRow();
        const auto& filtered = bar.filteredEntries();
        if (!juce::isPositiveAndBelow(row, (int)filtered.size())) return nullptr;
        return &bar.allEntries()[(size_t)filtered[(size_t)row]];
    }

    // The selection follows what is playing, whoever changed it, without loading anything.
    void showCurrent(bool force) {
        const auto name = processor.getCurrentPresetName();
        if (!force && name == shownName) return;
        shownName = name;
        const auto& filtered = bar.filteredEntries();
        int row = -1;
        for (int i = 0; i < (int)filtered.size(); ++i) if (bar.allEntries()[(size_t)filtered[(size_t)i]].name == name) row = i;
        following = true;
        if (row >= 0) { list.selectRow(row, false, true); list.scrollToEnsureRowIsOnscreen(row); }
        else list.deselectAllRows();
        following = false;
        refreshMarks();
    }

    void toggleStar(const PresetBar::Entry* e) {
        if (e == nullptr) return;
        processor.setFavourite(e->name, !processor.isFavourite(e->name));
        if (processor.browserFilter.favouritesOnly) { filterChanged(); return; }
        refreshMarks();
    }

    void askFlag() {
        const auto* e = selectedEntry();
        if (e == nullptr) return;
        const auto name = e->name;
        const auto existing = flags.find(name);
        dialog = std::make_unique<juce::AlertWindow>("Flag " + name, "What is wrong with it, or what do you like? This goes in the flag file.", juce::MessageBoxIconType::NoIcon, this);
        dialog->addTextEditor("note", existing != flags.end() ? existing->second : juce::String());
        dialog->addButton("Flag", 1, juce::KeyPress(juce::KeyPress::returnKey));
        if (existing != flags.end()) dialog->addButton("Remove flag", 2);
        dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetBrowser> self(this);
        dialog->enterModalState(true, juce::ModalCallbackFunction::create([self, name](int result) {
            if (self == nullptr || result == 0) return;
            if (result == 1) self->processor.setFlag(name, self->dialog->getTextEditorContents("note"));
            else self->processor.clearFlag(name);
            if (self->processor.browserFilter.flaggedOnly) self->filterChanged(); else self->refreshMarks();
        }), false);
    }

    void revealFlags() {
        const auto file = processor.flagsFile();
        if (!file.existsAsFile()) { file.getParentDirectory().createDirectory(); file.create(); }
        file.revealToUser();
    }

    // ---- the list ---------------------------------------------------------------------------------------------------
    int getNumRows() override { return (int)bar.filteredEntries().size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override {
        const auto& filtered = bar.filteredEntries();
        if (!juce::isPositiveAndBelow(row, (int)filtered.size())) return;
        const auto& e = bar.allEntries()[(size_t)filtered[(size_t)row]];
        if (selected) { g.setColour(amber); g.fillRoundedRectangle(0.0f, 0.5f, (float)width, (float)height - 1.0f, 2.0f); }
        const auto ink = selected ? juce::Colour(0xff0a0805) : amber, dim = selected ? juce::Colour(0xff2a2008) : amberDim.brighter(0.45f);
        if (favouriteNames.contains(e.name)) {
            juce::Path star;
            star.addStar({ 12.0f, (float)height * 0.5f }, 5, 3.0f, 7.0f, 0.0f);
            g.setColour(ink);
            g.fillPath(star);
        }
        if (flags.find(e.name) != flags.end()) {
            g.setColour(selected ? juce::Colour(0xff7a1408) : flagColour);
            g.fillRect(24, height / 2 - 6, 2, 13);
            juce::Path flag;
            flag.addTriangle(26.0f, (float)height * 0.5f - 6.0f, 34.0f, (float)height * 0.5f - 3.0f, 26.0f, (float)height * 0.5f);
            g.fillPath(flag);
        }
        g.setColour(ink);
        g.setFont(displayFont(14.0f));
        g.drawFittedText(e.name, (int)colName, 0, (int)(colType - colName) - 6, height, juce::Justification::centredLeft, 1, 0.8f);
        g.setColour(dim);
        g.setFont(displayFont(12.0f));
        g.drawText(PresetBar::typeOf(e), (int)colType, 0, (int)(colStyle - colType) - 6, height, juce::Justification::centredLeft);
        if (e.preset != nullptr) {
            g.drawFittedText(e.preset->styles.joinIntoString(", "), (int)colStyle, 0, (int)(colChar - colStyle) - 6, height, juce::Justification::centredLeft, 1, 0.75f);
            g.drawFittedText(e.preset->character.joinIntoString(", "), (int)colChar, 0, width - (int)colChar - 6, height, juce::Justification::centredLeft, 1, 0.75f);
        }
    }

    void selectedRowsChanged(int row) override {
        if (following || row < 0) { refreshMarks(); return; }
        const auto& filtered = bar.filteredEntries();
        if (!juce::isPositiveAndBelow(row, (int)filtered.size())) return;
        bar.loadEntry(filtered[(size_t)row]);
        shownName = processor.getCurrentPresetName();
        refreshMarks();
    }

    void listBoxItemClicked(int row, const juce::MouseEvent& e) override {   // the star column toggles, whichever row
        if (e.x < 22 && juce::isPositiveAndBelow(row, (int)bar.filteredEntries().size()))
            toggleStar(&bar.allEntries()[(size_t)bar.filteredEntries()[(size_t)row]]);
    }

    void listBoxItemDoubleClicked(int, const juce::MouseEvent&) override { onClose(); }
    void returnKeyPressed(int) override { onClose(); }

    static constexpr int rowH = 26;
    static constexpr float colName = 40.0f, colType = 262.0f, colStyle = 362.0f, colChar = 640.0f;
    const juce::Colour flagColour { 0xffff6a4d };

    MillstoneProcessor& processor;
    PresetBar& bar;
    std::function<void()> onClose;
    juce::TextEditor search;
    juce::ListBox list;
    Segment closeKey { "CLOSE" }, clearKey { "CLEAR" }, starKey { "FAVOURITE" }, flagKey { "FLAG" }, revealKey { "OPEN FLAG FILE" }, favouritesKey { "FAVOURITES" }, flaggedKey { "FLAGGED" };
    juce::OwnedArray<Group> groups;
    int showTitleY = 0;
    std::map<juce::String, juce::String> flags;
    juce::StringArray favouriteNames;
    juce::String shownName;
    bool following = false;
    std::unique_ptr<juce::AlertWindow> dialog;
};
