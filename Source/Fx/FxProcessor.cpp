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
        Effects::addParameters (layout);
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

void FxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    prepareBase (sampleRate);
    kick.prepare (sampleRate);
    gate.prepare (sampleRate);
    ducker.prepare (sampleRate);
    effects.prepare (sampleRate, samplesPerBlock);
    ensureScratchSize (samplesPerBlock);
}

void FxProcessor::ensureScratchSize (int numSamples)
{
    // Allokerar bara om DAW:en skickar ett större block än den lovade i prepareToPlay.
    if (kickBuffer.getNumSamples() >= numSamples)
        return;

    kickBuffer.setSize (1, numSamples, false, true, true);
    envelopeBuffer.setSize (1, numSamples, false, true, true);
    gateBuffer.setSize (1, numSamples, false, true, true);
    synthBuffer.setSize (2, numSamples, false, true, true);
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
    ensureScratchSize (numSamples);
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
            renderSamples (position, event.offset);
            handleEvent (event);
            position = event.offset;
        }
    }
    renderSamples (position, numSamples);

    // Synthens ljud: effekter före gaten -> gate -> effekter efter -> ducking -> + kick.
    const auto inputChannels = std::min (2, getTotalNumInputChannels());
    for (int ch = 0; ch < 2; ++ch)
        synthBuffer.copyFrom (ch, 0, buffer, std::min (ch, std::max (0, inputChannels - 1)), 0, numSamples);

    effects.update (clock.getBpm());
    effects.process (synthBuffer, numSamples, false, effectOrder);

    for (int ch = 0; ch < 2; ++ch)
        juce::FloatVectorOperations::multiply (synthBuffer.getWritePointer (ch), gateBuffer.getReadPointer (0), numSamples);

    effects.process (synthBuffer, numSamples, true, effectOrder);

    const auto masterGain = params.get (ParamID::gain);
    const auto* kickData = kickBuffer.getReadPointer (0);
    const auto* envelopeData = envelopeBuffer.getReadPointer (0);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto duck = ducker.next (envelopeData[i], duckAmount) * inputLevel;
        const auto kickSample = kickData[i] * masterGain;
        const auto left = synthBuffer.getSample (0, i) * duck + kickSample;
        const auto right = synthBuffer.getSample (1, i) * duck + kickSample;

        if (buffer.getNumChannels() == 1)
            buffer.setSample (0, i, 0.5f * (left + right));
        else
        {
            buffer.setSample (0, i, left);
            buffer.setSample (1, i, right);
        }
    }

    transportStatus.publish (clock);
    transportStatus.keyNote = 60 + key;
    transportStatus.patternActive = active;
    transportStatus.kickStep = active ? scheduler.getLastKickStep() : -1;
    transportStatus.bassStep = active ? scheduler.getLastBassStep() : -1;
}

void FxProcessor::renderSamples (int startSample, int endSample)
{
    auto* kickData = kickBuffer.getWritePointer (0);
    auto* envelopeData = envelopeBuffer.getWritePointer (0);
    auto* gateData = gateBuffer.getWritePointer (0);

    for (int i = startSample; i < endSample; ++i)
    {
        kickData[i] = kick.nextSample();
        envelopeData[i] = kick.getEnvelope();
        gateData[i] = gate.next (! active || ! gateOn);
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
