#pragma once

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
class KnobComponent : public juce::Component, public juce::DragAndDropTarget, private juce::Timer
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

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
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

    // Ytan under rubriken där modulens innehåll placeras.
    juce::Rectangle<int> getContentBounds() const { return getLocalBounds().reduced (10).withTrimmedTop (headerHeight - 4); }

    juce::Colour getAccent() const { return accent; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Theme::panel);
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (Theme::panelBorder);
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

        // Färgad linje överst och titel.
        g.setColour (accent);
        g.fillRoundedRectangle (bounds.getX() + 10.0f, bounds.getY(), 36.0f, 3.0f, 1.5f);
        g.setFont (Theme::font (14.0f, true));
        g.drawText (title, getLocalBounds().withHeight (headerHeight).withTrimmedLeft (power != nullptr ? 36 : 12),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        if (power != nullptr)
            power->setBounds (8, 4, 24, 22);
    }

private:
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
