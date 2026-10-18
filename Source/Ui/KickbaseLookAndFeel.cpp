#include "KickbaseLookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>

KickbaseLookAndFeel::KickbaseLookAndFeel()
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

void KickbaseLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    // Små rattar (t.ex. radrattarna i mönstret) ber om en smalare marginal, så att ratten fyller sin ruta.
    const auto margin = (float) (double) slider.getProperties().getWithDefault ("ringMargin", (double) Theme::knobRingMargin);
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (margin);
    const auto radius = std::min (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const auto arcRadius = radius - 3.5f;
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // 1. Nedsänkt cirkulär sockel i chassit bakom ratten
    const auto socketRadius = arcRadius + 2.5f;
    const auto socket = juce::Rectangle<float> (socketRadius * 2.0f, socketRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0e1019), centre.x, socket.getY(),
                                             juce::Colour (0xff181a28), centre.x, socket.getBottom(), false));
    g.fillEllipse (socket);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawEllipse (socket, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (socket.reduced (0.5f), 1.0f);

    // 2. Släckt spårring
    auto arc = [&] (float from, float to)
    {
        juce::Path path;
        path.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, from, to, true);
        return path;
    };

    g.setColour (Theme::track.withAlpha (0.45f));
    g.strokePath (arc (rotaryStartAngle, rotaryEndAngle), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 3. Tänd värdebåge: skarp lysande neonkärna med mjuk yttre glöd
    const auto bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const auto arcStart = bipolar ? (rotaryStartAngle + rotaryEndAngle) / 2.0f : rotaryStartAngle;
    const auto valueArc = arc (std::min (arcStart, angle), std::max (arcStart, angle));

    g.setColour (accent.withAlpha (slider.isEnabled() ? 0.28f : 0.08f));
    g.strokePath (valueArc, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (slider.isEnabled() ? accent : accent.withSaturation (0.1f).withAlpha (0.4f));
    g.strokePath (valueArc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Ljus prick vid spetsen
    const auto dot = centre.getPointOnCircumference (arcRadius, angle);
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (dot));

    // 4. Rattknopp: maskinsvarvad aluminium/gunmetal med koncentrisk ring-struktur ("ring-struktur")
    const auto bodyRadius = arcRadius - 4.5f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);

    // Mjuk djup skugga under ratten
    g.setColour (juce::Colours::black.withAlpha (0.60f));
    g.fillEllipse (body.translated (0.0f, 2.0f).expanded (1.0f));

    // Rita den proceduriella borstade ring-texturen
    const int imgSize = juce::jmax (16, (int) std::round (bodyRadius * 2.0f));
    const auto& capImage = Theme::circularBrushedTexture (imgSize);
    g.drawImage (capImage, body);

    // Fasad ytterkant (specular highlight uppe till vänster, skugga nere till höger)
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (body.reduced (bodyRadius * 0.16f), 1.0f);

    // 5. Fräst indikatorspår (milled notch)
    const auto rInner = bodyRadius * 0.32f;
    const auto rOuter = bodyRadius * 0.88f;
    const auto ptStart = centre.getPointOnCircumference (rInner, angle);
    const auto ptEnd = centre.getPointOnCircumference (rOuter, angle);
    const auto diff = ptEnd - ptStart;
    const auto len = std::sqrt (diff.x * diff.x + diff.y * diff.y);
    const auto normal = len > 0.001f ? juce::Point<float> (-diff.y / len * 0.8f, diff.x / len * 0.8f) : juce::Point<float>();

    // Skugga på ena sidan, ljus på andra
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.drawLine ({ ptStart - normal, ptEnd - normal }, 1.6f);
    g.setColour (juce::Colours::white.withAlpha (0.50f));
    g.drawLine ({ ptStart + normal, ptEnd + normal }, 1.6f);
    g.setColour (juce::Colours::white);
    g.drawLine ({ ptStart, ptEnd }, 1.6f);

    // Liten lysande spetsmarkör i modulens accentfärg
    g.setColour (accent.brighter (0.4f));
    g.fillEllipse (juce::Rectangle<float> (2.5f, 2.5f).withCentre (ptEnd));

    // 6. Infälld display endast för Cutoff (filterfrekvens, matchar Bild 3)
    const bool isCutoff = slider.getComponentID().containsIgnoreCase ("cutoff")
                       || slider.getName().equalsIgnoreCase ("CUTOFF");
    if (isCutoff)
    {
        const auto readout = body.reduced (bodyRadius * 0.36f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xee0e1019), centre.x, readout.getY(),
                                                 juce::Colour (0xee161928), centre.x, readout.getBottom(), false));
        g.fillEllipse (readout);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.drawEllipse (readout, 1.0f);
        g.setColour (accent.withAlpha (0.45f));
        g.drawEllipse (readout.reduced (0.5f), 1.0f);

        g.setColour (Theme::text);
        g.setFont (Theme::font (juce::jlimit (8.5f, 11.5f, bodyRadius * 0.44f), true));
        g.drawFittedText (slider.getTextFromValue (slider.getValue()),
                          readout.toNearestInt(), juce::Justification::centred, 1);
    }
}

void KickbaseLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    // Fader: ett nedsänkt spår med färgad fyllning underifrån och ett metallhandtag.
    const auto accent = slider.findColour (juce::Slider::trackColourId);
    const auto centreX = (float) x + (float) width * 0.5f;
    const auto groove = juce::Rectangle<float> (4.0f, (float) height).withCentre ({ centreX, (float) y + (float) height * 0.5f });
    g.setColour (Theme::knobBodyDark);
    g.fillRoundedRectangle (groove, 2.0f);
    g.setColour (accent.withAlpha (0.8f));
    g.fillRoundedRectangle (groove.withTop (sliderPos), 2.0f);

    const auto cap = juce::Rectangle<float> ((float) std::min (width, 22), 12.0f).withCentre ({ centreX, sliderPos });
    Theme::paintMetal (g, cap, 2.5f, juce::Colour (0xffd2d6e6), juce::Colour (0xff7e8499), false);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRect (cap.withHeight (1.5f).withCentre (cap.getCentre()));
}

void KickbaseLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    // På/av-knapp: lysande neonring med brytarstreck
    const auto accent = button.findColour (juce::ToggleButton::tickColourId);
    const auto bounds = button.getLocalBounds().toFloat().reduced (3.0f);
    const auto size = std::min (bounds.getWidth(), bounds.getHeight());
    const auto icon = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const auto on = button.getToggleState();
    const auto colour = on ? accent : Theme::textDim.withAlpha (highlighted ? 0.9f : 0.6f);

    if (on)
    {
        g.setColour (accent.withAlpha (0.25f));
        g.fillEllipse (icon.expanded (3.0f));
    }

    juce::Path ring;
    ring.addCentredArc (icon.getCentreX(), icon.getCentreY(), size * 0.36f, size * 0.36f, 0.0f,
                        juce::MathConstants<float>::pi * 0.2f, juce::MathConstants<float>::pi * 1.8f, true);
    g.setColour (colour);
    g.strokePath (ring, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine (icon.getCentreX(), icon.getY() + size * 0.1f, icon.getCentreX(), icon.getCentreY(), 2.0f);
}

void KickbaseLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto accent = button.findColour (juce::TextButton::buttonOnColourId);
    const auto on = button.getToggleState();
    const auto corner = bounds.getHeight() * 0.5f; // Kapsel/pill-form som i Bild 3

    if (on)
    {
        // Aktiv pill: djup mörk metallkropp med lysande accentkant och mjuk yttre glöd
        g.setColour (accent.withAlpha (0.22f));
        g.fillRoundedRectangle (bounds.expanded (1.5f), corner + 1.5f);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b3048), bounds.getX(), bounds.getY(),
                                                 juce::Colour (0xff1b1e30), bounds.getX(), bounds.getBottom(), false));
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (accent.withAlpha (0.85f));
        g.drawRoundedRectangle (bounds, corner, 1.2f);

        // Lysande LED-indikator till vänster i pillen
        const auto ledCentre = juce::Point<float> (bounds.getX() + 13.0f, bounds.getCentreY());
        g.setColour (accent.withAlpha (0.45f));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (ledCentre));
        g.setColour (accent.brighter (0.4f));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (ledCentre));
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre (ledCentre));
    }
    else
    {
        const auto lift = highlighted || down ? 0.08f : 0.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff25293d).brighter (lift), bounds.getX(), bounds.getY(),
                                                 juce::Colour (0xff171926).brighter (lift), bounds.getX(), bounds.getBottom(), false));
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (juce::Colour (0xff343952));
        g.drawRoundedRectangle (bounds, corner, 1.0f);

        // Ljus kant överst
        g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.16f : 0.08f));
        g.drawLine (bounds.getX() + corner * 0.6f, bounds.getY() + 0.5f, bounds.getRight() - corner * 0.6f, bounds.getY() + 0.5f, 1.0f);
    }
}

void KickbaseLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const auto on = button.getToggleState();
    auto bounds = button.getLocalBounds().reduced (4, 0);

    // En pill som kan lysa (på/av) har alltid plats för LED:en till vänster, hur smal knappen än är: texten får det som blir kvar.
    if (on || button.getClickingTogglesState())
        bounds.removeFromLeft (15);

    // Texten krymper först (ner till 7.5 pt) om knappen är smal, och förkortas med ... bara om den ändå inte ryms.
    auto font = getTextButtonFont (button, button.getHeight()).boldened();
    const auto text = button.getButtonText();
    auto textWidth = [&] (float size)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font.withHeight (size), text, 0.0f, 0.0f);
        return glyphs.getBoundingBox (0, -1, true).getWidth();
    };
    auto size = font.getHeight();
    while (size > 7.5f && textWidth (size) > (float) bounds.getWidth())
        size -= 0.5f;

    g.setFont (font.withHeight (size));
    g.setColour (on ? Theme::text : Theme::textDim);
    g.drawText (text, bounds, juce::Justification::centred, true);
}

void KickbaseLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);

    // Nedsänkt i metallen: mörk infälld sockel med 1px topp-highlight
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff10121e), 0.0f, bounds.getY(),
                                             juce::Colour (0xff181b2b), 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 5.0f);

    g.setColour (juce::Colour (0xff292e44));
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (box.hasKeyboardFocus (true) ? 0.30f : 0.08f));
    g.drawLine (bounds.getX() + 4.0f, bounds.getY() + 0.5f, bounds.getRight() - 4.0f, bounds.getY() + 0.5f, 1.0f);

    // Liten metallisk pil till höger
    const auto arrow = juce::Rectangle<float> ((float) width - 16.0f, (float) height / 2.0f - 2.0f, 8.0f, 4.0f);
    juce::Path path;
    path.addTriangle (arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
    g.setColour (Theme::textDim);
    g.fillPath (path);
}

void KickbaseLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}
