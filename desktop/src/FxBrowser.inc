// The FX preset browser: the whole rack's effects, kept apart from the synth presets, so a voice can stay while its effects change.
// Opened from the FX screen in the rack's top strip. Included by PluginEditor.cpp, after PresetBrowser.inc.

struct MillstoneEditor::FxBrowser : juce::Component, private juce::ListBoxModel {
    FxBrowser(MillstoneProcessor& p, std::function<void()> close) : processor(p), onClose(std::move(close)) {
        setComponentID("fx_browser");
        setWantsKeyboardFocus(true);
        search.setComponentID("fx_browser_search");
        search.setFont(displayFont(15.0f));
        search.setTextToShowWhenEmpty("Search name or sound...", amberDim);
        search.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff050403));
        search.setColour(juce::TextEditor::textColourId, amber);
        search.setColour(juce::CaretComponent::caretColourId, amber);
        search.setColour(juce::TextEditor::outlineColourId, amberDim);
        search.setColour(juce::TextEditor::focusedOutlineColourId, amber);
        search.setColour(juce::TextEditor::highlightColourId, amberDim);
        search.setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::black);
        search.setIndents(8, 6);
        search.onTextChange = [this] { refilter(); };
        search.onEscapeKey = [this] { onClose(); };
        search.onReturnKey = [this] { list.grabKeyboardFocus(); };
        addAndMakeVisible(search);

        list.setModel(this);
        list.setComponentID("fx_browser_list");
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
        addKey(closeKey, "fx_browser_close", [this] { onClose(); });
        addKey(clearKey, "fx_browser_clear", [this] { clearFilters(); });
        addKey(saveKey, "fx_browser_save", [this] { askSave(); });
        addKey(deleteKey, "fx_browser_delete", [this] { askDelete(); });

        addGroup("STYLE", styleChips, { "Classic", "Techno", "Dub", "House", "Rave", "Ambient", "Cinematic", "Experimental" });
        addGroup("USES", useChips, { "Delay", "Reverb", "Chorus", "Phaser" });
        addGroup("SHOW", packChips, { "Factory", "User" });
    }

    void opened() { reload(); showCurrent(true); }
    void sync() { showCurrent(false); }

    void resized() override {
        const auto P = panelArea();
        closeKey.setBounds((int)P.getRight() - 96, (int)P.getY() + 12, 80, 28);
        search.setBounds(60, (int)P.getY() + 50, 300, 32);
        clearKey.setBounds(366, (int)P.getY() + 50, 74, 32);
        int y = (int)P.getY() + 100;
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
            y += 40;
        }
        saveKey.setBounds(60, y + 6, 120, 32);
        deleteKey.setBounds(186, y + 6, 120, 32);
        const int listX = 460, listW = (int)P.getRight() - 18 - listX;
        list.setBounds(listX, (int)P.getY() + 72, listW, (int)P.getBottom() - 72 - 16 - (int)P.getY() - 60);
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
        g.drawText("FX PRESETS", 60, (int)P.getY() + 12, 220, 28, juce::Justification::centredLeft);
        g.setFont(displayFont(13.0f));
        g.setColour(amberDim.brighter(0.5f));
        g.drawText(juce::String((int)filtered.size()) + " of " + juce::String((int)racks.size()), 240, (int)P.getY() + 12, 200, 28, juce::Justification::centredRight);

        g.setFont(printFont(10.5f, true, 0.16f));
        g.setColour(amberDim.brighter(0.45f));
        for (auto* group : groups) g.drawText(group->title, 60, group->titleY, 300, 14, juce::Justification::centredLeft);

        const auto heads = list.getBounds().toFloat();
        g.setFont(printFont(10.5f, true, 0.12f));
        const float y = heads.getY() - 19;
        g.drawText("NAME", heads.getX() + colName, y, 200, 15, juce::Justification::centredLeft);
        g.drawText("STYLE", heads.getX() + colStyle, y, 200, 15, juce::Justification::centredLeft);
        g.drawText("USES", heads.getX() + colUses, y, 200, 15, juce::Justification::centredLeft);
        g.setColour(amberDim.withAlpha(0.6f));
        g.fillRect(heads.getX(), heads.getY() - 3, heads.getWidth(), 1.0f);

        if (const auto* rack = selected()) {
            g.setFont(displayFont(13.0f));
            g.setColour(amber);
            g.drawFittedText(rack->note, list.getBounds().withY(list.getBottom() + 12).withHeight(22), juce::Justification::centredLeft, 2, 0.8f);
        }
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (key == juce::KeyPress::escapeKey) { onClose(); return true; }
        return false;
    }

private:
    struct Group { juce::String title; juce::StringArray* field; juce::OwnedArray<Segment> chips; int titleY = 0; };

    juce::Rectangle<float> panelArea() const { return { 40.0f, 20.0f, (float)getWidth() - 80.0f, (float)getHeight() - 40.0f }; }

    void addGroup(const juce::String& title, juce::StringArray& field, const juce::StringArray& words) {
        auto* group = groups.add(new Group());
        group->title = title;
        group->field = &field;
        for (const auto& word : words) {
            auto* chip = group->chips.add(new Segment(word));
            chip->dark = true;
            chip->lampOn = amber;
            chip->setComponentID("fxchip:" + word);
            chip->onClick = [this, group, word] {
                if (group->field->contains(word)) group->field->removeString(word); else group->field->add(word);
                refilter();
            };
            addAndMakeVisible(chip);
        }
    }

    void clearFilters() {
        styleChips.clear(); useChips.clear(); packChips.clear();
        search.setText({}, false);
        refilter();
    }

    void reload() { racks = processor.allFxRacks(); refilter(); }

    bool matches(const MillstoneProcessor::FxRack& rack) const {
        const auto anyOf = [](const juce::StringArray& wanted, const juce::StringArray& has) {
            if (wanted.isEmpty()) return true;
            for (const auto& word : wanted) if (has.contains(word)) return true;
            return false;
        };
        if (!anyOf(styleChips, rack.styles)) return false;
        for (const auto& word : useChips) if (!rack.uses.contains(word)) return false;   // chosen effects must all be in it
        if (!packChips.isEmpty() && !packChips.contains(rack.user ? "User" : "Factory")) return false;
        for (const auto& word : juce::StringArray::fromTokens(search.getText(), " ", "")) {
            if (word.isEmpty()) continue;
            if (!rack.name.containsIgnoreCase(word) && !rack.note.containsIgnoreCase(word) && !rack.styles.joinIntoString(" ").containsIgnoreCase(word) && !rack.uses.joinIntoString(" ").containsIgnoreCase(word)) return false;
        }
        return true;
    }

    void refilter() {
        filtered.clear();
        for (int i = 0; i < (int)racks.size(); ++i) if (matches(racks[(size_t)i])) filtered.push_back(i);
        for (auto* group : groups)
            for (auto* chip : group->chips) {
                const auto word = chip->getButtonText();
                const bool on = group->field->contains(word);
                chip->setToggleState(on, juce::dontSendNotification);
                // One that would leave nothing is dimmed, so dead ends show before you hit them.
                bool useful = on;
                if (!useful) {
                    group->field->add(word);
                    for (const auto& rack : racks) if (matches(rack)) { useful = true; break; }
                    group->field->removeString(word);
                }
                chip->setAlpha(useful ? 1.0f : 0.35f);
            }
        list.updateContent();
        showCurrent(true);
        repaint();
    }

    const MillstoneProcessor::FxRack* selected() const {
        const int row = list.getSelectedRow();
        return juce::isPositiveAndBelow(row, (int)filtered.size()) ? &racks[(size_t)filtered[(size_t)row]] : nullptr;
    }

    // The selection follows what is loaded, whoever changed it, without loading anything.
    void showCurrent(bool force) {
        const auto name = processor.fxRackName();
        if (!force && name == shownName) return;
        shownName = name;
        int row = -1;
        for (int i = 0; i < (int)filtered.size(); ++i) if (racks[(size_t)filtered[(size_t)i]].name == name) row = i;
        following = true;
        if (row >= 0) { list.selectRow(row, false, true); list.scrollToEnsureRowIsOnscreen(row); }
        else list.deselectAllRows();
        following = false;
        refreshKeys();
    }

    void refreshKeys() {
        const auto* rack = selected();
        deleteKey.setEnabled(rack != nullptr && rack->user);
        deleteKey.setAlpha(deleteKey.isEnabled() ? 1.0f : 0.4f);
        list.repaint();
        repaint();
    }

    void askSave() {
        const auto* rack = selected();
        dialog = std::make_unique<juce::AlertWindow>("Save FX preset", "Saves the echo, reverb, chorus and phaser as they are now.", juce::MessageBoxIconType::NoIcon, this);
        dialog->addTextEditor("name", rack != nullptr && rack->user ? rack->name : juce::String(), "Name");
        dialog->addComboBox("style", { "Classic", "Techno", "Dub", "House", "Rave", "Ambient", "Cinematic", "Experimental" }, "Style");
        if (auto* box = dialog->getComboBoxComponent("style")) box->setText(rack != nullptr && !rack->styles.isEmpty() ? rack->styles[0] : juce::String("Dub"), juce::dontSendNotification);
        dialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<FxBrowser> self(this);
        dialog->enterModalState(true, juce::ModalCallbackFunction::create([self](int result) {
            if (self == nullptr || result == 0) return;
            const auto name = self->dialog->getTextEditorContents("name");
            auto* box = self->dialog->getComboBoxComponent("style");
            if (self->processor.saveFxRack(name, box != nullptr ? box->getText() : juce::String())) self->opened();
            else juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Not saved").withMessage("That name is empty, or belongs to a factory preset.").withButton("OK"), nullptr);
        }), false);
    }

    void askDelete() {
        const auto* rack = selected();
        if (rack == nullptr || !rack->user) return;
        const auto name = rack->name;
        dialog = std::make_unique<juce::AlertWindow>("Delete " + name, "This removes your FX preset. The effects stay as they are.", juce::MessageBoxIconType::NoIcon, this);
        dialog->addButton("Delete", 1);
        dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<FxBrowser> self(this);
        dialog->enterModalState(true, juce::ModalCallbackFunction::create([self, name](int result) {
            if (self == nullptr || result == 0) return;
            self->processor.deleteFxRack(name);
            self->opened();
        }), false);
    }

    int getNumRows() override { return (int)filtered.size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool isSelected) override {
        if (!juce::isPositiveAndBelow(row, (int)filtered.size())) return;
        const auto& rack = racks[(size_t)filtered[(size_t)row]];
        if (isSelected) { g.setColour(amber); g.fillRoundedRectangle(0.0f, 0.5f, (float)width, (float)height - 1.0f, 2.0f); }
        const auto ink = isSelected ? juce::Colour(0xff0a0805) : amber, dim = isSelected ? juce::Colour(0xff2a2008) : amberDim.brighter(0.45f);
        g.setColour(ink);
        g.setFont(displayFont(14.0f));
        g.drawFittedText(rack.name, (int)colName, 0, (int)(colStyle - colName) - 6, height, juce::Justification::centredLeft, 1, 0.8f);
        g.setColour(dim);
        g.setFont(displayFont(12.0f));
        g.drawFittedText(rack.user && rack.styles.isEmpty() ? juce::String("Yours") : rack.styles.joinIntoString(", "), (int)colStyle, 0, (int)(colUses - colStyle) - 6, height, juce::Justification::centredLeft, 1, 0.7f);
        g.drawFittedText(rack.uses.isEmpty() ? juce::String("None") : rack.uses.joinIntoString(", "), (int)colUses, 0, width - (int)colUses - 6, height, juce::Justification::centredLeft, 1, 0.75f);
    }

    void selectedRowsChanged(int row) override {
        if (following || !juce::isPositiveAndBelow(row, (int)filtered.size())) { refreshKeys(); return; }
        processor.loadFxRack(racks[(size_t)filtered[(size_t)row]].name);
        shownName = processor.fxRackName();
        refreshKeys();
    }

    void listBoxItemDoubleClicked(int, const juce::MouseEvent&) override { onClose(); }
    void returnKeyPressed(int) override { onClose(); }

    static constexpr int rowH = 26;
    static constexpr float colName = 12.0f, colStyle = 250.0f, colUses = 560.0f;

    MillstoneProcessor& processor;
    std::function<void()> onClose;
    juce::TextEditor search;
    juce::ListBox list;
    Segment closeKey { "CLOSE" }, clearKey { "CLEAR" }, saveKey { "SAVE" }, deleteKey { "DELETE" };
    juce::OwnedArray<Group> groups;
    juce::StringArray styleChips, useChips, packChips;
    std::vector<MillstoneProcessor::FxRack> racks;
    std::vector<int> filtered;
    juce::String shownName;
    bool following = false;
    std::unique_ptr<juce::AlertWindow> dialog;
};
