#pragma once

#include "../Core/PluginBase.h"
#include "HelpDialog.h"
#include "Theme.h"

// SETTINGS > ABOUT: version, licensen (AGPL-3.0), var källkoden finns, och texterna LICENSE och THIRD-PARTY ur programmet självt
// (BinaryData, samma filer som i repot). Visas över hela SETTINGS-sidan, som OptimizationsPanel; BACK går tillbaka.
class AboutPanel : public juce::Component
{
public:
    AboutPanel (PluginBase& processor, std::function<void()> onBack);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showText (bool license); // THIRD-PARTY (false) eller LICENSE (true)

    juce::TextButton backButton { "BACK" }, thirdPartyButton { "THIRD-PARTY" }, licenseButton { "LICENSE" }, helpButton;
    juce::Label title, licenceLine;
    juce::TextEditor text;
    juce::String thirdPartyText, licenseText;
};
