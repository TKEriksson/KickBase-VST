#include "InstrumentProcessor.h"
#include "InstrumentEditor.h"

namespace
{
    // En ton som börjar så här mycket efter ett steg räknas som "på steget".
    constexpr double noteOnToleranceSeconds = 0.003;


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
    // Ett nytt instrument börjar som Init: tre lager som låter bra ihop (ett sparat projekt skriver över det).
    loadFactoryPreset (Presets::Section::full, 0, 0);
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

    // Provlyssning från gränssnittet. En solo-källa (en oscillator eller samplingen) sätts före parametrarna läses, så att
    // de första samplesen redan är solo.
    const auto audition = auditionRequest.exchange (-1);
    if (audition >= 0 && numHeldNotes == 0)
        currentNote = previewNote.load(); // all provlyssning (kick, oscillatorer, sampling) spelas på den valda tonen
    if (audition == 5)
        kickSoloSamplesLeft = (int) (getSampleRate() * 2.0);
    if (audition >= 10 && audition < 100)
    {
        soloLayer = juce::jlimit (0, numLayers - 1, (audition - 10) / 10);
        soloSource = (audition - 10) % 10;
        soloSamplesLeft = (int) getSampleRate();
    }
    readBlockParams();

    if (audition == 0 || audition == 5)
    {
        duckCurve.trigger();
        kick.trigger (1.0f);
    }
    else if (juce::isPositiveAndBelow (audition - 1, numLayers))
    {
        basses[(size_t) (audition - 1)].noteOn (0, 1.0f, (int) getSampleRate());
    }
    else if (audition >= 100)
    {
        handlePreviewRequest (audition - 100);
    }
    else if (audition >= 10)
    {
        basses[(size_t) soloLayer].noteOn (0, 1.0f, (int) getSampleRate());
    }

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
    previewingLayer = patternActive ? previewLayer : -1;
    soloSamplesLeft = std::max (0, soloSamplesLeft - numSamples);
    kickSoloSamplesLeft = std::max (0, kickSoloSamplesLeft - numSamples);
    transportStatus.kickPlayTime = kick.getPlayTime();
    transportStatus.kickTriggers = kick.getTriggerCount();
    for (int layer = 0; layer < numLayers; ++layer)
        transportStatus.bassSamplePosition[(size_t) layer] = basses[(size_t) layer].getSampleFraction();
    transportStatus.kickStep = patternActive ? scheduler.getLastKickStep() : -1;
    for (int layer = 0; layer < numLayers; ++layer)
        transportStatus.bassSteps[(size_t) layer] = patternActive ? scheduler.getLastBassStep (layer) : -1;
}

void InstrumentProcessor::readBlockParams()
{
    lengths = Params::readPatternLengthsWith ([this] (const char* id) { return params.get (id); });
    kickOn = params.getBool (ParamID::kickOn);
    for (int layer = 0; layer < numLayers; ++layer)
    {
        layerOn[(size_t) layer] = params.bass (layer, BassField::on) >= 0.5f;
        duckAmounts[(size_t) layer] = params.bass (layer, BassField::duck); // med LFO-modulering
        gateScales[(size_t) layer] = params.bass (layer, BassField::gate);
        gateOn[(size_t) layer] = params.bass (layer, BassField::gateOn) >= 0.5f;
        gateGrid[(size_t) layer] = juce::jlimit (0, Pattern::numGrids - 1, (int) params.bass (layer, BassField::gateGrid));
        gateDepth[(size_t) layer] = params.bass (layer, BassField::gateDepth);
        const auto smoothMs = std::max (0.5f, params.bass (layer, BassField::gateSmooth));
        gateCoefficient[(size_t) layer] = (float) (1.0 - std::exp (-1.0 / (0.001 * smoothMs * getSampleRate())));
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
    auto kickParams = readModulatedKick (keyPitchClass());
    if (kickSoloSamplesLeft > 0 && getActiveClickSample() != nullptr)
    {
        kickParams.sampleMode = 2; // bara samplingen, i full nivå
        kickParams.sampleLevel = 1.0f;
    }
    kick.setParams (kickParams);
    for (int layer = 0; layer < numLayers; ++layer)
    {
        auto bassParams = readModulatedBass (layer, bassRootNote());
        if (soloSamplesLeft > 0 && layer == soloLayer)
        {
            // Lyssna på en enda källa: bara den är på, med full nivå (sub och brus tystas).
            for (int source = 0; source < 3; ++source)
            {
                auto& osc = source == 0 ? bassParams.oscA : source == 1 ? bassParams.oscB : bassParams.oscC;
                osc.on = source == soloSource;
                if (source == soloSource)
                    osc.level = 1.0f;
            }
            bassParams.sub = 0.0f;
            bassParams.noise = 0.0f;
            bassParams.sampleOn = soloSource == 3;
            if (soloSource == 3)
                bassParams.sampleLevel = 1.0f;
        }
        basses[(size_t) layer].setParams (bassParams);
    }
}

void InstrumentProcessor::handlePreviewRequest (int row)
{
    if (previewLayer == row)
    {
        stopPreview(); // samma knapp igen avbryter
        return;
    }
    if (patternActive || numHeldNotes > 0)
        return; // en ton spelar redan: mönstret hörs ändå

    if (! clock.isFollowingHost())
        clock.restartFreeRunAt (0);
    currentNote = previewNote.load(); // tonarten för förhandsspelningen, som om tonen hölls
    refreshEngineParams();
    previewLayer = row;
    patternActive = true;
    const auto rowBars = lengths.barsOf (row == Pattern::kickLayer ? 0 : row + 1);
    stopAtPpq = clock.getPpqAt (0) + (double) rowBars * clock.getBarLengthPpq(); // ett varv

    const auto tolerance = (int) (noteOnToleranceSeconds * getSampleRate());
    scheduler.collectJustPassed (clock, pattern, lengths, 0, tolerance);
    for (const auto& event : scheduler)
        handleEvent (event);
}

void InstrumentProcessor::stopPreview()
{
    previewLayer = -1;
    patternActive = false;
    stoppedAtPpq = clock.getPpqAt (0);
    for (auto& bass : basses)
        bass.release();
}

void InstrumentProcessor::handleMidi (const juce::MidiMessage& message, int sampleOffset)
{
    if (message.isNoteOn())
    {
        previewLayer = -1; // en riktig ton tar över från förhandsspelningen
        const auto note = message.getNoteNumber();
        const auto wasSilent = numHeldNotes == 0;

        // Räknar tonerna: FL skickar ibland "ton på" före "ton av" när ett mönster tar slut och nästa
        // börjar på exakt samma ton och sample. Då ska tonen fortfarande hållas, inte släppas.
        if (heldNotes[(size_t) note]++ == 0)
            ++numHeldNotes;

        currentNote = note;
        refreshEngineParams(); // ny tonart från och med nu

        if (wasSilent)
            startPattern (sampleOffset);
    }
    else if (message.isNoteOff())
    {
        const auto note = message.getNoteNumber();
        if (heldNotes[(size_t) note] > 0 && --heldNotes[(size_t) note] == 0 && --numHeldNotes == 0)
            stopPattern (sampleOffset);
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        if (numHeldNotes > 0)
        {
            heldNotes.fill (0);
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
    scheduler.collectJustPassed (clock, pattern, lengths, sampleOffset, tolerance);
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
        const auto patternStart = bar - (((bar % lengths.total) + lengths.total) % lengths.total);
        stopAtPpq = clock.getPpqOfBar (patternStart + lengths.total);
    }
}

void InstrumentProcessor::renderSegment (int startSample, int endSample)
{
    if (endSample <= startSample)
        return;

    auto position = startSample;

    if (patternActive)
    {
        scheduler.collect (clock, pattern, lengths, startSample, endSample, stopAtPpq);
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
        previewLayer = -1; // förhandsspelningen har spelat ett varv
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
        auto* sourceRight = layerBufferRight.getWritePointer (layer);
        auto* amplitudeData = amplitudeBuffer.getWritePointer (layer);
        for (int i = startSample; i < endSample; ++i)
        {
            const auto b = bass.next();
            sourceData[i] = b.left;
            sourceRight[i] = b.right;
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
        auto* dataRight = layerBufferRight.getWritePointer (layer);
        const auto* amplitude = amplitudeBuffer.getReadPointer (layer);
        auto& ducker = duckers[(size_t) layer];
        const auto amount = mixer.duckEnabled (Mixer::firstBassSource + layer) ? duckAmounts[(size_t) layer] : 0.0f;

        auto* before = tapBuffer.getWritePointer (0);
        auto* after = tapBuffer.getWritePointer (1);
        auto lastDuckGain = 1.0f;
        // GATE: stegen följer samma takt och mönsterlängd som tonerna, men med eget rutnät.
        const auto gating = gateOn[(size_t) layer] && patternActive;
        const auto gateStepsPerBar = Pattern::bassStepsPerBar ((BassGrid) gateGrid[(size_t) layer]);
        const auto gateClosed = 1.0f - gateDepth[(size_t) layer];
        auto& gateGainRef = gateGain[(size_t) layer];

        for (int i = 0; i < numSamples; ++i)
        {
            auto gateTarget = 1.0f;
            if (gating)
            {
                const auto ppq = clock.getPpqAt (i);
                const auto barLength = clock.getBarLengthPpq();
                const auto bar = clock.getBarIndex() + (juce::int64) std::floor ((ppq - clock.getBarStartPpq()) / barLength);
                const auto inBar = juce::jlimit (0.0, 0.999999, (ppq - clock.getPpqOfBar (bar)) / barLength);
                const auto barInRow = lengths.barInRow (1 + layer, bar); // -1: raden är klar och tyst, gaten rör inget
                gateTarget = barInRow < 0 || pattern.isGateStepOn (layer, barInRow * gateStepsPerBar + (int) (inBar * gateStepsPerBar)) ? 1.0f : gateClosed;
            }
            gateGainRef += (gateTarget - gateGainRef) * gateCoefficient[(size_t) layer];

            const auto levelLeft = data[i] * amplitude[i] * gateGainRef, levelRight = dataRight[i] * amplitude[i] * gateGainRef;
            const auto duckGain = ducker.next (duckData[i], amount);
            data[i] = levelLeft * duckGain;
            dataRight[i] = levelRight * duckGain;
            before[i] += 0.5f * (levelLeft + levelRight);
            after[i] += 0.5f * (data[i] + dataRight[i]);
            duckGainTaps[(size_t) layer].push (duckGain);
            lastDuckGain = duckGain;
        }
        duckReduction[(size_t) layer] = numSamples > 0 ? 1.0f - lastDuckGain : 0.0f;

        // Kanal A-C: amount av ljudet går genom kanalen, resten torrt.
        const auto channel = (int) params.bass (layer, BassField::fxChannel) - 1;
        const auto amountToChannel = params.bass (layer, BassField::fxAmount);
        const auto routed = channel >= 0 && effects.isActive (channel);
        if (routed)
            effects.send (channel, data, dataRight, numSamples, amountToChannel);
        const auto dryGain = routed ? 1.0f - amountToChannel : 1.0f;

        mixer.mix (Mixer::firstBassSource + layer, data, dataRight, numSamples, dryGain, maxBus, bypassBus);
        for (int i = 0; i < numSamples; ++i)
            layerOuts[(size_t) layer].add (i, data[i], dataRight[i]);
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
    layerBufferRight.setSize (numLayers, numSamples, false, true, true);
    tapBuffer.setSize (2, numSamples, false, true, true);
    amplitudeBuffer.setSize (numLayers, numSamples, false, true, true);
    bassBuffer.setSize (2, numSamples, false, true, true);
    maxBus.setSize (2, numSamples, false, true, true);
    bypassBus.setSize (2, numSamples, false, true, true);
}

void InstrumentProcessor::handleEvent (const StepEvent& event)
{
    // Förhandsspelning av en enskild rad: de andra raderna är tysta.
    if (previewLayer >= 0 && (event.type == StepEvent::Type::kick ? previewLayer != Pattern::kickLayer : event.layer != previewLayer))
        return;

    if (event.type == StepEvent::Type::kick)
    {
        // Duckkurvan följer kickstegen även när kicken är avstängd (pump utan kick).
        duckCurve.trigger();
        if (kickOn)
        {
            const auto& note = pattern.note (Pattern::kickLayer, event.step);
            kick.trigger (note.velocity.load(), note.pitch.load());
        }
        return;
    }

    const auto layer = event.layer;
    if (! layerOn[(size_t) layer])
        return;

    const auto& note = pattern.note (layer, event.step);
    const auto samplesPerTick = clock.getSamplesPerQuarter() / Pattern::ticksPerQuarter;

    // En glide-ton slår inte an: den glider tonen som klingar till sin tonhöjd under sin egen längd. Klingar inget
    // spelas den som en vanlig ton.
    if (note.slide.load() && basses[(size_t) layer].glideTo (note.pitch.load(), std::max (1, (int) (note.length.load() * samplesPerTick))))
        return;

    const auto holdTicks = pattern.holdTicks (layer, event.step, lengths.total * Pattern::ticksPerBar, gateScales[(size_t) layer]);
    const auto gateSamples = std::max (1, (int) (holdTicks * samplesPerTick));

    basses[(size_t) layer].noteOn (note.pitch.load(), note.velocity.load(), gateSamples);
}

juce::AudioProcessorEditor* InstrumentProcessor::createEditor()
{
    return new InstrumentEditor (*this);
}
