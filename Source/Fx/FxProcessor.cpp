#include "FxProcessor.h"
#include "FxEditor.h"

namespace
{
    constexpr double slideOverlap = 1.05;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        Params::Layout layout;
        Params::addPatternParams (layout, false);
        Params::addKickParams (layout);
        Params::addBassCommonParams (layout, 1.0f);
        Params::addFxParams (layout);
        Params::addMasterParams (layout);
        return layout;
    }
}

FxProcessor::FxProcessor()
    : PluginBase (BusesProperties()
                      .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                  createLayout(), true)
{
}

void FxProcessor::prepareToPlay (double sampleRate, int)
{
    prepareBase (sampleRate);
    kick.prepare (sampleRate);
    gate.prepare (sampleRate);
    ducker.prepare (sampleRate);
}

bool FxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return layouts.getMainInputChannelSet() == out
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void FxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    clock.beginBlock (getHostPosition(), numSamples);

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto key = params.getChoice (ParamID::fxKey);
    const auto patternBars = Params::barsFromChoice (params.getChoice (ParamID::patternBars));
    scheduler.setGrid ((BassGrid) params.getChoice (ParamID::bassGrid));
    kick.setParams (params.readKick (key));
    kick.setClickSample (getActiveClickSample());
    gate.setTimes (params.get (ParamID::gateAttack), params.get (ParamID::gateRelease));

    active = clock.isFollowingHost();
    kickOn = params.getBool (ParamID::kickOn);
    gateOn = params.getBool (ParamID::bassOn);
    duckAmount = params.get (ParamID::duckAmount);
    gateScale = params.get (ParamID::bassGate);
    inputLevel = params.get (ParamID::bassLevel) * params.get (ParamID::gain);

    auto position = 0;
    if (active)
    {
        scheduler.collect (clock, pattern, patternBars, 0, numSamples, std::numeric_limits<double>::infinity());
        for (const auto& event : scheduler)
        {
            processSamples (buffer, position, event.offset);
            handleEvent (event);
            position = event.offset;
        }
    }
    processSamples (buffer, position, numSamples);

    transportStatus.publish (clock);
    transportStatus.keyNote = 60 + key;
    transportStatus.patternActive = active;
    transportStatus.kickStep = active ? scheduler.getLastKickStep() : -1;
    transportStatus.bassStep = active ? scheduler.getLastBassStep() : -1;
}

void FxProcessor::processSamples (juce::AudioBuffer<float>& buffer, int startSample, int endSample)
{
    const auto masterGain = params.get (ParamID::gain);

    for (int i = startSample; i < endSample; ++i)
    {
        const auto kickSample = kick.nextSample() * masterGain;
        const auto gateLevel = gate.next (! active || ! gateOn);
        const auto duck = ducker.next (kick.getEnvelope(), duckAmount);
        const auto inputGain = gateLevel * duck * inputLevel;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, buffer.getSample (ch, i) * inputGain + kickSample);
    }
}

void FxProcessor::handleEvent (const StepEvent& event)
{
    if (event.type == StepEvent::Type::kick)
    {
        if (kickOn)
            kick.trigger (1.0f);
        return;
    }

    const auto grid = scheduler.getGrid();
    const auto& step = pattern.bassStep (grid, event.step);
    const auto samplesPerStep = clock.getSamplesPerQuarter() / Pattern::bassStepsPerQuarter (grid);

    // Slide håller gaten öppen in i nästa steg, så att de flyter ihop.
    const auto gateSamples = step.slide.load() ? (int) (samplesPerStep * slideOverlap)
                                               : std::max (1, (int) (step.gate.load() * gateScale * samplesPerStep));
    gate.open (step.velocity.load(), gateSamples);
}

juce::AudioProcessorEditor* FxProcessor::createEditor()
{
    return new FxEditor (*this);
}
