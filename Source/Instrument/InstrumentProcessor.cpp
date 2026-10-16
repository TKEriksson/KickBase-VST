#include "InstrumentProcessor.h"
#include "InstrumentEditor.h"

namespace
{
    // En ton som börjar så här mycket efter ett steg räknas som "på steget".
    constexpr double noteOnToleranceSeconds = 0.003;

    // Ett steg med slide hålls lite längre än steget, så att nästa steg hinner glida in.
    constexpr double slideOverlap = 1.05;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        Params::Layout layout;
        Params::addPatternParams (layout, true);
        Params::addKickParams (layout);
        for (int layer = 0; layer < Pattern::numLayers; ++layer)
        {
            Params::addBassCommonParams (layout, layer, 0.7f);
            Params::addBassSynthParams (layout, layer);
        }
        Effects::addParameters (layout);
        Sidechain::addParameters (layout);
        Modulation::addParameters (layout);
        MasterSection::addParameters (layout);
        Mixer::addParameters (layout);
        Params::addMasterParams (layout);
        return layout;
    }

    bool isMonoOrStereo (const juce::AudioChannelSet& set)
    {
        return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo();
    }
}

InstrumentProcessor::InstrumentProcessor()
    : PluginBase (BusesProperties()
                      .withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Kick", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("Bass 1", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("Bass 2", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("Bass 3", juce::AudioChannelSet::stereo(), false),
                  createLayout(), false)
{
}

void InstrumentProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    prepareBase (sampleRate);
    kick.prepare (sampleRate);
    kick.setSeed (1000);
    for (size_t layer = 0; layer < basses.size(); ++layer)
        basses[layer].setSeed (2000 + (juce::int64) layer);
    for (auto& bass : basses)
        bass.prepare (sampleRate);
    for (auto& ducker : duckers)
        ducker.prepare (sampleRate);
    effects.prepare (sampleRate, samplesPerBlock);
    effects.setModulator (&modulator);
    master.prepare (sampleRate, 2);
    mixer.prepare (sampleRate);
    setLatencySamples (master.getLatencySamples());
    ensureScratchSize (samplesPerBlock);

    patternActive = false;
    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (params.get (ParamID::gain));
}

bool InstrumentProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (! isMonoOrStereo (layouts.getMainOutputChannelSet()))
        return false;

    // Kick- och baslagrens utgångar är frivilliga.
    for (int bus = 1; bus < layouts.outputBuses.size(); ++bus)
    {
        const auto& set = layouts.outputBuses[bus];
        if (! set.isDisabled() && ! isMonoOrStereo (set))
            return false;
    }

    return layouts.inputBuses.isEmpty();
}

void InstrumentProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const auto numSamples = buffer.getNumSamples();
    ensureScratchSize (numSamples);
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
    clock.beginBlock (getHostPosition(), numSamples);
    modulator.update (clock.getPpqAt (0));
    readBlockParams();

    // Var kick och bas ska skrivas. Huvudutgången får alltid hela mixen; kick- och
    // basutgångarna får dessutom sin del var om DAW:en aktiverat dem.
    auto assign = [&] (Output& out, int busIndex)
    {
        out.numChannels = 0;
        if (auto* bus = getBus (false, busIndex); bus != nullptr && bus->isEnabled())
        {
            auto busBuffer = getBusBuffer (buffer, false, busIndex);
            out.numChannels = std::min (2, busBuffer.getNumChannels());
            for (int ch = 0; ch < out.numChannels; ++ch)
                out.channels[(size_t) ch] = busBuffer.getWritePointer (ch);
        }
    };
    assign (mainOut, 0);
    assign (kickOut, 1);
    for (int layer = 0; layer < numLayers; ++layer)
        assign (layerOuts[(size_t) layer], 2 + layer);

    // Dela upp blocket vid varje MIDI-händelse så att toner börjar och slutar på rätt sample.
    int position = 0;
    for (const auto metadata : midi)
    {
        const auto eventPosition = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderSegment (position, eventPosition);
        handleMidi (metadata.getMessage(), eventPosition);
        position = eventPosition;
    }
    renderSegment (position, numSamples);
    finishBlock (numSamples);

    masterGain.setTargetValue (params.get (ParamID::gain));
    masterGain.applyGain (buffer, numSamples);

    // Mastern (maximizer/limiter) på huvudutgången. Kick- och basutgångarna fördröjs lika
    // mycket som limiterns lookahead, så att de ligger i synk med huvudutgången.
    auto mainBus = getBusBuffer (buffer, false, 0);
    master.process (mainBus, numSamples);

    // Källor som mixern tagit förbi maximizern läggs på efteråt, lika fördröjda som resten.
    bypassBus.applyGain (0, numSamples, params.get (ParamID::gain));
    master.delayOnly (bypassBus, numSamples, 4);
    for (int ch = 0; ch < mainBus.getNumChannels(); ++ch)
        mainBus.addFrom (ch, 0, bypassBus, mainBus.getNumChannels() == 1 ? 0 : ch, 0, numSamples, mainBus.getNumChannels() == 1 ? 0.5f : 1.0f);
    if (mainBus.getNumChannels() == 1)
        mainBus.addFrom (0, 0, bypassBus, 1, 0, numSamples, 0.5f);

    for (int bus = 1; bus < getBusCount (false); ++bus)
    {
        if (auto* b = getBus (false, bus); b != nullptr && b->isEnabled())
        {
            auto busBuffer = getBusBuffer (buffer, false, bus);
            master.delayOnly (busBuffer, numSamples, bus - 1);
        }
    }

    transportStatus.publish (clock);
    transportStatus.keyNote = currentNote;
    transportStatus.patternActive = patternActive;
    transportStatus.kickStep = patternActive ? scheduler.getLastKickStep() : -1;
    for (int layer = 0; layer < numLayers; ++layer)
        transportStatus.bassSteps[(size_t) layer] = patternActive ? scheduler.getLastBassStep (layer) : -1;
}

void InstrumentProcessor::readBlockParams()
{
    patternBars = Params::barsFromChoice (params.getChoice (ParamID::patternBars));
    kickOn = params.getBool (ParamID::kickOn);
    for (int layer = 0; layer < numLayers; ++layer)
    {
        layerOn[(size_t) layer] = params.bass (layer, BassField::on) >= 0.5f;
        duckAmounts[(size_t) layer] = params.bass (layer, BassField::duck); // med LFO-modulering
        gateScales[(size_t) layer] = params.bass (layer, BassField::gate);
    }
    noteEnd = (NoteEnd) params.getChoice (ParamID::noteEnd);
    kick.setClickSample (getActiveClickSample());
    refreshEngineParams();

    sidechainMode = (Sidechain::Mode) params.getChoice ("scMode");
    const auto lengthQuarters = Sidechain::lengthInQuarters (params.getChoice ("scLength"));
    duckLengthSamples = lengthQuarters > 0.0 ? lengthQuarters * clock.getSamplesPerQuarter()
                                             : KickEngine::lengthSeconds (readModulatedKick (0)) * getSampleRate();
}

void InstrumentProcessor::refreshEngineParams()
{
    kick.setParams (readModulatedKick (keyPitchClass()));
    for (int layer = 0; layer < numLayers; ++layer)
        basses[(size_t) layer].setParams (readModulatedBass (layer, bassRootNote()));
}

void InstrumentProcessor::handleMidi (const juce::MidiMessage& message, int sampleOffset)
{
    if (message.isNoteOn())
    {
        const auto note = message.getNoteNumber();
        const auto wasSilent = numHeldNotes == 0;

        if (! heldNotes[(size_t) note])
        {
            heldNotes[(size_t) note] = true;
            ++numHeldNotes;
        }

        currentNote = note;
        refreshEngineParams(); // ny tonart från och med nu

        if (wasSilent)
            startPattern (sampleOffset);
    }
    else if (message.isNoteOff())
    {
        const auto note = message.getNoteNumber();
        if (heldNotes[(size_t) note])
        {
            heldNotes[(size_t) note] = false;
            if (--numHeldNotes == 0)
                stopPattern (sampleOffset);
        }
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        if (numHeldNotes > 0)
        {
            heldNotes.fill (false);
            numHeldNotes = 0;
            stopPattern (sampleOffset);
        }
    }
}

void InstrumentProcessor::startPattern (int sampleOffset)
{
    // Utan DAW-transport börjar den egna klockan om på tonen, men bara efter en paus på mer
    // än en takt. Kommer nästa ton snabbare (man spelar en basgång) fortsätter den i takt.
    const auto pause = clock.getPpqAt (sampleOffset) - stoppedAtPpq;
    if (! patternActive && pause > clock.getBarLengthPpq())
        clock.restartFreeRunAt (sampleOffset);

    patternActive = true;
    stopAtPpq = std::numeric_limits<double>::infinity();

    // Kom tonen en aning efter steget? Spela steget ändå.
    const auto tolerance = (int) (noteOnToleranceSeconds * getSampleRate());
    scheduler.collectJustPassed (clock, pattern, patternBars, sampleOffset, tolerance);
    for (const auto& event : scheduler)
        handleEvent (event);
}

void InstrumentProcessor::stopPattern (int sampleOffset)
{
    if (noteEnd == NoteEnd::cut)
    {
        patternActive = false;
        stoppedAtPpq = clock.getPpqAt (sampleOffset);
        for (auto& bass : basses)
            bass.release();
        return; // kicken klingar ut av sig själv
    }

    // Vilken takt tonen släpptes i. En ton som slutar precis på ett taktstreck räknas
    // till takten före, så att den inte spelar en takt för mycket.
    const auto ppq = clock.getPpqAt (sampleOffset);
    const auto bar = clock.getBarIndex()
                   + (juce::int64) std::floor ((ppq - clock.getBarStartPpq()) / clock.getBarLengthPpq() - 1.0e-6);

    if (noteEnd == NoteEnd::finishBar)
    {
        stopAtPpq = clock.getPpqOfBar (bar + 1);
    }
    else
    {
        const auto patternStart = bar - (((bar % patternBars) + patternBars) % patternBars);
        stopAtPpq = clock.getPpqOfBar (patternStart + patternBars);
    }
}

void InstrumentProcessor::renderSegment (int startSample, int endSample)
{
    if (endSample <= startSample)
        return;

    auto position = startSample;

    if (patternActive)
    {
        scheduler.collect (clock, pattern, patternBars, startSample, endSample, stopAtPpq);
        for (const auto& event : scheduler)
        {
            renderSamples (position, event.offset);
            handleEvent (event);
            position = event.offset;
        }
    }

    renderSamples (position, endSample);

    if (patternActive && clock.getPpqAt (endSample) >= stopAtPpq)
    {
        patternActive = false;
        stoppedAtPpq = stopAtPpq;
    }
}

void InstrumentProcessor::renderSamples (int startSample, int endSample)
{
    auto* kickData = kickBuffer.getWritePointer (0);
    auto* duckData = duckBuffer.getWritePointer (0);
    const auto useCurve = sidechainMode == Sidechain::Mode::curve;

    for (int i = startSample; i < endSample; ++i)
    {
        kickData[i] = kick.nextSample();
        const auto curveValue = duckCurve.next (sidechainCurve, duckLengthSamples);
        duckData[i] = useCurve ? curveValue : kick.getEnvelope();
    }

    for (int layer = 0; layer < numLayers; ++layer)
    {
        auto& bass = basses[(size_t) layer];
        auto* sourceData = layerBuffer.getWritePointer (layer);
        auto* amplitudeData = amplitudeBuffer.getWritePointer (layer);
        for (int i = startSample; i < endSample; ++i)
        {
            const auto b = bass.next();
            sourceData[i] = b.source;
            amplitudeData[i] = b.amplitude;
        }
    }
}

void InstrumentProcessor::finishBlock (int numSamples)
{
    // Varje baslager: källa * amplitud (envelope) * ducking med lagrets egen mängd. Lagret går
    // sedan (helt eller delvis) genom sin effektkanal; varje lagers egen utgång får lagret
    // utan effekter.
    const auto* duckData = duckBuffer.getReadPointer (0);
    mixer.beginBlock();
    maxBus.clear (0, numSamples);
    maxBus.clear (1, numSamples);
    bypassBus.clear (0, numSamples);
    bypassBus.clear (1, numSamples);
    effects.beginBlock (numSamples, clock.getBpm(), clock.getPpqAt (0));
    tapBuffer.clear (0, numSamples);
    tapBuffer.clear (1, numSamples);

    for (int layer = 0; layer < numLayers; ++layer)
    {
        auto* data = layerBuffer.getWritePointer (layer);
        const auto* amplitude = amplitudeBuffer.getReadPointer (layer);
        auto& ducker = duckers[(size_t) layer];
        const auto amount = mixer.duckEnabled (Mixer::firstBassSource + layer) ? duckAmounts[(size_t) layer] : 0.0f;

        auto* before = tapBuffer.getWritePointer (0);
        auto* after = tapBuffer.getWritePointer (1);
        auto lastDuckGain = 1.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const auto level = data[i] * amplitude[i];
            const auto duckGain = ducker.next (duckData[i], amount);
            data[i] = level * duckGain;
            before[i] += level;
            after[i] += data[i];
            duckGainTaps[(size_t) layer].push (duckGain);
            lastDuckGain = duckGain;
        }
        duckReduction[(size_t) layer] = numSamples > 0 ? 1.0f - lastDuckGain : 0.0f;

        // Kanal A-C: amount av ljudet går genom kanalen, resten torrt.
        const auto channel = (int) params.bass (layer, BassField::fxChannel) - 1;
        const auto amountToChannel = params.bass (layer, BassField::fxAmount);
        const auto routed = channel >= 0 && effects.isActive (channel);
        if (routed)
            effects.send (channel, data, nullptr, numSamples, amountToChannel);
        const auto dryGain = routed ? 1.0f - amountToChannel : 1.0f;

        mixer.mix (Mixer::firstBassSource + layer, data, nullptr, numSamples, dryGain, maxBus, bypassBus);
        for (int i = 0; i < numSamples; ++i)
            layerOuts[(size_t) layer].add (i, data[i], data[i]);
    }

    // Kickens egen effektkanal: amount av kicken går genom kanalen, resten torrt.
    const auto kickChannel = (int) params.get (ParamID::kickFxChannel) - 1;
    const auto kickAmount = params.get (ParamID::kickFxAmount);
    const auto kickRouted = kickChannel >= 0 && effects.isActive (kickChannel);
    if (kickRouted)
        effects.send (kickChannel, kickBuffer.getReadPointer (0), nullptr, numSamples, kickAmount);
    const auto kickDry = kickRouted ? 1.0f - kickAmount : 1.0f;

    mixer.mix (Mixer::kickSource, kickBuffer.getReadPointer (0), nullptr, numSamples, kickDry, maxBus, bypassBus);

    std::array<bool, numEffectChannels> channelDuck {};
    for (int ch = 0; ch < numEffectChannels; ++ch)
        channelDuck[(size_t) ch] = mixer.duckEnabled (Mixer::firstFxSource + ch);
    effects.process (numSamples, duckData, nullptr, channelDuck.data());
    for (int ch = 0; ch < numEffectChannels; ++ch)
    {
        const auto& returned = effects.output (ch);
        mixer.mix (Mixer::firstFxSource + ch, returned.getReadPointer (0), returned.getReadPointer (1), numSamples, 1.0f, maxBus, bypassBus);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        kickTap.push (kickBuffer.getSample (0, i));
        bassBeforeDuckTap.push (tapBuffer.getSample (0, i));
        bassAfterDuckTap.push (tapBuffer.getSample (1, i));
    }

    const auto* kickData = kickBuffer.getReadPointer (0);
    const auto* left = maxBus.getReadPointer (0);
    const auto* right = maxBus.getReadPointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        mainOut.add (i, left[i], right[i]);
        kickOut.add (i, kickData[i], kickData[i]);
    }
}

void InstrumentProcessor::ensureScratchSize (int numSamples)
{
    // Allokerar bara om DAW:en skickar ett större block än den lovade i prepareToPlay.
    if (kickBuffer.getNumSamples() >= numSamples)
        return;

    kickBuffer.setSize (1, numSamples, false, true, true);
    duckBuffer.setSize (1, numSamples, false, true, true);
    layerBuffer.setSize (numLayers, numSamples, false, true, true);
    tapBuffer.setSize (2, numSamples, false, true, true);
    amplitudeBuffer.setSize (numLayers, numSamples, false, true, true);
    bassBuffer.setSize (2, numSamples, false, true, true);
    maxBus.setSize (2, numSamples, false, true, true);
    bypassBus.setSize (2, numSamples, false, true, true);
}

void InstrumentProcessor::handleEvent (const StepEvent& event)
{
    if (event.type == StepEvent::Type::kick)
    {
        // Duckkurvan följer kickstegen även när kicken är avstängd (pump utan kick).
        duckCurve.trigger();
        if (kickOn)
            kick.trigger (1.0f);
        return;
    }

    const auto layer = event.layer;
    if (! layerOn[(size_t) layer])
        return;

    const auto& note = pattern.note (layer, event.step);
    const auto samplesPerTick = clock.getSamplesPerQuarter() / Pattern::ticksPerQuarter;
    const auto holdTicks = pattern.holdTicks (layer, event.step, patternBars * Pattern::ticksPerBar, gateScales[(size_t) layer]);
    const auto gateSamples = std::max (1, (int) (holdTicks * samplesPerTick));

    basses[(size_t) layer].noteOn (note.pitch.load(), note.velocity.load(), gateSamples, note.slide.load());
}

juce::AudioProcessorEditor* InstrumentProcessor::createEditor()
{
    return new InstrumentEditor (*this);
}
