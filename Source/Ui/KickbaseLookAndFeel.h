#pragma once

#include "Theme.h"

// Ritar rattar, knappar och menyer i Kickbases stil.
class KickbaseLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KickbaseLookAndFeel();

    // Tonhöjds-/semitonsrattar får fuchsia pekare och avläsning, övriga cyan.
    static bool isPitchKnob (const juce::Slider&);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return Theme::font (13.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    juce::Font getPopupMenuFont() override { return Theme::font (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return Theme::font (12.0f, true); }
};
