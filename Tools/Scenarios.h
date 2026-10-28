#pragma once

// Testlägen för verktygen (prestanda, signaturer, ljudtrådstester): ett "tungt" läge som använder så mycket av synthen som möjligt på en
// gång, så att en optimering eller rättning som ändrar ljudet i någon del syns i signaturen, och så att profileraren ser alla delar.

#include "../Source/Core/PluginBase.h"
#include "../Source/Core/OfflineRender.h"

namespace Scenarios
{
    // Testerna sparar presets och inställningar: de ska hamna i en tillfällig mapp, aldrig i användarens Dokument\Kickbase (en krasch
    // mitt i ett test skulle annars lämna testfiler bland användarens presets). Gäller så länge objektet lever.
    struct TemporaryUserFolders
    {
        TemporaryUserFolders()
        {
            root.deleteRecursively();
            root.createDirectory();
            Settings::useFile (root.getChildFile ("settings.xml"));
            Settings::setFolder ("presetsFolder", root.getChildFile ("Presets"));
            Settings::setFolder ("exportsFolder", root.getChildFile ("Exports"));
            Settings::setFolder ("libraryFolder", root.getChildFile ("Library"));
        }

        ~TemporaryUserFolders()
        {
            Settings::useFile ({});
            root.deleteRecursively();
        }

        const juce::File root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getChildFile ("kickbase_test_" + juce::String (juce::Time::currentTimeMillis()));
    };

    inline void setPlain (PluginBase& processor, const juce::String& id, float value)
    {
        if (auto* parameter = processor.apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        else
            std::printf ("Scenarios: okänd parameter %s\n", id.toRawUTF8());
    }

    inline void setInstrument (PluginBase& processor, int layer, InstrumentField field, float value)
    {
        setPlain (processor, ParamID::bass (layer, field), value);
    }

    inline int indexOf (PluginBase& processor, const juce::String& id)
    {
        return processor.modulator.indexOf (id);
    }

    // En kort WAV i temp-mappen (en avklingande ton med övertoner), för samplingar och egna wavetables.
    inline juce::File writeTestWav (const juce::String& name, double hz, double seconds, double sampleRate = 44100.0)
    {
        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickbase_scenario_" + name + ".wav");
        juce::AudioBuffer<float> audio (1, (int) (seconds * sampleRate));
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const auto t = i / sampleRate;
            const auto phase = juce::MathConstants<double>::twoPi * hz * t;
            audio.setSample (0, i, (float) ((0.6 * std::sin (phase) + 0.25 * std::sin (2.0 * phase + 0.3) + 0.1 * std::sin (5.0 * phase)) * std::exp (-3.0 * t)));
        }
        OfflineRender::writeWav (audio, sampleRate, file);
        return file;
    }

    // Allt på en gång: `instruments` instrument med tre oscillatorer (unison, olika tabeller och warp), filter med envelope, sub, brus, drive,
    // GATE, swing/humanize/delay, glide-toner, samplingar och egna wavetables; varje instrument skickas till en effektkanal och kanalerna
    // har tillsammans alla effekttyper, EQ och kompressor; LFO:er och instrumentens källor modulerar rattar; kicken har alla lager;
    // masterns EQ, bredd och stereoförstärkare är på.
    inline void makeHeavy (PluginBase& processor, int instruments)
    {
        processor.loadFactoryPreset (Presets::Section::full, 0);
        if (! processor.isEffectVersion())
            processor.setInstrumentCount (instruments);
        const auto count = processor.isEffectVersion() ? 1 : instruments;

        // Kicken: alla lager och en sampling som lager.
        setPlain (processor, ParamID::kickHarmonics, 0.4f);
        setPlain (processor, ParamID::kickNoise, 0.3f);
        setPlain (processor, ParamID::kickFm, 0.3f);
        setPlain (processor, ParamID::kickSub, 0.3f);
        setPlain (processor, ParamID::kickDrive, 0.3f);
        setPlain (processor, ParamID::kickLowCut, 30.0f);
        setPlain (processor, ParamID::kickHighCut, 12000.0f);
        setPlain (processor, ParamID::kickCrush, 0.1f);
        processor.loadClickSample (writeTestWav ("click", 180.0, 0.08));
        setPlain (processor, ParamID::kickSampleMode, 1.0f);

        const PatternTemplate templates[] { PatternTemplate::rolling, PatternTemplate::galloping, PatternTemplate::triplet, PatternTemplate::offbeat };
        const int tables[] { 3, 17, 28, 41, 55, 66, 77, 90, 101, 112 };
        for (int layer = 0; layer < count; ++layer)
        {
            if (! processor.isEffectVersion())
            {
                processor.pattern.applyLayerTemplate (layer, templates[layer % 4]);
                // En glide-ton i varje takt: en lång ton med en glide inuti.
                processor.pattern.addNote (layer, 4 * Pattern::ticksPerQuarter - 96, 96, 0, 0.9f);
                processor.pattern.addNote (layer, 4 * Pattern::ticksPerQuarter - 48, 24, 7, 0.9f, true);
            }

            if (processor.isEffectVersion())
                continue; // FX-versionens "lager" är bara gaten: inget ljud att ställa in

            for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
            {
                auto field = [osc] (InstrumentField aField) { return ParamID::oscField (osc, aField); };
                setInstrument (processor, layer, field (InstrumentField::aOn), 1.0f);
                setInstrument (processor, layer, field (InstrumentField::aLevel), 0.6f);
                setInstrument (processor, layer, field (InstrumentField::aTable), (float) tables[(layer + osc * 3) % 10]);
                setInstrument (processor, layer, field (InstrumentField::aPos), 0.2f + 0.3f * (float) osc);
                setInstrument (processor, layer, field (InstrumentField::aUnison), (float) (osc == 0 ? 7 : osc == 1 ? 4 : 3));
                setInstrument (processor, layer, field (InstrumentField::aDetune), 18.0f);
                setInstrument (processor, layer, field (InstrumentField::aWarp), (float) ((layer + osc) % 9));
                setInstrument (processor, layer, field (InstrumentField::aWarpAmount), 0.35f);
                setInstrument (processor, layer, field (InstrumentField::aRandom), 0.3f);
                setInstrument (processor, layer, field (InstrumentField::aStereo), 0.6f);
            }
            setInstrument (processor, layer, InstrumentField::filterType, (float) (layer % 4));
            setInstrument (processor, layer, InstrumentField::cutoff, 400.0f + 80.0f * (float) layer);
            setInstrument (processor, layer, InstrumentField::resonance, 0.4f);
            setInstrument (processor, layer, InstrumentField::envAmount, 3.0f);
            setInstrument (processor, layer, InstrumentField::envDecay, 140.0f);
            setInstrument (processor, layer, InstrumentField::keyTrack, 0.5f);
            setInstrument (processor, layer, InstrumentField::sub, 0.3f);
            setInstrument (processor, layer, InstrumentField::subWave, (float) (layer % 4));
            setInstrument (processor, layer, InstrumentField::noise, 0.1f);
            setInstrument (processor, layer, InstrumentField::noiseTone, 0.5f);
            setInstrument (processor, layer, InstrumentField::drive, 0.3f);
            setInstrument (processor, layer, InstrumentField::driveMode, (float) (layer % 5));
            setInstrument (processor, layer, InstrumentField::pitchEnvAmount, 3.0f);
            setInstrument (processor, layer, InstrumentField::warpEnv, 0.3f);
            setInstrument (processor, layer, InstrumentField::gateOn, layer % 3 == 0 ? 1.0f : 0.0f);
            setInstrument (processor, layer, InstrumentField::delay, (float) (layer % 3) * 4.0f);
            setInstrument (processor, layer, InstrumentField::swing, layer % 2 == 0 ? 0.3f : 0.0f);
            setInstrument (processor, layer, InstrumentField::humanize, layer % 3 == 1 ? 0.4f : 0.0f);
            setInstrument (processor, layer, InstrumentField::fxChannel, (float) (1 + layer % numEffectChannels));
            setInstrument (processor, layer, InstrumentField::fxAmount, 0.6f);
        }

        if (! processor.isEffectVersion())
        {
            // Samplingar och egna wavetables (också på OSC 3).
            processor.loadInstrumentSample (0, writeTestWav ("bass", 110.0, 0.5));
            setInstrument (processor, 0, InstrumentField::sampleOn, 1.0f);
            setInstrument (processor, 0, InstrumentField::sampleLevel, 0.5f);
            setInstrument (processor, 0, InstrumentField::sampleKey, 1.0f);
            if (count > 1)
                processor.loadUserTable (1, 2, writeTestWav ("table", 21.533, 0.2, 44100.0));
            setPlain (processor, ParamID::patSwing, 0.2f);
            setPlain (processor, ParamID::patHumanize, 0.1f);
        }

        // Effektkanalerna: alla typer, två per kanal, plus EQ och kompressor.
        for (int channel = 0; channel < numLegacyEffectChannels; ++channel)
        {
            for (int slot = 0; slot < 2; ++slot)
            {
                const auto type = 1 + (channel * 2 + slot) % (numEffectTypes - 2);
                Effects::setSlotType (processor.apvts, channel, slot, (EffectType) type);
            }
            setPlain (processor, Effects::channelParamID (channel, "EqHp"), 60.0f);
            setPlain (processor, Effects::channelParamID (channel, "Eq2G"), 3.0f);
            setPlain (processor, Effects::channelParamID (channel, "CompRatio"), 3.0f);
            setPlain (processor, Effects::channelParamID (channel, "CompThr"), -18.0f);
            setPlain (processor, Effects::channelParamID (channel, "Duck"), 0.5f);
        }
        if (processor.isEffectVersion())
        {
            setInstrument (processor, 0, InstrumentField::fxChannel, 1.0f);
            setInstrument (processor, 0, InstrumentField::fxAmount, 0.6f);
        }

        // Källornas EQ och mastern.
        if (! processor.isEffectVersion())
            setPlain (processor, "kickEq1G", 2.0f);
        setPlain (processor, "masterEq3G", 1.5f);
        setPlain (processor, "masterEnhance", 0.4f);
        setPlain (processor, "masterWidth", 1.2f);
        if (! processor.isEffectVersion())
            for (int layer = 0; layer < count; ++layer)
                setPlain (processor, SourceEq::prefixFor (1 + layer) + "EqHp", 35.0f);

        // Modulering: LFO:er (olika former) och instrumentens källor till rattar.
        setPlain (processor, "lfo1Shape", 0.0f);
        setPlain (processor, "lfo2Shape", 5.0f);
        setPlain (processor, "lfo3Shape", 6.0f);
        setPlain (processor, "lfo3Mode", 1.0f);
        setPlain (processor, "lfo3Hz", 3.0f);
        processor.modRoutes.add (0, indexOf (processor, ParamID::bass (0, InstrumentField::cutoff)), 0.3f);
        processor.modRoutes.add (1, indexOf (processor, ParamID::bass (0, InstrumentField::aPos)), 0.4f);
        processor.modRoutes.add (2, indexOf (processor, Effects::knobParamID (0, 0, 1)), 0.3f);
        processor.modRoutes.add (Modulation::firstMacro, indexOf (processor, ParamID::kickDrive), 0.2f);
        if (! processor.isEffectVersion())
        {
            for (int layer = 1; layer < count; ++layer)
                processor.modRoutes.add (Modulation::instrumentSource (layer, layer % Modulation::instrumentSources),
                                         indexOf (processor, ParamID::bass (layer, InstrumentField::cutoff)), 0.25f);
            setPlain (processor, "macro1", 0.5f);
        }
        processor.ensureTablesInUse();
    }
}
