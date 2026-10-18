#pragma once

#include "VisualTimer.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

class PluginBase;

// Högerklicksmenyn för en parameter: `menu` (egna val) följt av DAW:ens val, t.ex. FL Studios
// "Create automation clip" och "Link to controller" (lyssna efter en MIDI-ratt).
void showParameterMenu (juce::Component& source, juce::RangedAudioParameter& parameter, juce::PopupMenu menu);

// Processorn som komponenten tillhör (via editorn), eller nullptr.
PluginBase* findProcessor (const juce::Component& component);

// En ratt med namn under, kopplad till en parameter.
//
// - Dubbelklick återställer standardvärdet, högerklick visar menyn (DAW-automation m.m.).
// - Dra en LFO hit (från LFO-fliken) för att modulera ratten. Varje koppling visas som en ring
//   i LFO:ns färg; dra i ringen för att ändra mängden. En prick visar var ratten är just nu.
class KnobComponent : public juce::Component, public juce::DragAndDropTarget, private VisualTimer
{
public:
    KnobComponent (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& name, juce::Colour accent);
    ~KnobComponent() override;

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    // Beskrivning som visas när musen är över ratten (och i inforaden).
    void setDescription (const juce::String& text);

    // Namnet ovanför ratten i stället för under (t.ex. bredvid en meny som har sin rubrik ovanför).
    void setLabelAbove (bool above) { labelAbove = above; resized(); }

    // Ett nytt namn under ratten (t.ex. P1 och P2 byter namn efter filtermodellen).
    void setLabelText (const juce::String& text) { name = text; label.setText (text, juce::dontSendNotification); slider.setName (text); }

    // En liveindikator inuti ratten: en båge som visar ett värde 0..1 just nu (t.ex. hur mycket
    // basen duckas under kicken, så att man ser den pumpa).
    void setLiveMeter (std::function<float()> read, juce::Colour colour) { liveMeter = std::move (read); liveColour = colour; }

    class ModSlider : public juce::Slider
    {
    public:
        explicit ModSlider (KnobComponent& o) : owner (o) {}
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        KnobComponent& owner;
    };

    ModSlider slider { *this };

private:
    friend class ModSlider;
    bool labelAbove = false;

    struct Ring
    {
        int route;   // plats i ModRoutes
        int source;  // LFO
        float amount;
    };

    void timerCallback() override;
    std::vector<Ring> rings() const;
    int parameterIndex() const;
    int ringAt (juce::Point<float> position) const; // index i rings(), eller -1
    float ringRadius (int ring) const;
    juce::Point<float> knobCentre() const;
    float trackRadius() const;
    void showMenu();
    void updateTooltip (int hoveredRing);

    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::RangedAudioParameter* parameter = nullptr;
    juce::String name, description;
    juce::Colour accent;
    bool modulatable = false;
    bool dropHover = false;
    std::function<float()> liveMeter;
    juce::Colour liveColour;
    float lastLive = -1.0f;

    int dragRing = -1;        // ringen som dras, index i rings()
    int dragRoute = -1;
    float dragStartAmount = 0.0f;
    float dragStartY = 0.0f;
    int lastRoutesVersion = -1;
    mutable int cachedIndex = -1;
    float lastOffset = 0.0f;
};

// En liten rullgardinslista med etikett ovanför, kopplad till en valparameter.
// Högerklick visar menyn med DAW:ens val.
class ChoiceComponent : public juce::Component
{
public:
    ChoiceComponent (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& name);

    void resized() override;

    class Box : public juce::ComboBox
    {
    public:
        juce::RangedAudioParameter* parameter = nullptr;
        void mouseDown (const juce::MouseEvent&) override;
    };

    Box box;
    float weight = 1.6f; // bredd i rattceller i ControlGroups (valrutor får mer än en ratt; höj den för långa texter)

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// Kompakt FX-send i synthens flikrad: "FX", en kanalväljare (None/A/B/C) och en liten reglage för
// hur mycket av ljudet som går genom kanalen. Högerklick visar menyn med DAW:ens val.
class FxSendControl : public juce::Component
{
public:
    FxSendControl (juce::AudioProcessorValueTreeState& apvts, const juce::String& channelID, const juce::String& amountID,
                   const juce::String& name, juce::Colour accent);

    void resized() override;

    class AmountSlider : public juce::Slider
    {
    public:
        juce::RangedAudioParameter* parameter = nullptr;
        void mouseDown (const juce::MouseEvent&) override;
    };

private:
    juce::Label label;
    ChoiceComponent::Box channel;
    AmountSlider amount;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
};

// Raden längst ner som alltid förklarar det musen är över (tooltip-texten för kontrollen
// under musen, eller en allmän tips-text).
class InfoBar : public juce::Component, private juce::Timer
{
public:
    InfoBar();
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    juce::String text;
};

// Rattar i grupper med rubrik (OSC 1, FILTER ...): grupperna läggs i rader som var och en fyller
// hela bredden. Ritar gruppernas ramar och rubriker.
class ControlGroups
{
public:
    struct Group
    {
        juce::String title;
        std::vector<juce::Component*> controls;
        juce::String description; // visas när musen är över rubriken (via ägarens tooltip)
    };

    void addRow (std::vector<Group> row) { rows.push_back (std::move (row)); }
    int numRows() const { return (int) rows.size(); }

    // Lägger ut raderna i `area` (uppifrån). Valrutor (ChoiceComponent) centreras på höjden.
    void layout (juce::Rectangle<int> area, int rowHeight);
    void paint (juce::Graphics& g, juce::Colour accent) const;

    static constexpr int titleHeight = 12;

private:
    std::vector<std::vector<Group>> rows;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> frames; // beräknas i layout
};

// En panel med rubrik i modulens färg och (valfritt) en på/av-knapp kopplad till en parameter.
class ModulePanel : public juce::Component
{
public:
    static constexpr int headerHeight = 28;

    ModulePanel (const juce::String& titleIn, juce::Colour accentIn,
                 juce::AudioProcessorValueTreeState* apvts = nullptr, const juce::String& powerParamID = {})
        : title (titleIn), accent (accentIn)
    {
        if (apvts != nullptr && powerParamID.isNotEmpty())
        {
            power = std::make_unique<juce::ToggleButton>();
            power->setColour (juce::ToggleButton::tickColourId, accent);
            power->setTooltip ("Turns the " + titleIn.toLowerCase() + " on or off");
            addAndMakeVisible (*power);
            powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (*apvts, powerParamID, *power);
        }
    }

    // Ytan där modulens innehåll placeras (under rubriken, eller hela ytan i en flik).
    juce::Rectangle<int> getContentBounds() const
    {
        return embedded ? getLocalBounds().reduced (2, 0) : getLocalBounds().reduced (10).withTrimmedTop (headerHeight - 4);
    }

    juce::Colour getAccent() const { return accent; }
    bool isEmbedded() const { return embedded; }
    // Bredden flikraden ska reservera för modulens kontroller i rubriken (på/av, vyflikar, preset, FX-send).
    int headerExtrasWidth() const { return 380 + (headerTabs != nullptr ? headerTabsWidth + 8 : 0); }

    // I en flikpanel: ingen egen rubrik (fliken heter likadant). På/av-knappen och presetknappen
    // flyttas till komponenten som returneras, som flikpanelen visar i flikraden.
    juce::Component* embedInTab()
    {
        embedded = true;
        if (power != nullptr)
            headerControls.addAndMakeVisible (*power);
        if (headerTabs != nullptr)
            headerControls.addAndMakeVisible (*headerTabs);
        if (headerPreset != nullptr)
            headerControls.addAndMakeVisible (*headerPreset);
        if (headerFxSend != nullptr)
            headerControls.addAndMakeVisible (*headerFxSend);
        headerControls.power = power.get();
        headerControls.tabs = headerTabs;
        headerControls.tabsWidth = headerTabsWidth;
        headerControls.preset = headerPreset;
        headerControls.fxSend = headerFxSend;
        repaint();
        return &headerControls;
    }

    void paint (juce::Graphics& g) override
    {
        if (embedded)
            return; // flikpanelen ritar bakgrunden och fliken visar namnet

        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        Theme::paintMetal (g, bounds, 8.0f, juce::Colour (0xe62c3048), juce::Colour (0xe61a1d30));

        // Färgad linje överst och titel.
        g.setColour (accent);
        g.fillRoundedRectangle (bounds.getX() + 10.0f, bounds.getY(), 36.0f, 3.0f, 1.5f);
        g.setFont (Theme::font (14.0f, true));
        g.drawText (title, getLocalBounds().withHeight (headerHeight).withTrimmedLeft (power != nullptr ? 36 : 12),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        if (power != nullptr && ! embedded)
            power->setBounds (8, 4, 24, 22);
    }

protected:
    // Presetknappen som flyttas till flikraden i en flikpanel (se embedInTab).
    void setHeaderPreset (juce::Component* preset) { headerPreset = preset; }

    // FX-send (kanal och mängd) som visas i flikraden, mellan på/av-knappen och presetknappen.
    void setHeaderFxSend (juce::Component* fxSend) { headerFxSend = fxSend; }

    // Vyflikar (t.ex. SYNTH | SAMPLE) i flikraden, till vänster om presetknappen.
    void setHeaderTabs (juce::Component* tabs, int width) { headerTabs = tabs; headerTabsWidth = width; }

private:
    struct HeaderControls : juce::Component
    {
        juce::Component* power = nullptr;
        juce::Component* preset = nullptr;
        juce::Component* fxSend = nullptr;
        juce::Component* tabs = nullptr;
        int tabsWidth = 0;

        void resized() override
        {
            auto area = getLocalBounds();
            if (power != nullptr)
                power->setBounds (area.removeFromLeft (24));
            area.removeFromLeft (8);
            if (tabs != nullptr)
            {
                tabs->setBounds (area.removeFromLeft (tabsWidth));
                area.removeFromLeft (8);
            }
            if (fxSend != nullptr)
            {
                fxSend->setBounds (area.removeFromRight (168));
                area.removeFromRight (8);
            }
            if (preset != nullptr)
                preset->setBounds (area);
        }
    };

    HeaderControls headerControls;
    juce::Component* headerPreset = nullptr;
    juce::Component* headerFxSend = nullptr;
    juce::Component* headerTabs = nullptr;
    int headerTabsWidth = 0;
    bool embedded = false;
    juce::String title;
    juce::Colour accent;
    std::unique_ptr<juce::ToggleButton> power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
};

// Lägger ut komponenter i en jämn rad.
template <typename Container>
void layoutRow (Container& items, juce::Rectangle<int> area, int gap = 4)
{
    if (items.empty())
        return;

    const auto width = (area.getWidth() - gap * ((int) items.size() - 1)) / (int) items.size();
    for (auto* item : items)
    {
        item->setBounds (area.removeFromLeft (width));
        area.removeFromLeft (gap);
    }
}
