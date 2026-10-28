#include "FxProcessor.h"
#include "FxEditor.h"

namespace
{

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        Params::Layout layout;
        Params::addPatternParams (layout, false);
        Params::addKickParams (layout);
        Params::addInstrumentCommonParams (layout, 0, 1.0f); // gaten är lager 1
        Params::addFxParams (layout);
        Effects::addParameters (layout);
        Sidechain::addParameters (layout);
        Modulation::addParameters (layout);
        MasterSection::addParameters (layout);
        Params::addMasterParams (layout);
        Params::addKickToggleParams (layout); // readKick läser dem
        Params::addKickMixParam (layout);
        return layout;
    }
}

FxProcessor::FxProcessor()
    : PluginBase (BusesProperties()
                      .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                  createLayout(), true)
{
    scheduler.setNumLayers (1);
}

void FxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    prepareBase (sampleRate);
    kick.prepare (sampleRate);
    kick.setSeed (1000);
    gate.prepare (sampleRate);
    ducker.prepare (sampleRate);
    effects.prepare (sampleRate, samplesPerBlock);
    effects.setModulator (&modulator);
    master.prepare (sampleRate, 2);
    setLatencySamples (master.getLatencySamples());
    ensureScratchSize (samplesPerBlock);
}

void FxProcessor::ensureScratchSize (int numSamples)
{
    // Allokerar bara om DAW:en skickar ett större block än den lovade i prepareToPlay.
    if (kickBuffer.getNumSamples() >= numSamples)
        return;

    kickBuffer.setSize (1, numSamples, false, true, true);
    duckBuffer.setSize (1, numSamples, false, true, true);
    duckRawBuffer.setSize (1, numSamples, false, true, true);
    gateBuffer.setSize (1, numSamples, false, true, true);
    synthBuffer.setSize (2, numSamples, false, true, true);
}

bool FxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return layouts.getMainInputChannelSet() == out
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void FxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    processInControlChunks (buffer, midi, [this] (juce::AudioBuffer<float>& chunk, juce::MidiBuffer&) { processChunk (chunk); });
}

void FxProcessor::processChunk (juce::AudioBuffer<float>& buffer)
{

    const auto numSamples = buffer.getNumSamples();
    ensureScratchSize (numSamples);
    clock.beginBlock (getHostPosition(), numSamples);
    if (controlPhase == 0) // (bara på 32-rutnätet, se InstrumentProcessor::processChunk)
        modulator.update (clock.getPpqAt (0), clock.getBpm());

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
    // En synth före i kedjan kan skicka NaN eller oändliga värden: de skulle fastna i filtren och effekterna för alltid.
    if (! allFinite (buffer, numSamples))
        replaceNonFinite (buffer, getTotalNumInputChannels(), numSamples);

    const auto key = params.getChoice (ParamID::fxKey);
    lengths = Params::readPatternLengthsWith ([this] (const char* id) { return params.get (id); }, pattern, 1);
    const auto audition = auditionRequest.exchange (-1);
    if (audition == 5)
        kickSoloSamplesLeft = (int) (getSampleRate() * 2.0);
    auto kickParams = readModulatedKick (key);
    if (kickSoloSamplesLeft > 0 && getActiveClickSample() != nullptr)
    {
        kickParams.sampleMode = 2; // provlyssning av bara samplingen
        kickParams.sampleOn = true;
        kickParams.sampleLevel = 1.0f;
    }
    kickSoloSamplesLeft = std::max (0, kickSoloSamplesLeft - numSamples);
    kick.setParams (kickParams);
    kick.setClickSample (getActiveClickSample());
    if (audition == 0 || audition == 5) // provlyssning av kicken från gränssnittet
    {
        duckCurve.trigger();
        kick.trigger (1.0f);
    }
    gate.setTimes (params.get (ParamID::gateAttack), params.get (ParamID::gateRelease));

    active = clock.isFollowingHost();
    kickOn = params.getBool (ParamID::kickOn);
    gateOn = params.bass (0, InstrumentField::on) >= 0.5f;
    duckAmount = params.bass (0, InstrumentField::duck); // med LFO-modulering
    gateScale = params.bass (0, InstrumentField::gate);
    inputLevel = params.bass (0, InstrumentField::level) * params.get (ParamID::gain);

    useCurve = (Sidechain::Mode) params.getChoice ("scMode") == Sidechain::Mode::curve;
    const auto lengthQuarters = Sidechain::lengthInQuarters (params.getChoice ("scLength"));
    duckLengthSamples = lengthQuarters > 0.0 ? lengthQuarters * clock.getSamplesPerQuarter()
                                             : KickEngine::lengthSeconds (readModulatedKick (key)) * getSampleRate();

    auto position = 0;
    if (active)
    {
        scheduler.collect (clock, pattern, lengths, 0, numSamples, std::numeric_limits<double>::infinity());
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

    // Gate, sedan (helt eller delvis) genom effektkanalen, och ducking under kicken.
    for (int ch = 0; ch < 2; ++ch)
        juce::FloatVectorOperations::multiply (synthBuffer.getWritePointer (ch), gateBuffer.getReadPointer (0), numSamples);

    effects.beginBlock (numSamples, clock.getBpm(), clock.getPpqAt (0), controlPhase);
    const auto channel = (int) params.bass (0, InstrumentField::fxChannel) - 1;
    const auto amountToChannel = params.bass (0, InstrumentField::fxAmount);
    const auto routed = channel >= 0 && effects.isActive (channel);
    if (routed)
    {
        effects.send (channel, synthBuffer.getReadPointer (0), synthBuffer.getReadPointer (1), numSamples, amountToChannel);
        if (! params.hasBass (0, InstrumentField::fxSend) || params.bass (0, InstrumentField::fxSend) < 0.5f) // saknad = av (som förut)
            synthBuffer.applyGain (1.0f - amountToChannel);
    }

    // För gränssnittets vågformer: synthen före duckningen (mono-summa).
    for (int i = 0; i < numSamples; ++i)
        instrumentBeforeDuckTap.push (0.5f * (synthBuffer.getSample (0, i) + synthBuffer.getSample (1, i)));

    // Kickens envelope per sample (före duckern) till kanalernas duckning.
    duckRawBuffer.copyFrom (0, 0, duckBuffer, 0, 0, numSamples);
    auto* duckData = duckBuffer.getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        duckData[i] = ducker.next (duckData[i], duckAmount);
        duckGainTaps[0].push (duckData[i]);
    }
    duckReduction[0] = numSamples > 0 ? 1.0f - duckData[numSamples - 1] : 0.0f;

    for (int ch = 0; ch < 2; ++ch)
        juce::FloatVectorOperations::multiply (synthBuffer.getWritePointer (ch), duckData, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        kickTap.push (kickBuffer.getSample (0, i));
        instrumentAfterDuckTap.push (0.5f * (synthBuffer.getSample (0, i) + synthBuffer.getSample (1, i)));
    }

    effects.process (numSamples, duckRawBuffer.getReadPointer (0), &synthBuffer);

    const auto masterGain = params.get (ParamID::gain);
    const auto* kickData = kickBuffer.getReadPointer (0);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto kickValue = kickData[i] * masterGain;
        const auto left = synthBuffer.getSample (0, i) * inputLevel + kickValue;
        const auto right = synthBuffer.getSample (1, i) * inputLevel + kickValue;

        if (buffer.getNumChannels() == 1)
            buffer.setSample (0, i, 0.5f * (left + right));
        else
        {
            buffer.setSample (0, i, left);
            buffer.setSample (1, i, right);
        }
    }

    master.process (buffer, numSamples);

    if (! allFinite (buffer, numSamples))
    {
        // Något internt gav NaN/inf: töm blocket och börja om med rena tillstånd, i stället för att vara trasig tills pluginet laddas om.
        buffer.clear();
        kick.reset();
        gate.prepare (getSampleRate());
        ducker.prepare (getSampleRate());
        effects.reset();
        master.reset();
        ++nonFiniteRecoveries;
    }

    transportStatus.publish (clock);
    transportStatus.patternBar = (int) (((clock.getBarIndex() % lengths.total) + lengths.total) % lengths.total) + 1;
    transportStatus.patternProgress = ((float) (transportStatus.patternBar.load() - 1) + (float) juce::jlimit (0.0, 1.0, (clock.getPpqAt (0) - clock.getBarStartPpq()) / std::max (1.0e-6, clock.getBarLengthPpq()))) / (float) lengths.total;
    transportStatus.keyNote = 60 + key;
    transportStatus.patternActive = active;
    transportStatus.kickPlayTime = kick.getPlayTime();
    transportStatus.kickTriggers = kick.getTriggerCount();
    transportStatus.kickStep = active ? scheduler.getLastKickStep() : -1;
    transportStatus.instrumentSteps[0] = active ? scheduler.getLastInstrumentStep (0) : -1;
}

void FxProcessor::renderSamples (int startSample, int endSample)
{
    auto* kickData = kickBuffer.getWritePointer (0);
    auto* duckData = duckBuffer.getWritePointer (0);
    auto* gateData = gateBuffer.getWritePointer (0);

    for (int i = startSample; i < endSample; ++i)
    {
        kickData[i] = kick.nextSample();
        const auto curveValue = duckCurve.next (sidechainCurve, duckLengthSamples);
        duckData[i] = useCurve ? curveValue : kick.getEnvelope();
        gateData[i] = gate.next (! active || ! gateOn);
    }
}

void FxProcessor::handleEvent (const StepEvent& event)
{
    if (event.type == StepEvent::Type::kick)
    {
        duckCurve.trigger(); // duckkurvan följer kickstegen även när kicken är avstängd
        if (kickOn)
        {
            const auto& note = pattern.note (Pattern::kickLayer, event.step);
            kick.trigger (note.velocity.load(), note.pitch.load());
        }
        return;
    }

    if (event.type == StepEvent::Type::slice)
        return; // skivrader ("Slice N") är bara ljud i samplerösten

    const auto& note = pattern.note (0, event.step);
    if (note.slide.load())
        return; // en glide-ton styr bara tonhöjd, och här kommer tonhöjden från den andra synthen

    const auto samplesPerTick = clock.getSamplesPerQuarter() / Pattern::ticksPerQuarter;

    // Slide håller gaten öppen in i nästa ton, så att de flyter ihop.
    const auto holdTicks = pattern.holdTicks (0, event.step, lengths.total * Pattern::ticksPerBar, gateScale);
    gate.open (note.velocity.load(), std::max (1, (int) (holdTicks * samplesPerTick)));
}

juce::AudioProcessorEditor* FxProcessor::createEditor()
{
    return new FxEditor (*this);
}
