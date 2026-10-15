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
        Params::addBassCommonParams (layout, 0.7f);
        Params::addBassSynthParams (layout);
        Effects::addParameters (layout);
        Sidechain::addParameters (layout);
        Modulation::addParameters (layout);
        MasterSection::addParameters (layout);
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
                      .withOutput ("Bass", juce::AudioChannelSet::stereo(), false),
                  createLayout(), false)
{
}

void InstrumentProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    prepareBase (sampleRate);
    kick.prepare (sampleRate);
    bass.prepare (sampleRate);
    ducker.prepare (sampleRate);
    effects.prepare (sampleRate, samplesPerBlock);
    effects.setModulator (&modulator);
    master.prepare (sampleRate, 2);
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

    // Kick- och basutgångarna är frivilliga.
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
    assign (bassOut, 2);

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
    for (int bus = 1; bus <= 2; ++bus)
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
    transportStatus.bassStep = patternActive ? scheduler.getLastBassStep() : -1;
}

void InstrumentProcessor::readBlockParams()
{
    patternBars = Params::barsFromChoice (params.getChoice (ParamID::patternBars));
    scheduler.setGrid ((BassGrid) params.getChoice (ParamID::bassGrid));
    kickOn = params.getBool (ParamID::kickOn);
    bassOn = params.getBool (ParamID::bassOn);
    duckAmount = modulator.apply (Modulation::Destination::duckAmount, params.get (ParamID::duckAmount));
    bassGateScale = params.get (ParamID::bassGate);
    noteEnd = (NoteEnd) params.getChoice (ParamID::noteEnd);
    kick.setClickSample (getActiveClickSample());
    refreshEngineParams();

    sidechainMode = (Sidechain::Mode) params.getChoice ("scMode");
    sidechainPosition = (Sidechain::Position) params.getChoice ("scPosition");
    const auto lengthQuarters = Sidechain::lengthInQuarters (params.getChoice ("scLength"));
    duckLengthSamples = lengthQuarters > 0.0 ? lengthQuarters * clock.getSamplesPerQuarter()
                                             : KickEngine::lengthSeconds (params.readKick (0)) * getSampleRate();
}

void InstrumentProcessor::refreshEngineParams()
{
    kick.setParams (readModulatedKick (keyPitchClass()));
    bass.setParams (readModulatedBass (bassRootNote()));
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
    auto* sourceData = bassBuffer.getWritePointer (0);
    auto* amplitudeData = amplitudeBuffer.getWritePointer (0);
    const auto useCurve = sidechainMode == Sidechain::Mode::curve;

    for (int i = startSample; i < endSample; ++i)
    {
        kickData[i] = kick.nextSample();
        const auto curveValue = duckCurve.next (sidechainCurve, duckLengthSamples);
        duckData[i] = useCurve ? curveValue : kick.getEnvelope();

        const auto b = bass.next();
        sourceData[i] = b.source;
        amplitudeData[i] = b.amplitude;
    }
}

void InstrumentProcessor::finishBlock (int numSamples)
{
    // Basen: källa -> effekter före gaten -> gate (amplitud) -> effekter efter, med
    // ducking antingen före eller efter effekterna efter gaten.
    bassBuffer.copyFrom (1, 0, bassBuffer, 0, 0, numSamples);

    // Ducknivå -> förstärkning, sample för sample.
    auto* duckData = duckBuffer.getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
        duckData[i] = ducker.next (duckData[i], duckAmount);

    effects.update (clock.getBpm());
    effects.process (bassBuffer, numSamples, false, effectOrder);

    for (int ch = 0; ch < 2; ++ch)
        juce::FloatVectorOperations::multiply (bassBuffer.getWritePointer (ch), amplitudeBuffer.getReadPointer (0), numSamples);

    const auto duckBeforeEffects = sidechainPosition == Sidechain::Position::beforeEffects;
    if (duckBeforeEffects)
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::multiply (bassBuffer.getWritePointer (ch), duckData, numSamples);

    effects.process (bassBuffer, numSamples, true, effectOrder);

    // Efter effekterna duckas även ekon och reverbsvansar under kicken.
    if (! duckBeforeEffects)
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::multiply (bassBuffer.getWritePointer (ch), duckData, numSamples);

    const auto* kickData = kickBuffer.getReadPointer (0);
    const auto* left = bassBuffer.getReadPointer (0);
    const auto* right = bassBuffer.getReadPointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        mainOut.add (i, kickData[i] + left[i], kickData[i] + right[i]);
        kickOut.add (i, kickData[i], kickData[i]);
        bassOut.add (i, left[i], right[i]);
    }
}

void InstrumentProcessor::ensureScratchSize (int numSamples)
{
    // Allokerar bara om DAW:en skickar ett större block än den lovade i prepareToPlay.
    if (kickBuffer.getNumSamples() >= numSamples)
        return;

    kickBuffer.setSize (1, numSamples, false, true, true);
    duckBuffer.setSize (1, numSamples, false, true, true);
    amplitudeBuffer.setSize (1, numSamples, false, true, true);
    bassBuffer.setSize (2, numSamples, false, true, true);
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

    if (! bassOn)
        return;

    const auto grid = scheduler.getGrid();
    const auto& step = pattern.bassStep (grid, event.step);
    const auto samplesPerStep = clock.getSamplesPerQuarter() / Pattern::bassStepsPerQuarter (grid);
    const auto slide = step.slide.load();

    const auto gateSamples = slide ? (int) (samplesPerStep * slideOverlap)
                                   : std::max (1, (int) (step.gate.load() * bassGateScale * samplesPerStep));

    bass.noteOn (step.pitch.load(), step.velocity.load(), gateSamples, slide);
}

juce::AudioProcessorEditor* InstrumentProcessor::createEditor()
{
    return new InstrumentEditor (*this);
}
