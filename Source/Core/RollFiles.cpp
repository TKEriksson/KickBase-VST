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

    bool save (const Pattern& pattern, int layer, InstrumentGrid grid, const juce::File& file)
    {
        juce::XmlElement root ("KickbaseRoll");
        root.setAttribute ("version", 4); // 4: valfria lock0-lock2 på Note; 3: staplade toner; 2: längd = auto eller valt antal takter
        root.setAttribute ("bars", pattern.lengthBars (layer));
        root.setAttribute ("length", pattern.isLengthAuto (layer) ? juce::String ("auto") : juce::String (pattern.manualBars (layer)));
        root.setAttribute ("grid", Pattern::gridNames()[(int) grid]);

        const auto writeNote = [&root] (int tick, int index, int length, int pitch, float velocity, bool slide, const Pattern::Chord& chord)
        {
            auto* note = root.createNewChildElement ("Note");
            note->setAttribute ("tick", tick);
            note->setAttribute ("index", index);
            note->setAttribute ("length", length);
            note->setAttribute ("pitch", pitch);
            note->setAttribute ("velocity", (double) velocity);
            note->setAttribute ("glide", slide);
            for (size_t i = 0; i < chord.size(); ++i)
                if (chord[i] != 0)
                    note->setAttribute ("c" + juce::String ((int) i + 1), chord[i]); // fas P: bara toner med ackord får c1-c3
        };
        pattern.forEachNote (layer, [&] (int tick, const Pattern::Note& n)
        {
            writeNote (tick, 0, n.length.load(), n.pitch.load(), n.velocity.load(), n.slide.load(), n.getChord());
            for (int p = 0; p < Pattern::numLocks; ++p) // parameterlås på huvudtonen (rörs som sista attribut på senast skrivna Note)
                if (n.hasLock (p))
                    root.getChildElement (root.getNumChildElements() - 1)->setAttribute ("lock" + juce::String (p), (double) n.lock[(size_t) p].load());
            for (int index = 1; index < Pattern::maxStack; ++index)
                if (Pattern::StackNote stacked; pattern.getStack (layer, tick, index, stacked))
                    writeNote (tick, index, stacked.length, stacked.pitch, stacked.velocity, stacked.slide, {});
        });

        file.getParentDirectory().createDirectory();
        return root.writeTo (file);
    }

    bool load (Pattern& pattern, int layer, const juce::File& file, InstrumentGrid& grid)
    {
        const auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("KickbaseRoll"))
            return false;

        pattern.clearNotes (layer);
        for (const auto* note : xml->getChildIterator())
        {
            const auto tick = note->getIntAttribute ("tick", -1);
            if (! note->hasTagName ("Note") || ! juce::isPositiveAndBelow (tick, Pattern::maxTicks))
                continue;
            const auto length = note->getIntAttribute ("length", 24);
            const auto pitch = note->getIntAttribute ("pitch");
            const auto velocity = (float) note->getDoubleAttribute ("velocity", 0.8);
            const auto slide = note->getBoolAttribute ("glide");
            if (! pattern.note (layer, tick).on.load())
            {
                pattern.addNote (layer, tick, length, pitch, velocity, slide)
                    .setChord ({ note->getIntAttribute ("c1"), note->getIntAttribute ("c2"), note->getIntAttribute ("c3") });
                for (int p = 0; p < Pattern::numLocks; ++p)
                    if (note->hasAttribute ("lock" + juce::String (p)))
                        pattern.setLock (layer, tick, p, (float) note->getDoubleAttribute ("lock" + juce::String (p)));
            }
            else
            {
                Pattern::StackNote stacked { length, pitch, velocity, slide };
                pattern.addExtra (layer, tick, stacked);
            }
        }
        pattern.setCustom (layer, true);

        // Längden: version 2 sparar auto eller ett antal; en äldre fil sparade bara antalet (en längre rulle än tonerna: valt antal).
        pattern.refreshLengths();
        const auto lengthText = xml->getStringAttribute ("length");
        const auto savedBars = juce::jlimit (1, Pattern::maxBars, xml->getIntAttribute ("bars", 1));
        if (lengthText.isNotEmpty() && lengthText.containsOnly ("0123456789"))
            pattern.setManualBars (layer, lengthText.getIntValue());
        else if (lengthText.isEmpty() && savedBars > pattern.contentBars (layer))
            pattern.setManualBars (layer, savedBars);
        else
            pattern.setManualBars (layer, 0);
        const auto gridIndex = Pattern::gridNames().indexOf (xml->getStringAttribute ("grid"), true);
        grid = gridIndex >= 0 ? (InstrumentGrid) gridIndex : InstrumentGrid::sixteenth;
        return true;
    }

    // Ett spår med radens toner. Tomt namn = inget spårnamn; tempo och taktart skrivs bara på det första spåret (formatet är MIDI typ 1).
    static juce::MidiMessageSequence buildTrack (const Pattern& pattern, int layer, int bars, double bpm, int rootMidiNote, const juce::String& name)
    {
        juce::MidiMessageSequence track;
        if (name.isNotEmpty())
        {
            auto trackName = juce::MidiMessage::textMetaEvent (3, name); // meta 3 = spårnamn
            trackName.setTimeStamp (0.0);
            track.addEvent (trackName);
        }
        else
        {
            auto tempo = juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / std::max (20.0, bpm)));
            tempo.setTimeStamp (0.0);
            track.addEvent (tempo);
            auto signature = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
            signature.setTimeStamp (0.0);
            track.addEvent (signature);
        }

        const auto addMidiNote = [&track, rootMidiNote] (int tick, int length, int pitch, float noteVelocity)
        {
            const auto midiNote = juce::jlimit (0, 127, rootMidiNote + pitch);
            const auto velocity = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (noteVelocity * 127.0f));
            const auto end = (double) (tick + std::max (1, length));
            track.addEvent (juce::MidiMessage::noteOn (1, midiNote, velocity), (double) tick);
            track.addEvent (juce::MidiMessage::noteOff (1, midiNote), end);
        };
        pattern.forEachNote (layer, [&] (int tick, const Pattern::Note& n)
        {
            addMidiNote (tick, n.length.load(), n.pitch.load(), n.velocity.load());
            const auto midiNote = juce::jlimit (0, 127, rootMidiNote + n.pitch.load());
            const auto velocity = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (n.velocity.load() * 127.0f));
            const auto end = (double) (tick + std::max (1, n.length.load()));
            std::set<int> used { midiNote }; // samma nota två gånger på samma tick ger överlappande on/off
            for (const auto interval : n.getChord()) // fas P: ackordets toner blir egna MIDI-toner
            {
                const auto chordNote = juce::jlimit (0, 127, midiNote + interval);
                if (interval == 0 || ! used.insert (chordNote).second)
                    continue;
                track.addEvent (juce::MidiMessage::noteOn (1, chordNote, velocity), (double) tick);
                track.addEvent (juce::MidiMessage::noteOff (1, chordNote), end);
            }
            for (int index = 1; index < Pattern::maxStack; ++index)
                if (Pattern::StackNote stacked; pattern.getStack (layer, tick, index, stacked)
                                                && used.insert (juce::jlimit (0, 127, rootMidiNote + stacked.pitch)).second)
                    addMidiNote (tick, stacked.length, stacked.pitch, stacked.velocity);
        });

        auto end = juce::MidiMessage::endOfTrack();
        end.setTimeStamp ((double) (bars * Pattern::ticksPerBar));
        track.addEvent (end);
        track.updateMatchedPairs();
        return track;
    }

    int countSlideNotes (const Pattern& pattern, int layer)
    {
        auto count = 0;
        pattern.forEachNote (layer, [&count] (int, const Pattern::Note& n) { count += n.slide.load() ? 1 : 0; });
        return count;
    }

    bool exportMidi (const Pattern& pattern, int layer, int bars, double bpm, int rootMidiNote, const juce::File& file)
    {
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (Pattern::ticksPerQuarter);
        midi.addTrack (buildTrack (pattern, layer, bars, bpm, rootMidiNote, {}));

        // R9b: varje aktiv underrad med toner till instrumentet blir ett eget namngivet spår ("Bass 3 - SAMPLE"). Tysta underrader (inga toner) hoppas över.
        if (juce::isPositiveAndBelow (layer, RowRegistry::numInstruments))
            for (int id = RowRegistry::firstSubRow; id < Pattern::numRowSlots; ++id)
            {
                int instrument = 0;
                RowRegistry::Kind kind;
                if (! pattern.rows.get (id, instrument, kind) || instrument != layer)
                    continue;
                auto notes = 0;
                pattern.forEachNote (id, [&notes] (int, const Pattern::Note&) { ++notes; });
                if (notes == 0)
                    continue;
                const auto name = "Bass " + juce::String (layer + 1) + " - " + (kind == RowRegistry::Kind::slice ? "Slice " + juce::String (pattern.rows.sliceOf (id) + 1) : juce::String ("SAMPLE"));
                midi.addTrack (buildTrack (pattern, id, std::max (bars, pattern.lengthBars (id)), bpm, rootMidiNote, name));
            }

        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream stream (file);
        return stream.openedOk() && midi.writeTo (stream);
    }

    bool importMidi (Pattern& pattern, int layer, const juce::File& file, int rootMidiNote, int* skippedNotes)
    {
        auto skipped = 0;
        if (skippedNotes != nullptr)
            *skippedNotes = 0;

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
            {
                ++skipped;
                continue;
            }
            const auto length = std::max (1, (int) std::round ((end - start) * Pattern::ticksPerQuarter / timeFormat));
            found.push_back ({ tick, length, event->message.getNoteNumber(), event->message.getFloatVelocity() });
            lowest = std::min (lowest, event->message.getNoteNumber());
            highest = std::max (highest, event->message.getNoteNumber());
        }
        if (found.empty())
        {
            if (skippedNotes != nullptr)
                *skippedNotes = skipped;
            return false;
        }

        // Ryms inte tonerna inom ±96 halvtoner från grundtonen flyttas de en oktav i taget.
        auto shift = 0;
        while (lowest + shift - rootMidiNote < Pattern::minPitch)
            shift += 12;
        while (highest + shift - rootMidiNote > Pattern::maxPitch)
            shift -= 12;

        // MIDI-toner med samma tick blir separata staplade noter, inte ackord.
        std::stable_sort (found.begin(), found.end(), [] (const Found& a, const Found& b) { return a.tick != b.tick ? a.tick < b.tick : a.note < b.note; });
        pattern.clearNotes (layer);
        for (const auto& f : found)
        {
            const auto pitch = juce::jlimit (Pattern::minPitch, Pattern::maxPitch, f.note + shift - rootMidiNote);
            if (! pattern.note (layer, f.tick).on.load())
            {
                pattern.addNote (layer, f.tick, std::min (f.length, Pattern::maxTicks - f.tick), pitch, f.velocity);
                continue;
            }
            // Samma tonhöjd på samma tick är samma ton: dubbletten hoppas över (räknas inte som tappad).
            auto duplicate = pattern.note (layer, f.tick).pitch.load() == pitch;
            for (int i = 1; ! duplicate && i < Pattern::maxStack; ++i)
                if (Pattern::StackNote other; pattern.getStack (layer, f.tick, i, other) && other.pitch == pitch)
                    duplicate = true;
            if (duplicate)
                continue;
            Pattern::StackNote stacked { std::min (f.length, Pattern::maxTicks - f.tick), pitch, f.velocity, false };
            if (pattern.addExtra (layer, f.tick, stacked) < 0)
                ++skipped;
        }
        pattern.setCustom (layer, true);
        pattern.setManualBars (layer, 0); // längden följer det som lästes in
        if (skippedNotes != nullptr)
            *skippedNotes = skipped;
        return true;
    }
}
