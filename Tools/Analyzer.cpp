#include "Analyzer.h"
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cstdio>
#include <map>

namespace
{
    // Ungefär 11 kHz: räcker för kick och bas (och ljushet upp till ~5 kHz). Sätts till den
    // verkliga frekvensen efter decimering när filen lästs (filen kan vara 44,1 eller 48 kHz).
    double analysisRate = 11025.0;

    using Signal = std::vector<float>;

    //==============================================================================
    // Filter och nivåer

    Signal filter (const Signal& input, const juce::dsp::IIR::Coefficients<float>::Ptr& coefficients, int passes)
    {
        Signal output = input;
        for (int p = 0; p < passes; ++p)
        {
            juce::dsp::IIR::Filter<float> f (coefficients);
            for (auto& s : output)
                s = f.processSample (s);
        }
        return output;
    }

    Signal lowpass (const Signal& x, double hz, int passes = 2)
    {
        return filter (x, juce::dsp::IIR::Coefficients<float>::makeLowPass (analysisRate, (float) hz), passes);
    }

    Signal highpass (const Signal& x, double hz, int passes = 2)
    {
        return filter (x, juce::dsp::IIR::Coefficients<float>::makeHighPass (analysisRate, (float) hz), passes);
    }

    Signal bandpass (const Signal& x, double low, double high) { return lowpass (highpass (x, low), high); }

    float rms (const Signal& x, int start, int end)
    {
        start = std::max (0, start);
        end = std::min ((int) x.size(), end);
        if (end <= start)
            return 0.0f;
        double sum = 0.0;
        for (int i = start; i < end; ++i)
            sum += (double) x[(size_t) i] * x[(size_t) i];
        return (float) std::sqrt (sum / (end - start));
    }

    float toDb (float gain) { return juce::Decibels::gainToDecibels (gain, -120.0f); }

    double toMs (double samples) { return samples / analysisRate * 1000.0; }

    // Analytisk signal (Hilbert-transform med FFT). Skalan spelar ingen roll: bara relativa
    // nivåer och fasens förändring används.
    std::vector<juce::dsp::Complex<float>> analyticSignal (const Signal& x)
    {
        int order = 1;
        while ((1 << order) < (int) x.size() * 2)
            ++order;
        const auto size = 1 << order;
        juce::dsp::FFT fft (order);

        std::vector<juce::dsp::Complex<float>> time ((size_t) size), freq ((size_t) size);
        for (size_t i = 0; i < x.size(); ++i)
            time[i] = { x[i], 0.0f };

        fft.perform (time.data(), freq.data(), false);
        for (int k = 1; k < size / 2; ++k)
            freq[(size_t) k] *= 2.0f;          // positiva frekvenser dubbleras
        for (int k = size / 2 + 1; k < size; ++k)
            freq[(size_t) k] = { 0.0f, 0.0f }; // negativa tas bort
        fft.perform (freq.data(), time.data(), true);

        time.resize (x.size());
        return time;
    }

    // Momentan frekvens (Hz) ur fasens förändring, utjämnad över ~1 ms. Fungerar när en ton
    // dominerar, som kickens nästan rena sinus, även medan frekvensen sveper snabbt.
    Signal instantaneousFrequency (const Signal& x)
    {
        const auto z = analyticSignal (x);
        Signal raw (x.size(), 0.0f);
        for (size_t i = 1; i < z.size(); ++i)
            raw[i] = (float) (std::arg (z[i] * std::conj (z[i - 1])) * analysisRate / juce::MathConstants<double>::twoPi);

        const auto half = std::max (1, (int) (0.0005 * analysisRate));
        Signal smoothed (x.size(), 0.0f);
        for (int i = 0; i < (int) x.size(); ++i)
        {
            double sum = 0.0;
            int count = 0;
            for (int j = std::max (1, i - half); j <= std::min ((int) x.size() - 1, i + half); ++j, ++count)
                sum += raw[(size_t) j];
            smoothed[(size_t) i] = count > 0 ? (float) (sum / count) : 0.0f;
        }
        return smoothed;
    }

    // Momentan amplitud via analytisk signal. Ger en jämn volymkurva även för låga toner,
    // där korta RMS-fönster följer vågformen i stället.
    Signal envelopeOf (const Signal& x)
    {
        const auto z = analyticSignal (x);
        Signal envelope (x.size());
        for (size_t i = 0; i < x.size(); ++i)
            envelope[i] = std::abs (z[i]);
        return envelope;
    }

    //==============================================================================
    // Toner

    juce::String noteName (double hz)
    {
        if (hz <= 0.0)
            return "-";
        const auto midi = (int) std::round (69.0 + 12.0 * std::log2 (hz / 440.0));
        return juce::MidiMessage::getMidiNoteName (midi, true, true, 5); // FL:s oktavnamn (C5 = MIDI 60)
    }

    double centsOff (double hz)
    {
        const auto exact = 69.0 + 12.0 * std::log2 (hz / 440.0);
        return (exact - std::round (exact)) * 100.0;
    }

    // Grundtonen med YIN (de Cheveigné & Kawahara 2002): skillnadsfunktion med kumulativ
    // medelnormalisering. Klarar korta fönster med bara ett par perioder, där vanlig
    // autokorrelation ofta väljer fel oktav. `confidence` är 1 - normaliserad skillnad.
    double detectPitch (const Signal& x, int start, int length, float& confidence, double minHz = 30.0, double maxHz = 400.0)
    {
        confidence = 0.0f;
        start = std::max (0, start);
        length = std::min (length, (int) x.size() - start);
        const auto minLag = std::max (2, (int) (analysisRate / maxHz));
        const auto maxLag = std::min ((int) (analysisRate / minHz), length / 2);
        if (maxLag <= minLag + 2)
            return 0.0;

        const auto window = length - maxLag;
        const auto* data = x.data() + start;

        std::vector<double> d ((size_t) maxLag + 2, 0.0);
        for (int tau = 1; tau <= maxLag + 1; ++tau)
        {
            double sum = 0.0;
            for (int i = 0; i < window; ++i)
            {
                const auto diff = (double) data[i] - data[i + tau];
                sum += diff * diff;
            }
            d[(size_t) tau] = sum;
        }

        std::vector<double> cmnd ((size_t) maxLag + 2, 1.0);
        double running = 0.0;
        for (int tau = 1; tau <= maxLag + 1; ++tau)
        {
            running += d[(size_t) tau];
            cmnd[(size_t) tau] = running > 0.0 ? d[(size_t) tau] * tau / running : 1.0;
        }

        // Första lag under tröskeln (sedan dess lokala minimum), annars det globala minimumet.
        constexpr double threshold = 0.2;
        auto best = -1;
        for (int tau = minLag; tau <= maxLag; ++tau)
        {
            if (cmnd[(size_t) tau] < threshold)
            {
                while (tau + 1 <= maxLag && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            best = minLag;
            for (int tau = minLag; tau <= maxLag; ++tau)
                if (cmnd[(size_t) tau] < cmnd[(size_t) best])
                    best = tau;
        }

        confidence = (float) std::max (0.0, 1.0 - cmnd[(size_t) best]);
        if (confidence < 0.5f)
            return 0.0;

        // Parabolisk interpolation runt minimumet.
        const auto y0 = cmnd[(size_t) best - 1], y1 = cmnd[(size_t) best], y2 = cmnd[(size_t) best + 1];
        const auto denominator = y0 - 2.0 * y1 + y2;
        const auto shift = std::abs (denominator) > 1.0e-12 ? 0.5 * (y0 - y2) / denominator : 0.0;
        return analysisRate / ((double) best + shift);
    }

    //==============================================================================
    // Inläsning

    Signal loadMono (const juce::File& file, double startSeconds, double endSeconds, double& fileSeconds)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats(); // WAV, AIFF, FLAC och (på Windows) MP3 via Windows Media

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
            return {};

        const auto rate = reader->sampleRate;
        fileSeconds = (double) reader->lengthInSamples / rate;
        const auto start = (juce::int64) (std::max (0.0, startSeconds) * rate);
        const auto end = endSeconds > 0.0 ? std::min (reader->lengthInSamples, (juce::int64) (endSeconds * rate))
                                          : reader->lengthInSamples;
        const auto factor = std::max (1, (int) std::round (rate / 11025.0));
        analysisRate = rate / factor;
        std::printf ("  fil: %.0f Hz, analyseras i %.0f Hz\n", rate, analysisRate);

        // Läs i bitar, mixa till mono, låg-passa (mot vikning) och plocka var `factor`:e sample.
        Signal output;
        output.reserve ((size_t) ((end - start) / factor + 1));
        juce::AudioBuffer<float> chunk ((int) reader->numChannels, 65536);
        const auto antiAlias = juce::dsp::IIR::Coefficients<float>::makeLowPass (rate, (float) (analysisRate * 0.42));
        juce::dsp::IIR::Filter<float> antiAlias1 (antiAlias), antiAlias2 (antiAlias);
        int phase = 0;

        for (auto position = start; position < end; position += chunk.getNumSamples())
        {
            const auto count = (int) std::min ((juce::int64) chunk.getNumSamples(), end - position);
            reader->read (&chunk, 0, count, position, true, true);
            for (int i = 0; i < count; ++i)
            {
                auto mono = 0.0f;
                for (int ch = 0; ch < chunk.getNumChannels(); ++ch)
                    mono += chunk.getSample (ch, i);
                mono /= (float) chunk.getNumChannels();
                const auto filtered = antiAlias2.processSample (antiAlias1.processSample (mono));
                if (phase++ % factor == 0)
                    output.push_back (filtered);
            }
        }
        return output;
    }

    //==============================================================================
    // Tempo och slag

    struct Beats
    {
        double bpm = 0.0;
        double samplesPerBeat = 0.0;
        std::vector<double> onsets; // sample-position (i analysfrekvensen) för varje slag
    };

    Beats findBeats (const Signal& x)
    {
        // Kickens energi: låg-pass 120 Hz, likriktning och utjämning, i ramar om 2 ms.
        const auto low = lowpass (x, 120.0);
        constexpr double frameRate = 500.0;
        const auto hop = analysisRate / frameRate;
        std::vector<float> envelope;
        auto smooth = 0.0f;
        const auto coefficient = (float) (1.0 - std::exp (-1.0 / (0.005 * analysisRate)));
        double next = 0.0;
        for (size_t i = 0; i < low.size(); ++i)
        {
            smooth += (std::abs (low[i]) - smooth) * coefficient;
            if ((double) i >= next)
            {
                envelope.push_back (smooth);
                next += hop;
            }
        }

        std::vector<float> onset (envelope.size(), 0.0f);
        for (size_t i = 1; i < envelope.size(); ++i)
            onset[i] = std::max (0.0f, envelope[i] - envelope[i - 1]);

        // Autokorrelation för 120-160 BPM, i tiondels-BPM.
        Beats beats;
        auto bestScore = -1.0;
        for (int tenths = 1200; tenths <= 1600; ++tenths)
        {
            const auto period = 60.0 / (tenths / 10.0) * frameRate;
            double score = 0.0;
            for (size_t i = 0; i < onset.size(); ++i)
            {
                const auto j = (size_t) std::round ((double) i + period);
                if (j < onset.size())
                    score += (double) onset[i] * onset[j];
            }
            if (score > bestScore)
            {
                bestScore = score;
                beats.bpm = tenths / 10.0;
            }
        }

        // Slagens fas: den förskjutning som träffar mest kickenergi.
        const auto periodFrames = 60.0 / beats.bpm * frameRate;
        auto bestPhase = 0.0;
        bestScore = -1.0;
        for (double phase = 0.0; phase < periodFrames; phase += 0.25)
        {
            double score = 0.0;
            for (auto t = phase; t < (double) onset.size(); t += periodFrames)
                score += onset[(size_t) t];
            if (score > bestScore)
            {
                bestScore = score;
                bestPhase = phase;
            }
        }

        // Varje slag: brantaste stigningen inom ±16 ms (finjusteras sedan med alignBeats).
        beats.samplesPerBeat = 60.0 / beats.bpm * analysisRate;
        for (auto t = bestPhase; t < (double) onset.size(); t += periodFrames)
        {
            auto bestFrame = (int) t;
            for (int f = (int) t - 8; f <= (int) t + 8; ++f)
                if (f >= 0 && f < (int) onset.size() && onset[(size_t) f] > onset[(size_t) bestFrame])
                    bestFrame = f;
            beats.onsets.push_back (bestFrame * hop);
        }
        return beats;
    }

    // Medelvärdet av slagen i [first, first + count), från varje slags ansats.
    Signal averageBeats (const Signal& x, const std::vector<double>& onsets, int first, int count, int length)
    {
        Signal average ((size_t) length, 0.0f);
        for (int b = first; b < first + count; ++b)
        {
            const auto start = (int) std::round (onsets[(size_t) b]);
            for (int i = 0; i < length && start + i < (int) x.size(); ++i)
                if (start + i >= 0)
                    average[(size_t) i] += x[(size_t) (start + i)];
        }
        for (auto& v : average)
            v /= (float) count;
        return average;
    }

    // Finjusterar varje slags ansats till sample-precision: flytta slaget dit där kicken
    // stämmer bäst med medelkicken (korskorrelation), och upprepa. Utan det tar snabba
    // transienter (klick, pitch-svep) ut varandra i medelvärdet.
    void alignBeats (const Signal& x, std::vector<double>& onsets, int first, int count)
    {
        const auto kickLength = (int) (0.12 * analysisRate);
        const auto maxShift = (int) (0.004 * analysisRate);
        const auto filtered = lowpass (x, 1500.0, 1);

        for (int iteration = 0; iteration < 3; ++iteration)
        {
            const auto reference = averageBeats (filtered, onsets, first, count, kickLength);
            for (int b = first; b < first + count; ++b)
            {
                const auto start = (int) std::round (onsets[(size_t) b]);
                auto bestShift = 0;
                auto bestScore = -1.0e30;
                for (int shift = -maxShift; shift <= maxShift; ++shift)
                {
                    double score = 0.0;
                    for (int i = 0; i < kickLength; ++i)
                    {
                        const auto j = start + shift + i;
                        if (j >= 0 && j < (int) filtered.size())
                            score += (double) filtered[(size_t) j] * reference[(size_t) i];
                    }
                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestShift = shift;
                    }
                }
                onsets[(size_t) b] = start + bestShift;
            }
        }
    }

    // Flyttar alla slag till kickens verkliga ansats. Slagen hittas via energin under 120 Hz,
    // men kickens svep börjar högt och når dit först efter ~10 ms, så de ligger för sent.
    // I medelslaget syns svepet i 150-2000 Hz direkt: den brantaste stigningen där (från 30 ms
    // före till 5 ms efter) är den verkliga ansatsen. Returnerar flytten i ms.
    double moveToKickStart (const Signal& x, std::vector<double>& onsets, int first, int count)
    {
        const auto pre = (int) (0.030 * analysisRate);
        const auto length = pre + (int) (0.010 * analysisRate);
        const auto band = bandpass (x, 150.0, 2000.0);

        std::vector<double> shifted (onsets.begin(), onsets.end());
        for (auto& o : shifted)
            o -= pre;
        const auto envelope = envelopeOf (averageBeats (band, shifted, first, count, length));

        auto bestRise = 0.0f;
        auto bestIndex = pre;
        const auto step = std::max (1, (int) (0.0005 * analysisRate));
        for (int i = step; i < length; ++i)
        {
            const auto rise = envelope[(size_t) i] - envelope[(size_t) (i - step)];
            if (rise > bestRise)
            {
                bestRise = rise;
                bestIndex = i - step; // där stigningen börjar
            }
        }

        const auto offset = bestIndex - pre;
        for (int b = first; b < first + count; ++b)
            onsets[(size_t) b] += offset;
        return toMs (offset);
    }

    //==============================================================================
    // Översikt och val av parti

    struct Section
    {
        int firstBeat = 0, numBeats = 0;
    };

    Section overview (const Signal& x, const Beats& beats, double offsetSeconds)
    {
        const auto low = lowpass (x, 150.0);
        const auto high = highpass (x, 2000.0);

        std::puts ("\nOVERSIKT (per 4 takter): tid, bas-energi, diskant i forhallande till bas");
        constexpr int beatsPerBlock = 16;
        Section best;
        auto bestScore = 1.0e9f;

        for (size_t b = 0; b + beatsPerBlock < beats.onsets.size(); b += beatsPerBlock)
        {
            const auto start = (int) beats.onsets[b];
            const auto end = (int) beats.onsets[b + beatsPerBlock];
            const auto lowDb = toDb (rms (low, start, end));
            const auto ratioDb = toDb (rms (high, start, end)) - lowDb;
            const auto seconds = offsetSeconds + start / analysisRate;

            std::printf ("  %2d:%02d  bas %6.1f dB  diskant %+6.1f dB  %s\n",
                         (int) seconds / 60, (int) seconds % 60, lowDb, ratioDb,
                         lowDb > -30.0f && ratioDb < -18.0f ? "<- mest kick+bas" : "");

            // Bästa partiet: stark bas, lite diskant. 8 takter (två block) när det går.
            if (lowDb > -30.0f && ratioDb < bestScore)
            {
                bestScore = ratioDb;
                best = { (int) b, (int) std::min ((size_t) 2 * beatsPerBlock, beats.onsets.size() - b - 1) };
            }
        }
        return best;
    }

    //==============================================================================
    // Kicken

    void analyzeKick (const Signal& averageBeat, double sixteenth)
    {
        std::puts ("\nKICK (medelslag)");
        const auto kickRegion = (int) std::min ((double) averageBeat.size(), sixteenth);

        // Amplitud över tid (kroppen, under 250 Hz), relativt toppen, via analytisk signal.
        const auto bodyEnvelope = envelopeOf (lowpass (averageBeat, 250.0));
        auto peak = 0.0f;
        int peakAt = 0;
        for (int i = 0; i < kickRegion; ++i)
            if (bodyEnvelope[(size_t) i] > peak)
            {
                peak = bodyEnvelope[(size_t) i];
                peakAt = i;
            }

        // När kicken fallit 12 dB: efter det kan basen dominera, så svansen mäts före det.
        auto audibleEnd = kickRegion;
        for (int i = peakAt; i < kickRegion; ++i)
            if (toDb (bodyEnvelope[(size_t) i]) < toDb (peak) - 12.0f)
            {
                audibleEnd = i;
                break;
            }

        // Pitch-svep: momentan frekvens i det låg-passade medelslaget (kicken är nästan en sinus).
        const auto frequency = instantaneousFrequency (lowpass (averageBeat, 800.0));
        const auto sweepEnvelope = envelopeOf (lowpass (averageBeat, 800.0));
        auto sweepPeak = 0.0f;
        for (int i = 0; i < kickRegion; ++i)
            sweepPeak = std::max (sweepPeak, sweepEnvelope[(size_t) i]);

        std::printf ("  pitch-svep:");
        for (const auto ms : { 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 10.0, 13.0, 16.0, 20.0, 25.0, 30.0, 40.0, 50.0, 60.0, 80.0 })
        {
            const auto i = (int) (ms * 0.001 * analysisRate);
            if (i < audibleEnd && toDb (sweepEnvelope[(size_t) i]) > toDb (sweepPeak) - 20.0f)
                std::printf (" %.0fms:%.0fHz", ms, frequency[(size_t) i]);
        }
        std::puts ("");

        // Svansen: median av frekvensen från 25 ms tills kicken fallit 12 dB.
        std::vector<float> tail;
        for (int i = (int) (0.025 * analysisRate); i < audibleEnd; ++i)
            tail.push_back (frequency[(size_t) i]);
        if (! tail.empty())
        {
            std::sort (tail.begin(), tail.end());
            const auto hz = (double) tail[tail.size() / 2];
            std::printf ("  svans (25-%.0f ms): %.1f Hz = %s (%+.0f cent)\n", toMs (audibleEnd), hz, noteName (hz).toRawUTF8(), centsOff (hz));

            // Svepets längd: när frekvensen är inom 1 halvton från svansen.
            auto settled = -1;
            for (int i = (int) (0.001 * analysisRate); i < audibleEnd; ++i)
                if (frequency[(size_t) i] < hz * 1.06)
                {
                    settled = i;
                    break;
                }
            const auto startHz = frequency[(size_t) (0.001 * analysisRate)];
            std::printf ("  svep: %.0f Hz vid 1 ms (%.1f halvtoner over svansen), inom 1 halvton efter %.1f ms\n",
                         startHz, 12.0 * std::log2 (std::max (1.0f, startHz) / hz), settled >= 0 ? toMs (settled) : -1.0);
        }

        std::printf ("  envelope (dB fran topp, topp vid %.1f ms):", toMs (peakAt));
        for (const auto ms : { 1, 2, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 })
        {
            const auto i = (int) (ms * 0.001 * analysisRate);
            if (i < kickRegion)
                std::printf (" %d:%.0f", ms, toDb (bodyEnvelope[(size_t) i]) - toDb (peak));
        }
        std::puts ("");

        // När kroppen fallit 6, 12, 20 och 30 dB från toppen (inom första sextondelen).
        std::printf ("  fall:");
        for (const auto drop : { 6.0f, 12.0f, 20.0f, 30.0f })
        {
            auto at = -1;
            for (int i = peakAt; i < kickRegion; ++i)
                if (toDb (bodyEnvelope[(size_t) i]) < toDb (peak) - drop)
                {
                    at = i;
                    break;
                }
            if (at >= 0)
                std::printf (" -%.0fdB@%.0fms", drop, toMs (at));
            else
                std::printf (" -%.0fdB@>%.0fms", drop, toMs (kickRegion));
        }
        std::puts ("");

        // Klick: diskanten (över 2 kHz) de första 6 ms jämfört med kroppens topp.
        const auto highEnvelope = envelopeOf (highpass (averageBeat, 2000.0));
        auto clickPeak = 0.0f;
        for (int i = 0; i < (int) (0.006 * analysisRate); ++i)
            clickPeak = std::max (clickPeak, highEnvelope[(size_t) i]);
        auto clickEnd = 0;
        for (int i = 0; i < kickRegion; ++i)
            if (toDb (highEnvelope[(size_t) i]) > toDb (clickPeak) - 20.0f)
                clickEnd = i;
        std::printf ("  klick (>2 kHz): topp %.1f dB rel. kroppen, varar ~%.1f ms\n",
                     toDb (clickPeak) - toDb (peak), toMs (clickEnd));
    }

    //==============================================================================
    // Basen i medelslaget

    void printProfile (const char* label, const Signal& envelope, float reference, double sixteenth)
    {
        std::printf ("  %-6s:", label);
        const auto stepSamples = (int) (0.01 * analysisRate);
        for (int i = 0; i < (int) envelope.size(); i += stepSamples)
        {
            if (i > 0 && (int) (i / sixteenth) != (int) ((i - stepSamples) / sixteenth))
                std::printf (" |");
            std::printf (" %3.0f", toDb (envelope[(size_t) i]) - toDb (reference));
        }
        std::puts ("");
    }

    void analyzeBassSteps (const Signal& averageBeat, double sixteenth)
    {
        const auto bass = bandpass (averageBeat, 35.0, 350.0);
        const auto bassEnvelope = envelopeOf (bass);
        const auto brightEnvelope = envelopeOf (bandpass (averageBeat, 300.0, 1500.0)); // under hihatsen
        const auto kickEnvelope = envelopeOf (lowpass (averageBeat, 250.0));

        auto kickPeak = 0.0f;
        for (int i = 0; i < (int) sixteenth; ++i)
            kickPeak = std::max (kickPeak, kickEnvelope[(size_t) i]);

        std::puts ("\nVOLYMPROFIL (medelslag, dB rel. kickens topp, var 10:e ms; | = sextondel)");
        printProfile ("bas", bassEnvelope, kickPeak, sixteenth);
        printProfile ("ljus", brightEnvelope, kickPeak, sixteenth); // 300-1500 Hz

        // Basens ansatser: där basbandets volym stiger brantast (efter kickens kropp), och vilket
        // rutnät de stämmer med: sextondelar (1/4, 2/4, 3/4 av slaget) eller åttondelstrioler (1/3, 2/3).
        {
            const auto smoothing = std::max (1, (int) (0.003 * analysisRate));
            Signal smooth (bassEnvelope.size(), 0.0f);
            for (int i = 0; i < (int) smooth.size(); ++i)
            {
                double sum = 0.0;
                int n = 0;
                for (int j = std::max (0, i - smoothing); j <= std::min ((int) smooth.size() - 1, i + smoothing); ++j, ++n)
                    sum += bassEnvelope[(size_t) j];
                smooth[(size_t) i] = (float) (sum / n);
            }

            std::vector<std::pair<float, int>> rises;
            const auto gap = (int) (0.006 * analysisRate);
            for (int i = (int) (0.06 * analysisRate); i + gap < (int) smooth.size() - gap; ++i)
            {
                const auto rise = smooth[(size_t) (i + gap)] - smooth[(size_t) (i - gap)];
                const auto previous = smooth[(size_t) (i + gap - 1)] - smooth[(size_t) (i - gap - 1)];
                const auto next = smooth[(size_t) (i + gap + 1)] - smooth[(size_t) (i - gap + 1)];
                if (rise > 0.0f && rise >= previous && rise >= next)
                    rises.push_back ({ rise, i });
            }
            std::sort (rises.rbegin(), rises.rend());

            const auto beat = 4.0 * sixteenth;
            std::printf ("\nBASENS ANSATSER (starkast forst; tid i slaget, narmaste 1/16 och 1/8T):\n ");
            for (size_t k = 0; k < std::min ((size_t) 6, rises.size()); ++k)
            {
                const auto at = rises[k].second;
                const auto fraction = at / beat;
                const auto nearestSixteenth = std::round (fraction * 4.0) / 4.0;
                const auto nearestTriplet = std::round (fraction * 3.0) / 3.0;
                std::printf (" %.0fms (1/16: %+.0fms, 1/8T: %+.0fms)", toMs (at),
                             toMs ((fraction - nearestSixteenth) * beat), toMs ((fraction - nearestTriplet) * beat));
            }
            std::puts ("");
        }

        std::puts ("\nBAS (medelslag, sextondel 2-4 i slaget)");
        for (int step = 1; step < 4; ++step)
        {
            const auto start = (int) (step * sixteenth);
            const auto end = std::min ((int) ((step + 1) * sixteenth), (int) bassEnvelope.size());

            float confidence = 0.0f;
            const auto hz = detectPitch (bass, start + (int) (0.006 * analysisRate), end - start - (int) (0.012 * analysisRate), confidence);

            auto peak = 0.0f;
            int peakAt = start;
            for (int i = start; i < end; ++i)
                if (bassEnvelope[(size_t) i] > peak)
                {
                    peak = bassEnvelope[(size_t) i];
                    peakAt = i;
                }

            // Tonens början och slut: när den går över/under -12 dB från stegets topp.
            auto noteStart = start, noteEnd = end;
            for (int i = start; i < peakAt; ++i)
                if (toDb (bassEnvelope[(size_t) i]) > toDb (peak) - 12.0f) { noteStart = i; break; }
            for (int i = peakAt; i < end; ++i)
                if (toDb (bassEnvelope[(size_t) i]) < toDb (peak) - 12.0f) { noteEnd = i; break; }

            // Ljusheten i tonens början och 40 ms in (filter-envelope).
            const auto early = std::min (end - 1, peakAt + (int) (0.005 * analysisRate));
            const auto late = std::min (end - 1, peakAt + (int) (0.040 * analysisRate));
            const auto brightEarly = toDb (brightEnvelope[(size_t) early]) - toDb (bassEnvelope[(size_t) early]);
            const auto brightLate = toDb (brightEnvelope[(size_t) late]) - toDb (bassEnvelope[(size_t) late]);

            std::printf ("  steg %d: %-4s %5.1f Hz (%+3.0f c, saker %.2f)  topp %+5.1f dB rel. kick vid %3.0f ms  ton %3.0f-%3.0f ms  ljushet %+3.0f -> %+3.0f dB\n",
                         step + 1, noteName (hz).toRawUTF8(), hz, hz > 0 ? centsOff (hz) : 0.0, confidence,
                         toDb (peak) - toDb (kickPeak), toMs (peakAt - start), toMs (noteStart - start), toMs (noteEnd - start),
                         brightEarly, brightLate);
        }
    }

    //==============================================================================
    // Basgången per position i takten: ett medelslag för varje slag 1-4 (räknat från partiets
    // början), så att toner som bara spelas på t.ex. slag 3 inte jämnas ut av de andra.

    void analyzeBeatPositions (const Signal& x, const Beats& beats, const Section& section)
    {
        std::puts ("\nBASGANG PER SLAG I TAKTEN (medel per position; slag 1 = partiets forsta slag)");
        const auto bass = bandpass (x, 35.0, 350.0);
        const auto sixteenth = beats.samplesPerBeat / 4.0;
        const auto length = (int) beats.samplesPerBeat;

        for (int position = 0; position < 4; ++position)
        {
            // Medel av alla slag på den här positionen.
            Signal average ((size_t) length, 0.0f);
            int count = 0;
            for (int b = section.firstBeat + position; b < section.firstBeat + section.numBeats; b += 4, ++count)
            {
                const auto start = (int) std::round (beats.onsets[(size_t) b]);
                for (int i = 0; i < length && start + i < (int) bass.size(); ++i)
                    average[(size_t) i] += bass[(size_t) (start + i)];
            }
            if (count == 0)
                continue;
            for (auto& v : average)
                v /= (float) count;

            const auto envelope = envelopeOf (average);
            std::printf ("  slag %d (%2d st):", position + 1, count);
            for (int step = 1; step < 4; ++step)
            {
                const auto start = (int) (step * sixteenth);
                const auto end = std::min ((int) ((step + 1) * sixteenth), length);

                auto peak = 0.0f;
                for (int i = start; i < end; ++i)
                    peak = std::max (peak, envelope[(size_t) i]);

                float confidence = 0.0f;
                const auto hz = detectPitch (average, start + (int) (0.006 * analysisRate), end - start - (int) (0.010 * analysisRate), confidence);
                std::printf ("   steg %d: %-4s %5.1f Hz (%.2f) %4.0f dB", step + 1, noteName (hz).toRawUTF8(), hz, confidence, toDb (peak));
            }
            std::puts ("");
        }
    }

    //==============================================================================
    // Basgången takt för takt

    void analyzeBassline (const Signal& x, const Beats& beats, const Section& section)
    {
        std::puts ("\nBASGANG (sextondelar per takt; '.' = tyst/osaker, K = kicken)");
        const auto bass = bandpass (x, 35.0, 350.0);
        const auto sixteenth = beats.samplesPerBeat / 4.0;
        std::map<juce::String, int> noteCount;

        for (auto bar = section.firstBeat; bar + 4 <= section.firstBeat + section.numBeats; bar += 4)
        {
            std::printf ("  %6.2fs |", beats.onsets[(size_t) bar] / analysisRate);
            for (int step = 0; step < 16; ++step)
            {
                if (step % 4 == 0)
                {
                    std::printf (" K   ");
                    continue;
                }
                const auto beatStart = beats.onsets[(size_t) (bar + step / 4)];
                const auto start = (int) (beatStart + (step % 4) * sixteenth + 0.006 * analysisRate);
                const auto length = (int) (sixteenth - 0.012 * analysisRate);
                float confidence = 0.0f;
                const auto hz = detectPitch (bass, start, length, confidence);
                if (hz > 0.0 && confidence > 0.6f)
                {
                    const auto name = noteName (hz);
                    ++noteCount[name];
                    std::printf (" %-4s", name.toRawUTF8());
                }
                else
                    std::printf (" .   ");
                if (step % 4 == 3)
                    std::printf ("|");
            }
            std::puts ("");
        }

        std::printf ("  vanligaste toner:");
        std::vector<std::pair<int, juce::String>> sorted;
        for (const auto& [name, count] : noteCount)
            sorted.push_back ({ count, name });
        std::sort (sorted.rbegin(), sorted.rend());
        for (size_t i = 0; i < std::min ((size_t) 6, sorted.size()); ++i)
            std::printf (" %s(%d)", sorted[i].second.toRawUTF8(), sorted[i].first);
        std::puts ("");
    }
}

int analyzeFile (const juce::File& file, double startSeconds, double endSeconds)
{
    double fileSeconds = 0.0;
    std::printf ("%s\n", file.getFileName().toRawUTF8());
    const auto x = loadMono (file, startSeconds, endSeconds, fileSeconds);
    if (x.empty())
    {
        std::printf ("Kunde inte lasa %s\n", file.getFullPathName().toRawUTF8());
        return 1;
    }

    const auto offset = std::max (0.0, startSeconds);
    std::printf ("  langd %.0f s, analyserar %.1f s fran %.1f s\n", fileSeconds, x.size() / analysisRate, offset);

    auto beats = findBeats (x);
    std::printf ("  tempo: %.1f BPM, %d slag\n", beats.bpm, (int) beats.onsets.size());
    if (beats.onsets.size() < 8)
        return 1;

    auto section = overview (x, beats, offset);
    if ((startSeconds >= 0.0 && endSeconds > 0.0) || section.numBeats < 4)
        section = { 0, (int) beats.onsets.size() - 1 }; // användaren valde partiet själv

    const auto from = offset + beats.onsets[(size_t) section.firstBeat] / analysisRate;
    const auto to = offset + beats.onsets[(size_t) (section.firstBeat + section.numBeats)] / analysisRate;
    std::printf ("\nPARTI: %d:%05.2f - %d:%05.2f (%d slag)\n", (int) from / 60, std::fmod (from, 60.0),
                 (int) to / 60, std::fmod (to, 60.0), section.numBeats);

    // Medelslaget: alla slag i partiet ovanpå varandra, från kickens sample-exakta ansats.
    alignBeats (x, beats.onsets, section.firstBeat, section.numBeats);
    const auto moved = moveToKickStart (x, beats.onsets, section.firstBeat, section.numBeats);
    std::printf ("  kickens ansats: slagen flyttade %+.1f ms (fran basenergin till svepets borjan)\n", moved);
    const auto average = averageBeats (x, beats.onsets, section.firstBeat, section.numBeats, (int) beats.samplesPerBeat);

    const auto sixteenth = beats.samplesPerBeat / 4.0;
    analyzeKick (average, sixteenth);
    analyzeBassSteps (average, sixteenth);
    analyzeBeatPositions (x, beats, section);
    analyzeBassline (x, beats, section);
    return 0;
}
