#include "RollFiles.h"
#include "Settings.h"
#include "Params.h"
#include "Presets.h"

namespace RollFiles
{
    juce::File folder()
    {
        return Settings::presetsFolder().getChildFile ("Piano Roll");
    }

    juce::Array<juce::File> list()
    {
        auto files = folder().findChildFiles (juce::File::findFiles, false, "*.roll");
        files.sort();
        return files;
    }

    // Närmaste längd i listan (1, 2, 4 ... 64 takter) som rymmer `bars`.
    int barsToChoice (int bars)
    {
        auto choice = 0;
        while ((1 << choice) < bars && choice < 6)
            ++choice;
        return choice;
    }

    void applyLength (juce::AudioProcessorValueTreeState& apvts, int layer, int bars)
    {
        const auto total = std::max (Params::readPatternLengths (apvts).total, bars);
        auto set = [&apvts] (const char* id, int choice)
        {
            if (auto* parameter = apvts.getParameter (id))
                Presets::setParameter (*parameter, parameter->convertTo0to1 ((float) choice));
        };
        set (ParamID::patternBars, barsToChoice (total));
        set (ParamID::rowBars (layer == Pattern::kickLayer ? 0 : 1 + layer), bars >= total ? 0 : barsToChoice (bars) + 1);
    }

    bool save (const Pattern& pattern, int layer, int bars, BassGrid grid, const juce::File& file)
    {
        juce::XmlElement root ("KickbaseRoll");
        root.setAttribute ("version", 1);
        root.setAttribute ("bars", bars);
        root.setAttribute ("grid", Pattern::gridNames()[(int) grid]);

        for (int tick = 0; tick < Pattern::maxTicks; ++tick)
        {
            const auto& n = pattern.note (layer, tick);
            if (! n.on.load())
                continue;
            auto* note = root.createNewChildElement ("Note");
            note->setAttribute ("tick", tick);
            note->setAttribute ("length", n.length.load());
            note->setAttribute ("pitch", n.pitch.load());
            note->setAttribute ("velocity", (double) n.velocity.load());
            note->setAttribute ("glide", n.slide.load());
        }

        file.getParentDirectory().createDirectory();
        return root.writeTo (file);
    }

    bool load (Pattern& pattern, int layer, const juce::File& file, int& bars, BassGrid& grid)
    {
        const auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("KickbaseRoll"))
            return false;

        pattern.clearNotes (layer);
        for (const auto* note : xml->getChildIterator())
        {
            const auto tick = note->getIntAttribute ("tick", -1);
            if (note->hasTagName ("Note") && juce::isPositiveAndBelow (tick, Pattern::maxTicks))
                pattern.addNote (layer, tick, note->getIntAttribute ("length", 24), note->getIntAttribute ("pitch"),
                                 (float) note->getDoubleAttribute ("velocity", 0.8), note->getBoolAttribute ("glide"));
        }
        pattern.setCustom (layer, true);

        bars = 1 << barsToChoice (juce::jlimit (1, Pattern::maxBars, xml->getIntAttribute ("bars", 1)));
        const auto gridIndex = Pattern::gridNames().indexOf (xml->getStringAttribute ("grid"), true);
        grid = gridIndex >= 0 ? (BassGrid) gridIndex : BassGrid::sixteenth;
        return true;
    }

    bool exportMidi (const Pattern& pattern, int layer, int bars, double bpm, int rootMidiNote, const juce::File& file)
    {
        juce::MidiMessageSequence track;
        auto tempo = juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / std::max (20.0, bpm)));
        tempo.setTimeStamp (0.0);
        track.addEvent (tempo);
        auto signature = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
        signature.setTimeStamp (0.0);
        track.addEvent (signature);

        for (int tick = 0; tick < Pattern::maxTicks; ++tick)
        {
            const auto& n = pattern.note (layer, tick);
            if (! n.on.load())
                continue;

            const auto midiNote = juce::jlimit (0, 127, rootMidiNote + n.pitch.load());
            const auto velocity = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (n.velocity.load() * 127.0f));
            track.addEvent (juce::MidiMessage::noteOn (1, midiNote, velocity), (double) tick);
            track.addEvent (juce::MidiMessage::noteOff (1, midiNote), (double) (tick + std::max (1, n.length.load())));
        }

        auto end = juce::MidiMessage::endOfTrack();
        end.setTimeStamp ((double) (bars * Pattern::ticksPerBar));
        track.addEvent (end);
        track.updateMatchedPairs();

        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (Pattern::ticksPerQuarter);
        midi.addTrack (track);

        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream stream (file);
        return stream.openedOk() && midi.writeTo (stream);
    }

    bool importMidi (Pattern& pattern, int layer, const juce::File& file, int rootMidiNote, int& bars)
    {
        juce::FileInputStream stream (file);
        juce::MidiFile midi;
        if (! stream.openedOk() || ! midi.readFrom (stream))
            return false;

        const auto timeFormat = midi.getTimeFormat();
        if (timeFormat <= 0)
            return false; // SMPTE-tid stöds inte

        juce::MidiMessageSequence merged;
        for (int t = 0; t < midi.getNumTracks(); ++t)
            merged.addSequence (*midi.getTrack (t), 0.0);
        merged.updateMatchedPairs();

        struct Found { int tick, length, note; float velocity; };
        std::vector<Found> found;
        auto lowest = 127, highest = 0;
        for (int i = 0; i < merged.getNumEvents(); ++i)
        {
            const auto* event = merged.getEventPointer (i);
            if (! event->message.isNoteOn())
                continue;
            const auto start = event->message.getTimeStamp();
            const auto end = event->noteOffObject != nullptr ? event->noteOffObject->message.getTimeStamp() : start + timeFormat / 4.0;
            const auto tick = (int) std::round (start * Pattern::ticksPerQuarter / timeFormat);
            if (tick >= Pattern::maxTicks)
                continue;
            const auto length = std::max (1, (int) std::round ((end - start) * Pattern::ticksPerQuarter / timeFormat));
            found.push_back ({ tick, length, event->message.getNoteNumber(), event->message.getFloatVelocity() });
            lowest = std::min (lowest, event->message.getNoteNumber());
            highest = std::max (highest, event->message.getNoteNumber());
        }
        if (found.empty())
            return false;

        // Ryms inte tonerna inom ±96 halvtoner från grundtonen flyttas de en oktav i taget.
        auto shift = 0;
        while (lowest + shift - rootMidiNote < Pattern::minPitch)
            shift += 12;
        while (highest + shift - rootMidiNote > Pattern::maxPitch)
            shift -= 12;

        pattern.clearNotes (layer);
        auto lastEnd = 0;
        for (const auto& f : found)
        {
            if (pattern.note (layer, f.tick).on.load())
                continue; // högst en ton per tick
            pattern.addNote (layer, f.tick, std::min (f.length, Pattern::maxTicks - f.tick),
                             juce::jlimit (Pattern::minPitch, Pattern::maxPitch, f.note + shift - rootMidiNote), f.velocity);
            lastEnd = std::max (lastEnd, f.tick + f.length);
        }
        pattern.setCustom (layer, true);

        const auto needed = (lastEnd + Pattern::ticksPerBar - 1) / Pattern::ticksPerBar;
        bars = 1 << barsToChoice (needed);
        return true;
    }
}
