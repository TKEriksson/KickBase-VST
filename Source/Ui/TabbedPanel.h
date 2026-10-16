#pragma once

#include "Theme.h"

// En panel med flikar i rubriken. Varje flik visar en sida (komponent).
class TabbedPanel : public juce::Component
{
public:
    static constexpr int headerHeight = 30;

    // `rememberedTab` pekar på var den valda fliken sparas (t.ex. i processorn), så att
    // samma flik visas när fönstret öppnas igen.
    explicit TabbedPanel (int* rememberedTab = nullptr)
        : remembered (rememberedTab), initialTab (rememberedTab != nullptr ? *rememberedTab : 0) {}

    // `headerExtras` (t.ex. sidans på/av-knapp och presetknapp) visas till höger i flikraden
    // när fliken är vald, så att sidan inte behöver en egen rubrikrad.
    void addTab (const juce::String& name, juce::Colour accent, juce::Component& page, juce::Component* headerExtras = nullptr)
    {
        auto button = std::make_unique<juce::TextButton> (name);
        button->setColour (juce::TextButton::buttonOnColourId, accent);
        const auto index = (int) tabs.size();
        button->onClick = [this, index] { setCurrentTab (index); };
        addAndMakeVisible (*button);
        addChildComponent (page);
        if (headerExtras != nullptr)
            addChildComponent (headerExtras);
        tabs.push_back ({ std::move (button), &page, accent, headerExtras });

        // initialTab, inte *remembered: den skrivs över när de första flikarna läggs till.
        setCurrentTab (juce::jmin (initialTab, (int) tabs.size() - 1));
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
        const auto extrasArea = header.removeFromRight (200);
        for (auto& tab : tabs)
            if (tab.headerExtras != nullptr)
                tab.headerExtras->setBounds (extrasArea);
        const auto count = std::max (1, (int) tabs.size());
        const auto width = std::min (110, (header.getWidth() - 6 * (count - 1)) / count);
        for (auto& tab : tabs)
        {
            tab.button->setBounds (header.removeFromLeft (width));
            header.removeFromLeft (6);
        }

        const auto content = getLocalBounds().reduced (10).withTrimmedTop (headerHeight - 4);
        for (auto& tab : tabs)
            tab.page->setBounds (content);
    }

private:
    struct Tab
    {
        std::unique_ptr<juce::TextButton> button;
        juce::Component* page;
        juce::Colour accent;
        juce::Component* headerExtras;
    };

    std::vector<Tab> tabs;
    int current = 0;
    int* remembered;
    int initialTab;
};
