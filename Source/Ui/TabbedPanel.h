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
    static constexpr int headerHeight = 3 + Theme::Space::menuButtonH + 2 * Theme::Space::menuPadY + 3;
    static constexpr int extrasRowHeight = 44; // rad 2: sidans kontroller (väljare, på/av, preset) i remsa-stil

    // `rememberedTab` pekar på var den valda fliken sparas (t.ex. i processorn), så att
    // samma flik visas när fönstret öppnas igen.
    explicit TabbedPanel (int* rememberedTab = nullptr)
        : remembered (rememberedTab), initialTab (rememberedTab != nullptr ? *rememberedTab : 0) {}

    // E1: underflikar ritas som en segmenterad rad (egenskapen "subtab" läses av KickbaseLookAndFeel), huvudflikar som pills. Anropas före addTab.
    void setSubTabs() { subTabs = true; }

    // Remsans etikett längst till höger (t.ex. "TOOLS"). Utan etikett visas den valda flikens namn.
    void setStripLabel (const juce::String& text) { stripLabel = text; }

    // `headerExtras` (t.ex. sidans på/av-knapp och presetknapp) visas till höger i flikraden
    // när fliken är vald, så att sidan inte behöver en egen rubrikrad.
    void addTab (const juce::String& name, juce::Colour accent, juce::Component& page, juce::Component* headerExtras = nullptr, int extrasWidth = 380)
    {
        auto button = std::make_unique<TabButton> (name);
        button->setColour (juce::TextButton::buttonOnColourId, accent);
        button->getProperties().set ("subtab", subTabs);
        button->getProperties().set ("noshrink", true); // knapptexten krymper aldrig: knappen anpassas efter texten (pillWidth)
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
        tab.button->getProperties().set ("powerWidth", powerSpace); // LookAndFeel centrerar texten utanför ikonen
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

    // En avdelare efter den senast tillagda fliken (o): ett tunt streck i flikraden som grupperar flikarna. Inga gruppnamn, det tar plats.
    void addSeparator() { tabs.back().dividerAfter = true; }

    int getCurrentTab() const { return current; }
    juce::String getCurrentTabName() const { return tabs.empty() ? juce::String() : tabs[(size_t) current].button->getButtonText(); }

    void setCurrentTab (int index)
    {
        const auto previous = current;
        current = juce::jlimit (0, (int) tabs.size() - 1, index);
        // Ett flikbyte stoppar flikens egen förhandsspelning, men bara om flikens egen LISTEN-knapp startade den (mönstrets och
        // pianorullens PLAY ligger utanför flikarna och rörs inte).
        if (previous != current && previous < (int) tabs.size() && tabs[(size_t) previous].previewStop
            && previewButton != nullptr && previewButton->isHeldOrLatched())
        {
            previewButton->release();
            tabs[(size_t) previous].previewStop();
        }
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
        resized(); // sidans höjd beror på om fliken har en rad 2
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
            g.setColour (juce::Colour (0xff121b2e)); // nivå 2: blåtonad remsa
            g.fillRoundedRectangle (strip, 12.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0.0f, strip.getY(),
                                                     juce::Colours::transparentBlack, 0.0f, strip.getY() + 5.0f, false));
            g.fillRoundedRectangle (strip.reduced (1.0f, 1.0f), 11.0f);
            g.setColour (Theme::panelBorderDim);
            g.drawRoundedRectangle (strip.reduced (0.5f), 12.0f, 1.0f);

            g.setColour (Theme::panelBorderDim);
            for (const auto x : dividerXs)
                g.drawLine ((float) x + 0.5f, strip.getY() + 7.0f, (float) x + 0.5f, strip.getBottom() - 7.0f, 1.0f);

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
            Theme::paintDividerGlow (g, juce::Rectangle<float> (10.0f, strip.getBottom() + 5.0f, (float) getWidth() - 20.0f, 1.0f), false);
        }
        else
        {
            // Nivå 1 (huvudmeny) avgränsas från innehållet med en glödande avdelare (o, z).
            Theme::paintDividerGlow (g, juce::Rectangle<float> (10.0f, (float) headerHeight - 2.0f, (float) getWidth() - 20.0f, 1.0f), false);

            // Brödsmulan (> NAMN [> undersida ...]) som glasdisplay i rad 1 bredvid huvudmenyn ersätter den gamla 38 px-rubrikraden.
            if (crumbArea.getWidth() > 60)
            {
                auto crumb = ">  " +currentTab.button->getButtonText().toUpperCase();
                if (crumbSuffix != nullptr)
                    if (const auto suffix = crumbSuffix(); suffix.isNotEmpty())
                        crumb << "  >  " << suffix.toUpperCase();
                const auto glass = crumbArea.toFloat().reduced (0.0f, 1.0f);
                Theme::paintGlassDisplay (g, glass);
                Theme::drawGlowText (g, crumb, glass.reduced (10.0f, 0.0f), Theme::monoFont (9.5f, false), 0.14f,
                                     juce::Justification::centredLeft, Theme::cyan.withAlpha (0.9f));
            }

            // Rad 2: sidans egna kontroller i en mörk remsa (samma stil som underflikarna).
            if (currentTab.headerExtras != nullptr)
            {
                const auto strip = extrasRow.toFloat();
                g.setColour (juce::Colour (0xff06080d)); // nivå 3: mörkare, lägre remsa
                g.fillRoundedRectangle (strip, 10.0f);
                g.setColour (Theme::panelBorderDim);
                g.drawRoundedRectangle (strip.reduced (0.5f), 10.0f, 1.0f);
                Theme::paintDividerGlow (g, juce::Rectangle<float> (10.0f, strip.getBottom() + 3.0f, (float) getWidth() - 20.0f, 1.0f), false);
            }
        }
    }

    // Valfri andra nivån i brödsmulan (t.ex. ANALYZER > SPECTRUM): funktionen anropas vid varje ritning och returnerar undersidans namn.
    std::function<juce::String()> crumbSuffix;

    void resized() override
    {
        auto header = juce::Rectangle<int> (10, 3 + Theme::Space::menuPadY, std::max (0, getWidth() - 20), Theme::Space::menuButtonH);
        if (previewButton != nullptr)
        {
            previewButton->setBounds (header.removeFromRight (previewWidth));
            header.removeFromRight (6);
        }
        // Knappens bredd = textbredd + 2 x 14 px (+ plats för LED/ström-knapp): texten krymper aldrig.
        const auto pillsLeft = header.getX();
        const auto strip = header.expanded (4, Theme::Space::menuPadY);
        stripArea = strip;
        int usedWidth = 0;
        dividerXs.clear();
        for (auto& tab : tabs)
        {
            const auto width = pillWidth (tab);
            tab.button->setBounds (header.removeFromLeft (width));
            if (tab.power != nullptr)
            {
                tab.power->setBounds (tab.button->getBounds().removeFromRight (powerSpace).reduced (4, 6));
                tab.power->toFront (false);
            }
            header.removeFromLeft (tabGap);
            usedWidth += width + tabGap;
            if (tab.dividerAfter)
            {
                dividerXs.push_back (header.getX() + dividerGap / 2);
                header.removeFromLeft (dividerGap);
                usedWidth += dividerGap;
            }
        }
        const auto restArea = juce::Rectangle<int> (pillsLeft + usedWidth + 4, strip.getY(), std::max (0, header.getRight() + 4 - (pillsLeft + usedWidth + 4)), strip.getHeight());
        labelArea = subTabs ? restArea.withTrimmedRight (8) : juce::Rectangle<int>();
        crumbArea = subTabs ? juce::Rectangle<int>() : restArea.withTrimmedLeft (10).withTrimmedRight (6);

        extrasRow = juce::Rectangle<int> (10, headerHeight + 2, std::max (0, getWidth() - 20), extrasRowHeight);
        for (auto& tab : tabs)
        {
            auto content = getLocalBounds().reduced (Theme::Space::panelPad).withTrimmedTop (headerHeight - 12);
            if (tab.headerExtras != nullptr)
            {
                tab.headerExtras->setBounds (extrasRow.reduced (8, 4));
                content.setTop (extrasRow.getBottom() + Theme::Space::md);
            }
            tab.page->setBounds (content);
        }
    }

private:
    static constexpr int previewWidth = 30;
    static constexpr int powerSpace = 30; // plats för på/av-ikonen längst till höger på en flik
    static constexpr int tabGap = Theme::Space::menuGapX;
    static constexpr int dividerGap = 12; // extra plats för en avdelare mellan två flikgrupper

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
        bool dividerAfter = false;
    };

    // Samma typsnitt som KickbaseLookAndFeel::drawButtonText använder för "noshrink"-knappar (höjd 11, bold, kerning som i LookAndFeel).
    int pillWidth (const Tab& tab) const
    {
        auto font = tab.button->getLookAndFeel().getTextButtonFont (*tab.button, headerHeight).boldened().withHeight (11.0f);
        font.setExtraKerningFactor (subTabs ? 0.04f : 0.08f);
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font, tab.button->getButtonText().toUpperCase(), 0.0f, 0.0f);
        const auto text = (int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth());
        const auto led = subTabs ? 10 : 15;
        return text + 2 * 18 + led + (tab.power != nullptr ? powerSpace + 6 : 0); // ikon + marginal mot texten
    }

    std::vector<Tab> tabs;
    std::unique_ptr<ListenButton> previewButton;
    int current = 0;
    bool subTabs = false;
    juce::Rectangle<int> stripArea, labelArea, crumbArea, extrasRow;
    std::vector<int> dividerXs;
    juce::String stripLabel;
    int* remembered;
    int initialTab;
};
