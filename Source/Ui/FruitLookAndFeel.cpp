#include "FruitLookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>

FruitLookAndFeel::FruitLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, Theme::knobBodyDark);
    setColour (juce::PopupMenu::textColourId, Theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::track);
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::text);
    setColour (juce::ComboBox::textColourId, Theme::text);
    setColour (juce::ComboBox::arrowColourId, Theme::textDim);
    setColour (juce::Label::textColourId, Theme::text);
    setColour (juce::TextButton::textColourOffId, Theme::textDim);
    setColour (juce::TextButton::textColourOnId, Theme::text);
    setColour (juce::TooltipWindow::backgroundColourId, Theme::knobBodyDark);
    setColour (juce::TooltipWindow::textColourId, Theme::text);
    setColour (juce::TooltipWindow::outlineColourId, Theme::panelBorder);
    setColour (juce::AlertWindow::backgroundColourId, Theme::knobBodyDark);
    setColour (juce::AlertWindow::textColourId, Theme::text);
    setColour (juce::AlertWindow::outlineColourId, Theme::panelBorder);
    setColour (juce::TextEditor::backgroundColourId, Theme::knobBody);
    setColour (juce::TextEditor::textColourId, Theme::text);
    setColour (juce::TextEditor::outlineColourId, Theme::panelBorder);
    setColour (juce::TextEditor::focusedOutlineColourId, Theme::pattern);
    setColour (juce::TextEditor::highlightColourId, Theme::pattern.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, Theme::text);
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffc9cbe6));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, Theme::knobBodyDark);
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, Theme::knobBodyDark);
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Theme::kick.withAlpha (0.8f));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Theme::kick.withAlpha (0.3f));
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, Theme::knobBodyDark);
    setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, Theme::textDim);
}

void FruitLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = std::min (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const auto arcRadius = radius - 3.0f;
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Kropp: mörk skiva med svag gradient, som en upplyst ratt.
    const auto bodyRadius = arcRadius - 5.0f;
    g.setGradientFill (juce::ColourGradient (Theme::knobBody, centre.x, centre.y - bodyRadius,
                                             Theme::knobBodyDark, centre.x, centre.y + bodyRadius, false));
    g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre));
    g.setColour (Theme::panelBorder);
    g.drawEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre), 1.0f);

    // Spår och värdebåge, med en mjuk glöd under bågen.
    auto arc = [&] (float from, float to)
    {
        juce::Path path;
        path.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, from, to, true);
        return path;
    };

    g.setColour (Theme::track);
    g.strokePath (arc (rotaryStartAngle, rotaryEndAngle), juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolära parametrar (t.ex. Tune) ritas från mitten.
    const auto bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const auto arcStart = bipolar ? (rotaryStartAngle + rotaryEndAngle) / 2.0f : rotaryStartAngle;
    const auto valueArc = arc (std::min (arcStart, angle), std::max (arcStart, angle));

    g.setColour (accent.withAlpha (slider.isEnabled() ? 0.18f : 0.05f));
    g.strokePath (valueArc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (slider.isEnabled() ? accent : accent.withSaturation (0.1f).withAlpha (0.4f));
    g.strokePath (valueArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Prick vid värdet.
    const auto dot = centre.getPointOnCircumference (arcRadius, angle);
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (dot));

    // Värdet mitt i ratten.
    g.setColour (Theme::text);
    g.setFont (Theme::font (juce::jlimit (9.0f, 13.0f, bodyRadius * 0.42f)));
    g.drawFittedText (slider.getTextFromValue (slider.getValue()),
                      juce::Rectangle<float> (bodyRadius * 1.7f, bodyRadius).withCentre (centre).toNearestInt(),
                      juce::Justification::centred, 1, 0.8f);
}

void FruitLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    // På/av-knapp: en ring med ett streck, lyser i modulens färg när den är på.
    const auto accent = button.findColour (juce::ToggleButton::tickColourId);
    const auto bounds = button.getLocalBounds().toFloat().reduced (3.0f);
    const auto size = std::min (bounds.getWidth(), bounds.getHeight());
    const auto icon = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const auto on = button.getToggleState();
    const auto colour = on ? accent : Theme::textDim.withAlpha (highlighted ? 0.9f : 0.6f);

    if (on)
    {
        g.setColour (accent.withAlpha (0.2f));
        g.fillEllipse (icon.expanded (2.0f));
    }

    juce::Path ring;
    ring.addCentredArc (icon.getCentreX(), icon.getCentreY(), size * 0.36f, size * 0.36f, 0.0f,
                        juce::MathConstants<float>::pi * 0.2f, juce::MathConstants<float>::pi * 1.8f, true);
    g.setColour (colour);
    g.strokePath (ring, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine (icon.getCentreX(), icon.getY() + size * 0.1f, icon.getCentreX(), icon.getCentreY(), 1.8f);
}

void FruitLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto accent = button.findColour (juce::TextButton::buttonOnColourId);
    const auto on = button.getToggleState();

    g.setColour (on ? accent.withAlpha (0.25f) : Theme::knobBodyDark.withAlpha (highlighted || down ? 1.0f : 0.7f));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (on ? accent : Theme::panelBorder);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
}

void FruitLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId) : Theme::text);
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

void FruitLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
    g.setColour (Theme::knobBodyDark);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? Theme::textDim : Theme::panelBorder);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    // Liten pil till höger.
    const auto arrow = juce::Rectangle<float> ((float) width - 16.0f, (float) height / 2.0f - 2.0f, 8.0f, 4.0f);
    juce::Path path;
    path.addTriangle (arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
    g.setColour (Theme::textDim);
    g.fillPath (path);
}

void FruitLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}
