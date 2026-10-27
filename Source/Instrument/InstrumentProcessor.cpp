#include "InstrumentProcessor.h"
#include "InstrumentEditor.h"

namespace
{
    // En ton som börjar så här mycket efter ett steg räknas som "på steget".
    constexpr double noteOnToleranceSeconds = 0.003;

    // Och en "ton av" som kommer så här mycket efter ett steg räknas som att tonen slutade på steget: steget spelas inte. DAW:en (FL) skickar "ton av" när ett
    // mönster tar slut, och med avrundning till hela samplingar kan den hamna en eller några samplingar efter taktens första steg. Utan den här gränsen
    // spelade den gamla instansen ett sista steg (en kick) på taktens första slag samtidigt som nästa instans började: en dubbel kick.
    constexpr double noteOffToleranceSeconds = 0.003;


    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        Params::Layout layout;
        Params::addPatternParams (layout, true);
        Params::addKickParams (layout);
        SourceEq::addParameters (layout, "kick", "Kick");
        for (int layer = 0; layer < Pattern::numLayers; ++layer)
        {
            Params::addInstrumentCommonParams (layout, layer, 0.7f);
            Params::addInstrumentSynthParams (layout, layer);
            SourceEq::addParameters (layout, SourceEq::prefixFor (1 + layer), "Instr " + juce::String (layer + 1));
        }
        Effects::addParameters (layout);
        Sidechain::addParameters (layout);
        Modulation::addParameters (layout);
        MasterSection::addParameters (layout);
        Mixer::addParameters (layout);
        Params::addMasterParams (layout);
        Modulation::addExtraLfoParameters (layout); // LFO 4-10 (fas M): nya parametrar sist
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
                      .withOutput ("Instr 1", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("Instr 2", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("Instr 3", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("Instr 4", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("Instr 5", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("Instr 6", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("Instr 7", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("Instr 8", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("Instr 9", juce::AudioChannelSet::stereo(), false)
                      .withOutput ("Instr 10", juce::AudioChannelSet::stereo(), false),
                  createLayout(), false)
{
    lastInstrumentStartPpq.fill (-std::numeric_limits<double>::infinity());
    // Ett nytt instrument börjar som Init: tre lager som låter bra ihop (ett sparat projekt skriver över det).
    for (int layer = 0; layer < numLayers; ++layer)
        layerEqs.emplace_back (apvts, SourceEq::prefixFor (1 + layer));

    // Vem varje parameter hör till (för readModulatedParams): ett instrument (bassN...), kicken (kick...) eller inget av dem.
    for (auto* parameter : getParameters())
    {
        auto owner = ownerNone;
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
        {
            if (const auto layer = Presets::instrumentLayerOf (withId->paramID); layer >= 0)
                owner = layer;
            else if (withId->paramID.startsWith ("kick"))
                owner = ownerKick;
        }
        parameterOwner.push_back (owner);
    }
    loadFactoryPreset (Presets::Section::full, 0, 0);
}

void InstrumentProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    prepareBase (sampleRate);
    modulator.setSampleRate (sampleRate);
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
    kickEq.prepare (sampleRate);
    for (auto& eq : layerEqs)
        eq.prepare (sampleRate);
    mixer.prepare (sampleRate);
    setLatencySamples (master.getLatencySamples());
    ensureScratchSize (samplesPerBlock);

    patternActive = false;
    // Hållna toner hör till förra körningen: en "ton av" som aldrig kom (värden avbröt, offlinerenderns sista släpp ligger efter bufferten)
    // får inte hindra nästa "ton på" från att starta mönstret (wasSilent i handleMidi).
    heldNotes.fill (0);
    numHeldNotes = 0;
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
    processInControlChunks (buffer, midi, [this] (juce::AudioBuffer<float>& chunk, juce::MidiBuffer& chunkEvents) { processChunk (chunk, chunkEvents); });
}

void InstrumentProcessor::processChunk (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();

    const auto numSamples = buffer.getNumSamples();
    ensureScratchSize (numSamples);
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
    {
        int auditionRow = 0, auditionNote = 0, auditionVelocity = 0;
        bool auditionOn = false;
        while (popNoteAudition (auditionRow, auditionNote, auditionVelocity, auditionOn)) // enskilda toner från pianorullen
            handleLiveMidi (auditionOn ? juce::MidiMessage::noteOn (1, auditionNote, (juce::uint8) auditionVelocity) : juce::MidiMessage::noteOff (1, auditionNote), auditionRow);
        if (const auto allOffRows = takeAuditionAllOffRows(); allOffRows != 0)
            for (int row = 0; row < 16; ++row)
                if ((allOffRows >> row) & 1)
                    handleLiveMidi (juce::MidiMessage::allNotesOff (1), row);
    }
    clock.setForceFreeRun (previewLayer >= 0);
    clock.beginBlock (getHostPosition(), numSamples);
    for (int layer = 0; layer < numLayers; ++layer)
        basses[(size_t) layer].setTempo (clock.getBpm()); // ENV 2 och ENV 3 i läget Sync
    // Moduleringen uppdateras bara på 32-rutnätet. En bit som börjar mitt i ett fönster (när DAW-blocket slutade där) behåller
    // fönstrets värden, så att ljudet blir detsamma vid alla blockstorlekar, också udda.
    // AMP ENV / FILT ENV läses bara när någon koppling använder dem (annars 0, som inaktiva LFO:er).
    auto envSourcesUsed = false;
    for (int r = 0; controlPhase == 0 && r < Modulation::maxRoutes && ! envSourcesUsed; ++r)
        envSourcesUsed = Modulation::isEnvSource (modRoutes.routes[(size_t) r].source.load());
    for (int layer = 0; controlPhase == 0 && layer < numLayers; ++layer)
    {
        // Instrumentens källor (ENV 2, ENV 3, velocity, slump, AMP ENV, FILT ENV) som de var vid blockets början.
        if (layer < activeLayers)
            modulator.setInstrumentSources (layer, basses[(size_t) layer].getModulationValue (0), basses[(size_t) layer].getModulationValue (1),
                                            basses[(size_t) layer].getModulationValue (2), basses[(size_t) layer].getModulationValue (3),
                                            envSourcesUsed ? basses[(size_t) layer].getModulationValue (4) : 0.0f,
                                            envSourcesUsed ? basses[(size_t) layer].getModulationValue (5) : 0.0f);
        else
            modulator.setInstrumentSources (layer, 0.0f, 0.0f, 0.0f, 0.0f);
        modulator.setInstrumentNoteCount (layer, basses[(size_t) layer].getNoteCount());
    }
    if (controlPhase == 0)
        modulator.update (clock.getPpqAt (0), clock.getBpm());

    // Räknaren och mönstret börjar om vid 1.1 (egen klocka).
    if (restartRequest.exchange (false) && ! clock.isFollowingHost())
        clock.restartFreeRunAt (0);

    // Provlyssning från gränssnittet. En solo-källa (en oscillator eller samplingen) sätts före parametrarna läses, så att
    // de första samplesen redan är solo.
    const auto audition = auditionRequest.exchange (-1);
    if (audition >= 0 && numHeldNotes == 0)
        currentNote = previewNote.load(); // all provlyssning (kick, oscillatorer, sampling) spelas på den valda tonen
    if (audition == 5)
        kickSoloSamplesLeft = (int) (getSampleRate() * 2.0);
    // Provlyssning som hålls (håll ner musen på en graf): tonen klingar tills musen släpps och startar om om ljudet tar slut.
    const auto heldSolo = audition >= 500, heldWhole = audition >= 400 && audition < 500;
    constexpr auto heldSamples = std::numeric_limits<int>::max() / 2;
    if ((audition >= 300 && audition < 400) || heldSolo)
    {
        const auto base = heldSolo ? 500 : 300;
        soloLayer = juce::jlimit (0, numLayers - 1, (audition - base) / 10);
        soloSource = (audition - base) % 10;
        soloSamplesLeft = heldSolo ? heldSamples : (int) getSampleRate();
    }
    if (heldWhole)
        heldLayer = juce::jlimit (0, numLayers - 1, audition - 400);
    else if (heldSolo)
        heldLayer = soloLayer;
    else if (audition == PluginBase::auditionHoldStop && heldLayer >= 0)
    {
        basses[(size_t) heldLayer].release();
        heldLayer = -1;
        soloSamplesLeft = soloSamplesLeft > (int) getSampleRate() ? (int) getSampleRate() : soloSamplesLeft; // utklingningen hörs fortfarande bara som den källan
    }
    // Parametrarna läses bara på 32-rutnätet, så att ljudet inte beror på var DAW:en delar blocken. Ett nytt DAW-block (automation och
    // rattar kommer mellan blocken) ger en hel läsning vid nästa rutnätspunkt; däremellan läses bara det som moduleras. Medan en enskild
    // källa provlyssnas (räknas ner per bit) läses allt i varje bit.
    if (chunkOffset == 0)
        blockParamsPending = true;
    const auto soloing = soloSamplesLeft > 0 || kickSoloSamplesLeft > 0;
    if (soloing || paramsReadWhileSoloing || (controlPhase == 0 && blockParamsPending))
    {
        readBlockParams();
        blockParamsPending = false;
    }
    else if (controlPhase == 0 && modulator.isActive())
        readModulatedParams();
    paramsReadWhileSoloing = soloing;

    if (audition == 0 || audition == 5)
    {
        duckCurve.trigger();
        kick.trigger (1.0f);
    }
    else if (audition == PluginBase::previewStop)
    {
        stopPreview();
    }
    else if (heldSolo || heldWhole)
    {
        basses[(size_t) heldLayer].noteOn (0, 1.0f, heldSamples);
    }
    else if (audition >= 300)
    {
        basses[(size_t) soloLayer].noteOn (0, 1.0f, (int) getSampleRate());
    }
    else if (audition >= 200)
    {
        if (juce::isPositiveAndBelow (audition - 200, numLayers))
            basses[(size_t) (audition - 200)].noteOn (0, 1.0f, (int) getSampleRate());
    }
    else if (audition >= 100)
    {
        handlePreviewRequest (audition - 100);
    }
    // Hålls provlyssningen och ljudet har tagit slut (kort envelope, sampling utan LOOP): börja om, så att det loopar.
    if (heldLayer >= 0 && ! basses[(size_t) heldLayer].isActive())
        basses[(size_t) heldLayer].noteOn (0, 1.0f, heldSamples);

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
    const auto toleranceSamples = (int) (noteOffToleranceSeconds * getSampleRate());
    for (auto it = midi.begin(); it != midi.end(); ++it)
    {
        const auto metadata = *it;
        const auto eventPosition = juce::jlimit (0, numSamples, metadata.samplePosition);
        const auto message = metadata.getMessage();

        // Slutar tonen mönstret (Cut) för gott startas inga steg från strax före "ton av" och framåt (se noteOffToleranceSeconds), och ljud som ett steg startade i
        // en tidigare bit eller ett tidigare block tonas ut. Följer ett "ton på" direkt efter (nästa mönster i samma instans) fortsätter mönstret som vanligt.
        auto endsForGood = noteOffEndsPattern (message);
        for (auto later = std::next (it); endsForGood && later != midi.end() && (*later).samplePosition <= metadata.samplePosition + toleranceSamples; ++later)
            if ((*later).getMessage().isNoteOn())
                endsForGood = false;

        renderSegment (position, eventPosition, endsForGood ? eventPosition - toleranceSamples : std::numeric_limits<int>::max());
        if (endsForGood)
            fadeOutStepsBefore (eventPosition);
        handleMidi (message, eventPosition);
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
    master.delayOnly (bypassBus, numSamples, numLayers + 1);
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

    if (! allFinite (buffer, numSamples))
    {
        // Något gav NaN/inf: töm blocket och börja om med rena tillstånd, i stället för att vara trasig tills pluginet laddas om.
        buffer.clear();
        resetDspState();
        ++nonFiniteRecoveries;
    }

    transportStatus.publish (clock);
    transportStatus.patternBar = (int) (((clock.getBarIndex() % lengths.total) + lengths.total) % lengths.total) + 1;
    transportStatus.keyNote = currentNote;
    transportStatus.patternActive = patternActive;
    previewingLayer = patternActive ? previewLayer : -1;
    soloSamplesLeft = std::max (0, soloSamplesLeft - numSamples);
    kickSoloSamplesLeft = std::max (0, kickSoloSamplesLeft - numSamples);
    transportStatus.kickPlayTime = kick.getPlayTime();
    transportStatus.kickTriggers = kick.getTriggerCount();
    for (int layer = 0; layer < numLayers; ++layer)
    {
        transportStatus.instrumentSamplePosition[(size_t) layer] = layer < activeLayers ? basses[(size_t) layer].getSampleFraction() : -1.0f;
        transportStatus.instrumentSounding[(size_t) layer] = layer < activeLayers && basses[(size_t) layer].isActive();
    }
    transportStatus.kickStep = patternActive ? scheduler.getLastKickStep() : -1;
    for (int layer = 0; layer < numLayers; ++layer)
        transportStatus.instrumentSteps[(size_t) layer] = patternActive ? scheduler.getLastInstrumentStep (layer) : -1;
}

void InstrumentProcessor::readBlockParams()
{
    // (Mönsterlängderna, swing och humanize kan inte moduleras: de läses bara här, en gång per DAW-block.)
    lengths = Params::readPatternLengthsWith ([this] (const char* id) { return params.get (id); }, pattern, params.instruments());
    swing = params.get (ParamID::patSwing);
    humanize = params.get (ParamID::patHumanize);
    const auto solo = soloMask.load();
    kickOn = params.getBool (ParamID::kickOn) && (solo == 0 || (solo & kickSoloBit) != 0);

    // Instrument som tagits bort tystas direkt; deras mönster och inställningar finns kvar om de läggs till igen.
    const auto newActive = params.instruments();
    for (int layer = newActive; layer < activeLayers; ++layer)
        basses[(size_t) layer].reset();
    activeLayers = newActive;
    scheduler.setNumLayers (activeLayers);

    for (int layer = 0; layer < activeLayers; ++layer)
    {
        readLayerFields (layer, solo);
        basses[(size_t) layer].setNumVoices ((int) params.bass (layer, InstrumentField::voices));
    }
    noteEnd = (NoteEnd) params.getChoice (ParamID::noteEnd);
    kick.setClickSample (getActiveClickSample());
    refreshEngineParams();

    sidechainMode = (Sidechain::Mode) params.getChoice ("scMode");
    readDuckLength();
}

void InstrumentProcessor::readLayerFields (int layer, juce::uint64 solo)
{
    layerOn[(size_t) layer] = params.bass (layer, InstrumentField::on) >= 0.5f && (solo == 0 || (solo & (1ull << layer)) != 0);
    duckAmounts[(size_t) layer] = params.bass (layer, InstrumentField::duck); // med LFO-modulering
    gateScales[(size_t) layer] = params.bass (layer, InstrumentField::gate);
    delayMs[(size_t) layer] = params.bass (layer, InstrumentField::delay);
    nudgeMs[(size_t) layer] = params.bass (layer, InstrumentField::nudgeMs);
    swingRow[(size_t) layer] = params.bass (layer, InstrumentField::swing);
    humanizeRow[(size_t) layer] = params.bass (layer, InstrumentField::humanize);
    gateOn[(size_t) layer] = params.bass (layer, InstrumentField::gateOn) >= 0.5f;
    gateGrid[(size_t) layer] = juce::jlimit (0, Pattern::numGrids - 1, (int) params.bass (layer, InstrumentField::gateGrid));
    gateDepth[(size_t) layer] = params.bass (layer, InstrumentField::gateDepth);
    const auto smoothMs = std::max (0.5f, params.bass (layer, InstrumentField::gateSmooth));
    gateCoefficient[(size_t) layer] = (float) (1.0 - std::exp (-1.0 / (0.001 * smoothMs * getSampleRate())));
}

void InstrumentProcessor::readDuckLength()
{
    const auto lengthQuarters = Sidechain::lengthInQuarters (params.getChoice ("scLength"));
    duckLengthSamples = lengthQuarters > 0.0 ? lengthQuarters * clock.getSamplesPerQuarter()
                                             : KickEngine::lengthSeconds (readModulatedKick (0)) * getSampleRate();
}

void InstrumentProcessor::readModulatedParams()
{
    // Bara det som har en aktiv modulering just nu läses om (allt annat är oförändrat sedan blockets första bit): de instrument och
    // den kick som någon koppling går till. Samma värden som en hel läsning, men en bråkdel av arbetet var 32:a sample.
    std::array<bool, (size_t) numLayers> layerTouched {};
    auto kickTouched = false;
    for (const auto index : modulator.touchedParameters())
    {
        const auto owner = juce::isPositiveAndBelow (index, (int) parameterOwner.size()) ? parameterOwner[(size_t) index] : ownerNone;
        if (juce::isPositiveAndBelow (owner, numLayers))
            layerTouched[(size_t) owner] = true;
        else if (owner == ownerKick)
            kickTouched = true;
    }

    const auto solo = soloMask.load();
    for (int layer = 0; layer < activeLayers; ++layer)
    {
        if (! layerTouched[(size_t) layer])
            continue;
        readLayerFields (layer, solo);
        basses[(size_t) layer].setParams (readModulatedInstrument (layer, instrumentRootNote()));
    }
    if (kickTouched)
    {
        kick.setParams (readModulatedKick (keyPitchClass()));
        readDuckLength();
    }
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
    for (int layer = 0; layer < activeLayers; ++layer)
    {
        auto instrumentParams = readModulatedInstrument (layer, instrumentRootNote());
        if (soloSamplesLeft > 0 && layer == soloLayer)
        {
            // Lyssna på en enda källa: bara den är på, med full nivå (sub och brus tystas).
            for (int source = 0; source < 3; ++source)
            {
                auto& osc = source == 0 ? instrumentParams.oscA : source == 1 ? instrumentParams.oscB : instrumentParams.oscC;
                osc.on = source == soloSource;
                if (source == soloSource)
                    osc.level = 1.0f;
            }
            instrumentParams.sub = 0.0f;
            instrumentParams.noise = 0.0f;
            instrumentParams.sampleOn = soloSource == 3;
            if (soloSource == 3)
                instrumentParams.sampleLevel = 1.0f;
        }
        basses[(size_t) layer].setParams (instrumentParams);
    }
}

void InstrumentProcessor::handlePreviewRequest (int row)
{
    if (previewLayer >= 0)
        stopPreview(); // ett nytt tryck börjar om från början

    clock.setForceFreeRun (true);
    clock.restartFreeRunAt (0);
    currentNote = previewNote.load(); // tonarten för förhandsspelningen, som om tonen hölls
    refreshEngineParams();
    previewLayer = row;
    patternActive = true;
    stopAtPpq = std::numeric_limits<double>::infinity(); // loopar tills den stoppas

    previewFrom = juce::jmax (0, previewFromTick.load());
    previewBar0 = clock.getBarIndex();
    updatePreviewWindow();

    const auto tolerance = (int) (noteOnToleranceSeconds * getSampleRate());
    scheduler.collectJustPassed (clock, pattern, lengths, 0, tolerance);
    for (const auto& event : scheduler)
        handleEvent (event);
}

void InstrumentProcessor::updatePreviewWindow()
{
    StepScheduler::Window window;
    if (previewLayer >= 0 && previewLayer <= Pattern::kickLayer)
    {
        window.row = previewLayer;
        window.from = previewFrom;
        window.loopStart = previewLoopStart.load();
        window.loopEnd = previewLoopEnd.load();
        window.bar0 = previewBar0;
    }
    else if (previewLayer == PluginBase::previewAllRows && (previewLoopStart.load() >= 0 || previewFrom > 0))
    {
        window.allRows = true;
        window.from = previewFrom;
        window.loopStart = previewLoopStart.load();
        window.loopEnd = previewLoopEnd.load();
        window.bar0 = previewBar0;
    }
    scheduler.setWindow (window);
}

void InstrumentProcessor::stopPreview()
{
    previewLayer = -1;
    patternActive = false;
    stoppedAtPpq = clock.getPpqAt (0);
    for (auto& bass : basses)
        bass.release();
}

void InstrumentProcessor::fadeOutStepsBefore (int sampleOffset)
{
    // Steg som startade ljud inom noteOffToleranceSeconds före "ton av" (men i en tidigare bit eller ett tidigare block, så att steget inte kunde stoppas i förväg)
    // tonas ut på 3 ms. Steg i samma bit tas redan bort av stepLimit i renderSegment.
    const auto noteOffPpq = clock.getPpqAt (sampleOffset);
    const auto tolerancePpq = clock.getPpqAt ((int) (noteOffToleranceSeconds * getSampleRate())) - clock.getPpqAt (0);
    auto recent = [noteOffPpq, tolerancePpq] (double startPpq) { const auto distance = noteOffPpq - startPpq; return distance >= -1.0e-9 && distance <= tolerancePpq; };
    if (recent (lastKickStartPpq))
        kick.cancelCurrent (0.003);
    for (int layer = 0; layer < activeLayers; ++layer)
        if (recent (lastInstrumentStartPpq[(size_t) layer]))
            basses[(size_t) layer].fadeOutQuickly (3.0);
    forgetStepStarts();
}

void InstrumentProcessor::forgetStepStarts()
{
    lastKickStartPpq = -std::numeric_limits<double>::infinity();
    lastInstrumentStartPpq.fill (-std::numeric_limits<double>::infinity());
}

bool InstrumentProcessor::noteOffEndsPattern (const juce::MidiMessage& message) const
{
    if (! patternActive || noteEnd != NoteEnd::cut || numHeldNotes <= 0)
        return false;
    if (message.isNoteOff())
        return numHeldNotes == 1 && heldNotes[(size_t) message.getNoteNumber()] == 1; // sista tonen som hålls (FL:s "på före av" räknas av handleMidi)
    return message.isAllNotesOff() || message.isAllSoundOff();
}

bool InstrumentProcessor::handleLiveMidi (const juce::MidiMessage& message, int rowOverride)
{
    // MIDI IN: INSTRUMENT i pianorullen: tonerna spelas på radens ljud, utan att mönstret startar. Tonhöjd 0 är radens grundton (som i pianorullen).
    const auto row = rowOverride >= 0 ? rowOverride : midiInLayer.load();
    auto low = liveNotesLow.load(), high = liveNotesHigh.load();
    auto isSet = [&] (int note) { return (((note < 64 ? low : high) >> (note & 63)) & 1) != 0; };
    auto setBit = [&] (int note, bool on)
    {
        auto& word = note < 64 ? low : high;
        const auto bit = (uint64_t) 1 << (note & 63);
        word = on ? (word | bit) : (word & ~bit);
    };
    auto publish = [&]
    {
        liveNotesLow = low;
        liveNotesHigh = high;
        if (low == 0 && high == 0 && liveVoiceLayer >= 0)
        {
            basses[(size_t) liveVoiceLayer].release();
            liveVoiceLayer = -1;
        }
    };

    if (message.isNoteOn())
    {
        // Spelar värden startar värdens toner alltid mönstret; MIDI IN gäller bara när värden står still (pianorullens egen provlyssning, rowOverride, gäller alltid).
        if (rowOverride < 0 && clock.isHostPlaying())
            return false;
        if (row < 0 || row > Pattern::kickLayer)
            return false;
        if (rowOverride < 0)
            previewLayer = -1;
        const auto note = message.getNoteNumber();
        const auto pitch = juce::jlimit (Pattern::minPitch, Pattern::maxPitch, note - rowRootNote (row));
        if (row == Pattern::kickLayer)
        {
            if (kickOn)
                kick.trigger (message.getFloatVelocity(), pitch);
        }
        else if (row < numLayers && layerOn[(size_t) row])
        {
            liveVoiceLayer = row;
            basses[(size_t) row].noteOn (pitch, message.getFloatVelocity(), std::numeric_limits<int>::max() / 2);
        }
        setBit (note, true);
        publish();
        return true;
    }
    if (message.isNoteOff())
    {
        const auto note = message.getNoteNumber();
        if (! isSet (note))
            return false;
        setBit (note, false);
        publish();
        return true;
    }
    if ((message.isAllNotesOff() || message.isAllSoundOff()) && (low != 0 || high != 0))
    {
        low = high = 0;
        publish();
    }
    return false;
}

void InstrumentProcessor::handleMidi (const juce::MidiMessage& message, int sampleOffset)
{
    if (handleLiveMidi (message))
        return;

    // PREVIEW har företräde (Tobias): värdens toner avbryter inte förhandsspelningen och startar inget nytt mönster.
    // Stängs PREVIEW tar värdens toner över igen.
    if (previewLayer >= 0)
    {
        if (message.isNoteOff() && heldNotes[(size_t) message.getNoteNumber()] > 0
            && --heldNotes[(size_t) message.getNoteNumber()] == 0)
            --numHeldNotes; // en ton som hölls före förhandsspelningen; stoppar inte den
        return;
    }

    if (message.isNoteOn())
    {
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
    forgetStepStarts();
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
    forgetStepStarts();
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

int InstrumentProcessor::delaySamplesFor (const StepEvent& event)
{
    const auto samplesPerTick = clock.getSamplesPerQuarter() / Pattern::ticksPerQuarter;
    auto delay = 0.0;

    // Swing: de ojämna sextondelarna (andra halvan av varje åttondel) skjuts upp till som mest en tredjedels åttondel (swing 100 % = trioler).
    // Patternets swing och instrumentets egen läggs ihop (högst 100 %).
    const auto swingNow = std::min (1.0f, swing + swingRow[(size_t) event.layer]);
    if (swingNow > 0.0f && event.step % (Pattern::ticksPerQuarter / 2) >= Pattern::ticksPerQuarter / 4)
        delay += (double) swingNow * (Pattern::ticksPerQuarter / 6.0) * samplesPerTick;

    auto ms = (double) delayMs[(size_t) event.layer];
    const auto humanizeNow = std::min (1.0f, humanize + humanizeRow[(size_t) event.layer]);
    if (humanizeNow > 0.0f)
    {
        humanizeState ^= humanizeState << 13;
        humanizeState ^= humanizeState >> 17;
        humanizeState ^= humanizeState << 5;
        ms += (double) humanizeNow * 20.0 * ((double) (humanizeState >> 8) / (double) (1u << 24)); // 0-20 ms senare, slumpat
    }
    ms += (double) nudgeMs[(size_t) event.layer]; // fas PH: NUDGE +-2 ms; negativt äter av DELAY/swing/humanize (aldrig före notens egen tid: ingen latens)
    delay += ms * 0.001 * getSampleRate();
    delay = std::max (0.0, delay);
    return (int) delay;
}

void InstrumentProcessor::renderSegment (int startSample, int endSample, int stepLimit)
{
    if (endSample <= startSample)
        return;

    auto position = startSample;

    if (patternActive)
    {
        updatePreviewWindow(); // loopen kan ändras under spelning
        scheduler.collect (clock, pattern, lengths, startSample, std::max (startSample, std::min (endSample, stepLimit)), stopAtPpq);

        // Händelserna i tidsordning: det som väntat från tidigare segment och det som schemaläggaren gav (med swing och fördröjning).
        auto numDue = 0;
        auto addDue = [this, &numDue] (StepEvent event)
        {
            if (numDue >= (int) due.size())
                return;
            auto i = numDue++;
            // Tidsordning, och vid samma sample schemaläggningens ordning: en fördröjd händelse (swing, DELAY) som väntat från en tidigare
            // bit ska inte byta plats med en ny bara för att DAW:en delade blocken annorlunda.
            auto later = [] (const StepEvent& a, const StepEvent& b) { return a.offset > b.offset || (a.offset == b.offset && a.sequence > b.sequence); };
            while (i > 0 && later (due[(size_t) i - 1], event))
            {
                due[(size_t) i] = due[(size_t) i - 1];
                --i;
            }
            due[(size_t) i] = event;
        };

        const auto length = endSample - startSample;
        for (int i = 0; i < numPending;)
        {
            if (pending[(size_t) i].remaining < length)
            {
                auto event = pending[(size_t) i].event;
                event.offset = startSample + pending[(size_t) i].remaining;
                addDue (event);
                pending[(size_t) i] = pending[(size_t) --numPending];
            }
            else
            {
                pending[(size_t) i].remaining -= length;
                ++i;
            }
        }

        for (auto event : scheduler)
        {
            event.sequence = ++eventSequence;
            if (event.type == StepEvent::Type::bass)
                if (const auto delay = delaySamplesFor (event); delay > 0)
                {
                    const auto target = event.offset + delay;
                    if (target >= endSample)
                    {
                        if (numPending < (int) pending.size())
                            pending[(size_t) numPending++] = { event, target - endSample };
                        continue;
                    }
                    event.offset = target;
                }
            addDue (event);
        }

        for (int i = 0; i < numDue; ++i)
        {
            renderSamples (position, due[(size_t) i].offset);
            handleEvent (due[(size_t) i]);
            position = due[(size_t) i].offset;
        }
    }
    else
        numPending = 0;

    renderSamples (position, endSample);

    if (patternActive && clock.getPpqAt (endSample) >= stopAtPpq)
    {
        patternActive = false;
        previewLayer = -1; // förhandsspelningen har spelat ett varv
        stoppedAtPpq = stopAtPpq;
        numPending = 0;
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

    for (int layer = 0; layer < activeLayers; ++layer)
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
    kickEq.process (kickBuffer.getWritePointer (0), nullptr, numSamples);
    const auto* duckData = duckBuffer.getReadPointer (0);
    mixer.beginBlock();
    maxBus.clear (0, numSamples);
    maxBus.clear (1, numSamples);
    bypassBus.clear (0, numSamples);
    bypassBus.clear (1, numSamples);
    effects.beginBlock (numSamples, clock.getBpm(), clock.getPpqAt (0), controlPhase);
    tapBuffer.clear (0, numSamples);
    tapBuffer.clear (1, numSamples);

    for (int layer = 0; layer < activeLayers; ++layer)
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
        const auto gateStepsPerBar = Pattern::instrumentStepsPerBar ((InstrumentGrid) gateGrid[(size_t) layer]);
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
        layerEqs[(size_t) layer].process (data, dataRight, numSamples);
        if (instrumentTapsOn.load (std::memory_order_relaxed))
            for (int i = 0; i < numSamples; ++i)
                instrumentTaps[(size_t) layer].push (0.5f * (data[i] + dataRight[i]));

        // Kanal A-C: INSERT (standard) = amount av ljudet går genom kanalen, resten torrt. SEND = torrt orört, amount läggs till kanalen.
        const auto channel = (int) params.bass (layer, InstrumentField::fxChannel) - 1;
        const auto amountToChannel = params.bass (layer, InstrumentField::fxAmount);
        const auto routed = channel >= 0 && effects.isActive (channel);
        const auto sendMode = params.bass (layer, InstrumentField::fxSend) >= 0.5f;
        const auto dryGain = routed && ! sendMode ? 1.0f - amountToChannel : 1.0f;

        // Instrumentets fader och panorering gäller hela ljudet: både det torra och det som skickas till kanalen.
        mixer.mix (Mixer::firstBassSource + layer, data, dataRight, numSamples, dryGain, maxBus, bypassBus,
                   sendBus.getWritePointer (0), sendBus.getWritePointer (1));
        if (routed)
            effects.send (channel, sendBus.getReadPointer (0), sendBus.getReadPointer (1), numSamples, amountToChannel);
        for (int i = 0; i < numSamples; ++i)
            layerOuts[(size_t) layer].add (i, data[i], dataRight[i]);
    }

    // Kickens egen effektkanal: amount av kicken går genom kanalen, resten torrt.
    const auto kickChannel = (int) params.get (ParamID::kickFxChannel) - 1;
    const auto kickAmount = params.get (ParamID::kickFxAmount);
    const auto kickRouted = kickChannel >= 0 && effects.isActive (kickChannel);
    const auto kickSendMode = params.get (ParamID::kickFxSend) >= 0.5f;
    const auto kickDry = kickRouted && ! kickSendMode ? 1.0f - kickAmount : 1.0f;

    mixer.mix (Mixer::kickSource, kickBuffer.getReadPointer (0), nullptr, numSamples, kickDry, maxBus, bypassBus,
               sendBus.getWritePointer (0), sendBus.getWritePointer (1));
    if (kickRouted)
        effects.send (kickChannel, sendBus.getReadPointer (0), sendBus.getReadPointer (1), numSamples, kickAmount);

    std::array<bool, numEffectChannels> channelDuck {};
    for (int ch = 0; ch < numEffectChannels; ++ch)
        channelDuck[(size_t) ch] = mixer.duckEnabled (Mixer::firstFxSource + ch);
    // Det som SIDECHAIN-effekterna lyssnar på: kickens och instrumentens ljud (efter duckning och EQ, före effekter).
    SourceSignals signals;
    signals.left[0] = kickBuffer.getReadPointer (0);
    for (int layer = 0; layer < activeLayers; ++layer)
    {
        signals.left[(size_t) layer + 1] = layerBuffer.getReadPointer (layer);
        signals.right[(size_t) layer + 1] = layerBufferRight.getReadPointer (layer);
    }
    effects.setSources (signals);
    effects.process (numSamples, duckData, nullptr, channelDuck.data());
    for (int ch = 0; ch < numEffectChannels; ++ch)
    {
        if (effects.isIdle (ch))
            continue; // inget i kanalen: ingenting att mixa
        const auto& returned = effects.output (ch);
        mixer.mix (Mixer::firstFxSource + ch, returned.getReadPointer (0), returned.getReadPointer (1), numSamples, 1.0f, maxBus, bypassBus);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        kickTap.push (kickBuffer.getSample (0, i));
        instrumentBeforeDuckTap.push (tapBuffer.getSample (0, i));
        instrumentAfterDuckTap.push (tapBuffer.getSample (1, i));
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

void InstrumentProcessor::resetDspState()
{
    kick.reset();
    for (auto& bass : basses)
        bass.reset();
    for (auto& ducker : duckers)
        ducker.prepare (getSampleRate());
    effects.reset();
    master.reset();
    kickEq.reset();
    for (auto& eq : layerEqs)
        eq.reset();
    gateGain.fill (1.0f);
    numPending = 0;
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
    instrumentBuffer.setSize (2, numSamples, false, true, true);
    maxBus.setSize (2, numSamples, false, true, true);
    bypassBus.setSize (2, numSamples, false, true, true);
    sendBus.setSize (2, numSamples, false, true, true);
}

void InstrumentProcessor::handleEvent (const StepEvent& event)
{
    // Förhandsspelning av en enskild rad: de andra raderna är tysta. Samma sak för tangentbordet/MIDI när pianorullen valt en rad (midiInLayer).
    // Spelar värden gäller midiInLayer inte (värdens toner startar hela mönstret).
    const auto onlyRow = previewLayer >= 0 ? previewLayer : (clock.isHostPlaying() ? -1 : midiInLayer.load());
    if (onlyRow >= 0 && onlyRow != PluginBase::previewAllRows
        && (event.type == StepEvent::Type::kick ? onlyRow != Pattern::kickLayer : event.layer != onlyRow))
        return;

    if (event.type == StepEvent::Type::kick)
    {
        // Duckkurvan följer kickstegen även när kicken är avstängd (pump utan kick).
        duckCurve.trigger();
        if (kickOn)
        {
            const auto& note = pattern.note (Pattern::kickLayer, event.step);
            kick.trigger (note.velocity.load(), note.pitch.load());
            lastKickStartPpq = clock.getPpqAt (event.offset);
        }
        return;
    }

    const auto layer = event.layer;
    if (layer >= activeLayers || ! layerOn[(size_t) layer])
        return;

    const auto& note = pattern.note (layer, event.step);
    const auto samplesPerTick = clock.getSamplesPerQuarter() / Pattern::ticksPerQuarter;

    // En glide-ton slår inte an: den glider tonen som klingar till sin tonhöjd under sin egen längd. Klingar inget
    // spelas den som en vanlig ton.
    if (note.slide.load() && basses[(size_t) layer].glideTo (note.pitch.load(), std::max (1, (int) (note.length.load() * samplesPerTick))))
        return;

    const auto holdTicks = pattern.holdTicks (layer, event.step, lengths.total * Pattern::ticksPerBar, gateScales[(size_t) layer]);
    const auto gateSamples = std::max (1, (int) (holdTicks * samplesPerTick));

    // Fas P: ackordets extra toner (högst VOICES - 1) slås an först och grundtonen sist, så att grundtonen blir den senast spelade
    // rösten (glide, modulering) och den som stjäls sist. Med VOICES = 1, och för en glide-ton som glider, spelas bara grundtonen.
    auto& voices = basses[(size_t) layer];
    const auto chordNotes = std::min (voices.getNumVoices() - 1, Pattern::maxChordNotes);
    for (int i = 0; i < chordNotes; ++i)
        if (const auto interval = note.chord[(size_t) i].load(); interval != 0)
            voices.noteOn (juce::jlimit (Pattern::minPitch, Pattern::maxPitch, note.pitch.load() + interval), note.velocity.load(), gateSamples, true);
    // Polyfoni: staplade toner på samma tick, före grundtonen. En glide-stapel glider den klingande rösten (ingen ny röst).
    for (const auto& e : note.extra)
    {
        if (! e.on.load())
            continue;
        const auto length = e.length.load();
        if (e.slide.load() && voices.glideTo (e.pitch.load(), std::max (1, (int) (length * samplesPerTick))))
            continue;
        const auto samples = std::max (1, (int) (std::max (1.0, length * (double) gateScales[(size_t) layer]) * samplesPerTick));
        voices.noteOn (e.pitch.load(), e.velocity.load(), samples, true);
    }
    voices.noteOn (note.pitch.load(), note.velocity.load(), gateSamples);
    lastInstrumentStartPpq[(size_t) layer] = clock.getPpqAt (event.offset);
}

juce::AudioProcessorEditor* InstrumentProcessor::createEditor()
{
    return new InstrumentEditor (*this);
}
