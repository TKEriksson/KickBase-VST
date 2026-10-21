#include "FmPanel.h"

#include "FmPresets.h"
#include "Help.h"
#include "../Core/Params.h"

FmPanel::FmPanel (PluginBase& p, int layerIn, juce::Colour accentIn)
    : processor (p), layer (layerIn), accent (accentIn)
{
    using F = InstrumentField;
    auto id = [this] (F f) { return ParamID::bass (layer, f); };
    auto knob = [&] (std::unique_ptr<KnobComponent>& slot, F field, const char* name)
    {
        slot = std::make_unique<KnobComponent> (processor.apvts, id (field), name, accent);
        addAndMakeVisible (*slot);
        // Bara ändringar som inte kommer från ett presetval räknas: då är det inte längre presetens ljud.
        knobWatchers.push_back (std::make_unique<juce::ParameterAttachment> (*processor.apvts.getParameter (id (field)), [this] (float)
        {
            if (applyingPreset)
                return;
            if (auto* choice = processor.apvts.getParameter (ParamID::bass (layer, InstrumentField::fmPreset)))
                if (choice->getValue() > 0.0f)
                {
                    choice->beginChangeGesture();
                    choice->setValueNotifyingHost (0.0f);
                    choice->endChangeGesture();
                }
        }));
    };
    preset = std::make_unique<ChoiceComponent> (processor.apvts, id (F::fmPreset), "PRESET");
    addAndMakeVisible (*preset);
    knob (level, F::fmLevel, "LEVEL");
    knob (ratio, F::fmRatio, "RATIO");
    knob (index, F::fmIndex, "INDEX");
    knob (envAmount, F::fmEnvAmount, "ENV AMT");
    knob (envTime, F::fmEnvTime, "ENV TIME");
    knob (semis, F::fmSemis, "SEMIS");

    filterToggle.setButtonText ("THROUGH FILTER");
    filterToggle.setTooltip (Help::forParameter (id (F::fmFilter)));
    filterToggle.setColour (juce::ToggleButton::tickColourId, accent);
    filterToggle.setColour (juce::ToggleButton::textColourId, Theme::textDim);
    addAndMakeVisible (filterToggle);
    filterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, id (F::fmFilter), filterToggle);

    presetAttachment = std::make_unique<juce::ParameterAttachment> (*processor.apvts.getParameter (id (F::fmPreset)),
                                                                     [this] (float value) { presetChanged ((int) std::lround (value)); });
}

FmPanel::~FmPanel() = default;

void FmPanel::presetChanged (int index)
{
    if (index < 1 || applyingPreset)
        return;
    applyingPreset = true;
    FmPresets::apply (processor.apvts, layer, index);
    applyingPreset = false;
}

void FmPanel::paint (juce::Graphics& g)
{
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (10.0f));
    g.drawFittedText ("FM: a sine modulator bends the phase of a sine carrier. RATIO sets the timbre (whole numbers sound harmonic, others metallic), "
                      "INDEX how bright, ENV AMT / ENV TIME how fast the brightness falls after each note.",
                      getLocalBounds().reduced (10, 6).removeFromBottom (36), juce::Justification::topLeft, 2);
}

void FmPanel::resized()
{
    auto area = getLocalBounds().reduced (6, 4);
    area.removeFromBottom (40);
    auto top = area.removeFromTop (juce::jmin (area.getHeight(), 110));
    preset->setBounds (top.removeFromLeft (150).withTrimmedBottom (juce::jmax (0, top.getHeight() - 40)));
    top.removeFromLeft (8);
    const auto width = juce::jmin (90, top.getWidth() / 6);
    for (auto* k : { level.get(), ratio.get(), index.get(), envAmount.get(), envTime.get(), semis.get() })
        k->setBounds (top.removeFromLeft (width));
    filterToggle.setBounds (preset->getBounds().translated (0, 46).withHeight (22));
}
