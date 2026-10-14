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
                  createLayout())
{
}

void InstrumentProcessor::prepareToPlay (double sampleRate, int)
{
    prepareBase (sampleRate);
    kick.prepare (sampleRate);
    bass.prepare (sampleRate);

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
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
    clock.beginBlock (getHostPosition(), numSamples);
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

    masterGain.setTargetValue (params.get (ParamID::gain));
    masterGain.applyGain (buffer, numSamples);

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
    duckAmount = params.get (ParamID::duckAmount);
    bassGateScale = params.get (ParamID::bassGate);
    noteEnd = (NoteEnd) params.getChoice (ParamID::noteEnd);
    kick.setClickSample (getActiveClickSample());
    refreshEngineParams();
}

void InstrumentProcessor::refreshEngineParams()
{
    kick.setParams (params.readKick (keyPitchClass()));
    bass.setParams (params.readBass (keyPitchClass()));
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
    // Utan DAW-transport börjar den egna klockan på första tonen. Är mönstret fortfarande
    // igång (det spelar klart efter förra tonen) fortsätter det i samma takt.
    if (! patternActive)
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
        patternActive = false;
}

void InstrumentProcessor::renderSamples (int startSample, int endSample)
{
    for (int i = startSample; i < endSample; ++i)
    {
        const auto kickSample = kick.nextSample();

        // Basen trycks undan exakt så mycket som kicken låter just nu.
        const auto bassSample = bass.nextSample() * (1.0f - duckAmount * kick.getEnvelope());

        mainOut.add (i, kickSample + bassSample);
        kickOut.add (i, kickSample);
        bassOut.add (i, bassSample);
    }
}

void InstrumentProcessor::handleEvent (const StepEvent& event)
{
    if (event.type == StepEvent::Type::kick)
    {
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
