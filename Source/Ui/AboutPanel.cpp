#include "AboutPanel.h"
#include <BinaryData.h>

namespace
{
    void styleLabel (juce::Label& label, float size, juce::Colour colour, bool bold = false)
    {
        label.setFont (Theme::font (size, bold));
        label.setColour (juce::Label::textColourId, colour);
        label.setMinimumHorizontalScale (0.7f);
    }

    juce::String versionText()
    {
        return KICKBASE_VERSION; // sätts av CMake för den här filen (projektets VERSION)
    }
}

AboutPanel::AboutPanel (PluginBase& processor, std::function<void()> onBack)
{
    backButton.setTooltip ("Back to the other settings");
    backButton.onClick = std::move (onBack);
    addAndMakeVisible (backButton);

    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::about, "the ABOUT page");
    addAndMakeVisible (helpButton);

    styleLabel (title, 13.0f, Theme::text, true);
    title.setText (juce::String (processor.isEffectVersion() ? "KICKBASE FX" : "KICKBASE") + "   version " + versionText(), juce::dontSendNotification);
    addAndMakeVisible (title);

    styleLabel (licenceLine, 10.5f, Theme::text);
    licenceLine.setText ("Free software under the GNU Affero General Public License 3.0 (AGPL-3.0). Source code: see the Kickbase repository "
                         "(made public when Kickbase is released); the AGPL gives you the right to it.", juce::dontSendNotification);
    addAndMakeVisible (licenceLine);

    thirdPartyButton.setClickingTogglesState (true);
    thirdPartyButton.setRadioGroupId (4711);
    thirdPartyButton.setColour (juce::TextButton::buttonOnColourId, Theme::mix.withAlpha (0.6f));
    thirdPartyButton.setTooltip ("THIRD-PARTY - The software and content Kickbase is built with or can download, and their licences (JUCE, the VST3 SDK, AKWF ...).");
    thirdPartyButton.onClick = [this] { showText (false); };
    addAndMakeVisible (thirdPartyButton);

    licenseButton.setClickingTogglesState (true);
    licenseButton.setRadioGroupId (4711);
    licenseButton.setColour (juce::TextButton::buttonOnColourId, Theme::mix.withAlpha (0.6f));
    licenseButton.setTooltip ("LICENSE - The full text of the GNU Affero General Public License 3.0 that Kickbase is released under.");
    licenseButton.onClick = [this] { showText (true); };
    addAndMakeVisible (licenseButton);

    // THIRD-PARTY.md visas som vanlig text: markdownens ** och ` tas bort (LICENSE visas som den är).
    thirdPartyText = juce::String::fromUTF8 (BinaryData::THIRDPARTY_md, BinaryData::THIRDPARTY_mdSize).replace ("**", "").replace ("`", "");
    licenseText = juce::String::fromUTF8 (BinaryData::LICENSE, BinaryData::LICENSESize);

    text.setMultiLine (true, true);
    text.setReadOnly (true);
    text.setCaretVisible (false);
    text.setScrollbarsShown (true);
    text.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 10.5f, juce::Font::plain)));
    text.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0e101a));
    text.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff24283c));
    text.setColour (juce::TextEditor::textColourId, Theme::text.withAlpha (0.9f));
    addAndMakeVisible (text);

    thirdPartyButton.setToggleState (true, juce::dontSendNotification);
    showText (false);
}

void AboutPanel::showText (bool license)
{
    text.setText (license ? licenseText : thirdPartyText, false);
    text.moveCaretToTop (false);
}

void AboutPanel::paint (juce::Graphics& g)
{
    // Samma ram som OptimizationsPanel, över hela sidan.
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff121420), bounds.getX(), bounds.getY(), juce::Colour (0xff181b28), bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colour (0xff24283c));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (Theme::bass.withAlpha (0.92f));
    g.setFont (Theme::font (9.5f, true).withExtraKerningFactor (0.08f));
    g.drawText ("ABOUT  -  LICENCE AND THIRD-PARTY SOFTWARE", getLocalBounds().withHeight (16).withTrimmedLeft (8), juce::Justification::centredLeft);
}

void AboutPanel::resized()
{
    auto area = getLocalBounds().reduced (10, 0).withTrimmedTop (20).withTrimmedBottom (6);

    // Allt i en rad överst (Kickbase FX har ett lågt fönster, och textrutan ska få så mycket plats som möjligt).
    auto top = area.removeFromTop (24);
    backButton.setBounds (top.removeFromRight (70));
    top.removeFromRight (6);
    helpButton.setBounds (top.removeFromRight (44));
    top.removeFromRight (10);
    licenseButton.setBounds (top.removeFromRight (90));
    top.removeFromRight (6);
    thirdPartyButton.setBounds (top.removeFromRight (110));
    top.removeFromRight (10);
    title.setBounds (top);
    area.removeFromTop (2);
    licenceLine.setBounds (area.removeFromTop (18));
    area.removeFromTop (4);
    text.setBounds (area);
}
