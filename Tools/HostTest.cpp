// KickbaseHostTest: laddar den byggda VST3-filen som en värd (JUCE:s plugin-hosting), spelar ljud, öppnar och stänger
// editorn, sparar och läser tillstånd och laddar ur pluginet. Hittar krascher och hängningar vid avstängning som
// FL Studio annars visar (ett litet pluginval).
//
//   KickbaseHostTest <sökväg till .vst3> [antal varv]

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

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::puts ("Användning: KickbaseHostTest <sökväg till .vst3> [antal varv]");
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI init;
    const juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const auto rounds = argc > 2 ? juce::String (argv[2]).getIntValue() : 2;

    for (int round = 1; round <= rounds; ++round)
        if (! runOnce (file, round))
            return 1;

    std::puts ("Klart: laddade, spelade, öppnade och stängde editorn, och laddade ur utan fel");
    return 0;
}
