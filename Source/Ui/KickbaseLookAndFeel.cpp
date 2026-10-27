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

bool KickbaseLookAndFeel::isPitchKnob (const juce::Slider& slider)
{
    const auto id = slider.getComponentID() + " " + slider.getName();
    return id.containsIgnoreCase ("semi") || id.containsIgnoreCase ("pitch") || id.containsIgnoreCase ("tune");
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

    // 1. Platt mörk sockel bakom ratten (E1: ingen metallbevel)
    const auto socketRadius = arcRadius + 2.5f;
    const auto socket = juce::Rectangle<float> (socketRadius * 2.0f, socketRadius * 2.0f).withCentre (centre);
    g.setColour (Theme::chartBackground);
    g.fillEllipse (socket);
    g.setColour (Theme::panelBorderDim);
    g.drawEllipse (socket, 1.0f);

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

    // 4. Platt mörk knopp (skissen): yttre skiva 145 grader #283042 -> #181d28, inre skiva radiell #1e2433 -> #11141c med tunn kant
    const auto bodyRadius = arcRadius - 4.5f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    const auto dir = juce::Point<float> (0.5736f, 0.8192f) * bodyRadius; // 145 grader
    // Skissens .knob-outer: skugga 2px 2px 5px svart 0,6 nere till höger och högdager -1px -1px 3px vit 0,05 uppe till vänster
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillEllipse (body.translated (-1.0f, -1.0f).expanded (1.5f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (body.translated (2.0f, 2.0f).expanded (1.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff283042), centre - dir, juce::Colour (0xff181d28), centre + dir, false));
    g.fillEllipse (body);
    const auto inner = body.reduced (bodyRadius * 0.16f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1e2433), inner.getCentreX(), inner.getY() + inner.getHeight() * 0.3f,
                                             juce::Colour (0xff11141c), inner.getRight(), inner.getBottom(), true));
    g.fillEllipse (inner);
    g.setColour (juce::Colour (0xff334155).withAlpha (0.6f));
    g.drawEllipse (inner, 1.0f);

    // 5. Tunn lysande pekare: cyan (fuchsia för tonhöjd/semitoner)
    const auto pitchLike = isPitchKnob (slider);
    const auto pointer = pitchLike ? Theme::fuchsia : Theme::cyan;
    const auto ptStart = centre.getPointOnCircumference (bodyRadius * 0.30f, angle);
    const auto ptEnd = centre.getPointOnCircumference (bodyRadius * 0.86f, angle);
    g.setColour (pointer.withAlpha (0.25f));
    g.drawLine ({ ptStart, ptEnd }, 4.0f);
    g.setColour (pointer);
    g.drawLine ({ ptStart, ptEnd }, 1.5f);

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
        // Texten krymps så att den ryms i displayen (de mindre rattarna i FILTER-fliken blev annars "900..").
        const auto text = slider.getTextFromValue (slider.getValue());
        auto font = Theme::font (juce::jlimit (8.5f, 11.5f, bodyRadius * 0.44f), true);
        const auto available = readout.getWidth() * 0.92f;
        const auto textWidth = (float) juce::GlyphArrangement::getStringWidthInt (font, text);
        if (textWidth > available)
            font = font.withHeight (juce::jmax (5.5f, font.getHeight() * available / textWidth));
        g.setFont (font);
        g.drawText (text, readout, juce::Justification::centred, false);
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
    g.setColour (Theme::chartBackground);
    g.fillRoundedRectangle (groove, 2.0f);
    g.setColour (Theme::panelBorder);
    g.drawRoundedRectangle (groove, 2.0f, 1.0f);
    const auto fill = groove.withTop (sliderPos);
    g.setColour (accent.withAlpha (0.25f));
    g.fillRoundedRectangle (fill.expanded (2.0f, 0.0f), 3.0f);
    g.setColour (accent.withAlpha (0.9f));
    g.fillRoundedRectangle (fill, 2.0f);

    // Platt handtag med accentkant, som skissens rattar.
    const auto cap = juce::Rectangle<float> ((float) std::min (width, 22), 12.0f).withCentre ({ centreX, sliderPos });
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff283042), cap.getX(), cap.getY(),
                                             juce::Colour (0xff181d28), cap.getRight(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (accent.withAlpha (0.8f));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.fillRect (cap.withHeight (1.5f).withCentre (cap.getCentre()).reduced (4.0f, 0.0f));
}

void KickbaseLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    // På/av-knapp: lysande neonring med brytarstreck
    const auto accent = button.findColour (juce::ToggleButton::tickColourId);
    const auto bounds = button.getLocalBounds().toFloat().reduced (3.0f);
    const auto size = std::min (bounds.getWidth(), bounds.getHeight());
    // Knappar med egenskapen "labelled" visar sin text till vänster om ikonen, som sitter längst till höger.
    const bool labelled = (bool) button.getProperties().getWithDefault ("labelled", false);
    const auto icon = labelled ? juce::Rectangle<float> (size, size).withRightX (bounds.getRight()).withCentre ({ bounds.getRight() - size * 0.5f, bounds.getCentreY() })
                               : juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const auto on = button.getToggleState();
    const auto colour = on ? accent : Theme::textDim.withAlpha (highlighted ? 0.9f : 0.6f);

    if (labelled)
    {
        g.setColour (on ? accent : Theme::textDim.withAlpha (highlighted ? 0.9f : 0.75f));
        g.setFont (Theme::font (10.0f, true));
        g.drawText (button.getButtonText(), bounds.withRight (icon.getX() - 5.0f), juce::Justification::centredRight, false);
    }

    if (on)
    {
        g.setColour (accent.withAlpha (0.12f));
        g.fillEllipse (icon.expanded (3.0f));
        g.setColour (accent.withAlpha (0.35f));
        g.drawEllipse (icon.expanded (3.0f), 1.0f);
    }
    else
    {
        g.setColour (Theme::chartBackground);
        g.fillEllipse (icon.reduced (1.0f));
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

    // E1 form 3, kort/chip: kvadratiska kort (radie 3) för flik-i-flik (FX1-FX11, EQ/COMP/FX).
    if ((bool) button.getProperties().getWithDefault ("chip", false))
    {
        g.setColour (on ? accent.withAlpha (0.16f) : Theme::panelAlt.brighter (highlighted || down ? 0.15f : 0.0f));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (on ? Theme::cyan.withAlpha (0.8f) : Theme::panelBorder);
        g.drawRoundedRectangle (bounds, 3.0f, on ? 1.4f : 1.0f);
        if (on)
        {
            g.setColour (Theme::cyan);
            g.fillRect (juce::Rectangle<float> (bounds.getX() + 3.0f, bounds.getY() + 1.0f, bounds.getWidth() - 6.0f, 2.0f));
        }
        return;
    }

    // E1 form 2, underflik: platt rektangel (radie 8) utan egen kant eller fyllning (bara hover), på en mörk infälld rad.
    // Vald: fyllning #161f2e, cyan kant och prick.
    if ((bool) button.getProperties().getWithDefault ("subtab", false))
    {
        const auto segment = bounds.reduced (0.0f, 1.0f);
        if (on)
        {
            g.setColour (juce::Colour (0xff161f2e));
            g.fillRoundedRectangle (segment, 8.0f);
            g.setColour (Theme::cyan.withAlpha (0.5f));
            g.drawRoundedRectangle (segment, 8.0f, 1.0f);
            const auto dot = juce::Point<float> (segment.getX() + 11.0f, segment.getCentreY());
            g.setColour (Theme::cyan.withAlpha (0.35f));
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (dot));
            g.setColour (Theme::cyanLight);
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (dot));
        }
        else if (highlighted || down)
        {
            g.setColour (juce::Colour (0xff1e293b).withAlpha (0.5f));
            g.fillRoundedRectangle (segment, 8.0f);
        }
        return;
    }

    // E1 form 1, huvudflik: helt rund pill. Vald: gradient cyan -> blå, cyan kant, glöd och pingprick. Overksam: svag fyllning och kant.
    if (on)
    {
        g.setColour (Theme::cyan.withAlpha (0.20f));
        g.fillRoundedRectangle (bounds.expanded (2.0f), corner + 2.0f);
        g.setColour (Theme::cyan.withAlpha (0.12f));
        g.fillRoundedRectangle (bounds.expanded (4.0f), corner + 4.0f);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff06b6d4).withAlpha (0.30f), bounds.getX(), bounds.getY(),
                                                 juce::Colour (0xff2563eb).withAlpha (0.30f), bounds.getRight(), bounds.getBottom(), false));
        g.fillRoundedRectangle (bounds, corner);
        g.setColour (juce::Colour (0xff0b0d14).withAlpha (0.55f));
        g.fillRoundedRectangle (bounds.reduced (1.0f), corner);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff06b6d4).withAlpha (0.30f), bounds.getX(), bounds.getY(),
                                                 juce::Colour (0xff2563eb).withAlpha (0.30f), bounds.getRight(), bounds.getBottom(), false));
        g.fillRoundedRectangle (bounds, corner);
        g.setColour (juce::Colour (0xff22d3ee).withAlpha (0.8f));
        g.drawRoundedRectangle (bounds, corner, 1.2f);

        const auto ledCentre = juce::Point<float> (bounds.getX() + 13.0f, bounds.getCentreY());
        g.setColour (Theme::cyan.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (ledCentre));
        g.setColour (Theme::cyanLight);
        g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (ledCentre));
    }
    else
    {
        g.setColour (juce::Colour (0xff1e293b).withAlpha (highlighted || down ? 0.6f : 0.4f));
        g.fillRoundedRectangle (bounds, corner);
        g.setColour (juce::Colour (0xff334155).withAlpha (0.5f));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }
}

void KickbaseLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const auto on = button.getToggleState();
    auto bounds = button.getLocalBounds().reduced (4, 0);

    // En pill som kan lysa (på/av) har alltid plats för LED:en till vänster, hur smal knappen än är: texten får det som blir kvar.
    const bool sub = (bool) button.getProperties().getWithDefault ("subtab", false);
    const bool chip = (bool) button.getProperties().getWithDefault ("chip", false);
    if ((on || button.getClickingTogglesState()) && ! sub && ! chip)
        bounds.removeFromLeft (15);
    else if (sub && on)
        bounds.removeFromLeft (10);

    // Texten krymper först (ner till 7.5 pt) om knappen är smal, och förkortas med ... bara om den ändå inte ryms.
    auto font = getTextButtonFont (button, button.getHeight()).boldened();
    const bool noShrink = (bool) button.getProperties().getWithDefault ("noshrink", false);
    if (sub || noShrink)
        font = font.withHeight (11.0f); // navigeringsknappar: samma storlek överallt (TabbedPanel mäter bredden med samma typsnitt)
    else if (chip)
        font = Theme::monoFont (11.0f, true);
    font.setExtraKerningFactor (sub ? 0.04f : 0.08f);
    const auto text = button.getButtonText().toUpperCase();
    auto textWidth = [&] (float size)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font.withHeight (size), text, 0.0f, 0.0f);
        return glyphs.getBoundingBox (0, -1, true).getWidth();
    };
    auto size = font.getHeight();
    while (! noShrink && size > 7.5f && textWidth (size) > (float) bounds.getWidth())
        size -= 0.5f;

    g.setFont (font.withHeight (size));
    g.setColour (on ? (sub ? Theme::cyanLight : Theme::text) : Theme::textDim);
    g.drawText (text, bounds, juce::Justification::centred, true);
}

void KickbaseLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);

    // E1: platt mörk ruta med radie 8, tunn kant och cyan glöd vid fokus.
    g.setColour (Theme::chartBackground);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (Theme::panelBorder);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
    if (box.hasKeyboardFocus (true))
    {
        g.setColour (Theme::cyan.withAlpha (0.18f));
        g.drawRoundedRectangle (bounds, 8.0f, 3.0f);
        g.setColour (Theme::cyan.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
    }

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
