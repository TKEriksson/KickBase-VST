#include "WavetableEdit.h"
#include <juce_dsp/juce_dsp.h>
#include <complex>

namespace WavetableEdit
{
    namespace
    {
        constexpr int size = Wavetable::frameSize;
        constexpr float halfPi = juce::MathConstants<float>::halfPi;

        float peakOf (const float* data)
        {
            float peak = 0.0f;
            for (int i = 0; i < size; ++i)
                peak = std::max (peak, std::abs (data[i]));
            return peak;
        }
    }

    std::vector<float> framesOf (const Wavetable& table)
    {
        std::vector<float> frames ((size_t) table.getFrameCount() * size);
        for (int f = 0; f < table.getFrameCount(); ++f)
            std::copy (table.displayFrame (f), table.displayFrame (f) + size, frames.begin() + (size_t) f * size);
        return frames;
    }

    void analyse (const float* frame, int count, std::vector<float>& amp, std::vector<float>& phase)
    {
        juce::dsp::FFT fft (10); // size = 1024
        std::vector<std::complex<float>> in ((size_t) size), out ((size_t) size);
        for (int i = 0; i < size; ++i)
            in[(size_t) i] = frame[i];
        fft.perform (in.data(), out.data(), false);

        count = juce::jlimit (1, size / 2 - 1, count);
        amp.assign ((size_t) count, 0.0f);
        phase.assign ((size_t) count, 0.0f);
        for (int k = 1; k <= count; ++k)
        {
            const auto bin = out[(size_t) k];
            amp[(size_t) k - 1] = 2.0f * std::abs (bin) / (float) size;
            phase[(size_t) k - 1] = std::abs (bin) > 1.0e-6f ? std::arg (bin) + halfPi : 0.0f; // sin (x + p) = cos (x + p - pi/2)
        }
    }

    void apply (std::vector<float>& frames, int frame, const std::vector<Edit>& edits, bool normalise)
    {
        const auto count = (int) (frames.size() / size);
        if (count < 2)
            return;

        juce::dsp::FFT fft (10);
        std::vector<std::complex<float>> in ((size_t) size), spectrum ((size_t) size);
        const auto first = frame < 0 ? 0 : juce::jlimit (0, count - 1, frame);
        const auto last = frame < 0 ? count - 1 : first;

        for (int f = first; f <= last; ++f)
        {
            auto* data = frames.data() + (size_t) f * size;
            const auto before = peakOf (data);
            for (int i = 0; i < size; ++i)
                in[(size_t) i] = data[i];
            fft.perform (in.data(), spectrum.data(), false);

            for (const auto& e : edits)
            {
                if (e.harmonic < 1 || e.harmonic >= size / 2)
                    continue;
                const auto factor = std::polar (std::max (0.0f, e.gain), e.phaseShift);
                spectrum[(size_t) e.harmonic] *= factor;
                spectrum[(size_t) (size - e.harmonic)] = std::conj (spectrum[(size_t) e.harmonic]); // reellt: speglad bin
            }

            fft.perform (spectrum.data(), in.data(), true);
            for (int i = 0; i < size; ++i)
                data[i] = in[(size_t) i].real();

            if (normalise)
            {
                const auto after = peakOf (data);
                if (after > 1.0e-6f && before > 1.0e-6f)
                    for (int i = 0; i < size; ++i)
                        data[i] *= before / after;
            }
        }
    }
}
