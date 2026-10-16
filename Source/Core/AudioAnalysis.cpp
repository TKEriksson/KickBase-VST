#include "AudioAnalysis.h"
#include <juce_dsp/juce_dsp.h>

namespace
{
    juce::Colour heat (float v) // 0..1: svart, blått, magenta, orange, gult, vitt
    {
        v = juce::jlimit (0.0f, 1.0f, v);
        if (v < 0.25f) return juce::Colour::fromFloatRGBA (0.0f, 0.0f, v * 2.0f, 1.0f);
        if (v < 0.5f)  return juce::Colour::fromFloatRGBA ((v - 0.25f) * 3.0f, 0.0f, 0.5f + (v - 0.25f), 1.0f);
        if (v < 0.75f) return juce::Colour::fromFloatRGBA (0.75f + (v - 0.5f), (v - 0.5f) * 2.0f, 0.75f - (v - 0.5f) * 3.0f, 1.0f);
        return juce::Colour::fromFloatRGBA (1.0f, 0.5f + (v - 0.75f) * 2.0f, (v - 0.75f) * 4.0f, 1.0f);
    }

    float monoAt (const juce::AudioBuffer<float>& audio, int i)
    {
        return audio.getNumChannels() > 1 ? 0.5f * (audio.getSample (0, i) + audio.getSample (1, i)) : audio.getSample (0, i);
    }
}

namespace AudioAnalysis
{
    Metrics measure (const juce::AudioBuffer<float>& audio, double rate)
    {
        Metrics m;
        const auto n = audio.getNumSamples();
        const auto right = std::min (1, audio.getNumChannels() - 1);
        auto peak = 0.0f;
        double sum = 0.0, dc = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const auto l = audio.getSample (0, i), r = audio.getSample (right, i);
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
                const auto l = audio.getSample (0, start + i), r = audio.getSample (right, start + i);
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

    float kickLengthMs (const juce::AudioBuffer<float>& audio, double rate, double bpm)
    {
        const auto beat = std::min (audio.getNumSamples(), (int) (rate * 60.0 / bpm));
        auto top = 0.0f;
        for (int j = 0; j < beat; ++j)
            top = std::max (top, std::abs (audio.getSample (0, j)));

        auto last = 0;
        const auto window = std::max (1, (int) (rate * 0.004));
        for (int j = 0; j + window < beat; j += window)
        {
            auto local = 0.0f;
            for (int k = 0; k < window; ++k)
                local = std::max (local, std::abs (audio.getSample (0, j + k)));
            if (local > top * 0.01f)
                last = j + window;
        }
        return (float) (1000.0 * last / rate);
    }

    float kickTailHz (const juce::AudioBuffer<float>& audio, double rate, double bpm)
    {
        const auto length = kickLengthMs (audio, rate, bpm) * 0.001;
        if (length < 0.03)
            return 0.0f;

        // Andra halvan av kicken: där svepet har lagt sig.
        const auto from = (int) (rate * length * 0.45), to = std::min (audio.getNumSamples(), (int) (rate * length * 0.85));
        constexpr int order = 15, size = 1 << order;
        if (to - from < 256)
            return 0.0f;

        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) size * 2, 0.0f);
        const auto count = std::min (size, to - from);
        for (int i = 0; i < count; ++i)
        {
            const auto w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (count - 1));
            data[(size_t) i] = monoAt (audio, from + i) * w;
        }
        fft.performFrequencyOnlyForwardTransform (data.data(), true);

        const auto lowBin = (int) (25.0 / rate * size), highBin = (int) (150.0 / rate * size);
        auto best = lowBin;
        for (int b = lowBin; b <= highBin; ++b)
            if (data[(size_t) b] > data[(size_t) best])
                best = b;

        // Parabolisk interpolation runt toppen.
        const auto a = data[(size_t) best - 1], c = data[(size_t) best], d = data[(size_t) best + 1];
        const auto denominator = a - 2.0f * c + d;
        const auto offset = std::abs (denominator) > 1.0e-9f ? 0.5f * (a - d) / denominator : 0.0f;
        return (float) ((best + offset) * rate / size);
    }

    juce::Image spectrogram (const juce::AudioBuffer<float>& audio, double rate, double bpm, int order, double seconds)
    {
        const int size = 1 << order, bins = size / 2;
        constexpr int width = 1400, height = 360, left = 34, bottom = 16;
        const auto plotWidth = width - left, plotHeight = height - bottom;

        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) size, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> data ((size_t) size * 2);

        // Mjukvarubild: pixlarna skrivs direkt (setPixelAt), vilket inte fungerar mot en bild som ligger på grafikkortet.
        juce::Image image (juce::Image::RGB, width, height, true, juce::SoftwareImageType());
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
                data[(size_t) i] = j >= 0 && j < audio.getNumSamples() ? monoAt (audio, j) : 0.0f;
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
        return image;
    }

    bool writePng (const juce::Image& image, const juce::File& file)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream stream (file);
        return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream);
    }

    juce::String pngBase64 (const juce::Image& image)
    {
        juce::MemoryOutputStream stream;
        juce::PNGImageFormat().writeImageToStream (image, stream);
        return juce::Base64::toBase64 (stream.getData(), stream.getDataSize());
    }
}
