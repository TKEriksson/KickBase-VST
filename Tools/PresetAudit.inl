// Verktyg för att bedöma ljud utan att kunna lyssna (se docs/sound-design-roadmap.md, fas 3):
//   params   - parameterregistret: id, intervall, standard och val
//   spektrogram av varje rendering (render skriver <ut>.spec.png)
//   audit    - renderar fabrikspresets och mäter dem: topp, RMS, tyngdpunkt, band, stereo och kickens längd
// Inkluderas av Tool.cpp.

namespace
{
    // Hög upplösning i frekvens (4096-punkts FFT), logaritmisk frekvensaxel 20 Hz - 20 kHz.
    juce::Colour heat (float v) // 0..1
    {
        v = juce::jlimit (0.0f, 1.0f, v);
        if (v < 0.25f) return juce::Colour::fromFloatRGBA (0.0f, 0.0f, v * 2.0f, 1.0f);
        if (v < 0.5f)  return juce::Colour::fromFloatRGBA ((v - 0.25f) * 3.0f, 0.0f, 0.5f + (v - 0.25f), 1.0f);
        if (v < 0.75f) return juce::Colour::fromFloatRGBA (0.75f + (v - 0.5f), (v - 0.5f) * 2.0f, 0.75f - (v - 0.5f) * 3.0f, 1.0f);
        return juce::Colour::fromFloatRGBA (1.0f, 0.5f + (v - 0.75f) * 2.0f, (v - 0.75f) * 4.0f, 1.0f);
    }

    // `order`: FFT-storlek (12 = 4096 punkter, 10 = 1024 för tät tidsupplösning); `seconds` > 0 visar bara början.
    void writeSpectrogram (const juce::AudioBuffer<float>& audio, double rate, double bpm, const juce::File& file, int order = 12, double seconds = 0.0)
    {
        const int size = 1 << order, bins = size / 2;
        constexpr int width = 1400, height = 360, left = 34, bottom = 16;
        const auto plotWidth = width - left, plotHeight = height - bottom;

        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) size, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> data ((size_t) size * 2);

        juce::Image image (juce::Image::RGB, width, height, true);
        juce::Graphics g (image);
        g.fillAll (juce::Colours::black);

        const auto n = seconds > 0.0 ? std::min (audio.getNumSamples(), (int) (seconds * rate)) : audio.getNumSamples();
        for (int x = 0; x < plotWidth; ++x)
        {
            const auto centre = (int) ((double) x / plotWidth * n);
            const auto begin = centre - size / 2;
            for (int i = 0; i < size; ++i)
            {
                const auto j = begin + i;
                data[(size_t) i] = j >= 0 && j < n ? 0.5f * (audio.getSample (0, j) + audio.getSample (audio.getNumChannels() > 1 ? 1 : 0, j)) : 0.0f;
            }
            std::fill (data.begin() + size, data.end(), 0.0f);
            window.multiplyWithWindowingTable (data.data(), size);
            fft.performFrequencyOnlyForwardTransform (data.data(), true);

            for (int y = 0; y < plotHeight; ++y)
            {
                const auto f0 = 20.0 * std::pow (1000.0, (double) (plotHeight - y - 1) / plotHeight);
                const auto f1 = 20.0 * std::pow (1000.0, (double) (plotHeight - y) / plotHeight);
                const auto b0 = std::min (bins - 1, (int) (f0 / (rate * 0.5) * bins)), b1 = std::min (bins, std::max (b0 + 1, (int) (f1 / (rate * 0.5) * bins)));
                auto peak = 0.0f;
                for (int b = b0; b < b1; ++b)
                    peak = std::max (peak, data[(size_t) b]);
                const auto db = juce::Decibels::gainToDecibels (peak * 4.0f / (float) size, -120.0f);
                image.setPixelAt (left + x, y, heat ((db + 100.0f) / 80.0f));
            }
        }

        // Frekvensskala och slag.
        g.setFont (10.0f);
        for (const auto hz : { 30.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            const auto y = (int) (plotHeight - std::log (hz / 20.0) / std::log (1000.0) * plotHeight);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawHorizontalLine (y, (float) left, (float) width);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String ((int) hz), 0, y - 6, left - 3, 12, juce::Justification::right);
        }
        const auto secondsPerBeat = 60.0 / bpm, total = n / rate;
        for (int beat = 0; beat * secondsPerBeat < total; ++beat)
        {
            const auto x = left + (int) (beat * secondsPerBeat / total * plotWidth);
            g.setColour (juce::Colours::white.withAlpha (beat % 4 == 0 ? 0.45f : 0.12f));
            g.drawVerticalLine (x, 0.0f, (float) plotHeight);
            if (beat % 4 == 0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.drawText (juce::String (beat / 4 + 1), x + 2, plotHeight, 30, bottom, juce::Justification::left);
            }
        }

        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);
    }

    // Mått på en rendering (stereo).
    struct Metrics
    {
        float peakDb = -100, rmsDb = -100, dc = 0, centroid = 0, sideDb = -100, lowSideDb = -100;
        std::array<float, 5> bandsDb {}; // < 60, 60-250, 250-1k, 1k-5k, > 5k (dB rel. summan)
        bool finite = true;
    };

    Metrics measure (const juce::AudioBuffer<float>& audio, double rate)
    {
        Metrics m;
        const auto n = audio.getNumSamples();
        auto peak = 0.0f;
        double sum = 0.0, dc = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const auto l = audio.getSample (0, i), r = audio.getSample (1, i);
            if (! std::isfinite (l) || ! std::isfinite (r))
                m.finite = false;
            peak = std::max ({ peak, std::abs (l), std::abs (r) });
            sum += 0.5 * (l * l + r * r);
            dc += 0.5 * (l + r);
        }
        m.peakDb = juce::Decibels::gainToDecibels (peak, -100.0f);
        m.rmsDb = juce::Decibels::gainToDecibels ((float) std::sqrt (sum / std::max (1, n)), -100.0f);
        m.dc = (float) (dc / std::max (1, n));

        constexpr int order = 13, size = 1 << order, bins = size / 2;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) size, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> mid ((size_t) size * 2), side ((size_t) size * 2);
        std::array<double, 5> bands {};
        double centroidSum = 0.0, totalEnergy = 0.0, midEnergy = 0.0, sideEnergy = 0.0, lowMid = 0.0, lowSide = 0.0;

        for (int start = 0; start + size <= n; start += size / 2)
        {
            for (int i = 0; i < size; ++i)
            {
                const auto l = audio.getSample (0, start + i), r = audio.getSample (1, start + i);
                mid[(size_t) i] = 0.5f * (l + r);
                side[(size_t) i] = 0.5f * (l - r);
            }
            std::fill (mid.begin() + size, mid.end(), 0.0f);
            std::fill (side.begin() + size, side.end(), 0.0f);
            window.multiplyWithWindowingTable (mid.data(), size);
            window.multiplyWithWindowingTable (side.data(), size);
            fft.performFrequencyOnlyForwardTransform (mid.data(), true);
            fft.performFrequencyOnlyForwardTransform (side.data(), true);

            for (int b = 1; b < bins; ++b)
            {
                const auto hz = b * rate / size;
                const auto e = (double) mid[(size_t) b] * mid[(size_t) b], s = (double) side[(size_t) b] * side[(size_t) b];
                const auto band = hz < 60 ? 0 : hz < 250 ? 1 : hz < 1000 ? 2 : hz < 5000 ? 3 : 4;
                bands[(size_t) band] += e + s;
                centroidSum += hz * (e + s);
                totalEnergy += e + s;
                midEnergy += e;
                sideEnergy += s;
                if (hz < 150)
                {
                    lowMid += e;
                    lowSide += s;
                }
            }
        }
        if (totalEnergy > 0.0)
        {
            m.centroid = (float) (centroidSum / totalEnergy);
            for (size_t i = 0; i < bands.size(); ++i)
                m.bandsDb[i] = (float) (10.0 * std::log10 (std::max (1.0e-12, bands[i] / totalEnergy)));
            m.sideDb = (float) (10.0 * std::log10 (std::max (1.0e-12, sideEnergy / std::max (1.0e-12, midEnergy))));
            m.lowSideDb = (float) (10.0 * std::log10 (std::max (1.0e-12, lowSide / std::max (1.0e-12, lowMid))));
        }
        return m;
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
                     section == Section::kick ? "  längd" : "");

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
            const auto m = measure (audio, renderRate);

            // Kickens längd: tiden från första slaget tills nivån fallit 40 dB under toppen.
            juce::String extra;
            if (section == Section::kick)
            {
                const auto beat = (int) (renderRate * 60.0 / bpm);
                auto top = 0.0f;
                for (int j = 0; j < beat; ++j)
                    top = std::max (top, std::abs (audio.getSample (0, j)));
                auto last = 0;
                const auto window = (int) (renderRate * 0.004);
                for (int j = 0; j + window < beat; j += window)
                {
                    auto local = 0.0f;
                    for (int k = 0; k < window; ++k)
                        local = std::max (local, std::abs (audio.getSample (0, j + k)));
                    if (local > top * 0.01f)
                        last = j + window;
                }
                extra = juce::String::formatted ("  %4.0f ms", 1000.0 * last / renderRate);
            }

            const auto name = Presets::factoryName (section, false, i);
            std::printf ("%-34s %6.1f %6.1f %5.3f %7.0f | %6.1f %6.1f %6.1f %6.1f %6.1f | %6.1f %6.1f%s%s\n", name.toRawUTF8(), m.peakDb, m.rmsDb, m.dc, m.centroid,
                         m.bandsDb[0], m.bandsDb[1], m.bandsDb[2], m.bandsDb[3], m.bandsDb[4], m.sideDb, m.lowSideDb, extra.toRawUTF8(), m.finite ? "" : "  EJ ÄNDLIGT");

            if (pngFolder != juce::File())
            {
                pngFolder.createDirectory();
                const auto file = pngFolder.getChildFile (juce::String::formatted ("%02d ", i) + juce::File::createLegalFileName (name) + ".png");
                if (section == Section::kick)
                    writeSpectrogram (audio, renderRate, bpm, file, 10, 0.40); // en kick i närbild
                else
                    writeSpectrogram (audio, renderRate, bpm, file);
            }
        }
        return 0;
    }
}
