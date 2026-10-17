#pragma once

#include "Theme.h"

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

// En panel med flikar i rubriken. Varje flik visar en sida (komponent).
class TabbedPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int headerHeight = 30;

    // `rememberedTab` pekar på var den valda fliken sparas (t.ex. i processorn), så att
    // samma flik visas när fönstret öppnas igen.
    explicit TabbedPanel (int* rememberedTab = nullptr)
        : remembered (rememberedTab), initialTab (rememberedTab != nullptr ? *rememberedTab : 0) {}

    // `headerExtras` (t.ex. sidans på/av-knapp och presetknapp) visas till höger i flikraden
    // när fliken är vald, så att sidan inte behöver en egen rubrikrad.
    void addTab (const juce::String& name, juce::Colour accent, juce::Component& page, juce::Component* headerExtras = nullptr, int extrasWidth = 380)
    {
        auto button = std::make_unique<juce::TextButton> (name);
        button->setColour (juce::TextButton::buttonOnColourId, accent);
        const auto index = (int) tabs.size();
        button->onClick = [this, index] { setCurrentTab (index); };
        addAndMakeVisible (*button);
        addChildComponent (page);
        if (headerExtras != nullptr)
            addChildComponent (headerExtras);
        tabs.push_back ({ std::move (button), &page, accent, headerExtras, extrasWidth });

        // initialTab, inte *remembered: den skrivs över när de första flikarna läggs till.
        setCurrentTab (juce::jmin (initialTab, (int) tabs.size() - 1));
    }

    // En på/av-knapp på fliken, kopplad till en switch-parameter. Fliken blir dämpad när instrumentet är av.
    void addPower (int tabIndex, juce::AudioProcessorValueTreeState& apvts, const juce::String& parameterID)
    {
        auto& tab = tabs[(size_t) tabIndex];
        tab.power = std::make_unique<PowerButton> (tab.accent);
        tab.power->setTooltip ("ON / OFF - Turns this instrument on or off without opening the tab.");
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
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        Theme::paintMetal (g, bounds, 8.0f, juce::Colour (0xf022263a), juce::Colour (0xf0141624));
    }

    void resized() override
    {
        auto header = getLocalBounds().removeFromTop (headerHeight).reduced (10, 4).withTrimmedTop (2);
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

        const auto content = getLocalBounds().reduced (10).withTrimmedTop (headerHeight - 4);
        for (auto& tab : tabs)
            tab.page->setBounds (content);
    }

private:
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
    };

    std::vector<Tab> tabs;
    int current = 0;
    int* remembered;
    int initialTab;
};
