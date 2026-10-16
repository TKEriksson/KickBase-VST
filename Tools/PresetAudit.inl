// Verktyg för att bedöma ljud utan att kunna lyssna (se docs/sound-design-roadmap.md, fas 3):
//   params   - parameterregistret: id, intervall, standard och val
//   spektrogram av varje rendering (render skriver <ut>.spec.png)
//   audit    - renderar fabrikspresets och mäter dem: topp, RMS, tyngdpunkt, band, stereo och kickens längd
// Måtten och bilderna kommer från Source/Core/AudioAnalysis. Inkluderas av Tool.cpp.

namespace
{
    void writeSpectrogram (const juce::AudioBuffer<float>& audio, double rate, double bpm, const juce::File& file, int order = 12, double seconds = 0.0)
    {
        AudioAnalysis::writePng (AudioAnalysis::spectrogram (audio, rate, bpm, order, seconds), file);
    }

    // Dumpar parameterregistret (id, namn, intervall, standard, val) för dokumentation och för att skriva presets.
    int dumpParameters (const juce::String& kind)
    {
        auto processor = createProcessor (kind);
        for (auto* p : processor->getParameters())
        {
            auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);
            if (withId == nullptr)
                continue;
            juce::String line = withId->paramID.paddedRight (' ', 22) + p->getName (64).paddedRight (' ', 26);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p))
                line << "val: " << choice->choices.joinIntoString (" | ") << "  (standard " << choice->getIndex() << ")";
            else if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
                line << "av/på  (standard " << (p->getDefaultValue() > 0.5f ? "på" : "av") << ")";
            else if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto range = ranged->getNormalisableRange();
                line << range.start << " .. " << range.end << "  (standard " << ranged->convertFrom0to1 (ranged->getDefaultValue())
                     << ", visas " << p->getText (p->getDefaultValue(), 32) << ")";
            }
            std::puts (line.toRawUTF8());
        }
        return 0;
    }

    // Renderar fabrikspresets i en del och mäter dem. `only` >= 0 väljer ett enda preset.
    int auditPresets (const juce::String& sectionName, int only, const juce::File& pngFolder, double bpm)
    {
        using Section = Presets::Section;
        Section section = Section::full;
        for (int s = 0; s < Presets::numSections; ++s)
            if (Presets::sectionName ((Section) s).equalsIgnoreCase (sectionName))
                section = (Section) s;

        const auto count = Presets::factoryCount (section, false);
        std::printf ("%-34s %6s %6s %5s %7s | %6s %6s %6s %6s %6s | %6s %6s%s\n", "PRESET", "peak", "rms", "dc", "centr.", "<60", "60-250", "250-1k", "1-5k", ">5k", "side", "lowsd",
                     section == Section::kick ? "  längd  svans" : "");

        for (int i = 0; i < count; ++i)
        {
            if (only >= 0 && i != only)
                continue;

            const auto owner = createProcessor ("instrument");
            auto& processor = *owner;
            processor.setPlayConfigDetails (0, 2, renderRate, blockSize);
            processor.loadFactoryPreset (section, i);

            // Delen bedöms för sig: kickens presets utan bas, basens utan kick.
            auto set = [&processor] (const char* id, float value)
            {
                if (auto* p = processor.apvts.getParameter (id))
                    p->setValueNotifyingHost (p->convertTo0to1 (value));
            };
            if (section == Section::kick)
                for (int layer = 0; layer < ParamID::numBassLayers; ++layer)
                    set (ParamID::bass (layer, BassField::on).toRawUTF8(), 0.0f);
            if (section == Section::bass)
                set (ParamID::kickOn, 0.0f);

            OfflineRender::Settings settings;
            settings.sampleRate = renderRate;
            settings.blockSize = blockSize;
            settings.bpm = bpm;
            settings.bars = 4;
            settings.notes = { 52 }; // E4: basgrundton E1 (41 Hz)
            settings.extraBars = 1;
            const auto audio = OfflineRender::render (processor, settings);
            const auto m = AudioAnalysis::measure (audio, renderRate);

            juce::String extra;
            if (section == Section::kick)
                extra = juce::String::formatted ("  %4.0f ms %4.0f Hz", AudioAnalysis::kickLengthMs (audio, renderRate, bpm), AudioAnalysis::kickTailHz (audio, renderRate, bpm));

            const auto name = Presets::factoryName (section, false, i);
            std::printf ("%-34s %6.1f %6.1f %5.3f %7.0f | %6.1f %6.1f %6.1f %6.1f %6.1f | %6.1f %6.1f%s%s\n", name.toRawUTF8(), m.peakDb, m.rmsDb, m.dc, m.centroid,
                         m.bandsDb[0], m.bandsDb[1], m.bandsDb[2], m.bandsDb[3], m.bandsDb[4], m.sideDb, m.lowSideDb, extra.toRawUTF8(), m.finite ? "" : "  EJ ÄNDLIGT");

            if (pngFolder != juce::File())
            {
                const auto file = pngFolder.getChildFile (juce::String::formatted ("%02d ", i) + juce::File::createLegalFileName (name) + ".png");
                if (section == Section::kick)
                    writeSpectrogram (audio, renderRate, bpm, file, 11, 0.40); // en kick i närbild
                else
                    writeSpectrogram (audio, renderRate, bpm, file);
            }
        }
        return 0;
    }
}
