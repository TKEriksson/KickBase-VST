// KickbaseHostTest: laddar den byggda VST3-filen som en värd (JUCE:s plugin-hosting), spelar ljud, öppnar och stänger
// editorn, sparar och läser tillstånd och laddar ur pluginet. Hittar krascher och hängningar vid avstängning som
// FL Studio annars visar (ett litet pluginval).
//
//   KickbaseHostTest <sökväg till .vst3> [antal varv]
//   KickbaseHostTest <sökväg till .vst3> state save|load <fil>
//     save: ändrar några parametrar, spelar och sparar tillståndet (som ett projekt) och pluginets id i <fil>.
//     load: läser in tillståndet, kontrollerar att id:t är detsamma och att tillståndet sparas identiskt igen
//           (så bevisas att ett projekt sparat med ett äldre bygge laddas som förut, t.ex. efter att CMake-målen döpts om).

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>
#include <map>

namespace
{
    void pump (int milliseconds)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (milliseconds);
    }

    bool runOnce (const juce::File& file, int round)
    {
        juce::AudioPluginFormatManager formats;
        formats.addFormat (std::make_unique<juce::VST3PluginFormat>());

        juce::OwnedArray<juce::PluginDescription> found;
        formats.getFormat (0)->findAllTypesForFile (found, file.getFullPathName());
        if (found.isEmpty())
        {
            std::puts ("FEL: hittade ingen plugin i filen");
            return false;
        }

        for (auto* description : found)
        {
            std::printf ("  [%d] %s (%s)\n", round, description->name.toRawUTF8(), description->isInstrument ? "instrument" : "effekt");
            juce::String error;
            auto instance = formats.createPluginInstance (*description, 44100.0, 512, error);
            if (instance == nullptr)
            {
                std::printf ("FEL: kunde inte skapa instansen: %s\n", error.toRawUTF8());
                return false;
            }

            instance->prepareToPlay (44100.0, 512);

            juce::AudioBuffer<float> buffer (std::max (2, instance->getTotalNumOutputChannels()), 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);
            for (int block = 0; block < 400; ++block)
            {
                buffer.clear();
                instance->processBlock (buffer, midi);
                midi.clear();
            }

            // Öppna editorn, låt den rita och ticka en stund, stäng den.
            if (instance->hasEditor())
            {
                std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorAndMakeActive());
                if (editor != nullptr)
                {
                    editor->setVisible (false);
                    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
                    pump (400);
                    std::printf ("      editor %dx%d\n", image.getWidth(), image.getHeight());
                }
            }

            // Spara och läs tillståndet, ändra en parameter mitt i.
            juce::MemoryBlock state;
            instance->getStateInformation (state);
            if (auto* parameter = instance->getParameters()[10])
                parameter->setValue (0.3f);
            instance->setStateInformation (state.getData(), (int) state.getSize());

            // En editor till (som när fönstret öppnas igen) och sedan avslut i samma ordning som en värd.
            {
                std::unique_ptr<juce::AudioProcessorEditor> second (instance->createEditorAndMakeActive());
                pump (150);
            }
            instance->releaseResources();
            instance.reset();
        }
        return true;
    }
}

namespace
{
    int stateTest (const juce::File& file, bool save, const juce::File& stateFile)
    {
        juce::AudioPluginFormatManager formats;
        formats.addFormat (std::make_unique<juce::VST3PluginFormat>());
        juce::OwnedArray<juce::PluginDescription> found;
        formats.getFormat (0)->findAllTypesForFile (found, file.getFullPathName());
        if (found.isEmpty())
        {
            std::puts ("FEL: hittade ingen plugin i filen");
            return 1;
        }

        auto& description = *found.getFirst();
        const auto id = juce::String::toHexString (description.uniqueId) + "/" + juce::String::toHexString (description.deprecatedUid); // VST3-klassens id (inte filens sökväg)
        juce::String error;
        auto instance = formats.createPluginInstance (description, 44100.0, 512, error);
        if (instance == nullptr)
        {
            std::printf ("FEL: kunde inte skapa instansen: %s\n", error.toRawUTF8());
            return 1;
        }
        instance->prepareToPlay (44100.0, 512);
        std::printf ("  %s, id %s, %d parametrar\n", description.name.toRawUTF8(), id.toRawUTF8(), instance->getParameters().size());

        if (save)
        {
            const auto& parameters = instance->getParameters();
            for (int i = 5; i < parameters.size(); i += 97)
                parameters[i]->setValue (0.37f);
            juce::AudioBuffer<float> buffer (std::max (2, instance->getTotalNumOutputChannels()), 512);
            juce::MidiBuffer midi;
            for (int block = 0; block < 20; ++block)
                instance->processBlock (buffer, midi);
            juce::MemoryBlock state;
            instance->getStateInformation (state);
            juce::MemoryOutputStream out;
            out.writeString (id);
            out.writeInt64 ((juce::int64) state.getSize());
            out.write (state.getData(), state.getSize());
            stateFile.replaceWithData (out.getData(), out.getDataSize());
            // Alla parametrars namn och värden, så att en nyare version kan jämföras mot dem (state load): nya parametrar får inte ändra något gammalt.
            // Värdena i en NY instans som laddat tillståndet: det är de som tillståndet verkligen definierar (valparametrar snappas till sina steg, och
            // värdens MIDI CC-parametrar sparas inte alls).
            auto second = formats.createPluginInstance (description, 44100.0, 512, error);
            if (second == nullptr)
            {
                std::printf ("FEL: kunde inte skapa den andra instansen: %s\n", error.toRawUTF8());
                return 1;
            }
            second->prepareToPlay (44100.0, 512);
            second->setStateInformation (state.getData(), (int) state.getSize());
            juce::String values;
            for (auto* parameter : second->getParameters())
                values << parameter->getName (200) << "|" << juce::String (parameter->getValue(), 9) << "\n";
            second->releaseResources();
            stateFile.withFileExtension ("params.txt").replaceWithText (values);
            std::printf ("  sparade %d byte och %d parametrar\n", (int) state.getSize(), instance->getParameters().size());
            return 0;
        }

        juce::MemoryBlock data;
        if (! stateFile.loadFileAsData (data))
        {
            std::puts ("FEL: kunde inte läsa tillståndsfilen");
            return 1;
        }
        juce::MemoryInputStream in (data, false);
        const auto savedId = in.readString();
        juce::MemoryBlock saved;
        in.readIntoMemoryBlock (saved, (juce::ssize_t) in.readInt64());
        instance->setStateInformation (saved.getData(), (int) saved.getSize());
        juce::MemoryBlock again;
        instance->getStateInformation (again);
        const auto sameId = savedId == id, sameState = again == saved;
        std::printf ("  id %s, tillståndet sparas %s igen (%d byte)\n", sameId ? "samma" : ("ANNAT (sparat " + savedId + ")").toRawUTF8(),
                     sameState ? "identiskt" : "OLIKT", (int) again.getSize());
        // Mot de sparade värdena (om den gamla versionens parametrar finns i filen): varje parameter som finns i båda ska ha samma värde, och en ny
        // parameter får inte ha ändrats; alla Filter Model (instrument 1-10) ska stå på Kickbase (värde 0), som i ett projekt som är äldre än parametern.
        auto valueMismatches = 0, compared = 0, filterModels = 0, filterModelsNotDefault = 0, notInSavedFile = 0;
        // Nya parametrar (inte i den sparade filen) ska stå på sitt standardvärde, och särskilt: Filter 2 av, Filter P1 = 0, Filter P2 = 1 (fas D2/D3).
        auto newNotDefault = 0, filter2Count = 0, filter2NotOff = 0, p1Count = 0, p1NotZero = 0, p2Count = 0, p2NotOne = 0;
        const auto valuesFile = stateFile.withFileExtension ("params.txt");
        if (valuesFile.existsAsFile())
        {
            std::map<juce::String, float> savedValues;
            for (const auto& line : juce::StringArray::fromLines (valuesFile.loadFileAsString()))
                if (line.contains ("|"))
                    savedValues[line.upToLastOccurrenceOf ("|", false, false)] = line.fromLastOccurrenceOf ("|", false, false).getFloatValue();
            for (auto* parameter : instance->getParameters())
            {
                const auto name = parameter->getName (200);
                const auto savedEntry = savedValues.find (name);
                if (name.contains ("Filter Model"))
                {
                    ++filterModels;
                    filterModelsNotDefault += juce::approximatelyEqual (parameter->getValue(), 0.0f) && parameter->getText (parameter->getValue(), 20) == "Kickbase" ? 0 : 1;
                }
                if (savedEntry == savedValues.end())
                {
                    ++notInSavedFile;
                    if (! juce::approximatelyEqual (parameter->getValue(), parameter->getDefaultValue()))
                        if (++newNotDefault <= 10)
                            std::printf ("    ny parameter inte på standardvärdet: %s (%s)\n", name.toRawUTF8(), parameter->getCurrentValueAsText().toRawUTF8());
                    if (name.endsWith ("Filter 2 On"))
                    {
                        ++filter2Count;
                        filter2NotOff += parameter->getValue() < 0.5f ? 0 : 1;
                    }
                    if (name.endsWith ("Filter P1"))
                    {
                        ++p1Count;
                        p1NotZero += juce::approximatelyEqual (parameter->getValue(), 0.0f) ? 0 : 1;
                    }
                    if (name.endsWith ("Filter P2"))
                    {
                        ++p2Count;
                        p2NotOne += juce::approximatelyEqual (parameter->getValue(), 1.0f) ? 0 : 1;
                    }
                    continue;
                }
                ++compared;
                if (std::abs (savedEntry->second - parameter->getValue()) > 1.0e-6f)
                {
                    if (++valueMismatches <= 30)
                        std::printf ("    skiljer: %s sparad %.6f nu %.6f (%s)\n", name.toRawUTF8(), savedEntry->second, parameter->getValue(), parameter->getCurrentValueAsText().toRawUTF8());
                }
            }
            std::printf ("  %d parametrar jämförda mot den sparade filen: %d skiljer; %d parametrar fanns inte i den (nya); Filter Model: %d st, %d inte på Kickbase\n",
                         compared, valueMismatches, notInSavedFile, filterModels, filterModelsNotDefault);
            if (notInSavedFile > 0)
                std::printf ("  nya parametrar: %d inte på standardvärdet; Filter 2 On: %d st, %d på; Filter P1: %d st, %d inte 0; Filter P2: %d st, %d inte 1\n",
                             newNotDefault, filter2Count, filter2NotOff, p1Count, p1NotZero, p2Count, p2NotOne);
            filterModelsNotDefault += newNotDefault + filter2NotOff + p1NotZero + p2NotOne; // (räknas som fel på samma sätt)
        }
        instance->releaseResources();
        // En nyare version har fler parametrar än filen: då sparas tillståndet inte identiskt igen, men inget gammalt värde får ha ändrats.
        const auto newerOk = ! sameState && valuesFile.existsAsFile() && notInSavedFile > 0 && valueMismatches == 0 && filterModelsNotDefault == 0;
        return sameId && (sameState || newerOk) && valueMismatches == 0 && filterModelsNotDefault == 0 ? 0 : 1;
    }
}

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::puts ("Användning: KickbaseHostTest <sökväg till .vst3> [antal varv]");
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI init;
    const juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    if (argc > 4 && juce::String (argv[2]) == "state")
        return stateTest (file, juce::String (argv[3]) == "save", juce::File::getCurrentWorkingDirectory().getChildFile (argv[4]));
    const auto rounds = argc > 2 ? juce::String (argv[2]).getIntValue() : 2;

    for (int round = 1; round <= rounds; ++round)
        if (! runOnce (file, round))
            return 1;

    std::puts ("Klart: laddade, spelade, öppnade och stängde editorn, och laddade ur utan fel");
    return 0;
}
