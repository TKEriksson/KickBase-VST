#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

// En ratt med namn under, kopplad till en parameter. Dubbelklick återställer standardvärdet.
class KnobComponent : public juce::Component
{
public:
    KnobComponent (juce::AudioProcessorValueTreeState& apvts, const char* paramID, const juce::String& name, juce::Colour accent)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (Theme::font (11.0f, true));
        label.setColour (juce::Label::textColourId, Theme::textDim);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);

        if (auto* parameter = apvts.getParameter (paramID))
        {
            slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
            slider.setTooltip (parameter->getName (64));
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromBottom (14));
        slider.setBounds (area);
    }

    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// En liten rullgardinslista med etikett ovanför, kopplad till en valparameter.
class ChoiceComponent : public juce::Component
{
public:
    ChoiceComponent (juce::AudioProcessorValueTreeState& apvts, const char* paramID, const juce::String& name)
    {
        if (auto* parameter = apvts.getParameter (paramID))
            box.addItemList (parameter->getAllValueStrings(), 1);
        addAndMakeVisible (box);

        label.setText (name, juce::dontSendNotification);
        label.setFont (Theme::font (11.0f, true));
        label.setColour (juce::Label::textColourId, Theme::textDim);
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramID, box);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        label.setBounds (area.removeFromTop (14));
        box.setBounds (area.removeFromTop (24));
    }

    juce::ComboBox box;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// En panel med rubrik i modulens färg och (valfritt) en på/av-knapp kopplad till en parameter.
class ModulePanel : public juce::Component
{
public:
    static constexpr int headerHeight = 28;

    ModulePanel (const juce::String& titleIn, juce::Colour accentIn,
                 juce::AudioProcessorValueTreeState* apvts = nullptr, const char* powerParamID = nullptr)
        : title (titleIn), accent (accentIn)
    {
        if (apvts != nullptr && powerParamID != nullptr)
        {
            power = std::make_unique<juce::ToggleButton>();
            power->setColour (juce::ToggleButton::tickColourId, accent);
            power->setTooltip ("On/Off");
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
