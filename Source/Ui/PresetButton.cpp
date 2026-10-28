#include "PresetButton.h"

namespace
{
    enum MenuIds
    {
        factoryBase = 1,
        userBase = 1000,
        saveId = 5000,
        openFolderId = 5001,
        nothingId = 5002
    };

    juce::String displayName (const PluginBase& processor, Presets::Section section)
    {
        if (section == Presets::Section::bass)
            return processor.isEffectVersion() ? "Gate" : "Instrument";
        if (section == Presets::Section::master)
            return "Maximizer";
        return Presets::sectionName (section);
    }

    const juce::String arrow = juce::String::fromUTF8 ("  \xe2\x96\xbe"); // "  " + liten pil nedåt
}

PresetButton::PresetButton (PluginBase& p, Presets::Section sectionIn, juce::Colour accent, int layerIn)
    : processor (p), section (sectionIn), layer (layerIn)
{
    button.setColour (juce::TextButton::buttonOnColourId, accent);
    button.setTooltip (displayName (processor, section) + " presets");
    button.onClick = [this] { showMenu(); };
    addAndMakeVisible (button);
    setWantsKeyboardFocus (true); // piltangenter bläddrar bland presets (keyPressed)

    updateText();
    startTimerHz (4); // projekt och presets kan laddas medan fönstret är öppet
}

void PresetButton::setLayer (int newLayer, juce::Colour accent)
{
    layer = newLayer;
    button.setColour (juce::TextButton::buttonOnColourId, accent);
    updateText();
}

PresetButton::~PresetButton()
{
    if (saveDialog != nullptr)
    {
        saveDialog->setLookAndFeel (nullptr);
        saveDialog->exitModalState (0);
    }
}

void PresetButton::updateText()
{
    const auto name = processor.getPresetName (section, layer);
    const auto text = (name.isEmpty() || (emptyLabel != "Preset" && name == "Default") ? emptyLabel : name) + arrow;
    if (button.getButtonText() != text)
        button.setButtonText (text);
}

std::vector<PresetButton::Entry> PresetButton::buildEntries() const
{
    std::vector<Entry> entries;
    const auto categorised = usesCategories();
    const auto patterns = section == Presets::Section::pattern;
    const auto channelPresets = section == Presets::Section::effects;

    // Lägger till raderna kategori för kategori (i tabellens ordning), och sist dem utan kategori.
    const auto addGrouped = [&] (std::vector<Entry> group)
    {
        if (! categorised)
        {
            entries.insert (entries.end(), group.begin(), group.end());
            return;
        }
        for (const auto& category : PresetCategories::names())
            for (const auto& entry : group)
                if (entry.category == category)
                    entries.push_back (entry);
        for (const auto& entry : group)
            if (entry.category.isEmpty())
                entries.push_back (entry);
    };

    if (! patterns && ! channelPresets)
    {
        const auto effectVersion = processor.isEffectVersion();
        std::vector<Entry> factory;
        for (int i = 0; i < Presets::factoryCount (section, effectVersion); ++i)
        {
            Entry entry;
            entry.name = Presets::factoryName (section, effectVersion, i);
            entry.category = categorised ? PresetCategories::factoryCategory (section, entry.name) : juce::String();
            entry.factoryIndex = i;
            factory.push_back (entry);
        }
        addGrouped (factory);
    }

    std::vector<Entry> user;
    for (const auto& file : processor.getUserPresets (section, layer))
    {
        Entry entry;
        entry.name = file.getFileNameWithoutExtension();
        entry.category = categorised ? PluginBase::readPresetCategory (file) : juce::String();
        entry.file = file;
        user.push_back (entry);
    }
    addGrouped (user);
    return entries;
}

void PresetButton::loadEntry (const Entry& entry)
{
    if (entry.factoryIndex >= 0)
        processor.loadFactoryPreset (section, entry.factoryIndex, layer);
    else
        loadFile (entry.file);
    updateText();
}

void PresetButton::step (int direction)
{
    const auto entries = buildEntries();
    if (entries.empty())
        return;
    const auto current = processor.getPresetName (section, layer);
    auto index = -1;
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].name == current)
            index = i;
    const auto count = (int) entries.size();
    const auto next = index < 0 ? (direction > 0 ? 0 : count - 1) : (index + direction + count) % count;
    loadEntry (entries[(size_t) next]);
}

bool PresetButton::keyPressed (const juce::KeyPress& key)
{
    if (key.getModifiers().isAnyModifierKeyDown())
        return false;
    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::upKey)
        step (-1);
    else if (key == juce::KeyPress::rightKey || key == juce::KeyPress::downKey)
        step (1);
    else
        return false;
    return true;
}

void PresetButton::showMenu()
{
    const auto current = processor.getPresetName (section, layer);
    const auto entries = buildEntries();
    const auto categorised = usesCategories();

    juce::PopupMenu menu;
    // Mönster: bara de man sparat själv. Ett sparat mönster innehåller alla rader (kick, instrument, pianorullar, längder, GATE och groove), så ingen
    // rad råkar bli kvar eller tömd. Färdiga rytmer för en rad finns i FILL.
    const auto patterns = section == Presets::Section::pattern;
    const auto channelPresets = section == Presets::Section::effects;

    // Menyns id är radens plats i `entries`, så undermenyerna har unika id.
    const auto addGroup = [&] (const juce::String& header, bool wantFactory)
    {
        auto any = false;
        for (const auto& entry : entries)
            any = any || (entry.factoryIndex >= 0) == wantFactory;
        if (! any)
            return;
        menu.addSectionHeader (header);
        if (! categorised)
        {
            for (size_t i = 0; i < entries.size(); ++i)
                if ((entries[i].factoryIndex >= 0) == wantFactory)
                    menu.addItem (factoryBase + (int) i, entries[i].name, true, entries[i].name == current);
            return;
        }
        for (const auto& category : PresetCategories::names())
        {
            juce::PopupMenu sub;
            auto ticked = false;
            for (size_t i = 0; i < entries.size(); ++i)
                if ((entries[i].factoryIndex >= 0) == wantFactory && entries[i].category == category)
                {
                    sub.addItem (factoryBase + (int) i, entries[i].name, true, entries[i].name == current);
                    ticked = ticked || entries[i].name == current;
                }
            if (sub.getNumItems() > 0)
                menu.addSubMenu (category, sub, true, nullptr, ticked);
        }
        for (size_t i = 0; i < entries.size(); ++i)
            if ((entries[i].factoryIndex >= 0) == wantFactory && entries[i].category.isEmpty())
                menu.addItem (factoryBase + (int) i, entries[i].name, true, entries[i].name == current);
    };

    if (patterns || channelPresets)
    {
        menu.addSectionHeader (patterns ? "PATTERN PRESETS - " + (layer == Pattern::kickLayer ? juce::String ("KICK") : processor.getLayerName (layer))
                                        : "FX PRESETS - " + Effects::channelLabel (layer));
        if (entries.empty())
            menu.addItem (nothingId, "(none saved yet)", false);
        for (size_t i = 0; i < entries.size(); ++i)
            menu.addItem (factoryBase + (int) i, entries[i].name, true, entries[i].name == current);
    }
    else
    {
        addGroup ("FACTORY", true);
        addGroup ("USER", false);
    }

    menu.addSeparator();
    menu.addItem (saveId, "Save preset...");
    menu.addItem (openFolderId, "Open preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button),
                        [this, entries] (int result)
    {
        if (result >= factoryBase && result < factoryBase + (int) entries.size())
            loadEntry (entries[(size_t) (result - factoryBase)]);
        else if (result == saveId)
            askForNameAndSave();
        else if (result == openFolderId)
        {
            const auto folder = Presets::folder (section, processor.isEffectVersion());
            folder.createDirectory();
            folder.startAsProcess();
        }

        updateText();
    });
}

void PresetButton::loadFile (const juce::File& file)
{
    juce::Component::SafePointer<PresetButton> safeThis (this);

    if (section == Presets::Section::bass || section == Presets::Section::kick)
    {
        // Ett instrument (eller en kick) kan ha en länkad effekt: den läggs på första lediga FX-kanal.
        const auto link = PluginBase::readPresetLink (file);
        askFxChannel (link.kind == "effect" ? link.name : juce::String(), [safeThis, file] (int fxChannel)
        {
            if (safeThis == nullptr)
                return;
            safeThis->processor.loadInstrumentPreset (safeThis->section, safeThis->layer, file, fxChannel);
            safeThis->updateText();
        });
        return;
    }

    if (section == Presets::Section::pattern)
    {
        loadPatternFile (file);
        return;
    }

    if (section != Presets::Section::effects || ! processor.fxChannelHasEffects (layer))
    {
        processor.loadUserPreset (section, file, layer);
        return;
    }

    // Ett FX-preset ersätter kanalens effekter, EQ och kompressor: fråga först om något finns att förlora.
    juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Load FX preset")
                                      .withMessage ("Loading \"" + file.getFileNameWithoutExtension() + "\" replaces the effects, EQ and compressor of " + Effects::channelLabel (layer)
                                                    + ". The other channels are not touched. Continue?")
                                      .withButton ("Replace").withButton ("Cancel"),
                                  [safeThis, file] (int choice)
    {
        if (safeThis == nullptr || choice != 1)
            return;
        safeThis->processor.loadUserPreset (safeThis->section, file, safeThis->layer);
        safeThis->updateText();
    });
}

void PresetButton::loadPatternFile (const juce::File& file)
{
    juce::Component::SafePointer<PresetButton> safeThis (this);
    const auto kickRow = layer == Pattern::kickLayer;
    const auto rowName = kickRow ? juce::String ("KICK") : processor.getLayerName (layer);
    const auto presetName = file.getFileNameWithoutExtension();
    const auto link = PluginBase::readPresetLink (file);
    const auto instrumentFile = link.valid() ? processor.findUserPreset (kickRow ? Presets::Section::kick : Presets::Section::bass, link.name) : juce::File();

    if (instrumentFile.existsAsFile())
    {
        // Presetet hör ihop med ett instrument: fråga om det ska ersätta det som finns, så att inget instrument försvinner av misstag.
        const auto what = kickRow ? juce::String ("kick") : juce::String ("instrument");
        juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle ("Load pattern preset")
                                          .withMessage ("\"" + presetName + "\" comes with the " + what + " preset \"" + link.name + "\". Replace the " + what + " of " + rowName
                                                        + " with it? The notes, piano roll and GATE of " + rowName + " are replaced either way. The other rows are not touched.")
                                          .withButton ("Replace the " + what).withButton ("Keep my " + what).withButton ("Cancel"),
                                      [safeThis, file, instrumentFile] (int choice)
        {
            if (safeThis == nullptr || choice == 0)
                return;
            if (choice == 2)
            {
                safeThis->processor.loadPatternPreset (safeThis->layer, file, false);
                safeThis->updateText();
                return;
            }
            const auto effect = PluginBase::readPresetLink (instrumentFile);
            safeThis->askFxChannel (effect.kind == "effect" ? effect.name : juce::String(), [safeThis, file] (int fxChannel)
            {
                if (safeThis == nullptr)
                    return;
                safeThis->processor.loadPatternPreset (safeThis->layer, file, true, fxChannel);
                safeThis->updateText();
            });
        });
        return;
    }

    auto hasNotes = false;
    processor.pattern.forEachNote (layer, [&hasNotes] (int, const Pattern::Note&) { hasNotes = true; });
    const auto missing = link.valid(); // länkat instrument finns inte längre
    if (! hasNotes && ! missing)
    {
        processor.loadUserPreset (section, file, layer);
        return;
    }

    juce::AlertWindow::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                      .withTitle ("Load pattern preset")
                                      .withMessage ("Loading \"" + presetName + "\" replaces the notes, piano roll and GATE of " + rowName + ". The other rows are not touched."
                                                    + (missing ? juce::String (" The " + juce::String (kickRow ? "kick" : "instrument") + " preset \"" + link.name + "\" it was saved with no longer exists, so only the pattern is loaded.")
                                                               : juce::String()) + " Continue?")
                                      .withButton ("Replace").withButton ("Cancel"),
                                  [safeThis, file] (int choice)
    {
        if (safeThis == nullptr || choice != 1)
            return;
        safeThis->processor.loadPatternPreset (safeThis->layer, file, false);
        safeThis->updateText();
    });
}

void PresetButton::askFxChannel (const juce::String& fxName, std::function<void (int)> done)
{
    if (fxName.isEmpty())
    {
        done (-1);
        return;
    }
    if (! processor.findUserPreset (Presets::Section::effects, fxName).existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Linked effect missing",
                                                "This preset was saved with the FX preset \"" + fxName + "\", which no longer exists. It is loaded without the effect.",
                                                "OK", nullptr, juce::ModalCallbackFunction::create ([done] (int) { done (-1); }));
        return;
    }

    const auto freeChannel = processor.firstFreeFxChannel();
    if (freeChannel >= 0)
    {
        done (freeChannel);
        return;
    }

    // Alla FX-kanaler används: välj vilken som ersätts, ladda utan effekten, eller avbryt (stäng menyn).
    juce::PopupMenu menu;
    menu.addSectionHeader ("No free FX channel for \"" + fxName + "\"");
    for (int channel = 0; channel < numLegacyEffectChannels; ++channel)
    {
        const auto name = processor.getPresetName (Presets::Section::effects, channel);
        menu.addItem (1 + channel, "Replace " + Effects::channelLabel (channel) + (name.isEmpty() ? juce::String() : " (" + name + ")"));
    }
    menu.addSeparator();
    menu.addItem (100, "Load without the effect");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&button), [done] (int result)
    {
        if (result >= 1 && result <= numLegacyEffectChannels)
            done (result - 1);
        else if (result == 100)
            done (-1);
    });
}

void PresetButton::askForNameAndSave()
{
    auto* window = new juce::AlertWindow ("Save " + displayName (processor, section) + " preset",
                                          "Name:", juce::MessageBoxIconType::NoIcon);
    window->setLookAndFeel (&getLookAndFeel());
    saveDialog = window;

    const auto current = processor.getPresetName (section, layer);
    window->addTextEditor ("name", current.isEmpty() ? juce::String ("My preset") : current);

    // Länkar: ett instrument-/kick-preset kan ta med sin effekt (sparas som FX-preset), ett mönster-preset sitt instrument (sparas som instrument-preset).
    const auto instrumentPreset = section == Presets::Section::bass || section == Presets::Section::kick;
    const auto patternRow = section == Presets::Section::pattern;
    const auto kickRow = layer == Pattern::kickLayer;
    const auto sourceSection = patternRow ? (kickRow ? Presets::Section::kick : Presets::Section::bass) : section;
    const auto sourceLayer = patternRow && kickRow ? 0 : layer;
    const auto effectChannel = instrumentPreset || patternRow ? processor.sendChannelOf (sourceSection, sourceLayer) : -1;
    const auto effectLinkable = effectChannel >= 0 && processor.fxChannelHasEffects (effectChannel);

    auto baseName = current.isEmpty() || current == "Default" ? juce::String ("My preset") : current;
    if (patternRow)
    {
        const auto instrumentName = processor.getPresetName (sourceSection, sourceLayer);
        baseName = instrumentName.isEmpty() || instrumentName == "Default" ? (kickRow ? juce::String ("My kick") : processor.getLayerName (layer)) : instrumentName;
        window->addComboBox ("link", { "Pattern only", kickRow ? "Also include the kick (saved as a kick preset)" : "Also include the instrument (saved as an instrument preset)" }, "Include:");
        window->addTextEditor ("instrument", baseName, kickRow ? "Kick preset name:" : "Instrument preset name:");
    }
    else if (effectLinkable)
    {
        window->addComboBox ("link", { "Preset only", "Also save its effect (" + Effects::channelLabel (effectChannel) + ") as an FX preset" }, "Include:");
    }
    if (effectLinkable)
    {
        const auto fxName = processor.getPresetName (Presets::Section::effects, effectChannel);
        window->addTextEditor ("fx", fxName.isNotEmpty() ? fxName : baseName + " FX", "FX preset name:");
    }
    // Kategori (kick och instrument): hör ihop med instrumentet, så även ett mönster som tar med instrumentet får rutan.
    const auto categoryApplies = PresetCategories::usedBy (sourceSection) && ! processor.isEffectVersion() && (instrumentPreset || patternRow);
    if (categoryApplies)
    {
        window->addComboBox ("category", PresetCategories::names(), "Category:");
        if (auto* box = window->getComboBoxComponent ("category"))
            box->setSelectedItemIndex (PresetCategories::names().indexOf (PresetCategories::defaultName()), juce::dontSendNotification);
    }
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetButton> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window] (int result)
    {
        if (result != 1 || safeThis == nullptr)
            return;

        const auto name = window->getTextEditorContents ("name");
        const auto* link = window->getComboBoxComponent ("link");
        const auto withLink = link != nullptr && link->getSelectedItemIndex() == 1;
        const auto fxName = withLink && window->getTextEditor ("fx") != nullptr ? window->getTextEditorContents ("fx") : juce::String();
        const auto* categoryBox = window->getComboBoxComponent ("category");
        const auto category = categoryBox != nullptr ? categoryBox->getText() : juce::String();
        auto saved = false;
        if (safeThis->section == Presets::Section::bass || safeThis->section == Presets::Section::kick)
            saved = safeThis->processor.saveInstrumentPreset (safeThis->section, safeThis->layer, name, fxName, category);
        else if (safeThis->section == Presets::Section::pattern)
            saved = safeThis->processor.savePatternPreset (safeThis->layer, name, withLink ? window->getTextEditorContents ("instrument") : juce::String(), fxName, category);
        else
            saved = safeThis->processor.saveUserPreset (safeThis->section, name, safeThis->layer);
        if (! saved)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not save preset",
                                                    "Check that the name is valid: " + name);
        safeThis->updateText();
    }), true);
}
