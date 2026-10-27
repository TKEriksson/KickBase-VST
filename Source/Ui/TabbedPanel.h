#pragma once

#include "Theme.h"
#include "ListenButton.h"

// En liten på/av-knapp (ström-symbol) som sitter på en flik, så att man kan stänga av ett instrument utan att öppna fliken.
class PowerButton : public juce::Button
{
public:
    explicit PowerButton (juce::Colour accentColour) : juce::Button ("power"), accent (accentColour) { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        const auto area = getLocalBounds().toFloat().reduced (2.0f);
        const auto radius = std::min (area.getWidth(), area.getHeight()) * 0.5f - 1.0f;
        const auto centre = area.getCentre();
        const auto colour = getToggleState() ? accent : Theme::textDim.withAlpha (0.55f);
        g.setColour (colour.withAlpha (highlighted ? 1.0f : 0.9f));

        juce::Path symbol;
        symbol.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
        symbol.startNewSubPath (centre.x, centre.y - radius - 0.5f);
        symbol.lineTo (centre.x, centre.y - 0.5f);
        g.strokePath (symbol, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    juce::Colour accent;
};

// En flikknapp som reagerar på att man drar en modulationskälla ("mod:...") över den: efter en kort stund byter fliken till sig själv,
// så att man kan släppa källan på en kontroll i en annan flik (t.ex. en effekt) utan att släppa den först. Själv tar den inte emot något.
class TabButton : public juce::TextButton, public juce::DragAndDropTarget, private juce::Timer
{
public:
    using juce::TextButton::TextButton;

    std::function<void()> onDragHover;

    bool isInterestedInDragSource (const SourceDetails& details) override { return details.description.toString().startsWith ("mod:"); }
    void itemDragEnter (const SourceDetails&) override { startTimer (hoverMs); highlight (true); }
    void itemDragExit (const SourceDetails&) override { stopTimer(); highlight (false); }
    void itemDropped (const SourceDetails&) override { stopTimer(); highlight (false); }

private:
    static constexpr int hoverMs = 700;

    void highlight (bool on)
    {
        if (on)
            setColour (juce::TextButton::buttonColourId, findColour (juce::TextButton::buttonOnColourId).withAlpha (0.45f));
        else
            removeColour (juce::TextButton::buttonColourId);
        repaint();
    }

    void timerCallback() override
    {
        stopTimer();
        highlight (false);
        if (onDragHover)
            onDragHover();
    }
};

// En panel med flikar i rubriken. Varje flik visar en sida (komponent).
class TabbedPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int headerHeight = 30;
    static constexpr int workspaceHeaderHeight = 38;

    // `rememberedTab` pekar på var den valda fliken sparas (t.ex. i processorn), så att
    // samma flik visas när fönstret öppnas igen.
    explicit TabbedPanel (int* rememberedTab = nullptr)
        : remembered (rememberedTab), initialTab (rememberedTab != nullptr ? *rememberedTab : 0) {}

    // E1: underflikar ritas som en segmenterad rad (egenskapen "subtab" läses av KickbaseLookAndFeel), huvudflikar som pills. Anropas före addTab.
    void setSubTabs() { subTabs = true; }

    // `headerExtras` (t.ex. sidans på/av-knapp och presetknapp) visas till höger i flikraden
    // när fliken är vald, så att sidan inte behöver en egen rubrikrad.
    void addTab (const juce::String& name, juce::Colour accent, juce::Component& page, juce::Component* headerExtras = nullptr, int extrasWidth = 380)
    {
        auto button = std::make_unique<TabButton> (name);
        button->setColour (juce::TextButton::buttonOnColourId, accent);
        button->getProperties().set ("subtab", subTabs);
        const auto index = (int) tabs.size();
        button->onClick = [this, index] { setCurrentTab (index); };
        button->onDragHover = [this, index] { setCurrentTab (index); };
        addAndMakeVisible (*button);
        addChildComponent (page);
        if (headerExtras != nullptr)
            addChildComponent (headerExtras);
        tabs.push_back ({ std::move (button), &page, accent, headerExtras, extrasWidth });

        // initialTab, inte *remembered: den skrivs över när de första flikarna läggs till.
        setCurrentTab (juce::jmin (initialTab, (int) tabs.size() - 1));
    }

    // En på/av-knapp på fliken, kopplad till en switch-parameter. Fliken blir dämpad när instrumentet är av.
    void addPower (int tabIndex, juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterID,
                   const juce::String& tooltip = "ON / OFF - Turns this instrument on or off without opening the tab.")
    {
        auto& tab = tabs[(size_t) tabIndex];
        tab.power = std::make_unique<PowerButton> (tab.accent);
        tab.power->setTooltip (tooltip);
        addAndMakeVisible (*tab.power);
        tab.powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, parameterID, *tab.power);
        auto* button = tab.button.get();
        auto* power = tab.power.get();
        tab.power->onStateChange = [button, power]
        {
            const auto colour = power->getToggleState() ? juce::Colour() : Theme::textDim.withAlpha (0.45f);
            if (power->getToggleState())
            {
                button->removeColour (juce::TextButton::textColourOffId);
                button->removeColour (juce::TextButton::textColourOnId);
            }
            else
            {
                button->setColour (juce::TextButton::textColourOffId, colour);
                button->setColour (juce::TextButton::textColourOnId, colour);
            }
        };
        tab.power->onStateChange();
        resized();
    }

    // Förhandsspelning för en flik: högtalaren längst till höger i flikraden (håll = spela, dubbelklick = fastlåst) och gäller bara när fliken
    // är vald. Flikar utan anrop visar ingen knapp.
    void setPreview (int tabIndex, std::function<void()> onStart, std::function<void()> onStop, const juce::String& tooltip)
    {
        auto& tab = tabs[(size_t) tabIndex];
        tab.previewStart = std::move (onStart);
        tab.previewStop = std::move (onStop);
        tab.previewTooltip = tooltip;
        if (! previewButton)
        {
            previewButton = std::make_unique<ListenButton>();
            previewButton->onStart = [this] { if (tabs[(size_t) current].previewStart) tabs[(size_t) current].previewStart(); };
            previewButton->onStop = [this] { if (tabs[(size_t) current].previewStop) tabs[(size_t) current].previewStop(); };
            addAndMakeVisible (*previewButton);
        }
        updatePreviewButton();
        resized();
    }

    // Flikens text kommer från en funktion (t.ex. ett lagers egna namn) och uppdateras när den ändras.
    void setNameProvider (int tabIndex, std::function<juce::String()> provider)
    {
        tabs[(size_t) tabIndex].nameProvider = std::move (provider);
        startTimerHz (4);
    }

    void setCurrentTab (int index)
    {
        current = juce::jlimit (0, (int) tabs.size() - 1, index);
        if (remembered != nullptr)
            *remembered = current;

        for (int i = 0; i < (int) tabs.size(); ++i)
        {
            tabs[(size_t) i].button->setToggleState (i == current, juce::dontSendNotification);
            tabs[(size_t) i].page->setVisible (i == current);
            if (auto* extras = tabs[(size_t) i].headerExtras)
                extras->setVisible (i == current);
        }
        updatePreviewButton();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        Theme::paintMetal (g, bounds, 8.0f, juce::Colour(), juce::Colour());

        if (tabs.empty())
            return;
        const auto& currentTab = tabs[(size_t) current];

        if (subTabs)
        {
            // Mörk infälld remsa bakom underflikarna, med flikens namn som liten mono-etikett längst till höger.
            const auto strip = stripArea.toFloat();
            g.setColour (Theme::insetStrip);
            g.fillRoundedRectangle (strip, 12.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0.0f, strip.getY(),
                                                     juce::Colours::transparentBlack, 0.0f, strip.getY() + 5.0f, false));
            g.fillRoundedRectangle (strip.reduced (1.0f, 1.0f), 11.0f);
            g.setColour (Theme::panelBorderDim);
            g.drawRoundedRectangle (strip.reduced (0.5f), 12.0f, 1.0f);

            // Stjälk under den valda underfliken (skissen: .tab-stem / .tab-stem-inner), så att den binder till innehållet under remsan.
            const auto cx = currentTab.button->getBounds().toFloat().getCentreX();
            const auto y = strip.getBottom() - 0.5f;
            juce::Path stem, stemInner;
            stem.addTriangle (cx - 6.0f, y, cx + 6.0f, y, cx, y + 6.0f);
            stemInner.addTriangle (cx - 5.0f, y, cx + 5.0f, y, cx, y + 4.5f);
            g.setColour (Theme::panelBorderDim);
            g.fillPath (stem);
            g.setColour (Theme::panelAlt);
            g.fillPath (stemInner);

            if (labelArea.getWidth() > 50)
            {
                g.setColour (Theme::panelBorderDim);
                g.drawVerticalLine (labelArea.getX(), (float) labelArea.getY() + 4.0f, (float) labelArea.getBottom() - 4.0f);
                g.setColour (Theme::slate500);
                Theme::drawTracked (g, currentTab.button->getButtonText(), labelArea.withTrimmedLeft (8).toFloat(), Theme::monoFont (10.0f, false),
                                    0.14f, juce::Justification::centredLeft);
            }
        }
        else
        {
            // Stjälk: en liten triangel under den valda fliken som binder den till innehållet under.
            const auto b = currentTab.button->getBounds().toFloat();
            const auto cx = b.getCentreX();
            const auto y = b.getBottom() + 1.0f;
            juce::Path stem;
            stem.addTriangle (cx - 7.0f, y, cx + 7.0f, y, cx, y + 7.0f);
            g.setColour (Theme::panelBorderDim);
            g.fillPath (stem);
            juce::Path inner;
            inner.addTriangle (cx - 5.0f, y, cx + 5.0f, y, cx, y + 5.5f);
            g.setColour (Theme::panelAlt);
            g.fillPath (inner);

            // Arbetsytans rubrikrad: mono-brödsmula i cyan och flikens namn i fet vit text.
            const auto hb = workspaceHeader.toFloat();
            g.setColour (Theme::panel);
            g.fillRoundedRectangle (hb, 12.0f);
            g.setColour (juce::Colour (0xcc1e293b));
            g.drawRoundedRectangle (hb.reduced (0.5f), 12.0f, 1.0f);
            const auto name = currentTab.button->getButtonText();
            auto text = workspaceHeader.reduced (14, 4);
            g.setColour (Theme::cyan.withAlpha (0.85f));
            Theme::drawTracked (g, "KICKBASE  >  " + name, text.removeFromTop (12).toFloat(), Theme::monoFont (9.5f, false), 0.14f,
                                juce::Justification::centredLeft);
            g.setColour (Theme::cyan.withAlpha (0.18f));
            g.setFont (Theme::font (15.0f, true));
            g.drawText (name.toUpperCase(), text.translated (0, 1), juce::Justification::centredLeft);
            g.setColour (juce::Colours::white);
            g.drawText (name.toUpperCase(), text, juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto header = getLocalBounds().removeFromTop (headerHeight).reduced (10, 4).withTrimmedTop (2);
        if (previewButton != nullptr)
        {
            previewButton->setBounds (header.removeFromRight (previewWidth));
            header.removeFromRight (6);
        }
        auto reserved = 0;
        for (auto& tab : tabs)
            if (tab.headerExtras != nullptr)
                reserved = std::max (reserved, tab.extrasWidth);
        const auto extrasArea = header.removeFromRight (reserved);
        for (auto& tab : tabs)
            if (tab.headerExtras != nullptr)
                tab.headerExtras->setBounds (extrasArea.withTrimmedLeft (extrasArea.getWidth() - tab.extrasWidth));
        const auto count = std::max (1, (int) tabs.size());
        const auto width = std::min (110, (header.getWidth() - 6 * (count - 1)) / count);
        stripArea = header.expanded (4, 3).withRight (header.getRight() + 4);
        labelArea = stripArea.withTrimmedLeft (std::max (0, count * (width + 6) + 4)).withTrimmedRight (8);
        for (auto& tab : tabs)
        {
            tab.button->setBounds (header.removeFromLeft (width));
            if (tab.power != nullptr)
            {
                tab.power->setBounds (tab.button->getBounds().removeFromRight (22).reduced (3));
                tab.power->toFront (false);
            }
            header.removeFromLeft (6);
        }

        auto content = getLocalBounds().reduced (10).withTrimmedTop (headerHeight - 4);
        if (! subTabs)
        {
            content.removeFromTop (6);
            workspaceHeader = content.removeFromTop (workspaceHeaderHeight);
            content.removeFromTop (6);
        }
        for (auto& tab : tabs)
            tab.page->setBounds (content);
    }

private:
    static constexpr int previewWidth = 30;

    void updatePreviewButton()
    {
        if (previewButton == nullptr || tabs.empty())
            return;
        const auto& tab = tabs[(size_t) current];
        previewButton->setVisible (tab.previewStart != nullptr);
        previewButton->setAccent (tab.accent);
        previewButton->setTooltip (tab.previewTooltip);
    }

    void timerCallback() override
    {
        for (auto& tab : tabs)
            if (tab.nameProvider != nullptr)
                if (const auto name = tab.nameProvider(); name != tab.button->getButtonText())
                    tab.button->setButtonText (name);
    }

    struct Tab
    {
        std::unique_ptr<juce::TextButton> button;
        juce::Component* page;
        juce::Colour accent;
        juce::Component* headerExtras;
        int extrasWidth = 380;
        std::unique_ptr<PowerButton> power;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
        std::function<juce::String()> nameProvider;
        std::function<void()> previewStart, previewStop;
        juce::String previewTooltip;
    };

    std::vector<Tab> tabs;
    std::unique_ptr<ListenButton> previewButton;
    int current = 0;
    bool subTabs = false;
    juce::Rectangle<int> stripArea, labelArea, workspaceHeader;
    int* remembered;
    int initialTab;
};
