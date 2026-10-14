#include "InstrumentProcessor.h"
#include "InstrumentEditor.h"

namespace
{
    constexpr auto gainID = "gain";

    // En ton som börjar så här mycket efter ett taktslag räknas som "på slaget".
    constexpr double noteOnToleranceSeconds = 0.003;

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { gainID, 1 }, "Gain", 0.0f, 1.0f, 0.5f));
        return layout;
    }
}

InstrumentProcessor::InstrumentProcessor()
    : PluginBase (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true), createLayout())
{
}

void InstrumentProcessor::prepareToPlay (double sampleRate, int)
{
    clock.prepare (sampleRate);
    click.prepare (sampleRate);

    gain.reset (sampleRate, 0.02);
    gain.setCurrentAndTargetValue (getParam (gainID));
}

bool InstrumentProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void InstrumentProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const auto numSamples = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
    clock.beginBlock (getHostPosition(), numSamples);

    // Dela upp blocket vid varje MIDI-händelse så att toner börjar och slutar på rätt sample.
    int position = 0;
    for (const auto metadata : midi)
    {
        const auto eventPosition = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderSegment (buffer, position, eventPosition);
        handleMidi (metadata.getMessage(), eventPosition);
        position = eventPosition;
    }
    renderSegment (buffer, position, numSamples);

    gain.setTargetValue (getParam (gainID));
    gain.applyGain (buffer, numSamples);

    transportStatus.publish (clock);
    transportStatus.heldNote = numHeldNotes > 0 ? currentNote : -1;
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

        if (wasSilent)
        {
            // Utan DAW-transport börjar den egna klockan på första tonen.
            clock.restartFreeRunAt (sampleOffset);

            // Med DAW-transport: kom tonen en aning efter slaget? Spela slaget ändå.
            const auto tolerance = (int) (noteOnToleranceSeconds * getSampleRate());
            if (const auto step = clock.takeStepJustPassed (quarterCursor, sampleOffset, tolerance))
                triggerClick (*step);
        }
    }
    else if (message.isNoteOff())
    {
        const auto note = message.getNoteNumber();
        if (heldNotes[(size_t) note])
        {
            heldNotes[(size_t) note] = false;
            --numHeldNotes;
        }
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        heldNotes.fill (false);
        numHeldNotes = 0;
    }
}

void InstrumentProcessor::renderSegment (juce::AudioBuffer<float>& buffer, int startSample, int endSample)
{
    auto renderedUpTo = startSample;

    if (numHeldNotes > 0)
    {
        clock.forEachStep (quarterCursor, startSample, endSample, [&] (int offset, int stepInBar)
        {
            renderClick (buffer, renderedUpTo, offset);
            triggerClick (stepInBar);
            renderedUpTo = offset;
        });
    }

    // Klicket klingar ut även efter att tonen släppts.
    renderClick (buffer, renderedUpTo, endSample);
}

void InstrumentProcessor::renderClick (juce::AudioBuffer<float>& buffer, int startSample, int endSample)
{
    for (int i = startSample; i < endSample; ++i)
    {
        const auto sample = click.nextSample();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, sample);
    }
}

void InstrumentProcessor::triggerClick (int stepInBar)
{
    // Tonens tonklass i oktav 5 (C5 = 523 Hz), en oktav upp och starkare på ettan.
    const auto isDownbeat = stepInBar == 0;
    const auto note = 72 + currentNote % 12 + (isDownbeat ? 12 : 0);
    click.trigger (juce::MidiMessage::getMidiNoteInHertz (note), isDownbeat ? 1.0f : 0.6f);
}

juce::AudioProcessorEditor* InstrumentProcessor::createEditor()
{
    return new InstrumentEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InstrumentProcessor();
}
