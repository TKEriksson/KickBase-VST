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
            std::printf ("  sparade %d byte\n", (int) state.getSize());
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
        instance->releaseResources();
        return sameId && sameState ? 0 : 1;
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
