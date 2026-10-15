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

    void addTab (const juce::String& name, juce::Colour accent, juce::Component& page)
    {
        auto button = std::make_unique<juce::TextButton> (name);
        button->setColour (juce::TextButton::buttonOnColourId, accent);
        const auto index = (int) tabs.size();
        button->onClick = [this, index] { setCurrentTab (index); };
        addAndMakeVisible (*button);
        addChildComponent (page);
        tabs.push_back ({ std::move (button), &page, accent });

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
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (Theme::panelBorder);
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

        if (! tabs.empty())
        {
            g.setColour (tabs[(size_t) current].accent);
            g.fillRoundedRectangle (bounds.getX() + 10.0f, bounds.getY(), 36.0f, 3.0f, 1.5f);
        }
    }

    void resized() override
    {
        auto header = getLocalBounds().removeFromTop (headerHeight).reduced (10, 4).withTrimmedTop (2);
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
    };

    std::vector<Tab> tabs;
    int current = 0;
    int* remembered;
    int initialTab;
};
