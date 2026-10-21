// `tool paramtest`: varje parameter sätts till ett värde som skiljer sig från standardvärdet, projektet sparas och laddas i en ny processor,
// och det laddade värdet jämförs med det SATTA (inte med en ny sparning). Parametrar som inte överlever listas, ingenting tystas.

#include "../Source/Instrument/InstrumentProcessor.h"
#include "../Source/Fx/FxProcessor.h"

namespace
{
    struct Setting
    {
        juce::RangedAudioParameter* parameter = nullptr;
        int index = 0; // plats i getParameters(), samma i den laddade processorn
        float defaultValue = 0.0f;
        float setValue = 0.0f;
        bool differs = false;
    };

    // Första kandidaten som efter avrundning (meny, heltal, av/på) skiljer sig från standardvärdet.
    Setting applyNonDefault (juce::RangedAudioParameter& parameter, int index)
    {
        Setting s;
        s.parameter = &parameter;
        s.index = index;
        s.defaultValue = parameter.getDefaultValue(); // inte getValue(): en lyssnare kan redan ha flyttat parametern
        for (const auto candidate : { 0.37f, 0.81f, 0.0f, 1.0f, 1.0f - s.defaultValue })
        {
            parameter.setValueNotifyingHost (candidate);
            s.setValue = parameter.getValue();
            if (std::abs (s.setValue - s.defaultValue) > 1.0e-4f)
            {
                s.differs = true;
                break;
            }
        }
        return s;
    }

    juce::String nameOf (const juce::AudioProcessorParameter& parameter)
    {
        if (auto* withId = dynamic_cast<const juce::AudioProcessorParameterWithID*> (&parameter))
            return withId->paramID;
        return parameter.getName (64);
    }

    // Returnerar antal fel. Alla parametrar i `source` sätts, sparas och jämförs med en ny processor av samma typ.
    // `untestableTotal` räknas upp med antalet parametrar som inte kunde provas.
    template <typename Processor>
    int sweep (const char* label, int& untestableTotal)
    {
        auto source = std::make_unique<Processor> ();
        // Laddningen rensar LFO utanför activeLfos som saknar rutter (PluginBase.cpp applyLoadedLfos/clearLfo, ~:829-855) och återställer deras parametrar.
        // Det är avsett produktbeteende; testet aktiverar alla LFO före sweepen så att deras parametrar verkligen provas.
        for (auto lfo = 0; lfo < source->availableLfos(); ++lfo)
            source->setLfoActive (lfo, true);
        const auto& all = source->getParameters();
        std::vector<Setting> settings;
        auto problems = 0;

        for (auto index = 0; index < all.size(); ++index)
        {
            auto* p = all[index];
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (ranged == nullptr)
            {
                std::printf ("FEL %s: %s är ingen RangedAudioParameter, kan inte testas\n", label, nameOf (*p).toRawUTF8());
                ++problems;
                continue;
            }
            settings.push_back (applyNonDefault (*ranged, index));
        }

        auto untestable = 0, coupled = 0;
        for (const auto& s : settings)
        {
            if (! s.differs)
            {
                std::printf ("OTESTAD %s: %s (inget värde skiljer sig från standard %.3f)\n", label, nameOf (*s.parameter).toRawUTF8(), s.defaultValue);
                ++untestable;
            }
            // Satt värde som en annan parameters lyssnare ändrade under sweepen: listas, men jämförs ändå mot det satta.
            else if (std::abs (s.parameter->getValue() - s.setValue) > 1.0e-4f)
            {
                std::printf ("KOPPLAD %s: %s ändrades av annan parameter (satt %.4f, före sparning %.4f)\n", label, nameOf (*s.parameter).toRawUTF8(), s.setValue, s.parameter->getValue());
                ++coupled;
            }
        }

        juce::MemoryBlock state;
        source->getStateInformation (state);
        auto loaded = std::make_unique<Processor> ();
        loaded->setStateInformation (state.getData(), (int) state.getSize());

        const auto& loadedAll = loaded->getParameters();
        if (loadedAll.size() != all.size())
        {
            std::printf ("FEL %s: parameterantal %d mot %d efter laddning\n", label, (int) all.size(), (int) loadedAll.size());
            ++problems;
        }

        auto survived = 0, tested = 0;
        for (const auto& s : settings)
        {
            if (! s.differs || s.index >= loadedAll.size())
                continue;
            ++tested;
            const auto got = loadedAll[s.index]->getValue();
            if (std::abs (got - s.setValue) <= 1.0e-4f)
                ++survived;
            else
            {
                std::printf ("FEL %s: %s satt %.4f, laddat %.4f (standard %.4f)\n", label, nameOf (*s.parameter).toRawUTF8(), s.setValue, got, s.defaultValue);
                ++problems;
            }
        }

        std::printf ("%s: %d parametrar (getParameters), %d testade, %d överlevde, %d otestade, %d kopplade, %d fel\n",
                     label, (int) all.size(), tested, survived, untestable, coupled, problems);
        if (tested + untestable + (int) (all.size() - settings.size()) != (int) all.size())
        {
            std::printf ("FEL %s: räkningen går inte ihop med getParameters().size()\n", label);
            ++problems;
        }
        untestableTotal += untestable;
        return problems;
    }
}

int paramTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto untestable = 0;
    const auto problems = sweep<InstrumentProcessor> ("instrument", untestable) + sweep<FxProcessor> ("fx", untestable);
    if (problems == 0)
        std::printf ("paramtest: ok (%d otestade)\n", untestable);
    else
        std::printf ("paramtest: FEL, se raderna ovan (%d otestade)\n", untestable);
    return problems == 0 ? 0 : 1;
}
