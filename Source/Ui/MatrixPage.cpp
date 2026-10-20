#include "MatrixPage.h"

namespace
{
    constexpr int rowHeight = 28;

    // Källans namn, med instrumentets eget namn.
    juce::String labelOf (const PluginBase& p, int source)
    {
        if (Modulation::isInstrumentSource (source))
            return p.getLayerName (Modulation::layerOfSource (source)) + " " + Modulation::kindName (Modulation::kindOfSource (source));
        return Modulation::sourceName (source);
    }

    // Handritade källor: LFO:er, makron och instrumentkällor. CODE-källor visas på sina rutter men väljs bara från Live Code.
    juce::PopupMenu sourceMenu (const PluginBase& p, int current)
    {
        juce::PopupMenu menu;
        for (int lfo = 0; lfo < Modulation::maxLfos; ++lfo) // bara aktiva LFO:er (fas M)
            if (const auto source = Modulation::lfoSource (lfo); p.isLfoActive (lfo) || current == source)
                menu.addItem (1 + source, Modulation::sourceName (source), true, current == source);
        menu.addSeparator();
        for (int macro = 0; macro < Modulation::numMacros; ++macro)
            menu.addItem (1 + Modulation::firstMacro + macro, Modulation::sourceName (Modulation::firstMacro + macro), true, current == Modulation::firstMacro + macro);
        if (! p.isEffectVersion())
        {
            menu.addSeparator();
            for (int layer = 0; layer < p.activeInstruments(); ++layer)
            {
                juce::PopupMenu sub;
                auto containsCurrent = false;
                for (int kind = 0; kind < Modulation::instrumentKinds; ++kind)
                {
                    const auto source = Modulation::instrumentSource (layer, kind);
                    sub.addItem (1 + source, Modulation::kindName (kind), true, current == source);
                    containsCurrent = containsCurrent || current == source;
                }
                menu.addSubMenu (p.getLayerName (layer), sub, true, nullptr, containsCurrent);
            }
        }
        return menu;
    }

    juce::String subGroupOf (InstrumentField f)
    {
        using F = InstrumentField;
        const auto n = (int) f;
        if (n >= (int) F::aTable && n < (int) F::aTable + ParamID::oscFieldCount)         return "OSC 1";
        if (n >= (int) F::bTable && n < (int) F::bTable + ParamID::oscFieldCount)         return "OSC 2";
        if (n >= (int) F::cTable && n < (int) F::cTable + ParamID::oscFieldCount)         return "OSC 3";
        if (f == F::sampleLevel || f == F::sampleRoot || f == F::sampleStart)             return "SAMPLE";
        if (n >= (int) F::env2Attack && n <= (int) F::env2Release)                        return "ENV 2";
        if (n >= (int) F::env3Attack && n <= (int) F::env3Release)                        return "ENV 3";
        if (f == F::octave || f == F::tune || f == F::pitchEnvAmount || f == F::pitchEnvDecay) return "PITCH";
        if (f == F::sub || f == F::subWave || f == F::subOctave)                          return "SUB";
        if (f == F::noise || f == F::noiseTone || f == F::noiseType)                                         return "NOISE";
        if (f == F::keyTrack || f == F::cutoff || f == F::resonance || f == F::filterType) return "FILTER";
        if (f == F::envAmount || f == F::envAttack || f == F::envDecay || f == F::velToEnv)                    return "FILTER ENV";
        if (f == F::attack || f == F::decay || f == F::sustain || f == F::release)        return "VOLUME ENV";
        if (f == F::warpEnv || f == F::warpEnvDecay || f == F::warpEnvSource)                                                             return "WARP ENV";
        if (f == F::polarity || f == F::phaseDeg || f == F::nudgeMs)                      return "PHASE";
        if (f == F::drive || f == F::driveMode)                                           return "DRIVE";
        return "OUT";
    }

    // Målmenyn: kicken, varje instrument (med undergrupper), effektkanalerna och resten. Menyvalets id är parameterns plats + 1.
    juce::PopupMenu targetMenu (PluginBase& p, int current)
    {
        struct Entry { int index; juce::String group, sub, label; };
        std::vector<Entry> entries;
        const auto& parameters = p.getParameters();

        for (int i = 0; i < parameters.size(); ++i)
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameters[i]);
            if (ranged == nullptr || ! Modulation::isModulatable (*ranged))
                continue;

            const auto id = ranged->getParameterID();
            const auto name = ranged->getName (64);

            if (id.startsWith ("kick"))
            {
                entries.push_back ({ i, "KICK", {}, name.fromFirstOccurrenceOf ("Kick ", false, false) });
            }
            else if (const auto layer = Presets::instrumentLayerOf (id); layer >= 0)
            {
                if (layer >= p.activeInstruments())
                    continue;
                const auto prefix = "Instr " + juce::String (layer + 1) + " ";
                auto field = InstrumentField::count;
                const auto fieldName = id.substring (id[5] >= '0' && id[5] <= '9' ? 6 : 5);
                for (int f = 0; f < (int) InstrumentField::count; ++f)
                    if (fieldName == ParamID::instrumentFieldName ((InstrumentField) f))
                        field = (InstrumentField) f;
                if (field == InstrumentField::glide)
                    continue; // används inte längre: en koppling dit skulle inte göra något
                auto label = name.fromFirstOccurrenceOf (prefix, false, false);
                if (label.startsWith ("Osc "))
                    label = label.substring (6); // "Osc A Pos" -> "Pos"
                entries.push_back ({ i, p.isEffectVersion() ? juce::String ("GATE") : p.getLayerName (layer),
                                     field == InstrumentField::count ? juce::String() : subGroupOf (field), label });
            }
            else if (Effects::isEffectParameter (id))
            {
                const auto channel = id[2] - 'A';
                const auto group = Effects::channelLabel (channel);
                if (id.length() > 4 && id[3] >= '1' && id[3] <= '9' && id[4] == 'P')
                {
                    const int slot = (int) id[3] - '1';
                    const int knob = id.substring (5).getIntValue() - 1;
                    const auto& list = Effects::knobs (Effects::slotType (p.apvts, channel, slot));
                    entries.push_back ({ i, group, "SLOT " + juce::String (slot + 1) + " (" + Effects::typeNames()[(int) Effects::slotType (p.apvts, channel, slot)] + ")",
                                         juce::isPositiveAndBelow (knob, (int) list.size()) ? list[(size_t) knob].name : "P" + juce::String (knob + 1) });
                }
                else
                    entries.push_back ({ i, group, "CHANNEL", name.fromFirstOccurrenceOf (group + " ", false, false) });
            }
            else
                entries.push_back ({ i, "OTHER", {}, name });
        }

        // Gruppera i den ordning de först förekommer.
        juce::StringArray groupOrder;
        for (const auto& entry : entries)
            groupOrder.addIfNotAlreadyThere (entry.group);

        juce::PopupMenu menu;
        for (const auto& group : groupOrder)
        {
            juce::StringArray subOrder;
            for (const auto& entry : entries)
                if (entry.group == group)
                    subOrder.addIfNotAlreadyThere (entry.sub);

            juce::PopupMenu groupMenu;
            auto groupHasCurrent = false;
            for (const auto& sub : subOrder)
            {
                juce::PopupMenu subMenu;
                auto& into = sub.isEmpty() ? groupMenu : subMenu; // utan undergrupp ligger valen direkt i gruppen
                auto subHasCurrent = false;
                for (const auto& entry : entries)
                    if (entry.group == group && entry.sub == sub)
                    {
                        into.addItem (1 + entry.index, entry.label, true, entry.index == current);
                        subHasCurrent = subHasCurrent || entry.index == current;
                    }
                groupHasCurrent = groupHasCurrent || subHasCurrent;
                if (sub.isNotEmpty())
                    groupMenu.addSubMenu (sub, subMenu, true, nullptr, subHasCurrent);
            }
            menu.addSubMenu (group, groupMenu, true, nullptr, groupHasCurrent);
        }
        return menu;
    }
}

//==============================================================================
class MatrixPage::Row : public juce::Component
{
public:
    Row (MatrixPage& ownerIn, int routeIndex) : owner (ownerIn), index (routeIndex)
    {
        auto& route = owner.processor.modRoutes.routes[(size_t) index];
        const auto source = route.source.load(), target = route.target.load();
        const auto& parameters = owner.processor.getParameters();

        sourceButton.setButtonText (owner.sourceLabel (source));
        sourceButton.setColour (juce::TextButton::textColourOffId, Theme::sourceColour (source));
        sourceButton.setTooltip ("SOURCE - What moves the knob: an LFO, a macro, or an instrument's ENV 2, ENV 3, velocity or random value. Click to change it.");
        sourceButton.onClick = [this] { chooseSource(); };
        addAndMakeVisible (sourceButton);

        amount.setSliderStyle (juce::Slider::LinearHorizontal);
        amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 18);
        amount.setRange (-1.0, 1.0, 0.01);
        amount.setValue (route.amount.load(), juce::dontSendNotification);
        amount.setColour (juce::Slider::trackColourId, Theme::sourceColour (source));
        amount.setColour (juce::Slider::thumbColourId, Theme::text);
        amount.setColour (juce::Slider::textBoxTextColourId, Theme::text);
        amount.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        amount.textFromValueFunction = [] (double v) { return (v > 0.0 ? "+" : "") + juce::String (juce::roundToInt (v * 100.0)) + " %"; };
        amount.updateText();
        amount.setDoubleClickReturnValue (true, 0.25);
        amount.setTooltip ("AMOUNT - How far the source moves the knob, as a share of the knob's whole travel (negative = the other way). Double-click for 25 %.");
        amount.onValueChange = [this]
        {
            owner.processor.modRoutes.routes[(size_t) index].amount = (float) amount.getValue();
            owner.processor.markStateChanged();
        };
        addAndMakeVisible (amount);

        targetButton.setButtonText (juce::isPositiveAndBelow (target, parameters.size()) ? parameters[target]->getName (48) : juce::String ("-"));
        targetButton.setTooltip ("DESTINATION - The knob this route moves. Click to pick another one.");
        targetButton.onClick = [this] { chooseTarget(); };
        addAndMakeVisible (targetButton);

        removeButton.setTooltip ("Remove this route");
        removeButton.onClick = [this]
        {
            owner.processor.modRoutes.remove (index);
            owner.processor.markStateChanged();
        };
        addAndMakeVisible (removeButton);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.0f, 1.0f);
        g.setColour (juce::Colour (index % 2 == 0 ? 0x30ffffff : 0x18ffffff));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (Theme::sourceColour (owner.processor.modRoutes.routes[(size_t) index].source.load()));
        g.fillRoundedRectangle (bounds.withWidth (3.0f), 1.5f);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f, true));
        g.drawText (juce::String::fromUTF8 ("\xe2\x86\x92"), arrowArea, juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (6, 2);
        sourceButton.setBounds (area.removeFromLeft (150));
        area.removeFromLeft (8);
        amount.setBounds (area.removeFromLeft (230));
        arrowArea = area.removeFromLeft (26);
        removeButton.setBounds (area.removeFromRight (24));
        area.removeFromRight (6);
        targetButton.setBounds (area);
    }

private:
    void chooseSource()
    {
        auto& route = owner.processor.modRoutes.routes[(size_t) index];
        const auto target = route.target.load();
        auto menu = sourceMenu (owner.processor, route.source.load());
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sourceButton), [safe = juce::Component::SafePointer<Row> (this), target] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            auto& modRoutes = safe->owner.processor.modRoutes;
            const auto existing = modRoutes.find (result - 1, target);
            if (existing >= 0 && existing != safe->index)
                return; // den kopplingen finns redan
            modRoutes.routes[(size_t) safe->index].source = result - 1;
            ++modRoutes.version;
            safe->owner.processor.markStateChanged();
        });
    }

    void chooseTarget()
    {
        auto& route = owner.processor.modRoutes.routes[(size_t) index];
        const auto source = route.source.load();
        auto menu = targetMenu (owner.processor, route.target.load());
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&targetButton), [safe = juce::Component::SafePointer<Row> (this), source] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            auto& modRoutes = safe->owner.processor.modRoutes;
            const auto existing = modRoutes.find (source, result - 1);
            if (existing >= 0 && existing != safe->index)
                return;
            modRoutes.routes[(size_t) safe->index].target = result - 1;
            ++modRoutes.version;
            safe->owner.processor.markStateChanged();
        });
    }

    MatrixPage& owner;
    const int index;
    juce::TextButton sourceButton, targetButton, removeButton { juce::String::fromUTF8 ("\xc3\x97") };
    juce::Slider amount;
    juce::Rectangle<int> arrowArea;
};

//==============================================================================
MatrixPage::~MatrixPage() = default;

MatrixPage::MatrixPage (PluginBase& p) : processor (p)
{
    for (int macro = 0; macro < Modulation::numMacros; ++macro)
    {
        macroKnobs.push_back (std::make_unique<KnobComponent> (p.apvts, Modulation::macroID (macro), "MACRO " + juce::String (macro + 1), Theme::sourceColour (Modulation::firstMacro + macro)));
        macroKnobs.back()->setDescription ("A knob of your own: turn it by hand (or automate it from the DAW) and let it move other knobs. Drag the handle under it onto "
                                           "a knob, or set it up in the list. One macro can move many knobs at once.");
        addAndMakeVisible (*macroKnobs.back());
        macroHandles.push_back (std::make_unique<LfoDragHandle> (Modulation::firstMacro + macro, "DRAG"));
        addAndMakeVisible (*macroHandles.back());
    }

    addButton.setTooltip ("ADD ROUTE - Pick a knob and an LFO moves it (change the source and the amount in the list afterwards). You can also drag a source's handle "
                          "(LFO tab, macros here, ENV 2/3 on the instrument page) straight onto a knob.");
    addButton.onClick = [this] { addRoute(); };
    addAndMakeVisible (addButton);
    clearButton.setTooltip ("Remove every route");
    clearButton.onClick = [this]
    {
        juce::Component::SafePointer<MatrixPage> safe (this);
        juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::WarningIcon, "Clear the matrix",
                                                                                   "Remove all modulation routes?", "Remove all", "Cancel"),
                                      [safe] (int choice)
        {
            if (safe == nullptr || choice != 1)
                return;
            safe->processor.modRoutes.clear();
            safe->processor.markStateChanged();
        });
    };
    addAndMakeVisible (clearButton);

    countLabel.setFont (Theme::font (11.0f, true));
    countLabel.setColour (juce::Label::textColourId, Theme::textDim);
    countLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (countLabel);

    emptyLabel.setText ("No routes yet. Drag a handle (LFO, macro, ENV 2/3) onto any knob, or press + ADD ROUTE.", juce::dontSendNotification);
    emptyLabel.setFont (Theme::font (12.0f));
    emptyLabel.setColour (juce::Label::textColourId, Theme::textDim);
    emptyLabel.setJustificationType (juce::Justification::centred);
    addChildComponent (emptyLabel);

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    rebuild();
    startTimerHz (5);
}

juce::String MatrixPage::sourceLabel (int source) const
{
    return labelOf (processor, source);
}

void MatrixPage::addRoute()
{
    auto menu = targetMenu (processor, -1);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addButton), [safe = juce::Component::SafePointer<MatrixPage> (this)] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        // Källan: den första aktiva LFO:n (ingen aktiv: LFO 1 läggs till).
        auto& p = safe->processor;
        auto source = -1;
        for (int lfo = 0; lfo < p.availableLfos() && source < 0; ++lfo)
            if (p.isLfoActive (lfo))
                source = Modulation::lfoSource (lfo);
        if (source < 0)
        {
            p.setLfoActive (0, true);
            source = 0;
        }
        if (safe->processor.modRoutes.add (source, result - 1, 0.25f) < 0)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Matrix", "All routes are used (" + juce::String (Modulation::maxRoutes) + ").");
        safe->processor.markStateChanged();
    });
}

void MatrixPage::timerCallback()
{
    if (processor.modRoutes.version.load() != lastVersion || processor.activeInstruments() != lastActive
        || processor.layerNamesVersion.load() != lastNamesVersion)
        rebuild();
}

void MatrixPage::rebuild()
{
    lastVersion = processor.modRoutes.version.load();
    lastActive = processor.activeInstruments();
    lastNamesVersion = processor.layerNamesVersion.load();

    rows.clear();
    list.removeAllChildren();
    auto used = 0;
    for (int i = 0; i < Modulation::maxRoutes; ++i)
        if (processor.modRoutes.routes[(size_t) i].source.load() >= 0)
        {
            rows.push_back (std::make_unique<Row> (*this, i));
            list.addAndMakeVisible (*rows.back());
            ++used;
        }
    countLabel.setText (juce::String (used) + " / " + juce::String (Modulation::maxRoutes) + " routes", juce::dontSendNotification);
    emptyLabel.setVisible (used == 0);
    clearButton.setEnabled (used > 0);
    addButton.setEnabled (used < Modulation::maxRoutes);
    resized();
}

void MatrixPage::paint (juce::Graphics& g)
{
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f, true).withExtraKerningFactor (0.08f));
    g.drawText ("MACROS", macroTitleArea, juce::Justification::centredLeft);
}

void MatrixPage::resized()
{
    auto area = getLocalBounds();

    // Makrona till vänster: fyra rattar med handtag under.
    auto macroArea = area.removeFromLeft (330);
    area.removeFromLeft (14);
    macroTitleArea = macroArea.removeFromTop (14);
    const auto cell = macroArea.getWidth() / Modulation::numMacros;
    for (int macro = 0; macro < Modulation::numMacros; ++macro)
    {
        auto column = macroArea.removeFromLeft (cell);
        macroHandles[(size_t) macro]->setBounds (column.removeFromBottom (26).reduced (3, 0));
        column.removeFromBottom (4);
        macroKnobs[(size_t) macro]->setBounds (column);
    }

    auto top = area.removeFromTop (26);
    addButton.setBounds (top.removeFromLeft (120));
    top.removeFromLeft (6);
    clearButton.setBounds (top.removeFromLeft (90));
    countLabel.setBounds (top);
    area.removeFromTop (4);

    viewport.setBounds (area);
    emptyLabel.setBounds (area);
    const auto width = area.getWidth() - viewport.getScrollBarThickness() - 2;
    list.setSize (width, std::max (1, (int) rows.size()) * rowHeight);
    auto y = 0;
    for (auto& row : rows)
    {
        row->setBounds (0, y, width, rowHeight);
        y += rowHeight;
    }
}
