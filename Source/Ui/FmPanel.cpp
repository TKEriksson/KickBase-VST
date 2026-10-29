#include "FmPanel.h"

#include "FmPresets.h"
#include "Help.h"
#include "HelpDialog.h"
#include "../Core/Params.h"
#include "../Core/UserChangeWatcher.h"

// Ritar FM-ljudets form ur ratterna (samma formler som InstrumentSynth.cpp:610-618, utan ljudmotorn). Läser parametrarna 15 gånger i sekunden.
class FmPanel::Diagram : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    enum class Kind { wave, indexOverTime };

    Diagram (juce::AudioProcessorValueTreeState& apvtsIn, int layerIn, Kind kindIn, juce::Colour accentIn)
        : apvts (apvtsIn), layer (layerIn), kind (kindIn), accent (accentIn)
    {
        using F = InstrumentField;
        index = apvts.getRawParameterValue (ParamID::bass (layer, F::fmIndex));
        envAmount = apvts.getRawParameterValue (ParamID::bass (layer, F::fmEnvAmount));
        envMs = apvts.getRawParameterValue (ParamID::bass (layer, F::fmEnvTime));
        maxTime = apvts.getRawParameterValue (ParamID::bass (layer, F::maxTime));
        ratio = apvts.getRawParameterValue (ParamID::bass (layer, F::fmRatio));
        setTooltip (kind == Kind::wave ? "FM output at the start of a note: two periods, with RATIO and INDEX applied."
                                       : "INDEX over time: it starts at INDEX and falls toward INDEX x (1 - ENV AMT) over ENV TIME (to -60 dB). The time axis is fixed (follows MAX), so the curve changes when ENV TIME is turned.");
        startTimerHz (15);
    }

    void paint (juce::Graphics& g) override
    {
        auto box = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0b0d18));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (Theme::text.withAlpha (0.2f));
        g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.0f);

        g.setFont (Theme::font (10.0f));
        g.setColour (Theme::textDim);
        g.drawText (kind == Kind::wave ? "OUTPUT (2 periods)" : "INDEX OVER TIME", getLocalBounds().reduced (6, 3).removeFromTop (12), juce::Justification::topLeft);

        auto plot = getLocalBounds().toFloat().reduced (6.0f, 6.0f).withTrimmedTop (14.0f).withTrimmedBottom (kind == Kind::indexOverTime ? 12.0f : 0.0f);
        if (plot.getWidth() < 10.0f || plot.getHeight() < 10.0f)
            return;
        g.setColour (Theme::text.withAlpha (0.12f));
        g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());

        const auto idx = juce::jlimit (0.0f, 1.0f, index->load());
        const auto amount = juce::jlimit (0.0f, 1.0f, envAmount->load());
        juce::Path path;
        const auto steps = juce::jmax (2, (int) plot.getWidth());
        if (kind == Kind::wave)
        {
            const auto r = (double) juce::jlimit (0.25f, 8.0f, ratio->load());
            const auto radians = 8.0 * idx * idx;
            for (int i = 0; i <= steps; ++i)
            {
                const auto phase = 2.0 * (double) i / steps; // 0..2 perioder av bäraren
                const auto v = (float) std::sin (juce::MathConstants<double>::twoPi * (phase + 0.0) + radians * std::sin (juce::MathConstants<double>::twoPi * phase * r));
                const auto x = plot.getX() + plot.getWidth() * (float) i / (float) steps;
                const auto y = plot.getCentreY() - v * plot.getHeight() * 0.5f;
                i == 0 ? path.startNewSubPath (x, y) : path.lineTo (x, y);
            }
        }
        else
        {
            const auto ms = juce::jmax (1.0f, envMs->load());
            const auto axisMs = ParamID::timeCapMs (ParamID::timeCapSpec (5), juce::jlimit (0, ParamID::numMaxTimeSteps - 1, (int) (maxTime->load() + 0.5f))); // fast axel: 400 ms vid MAX 1 s
            for (int i = 0; i <= steps; ++i)
            {
                const auto t = axisMs * (float) i / (float) steps / ms; // i enheter av ENV TIME
                const auto level = std::pow (0.001f, t);         // -60 dB vid t = 1, som motorns koefficient (InstrumentSynth.cpp:393)
                const auto v = idx * (1.0f - amount + amount * level);
                const auto x = plot.getX() + plot.getWidth() * (float) i / (float) steps;
                const auto y = plot.getBottom() - v * plot.getHeight();
                i == 0 ? path.startNewSubPath (x, y) : path.lineTo (x, y);
            }
            g.setColour (Theme::textDim);
            g.drawText ("0", getLocalBounds().reduced (6, 2).removeFromBottom (11), juce::Justification::bottomLeft);
            g.drawText (juce::String (juce::roundToInt (axisMs)) + " ms", getLocalBounds().reduced (6, 2).removeFromBottom (11), juce::Justification::bottomRight);
        }
        g.setColour (accent);
        g.strokePath (path, juce::PathStrokeType (1.6f));
    }

private:
    void timerCallback() override
    {
        const std::array<float, 5> now { index->load(), envAmount->load(), envMs->load(), ratio->load(), maxTime->load() };
        if (now != last)
        {
            last = now;
            repaint();
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    const int layer;
    const Kind kind;
    juce::Colour accent;
    std::atomic<float>* index = nullptr;
    std::atomic<float>* envAmount = nullptr;
    std::atomic<float>* envMs = nullptr;
    std::atomic<float>* maxTime = nullptr;
    std::atomic<float>* ratio = nullptr;
    std::array<float, 5> last {};
};

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
        knobWatchers.push_back (std::make_unique<UserChangeWatcher> (*processor.apvts.getParameter (id (field)), [this] (float)
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

    filterToggle.setButtonText ("FILTER");
    filterToggle.getProperties().set ("labelled", true);
    filterToggle.setTooltip (Help::forParameter (id (F::fmFilter)));
    filterToggle.setColour (juce::ToggleButton::tickColourId, accent);
    filterToggle.setColour (juce::ToggleButton::textColourId, Theme::textDim);
    addAndMakeVisible (filterToggle);
    filterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, id (F::fmFilter), filterToggle);

    waveDiagram = std::make_unique<Diagram> (processor.apvts, layer, Diagram::Kind::wave, accent);
    envDiagram = std::make_unique<Diagram> (processor.apvts, layer, Diagram::Kind::indexOverTime, accent);
    addAndMakeVisible (*waveDiagram);
    addAndMakeVisible (*envDiagram);

    HelpDialog::setupButton (helpButton, *this, HelpDialog::Topic::fm, "the FM engine (presets, ratio, index, envelope)");
    addAndMakeVisible (helpButton);

    // Bara användarens menyval fyller i ratterna; laddning, undo och automation av fmPreset får inte skriva över andra parametrar.
    presetWatcher = std::make_unique<UserChangeWatcher> (*processor.apvts.getParameter (id (F::fmPreset)),
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
    auto topRow = area.removeFromTop (18);
    filterToggle.setBounds (topRow.removeFromRight (60));
    topRow.removeFromRight (6);
    helpButton.setBounds (topRow.removeFromRight (40));
    auto top = area.removeFromTop (juce::jmin (area.getHeight(), 110));
    preset->setBounds (top.removeFromLeft (150).withTrimmedBottom (juce::jmax (0, top.getHeight() - 40)));
    top.removeFromLeft (8);
    const auto width = juce::jmin (90, top.getWidth() / 6);
    for (auto* k : { level.get(), ratio.get(), index.get(), envAmount.get(), envTime.get(), semis.get() })
        k->setBounds (top.removeFromLeft (width));

    // Under rattarna: vågformen och index över tid, sida vid sida.
    area.removeFromTop (6);
    if (area.getHeight() >= 40)
    {
        waveDiagram->setBounds (area.removeFromLeft (area.getWidth() / 2 - 3));
        area.removeFromLeft (6);
        envDiagram->setBounds (area);
    }
    else
    {
        waveDiagram->setBounds ({});
        envDiagram->setBounds ({});
    }
}
