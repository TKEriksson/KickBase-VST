// `tool antest`: syntetiska tester av mixanalysens (AN) regler. Varje regel ska slå till på en signal som har felet och inte på en ren signal
// (tyst, ren sinus, kick+bas utan kollision), och trösklarna testas strax på var sida om gränsen. Trösklarna ändras inte här: ser en regel
// konstig ut skrivs en rad "OBS:" (påverkar inte resultatet) som projektledaren kan läsa.

#include "../Source/Core/MixAnalysis.h"

namespace
{
    using namespace MixAnalysis;

    constexpr int rate = 48000, barLength = 96000; // en takt vid 120 bpm

    bool hasTitle (const Report& report, const char* part)
    {
        for (const auto& finding : report.findings)
            if (finding.title.contains (part))
                return true;
        return false;
    }

    bool hasCategory (const Report& report, const char* category)
    {
        for (const auto& finding : report.findings)
            if (finding.category == category)
                return true;
        return false;
    }

    Severity severityOf (const Report& report, const char* part)
    {
        for (const auto& finding : report.findings)
            if (finding.title.contains (part))
                return finding.severity;
        return Severity::info;
    }

    bool hasId (const PhaseDescription& d, const char* id)
    {
        for (auto& item : d.findings)
            if (item.getProperty ("id", {}).toString() == id)
                return true;
        return false;
    }

    // Spektrumunderlag: två takter, 32 ramar, alla band tysta. Kick, instrument A (bas) och mastern finns.
    Capture makeCapture()
    {
        Capture c;
        c.rate = rate;
        c.bpm = 140.0;
        c.bars = 2;
        c.resize (32);
        for (int f = 0; f < 32; ++f)
            c.frameBar[(size_t) f] = f / 16;
        c.present[(size_t) kickSource] = c.present[(size_t) firstInstrument] = c.present[(size_t) masterSource] = true;
        c.names[(size_t) kickSource] = "Kick";
        c.names[(size_t) firstInstrument] = "A";
        c.names[(size_t) masterSource] = "Master";
        return c;
    }

    void setBand (Capture& c, int source, int b, float db)
    {
        for (int f = 0; f < c.frames; ++f)
            c.band (source, f, b) = db;
    }

    // Fasmått som om 32 sextondelar mättes: `active` av dem hörs, alla med korrelationen `corr`.
    PhaseMetrics makeMetrics (float corr, int active, float sumVsMax = 0.0f)
    {
        PhaseMetrics m;
        m.valid = true;
        m.lowCorrelation.assign (32, corr);
        m.lowActive.assign (32, 0);
        for (int i = 0; i < active; ++i)
            m.lowActive[(size_t) i] = 1;
        m.sumVsMaxDb = sumVsMax;
        m.stereoCorrelation.assign (2, { 1.0f, 1.0f, 1.0f, 1.0f });
        m.midSideDb.assign (2, { 0.0f, 0.0f, 0.0f, 0.0f });
        return m;
    }

    // Kick på 50 Hz (en sinus som klingar ut) och en bas; ett slag var 0,5 s. `hitGain` skalar varje slag, `tailDb` > -200 sätter nivån där slaget slutar.
    PhaseInput makeSignals (float bassSign, double bassHz, std::vector<float> hitGain = { 1.0f, 1.0f, 1.0f, 1.0f }, double tailDb = -200.0)
    {
        PhaseInput in;
        in.rate = rate;
        in.samplesPerBar = barLength;
        in.bars = 1;
        in.onsets = { 0, 24000, 48000, 72000 };
        const auto sampleEnd = std::abs (std::sin (juce::MathConstants<double>::twoPi * 50.0 * 23999.0 / rate)); // kicken slutar nära ett nollgenomgång
        const auto tau = tailDb > -199.0 ? -(23999.0 / rate) / std::log (std::pow (10.0, tailDb / 20.0) / (0.5 * sampleEnd)) : 0.03;
        std::vector<float> kick ((size_t) barLength), bass ((size_t) barLength);
        for (int s = 0; s < barLength; ++s)
        {
            const auto t = (double) (s % 24000) / rate;
            const auto env = std::exp (-t / tau) * hitGain[(size_t) (s / 24000)];
            kick[(size_t) s] = 0.5f * (float) (std::sin (juce::MathConstants<double>::twoPi * 50.0 * t) * env);
            bass[(size_t) s] = bassSign * 0.5f * (float) (std::sin (juce::MathConstants<double>::twoPi * bassHz * t) * std::exp (-t / 0.03));
        }
        in.kickL = in.kickR = kick;
        in.bassL = in.bassR = bass;
        in.masterL = in.masterR = kick;
        return in;
    }

    // Mastern med olika signal i vänster och höger: en 60 Hz-ton i båda och en 80 Hz-ton (ortogonal mot den, helt antal perioder per takt) med `side` i höger.
    PhaseInput makeStereo (float side)
    {
        auto in = makeSignals (1.0f, 100.0);
        for (int s = 0; s < barLength; ++s)
        {
            const auto t = (double) s / rate;
            const auto a = (float) std::sin (juce::MathConstants<double>::twoPi * 60.0 * t);
            const auto b = (float) std::sin (juce::MathConstants<double>::twoPi * 80.0 * t);
            in.masterL[(size_t) s] = 0.3f * a;
            in.masterR[(size_t) s] = 0.3f * (a + side * b);
        }
        return in;
    }
}

int anTest()
{
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what)
    {
        std::puts ((juce::String (ok ? "ok:  " : "FEL: ") + what).toRawUTF8());
        failed = failed || ! ok;
    };
    auto note = [] (const juce::String& what) { std::puts (("OBS: " + what).toRawUTF8()); };
    const Options options;

    // --- Tysta signaler: ingen regel får slå till.
    {
        auto c = makeCapture();
        c.phase = measurePhase (makeSignals (1.0f, 100.0, { 0.0f, 0.0f, 0.0f, 0.0f }));
        auto silent = makeSignals (1.0f, 100.0);
        for (auto* v : { &silent.kickL, &silent.kickR, &silent.bassL, &silent.bassR, &silent.masterL, &silent.masterR })
            std::fill (v->begin(), v->end(), 0.0f);
        c.phase = measurePhase (silent);
        const auto report = evaluate (c, options, nullptr);
        check (report.valid && c.phase.valid, "tyst: analysen är giltig");
        check (report.findings.empty(), "tyst: inga fynd alls (" + juce::String ((int) report.findings.size()) + " fynd)");
        for (const auto& finding : report.findings)
            note ("tyst gav ändå: " + finding.title);
        const auto text = describePhase (c.phase);
        check (text.findings.isEmpty() && text.fixes.isEmpty(), "tyst: describePhase ger inga fynd eller förslag");
        check (c.phase.hits.size() == 4 && c.phase.clickHits == 0 && c.phase.peakSpreadDb == 0.0f, "tyst: inga klick och ingen toppspridning");
    }

    // --- Ren sinus (1 kHz) i mastern och i A: ett band, inget annat.
    {
        auto c = makeCapture();
        setBand (c, firstInstrument, 16, -10.0f);
        setBand (c, masterSource, 16, -10.0f);
        PhaseInput in;
        in.rate = rate;
        in.samplesPerBar = barLength;
        in.bars = 1;
        in.kickL.assign ((size_t) barLength, 0.0f);
        in.kickR = in.bassL = in.bassR = in.kickL;
        in.masterL = in.masterR = in.kickL;
        for (int s = 0; s < barLength; ++s)
            in.masterL[(size_t) s] = in.masterR[(size_t) s] = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 1000.0 * s / rate);
        c.phase = measurePhase (in);
        const auto report = evaluate (c, options, nullptr);
        check (report.valid && report.findings.empty(), "ren sinus 1 kHz: inga fynd (" + juce::String ((int) report.findings.size()) + " fynd)");
        for (const auto& finding : report.findings)
            note ("sinus gav ändå: " + finding.title);
        check (describePhase (c.phase).findings.isEmpty(), "ren sinus: describePhase ger inga fynd");
    }

    // --- Kick + bas utan kollision: kicken i 50 Hz-bandet, basen i 80-100 Hz, mono, kicken klingar ut.
    {
        auto c = makeCapture();
        setBand (c, kickSource, 3, -10.0f);
        setBand (c, firstInstrument, 5, -10.0f);
        setBand (c, masterSource, 3, -10.0f);
        setBand (c, masterSource, 5, -10.0f);
        c.phase = measurePhase (makeSignals (1.0f, 100.0));
        const auto report = evaluate (c, options, nullptr);
        check (c.phase.valid && c.phase.lowCorrelation[0] > -0.3f, "kick+bas utan kollision: korrelationen " + juce::String (c.phase.lowCorrelation[0], 2) + " är över -0,3");
        for (auto* category : { "phase", "stereo", "mud", "master", "sub", "collision" })
            check (! hasCategory (report, category), juce::String ("kick+bas utan kollision: inget fynd i ") + category);
        for (const auto& finding : report.findings)
            note ("kick+bas utan kollision gav: [" + finding.category + "] " + finding.title);
        check (describePhase (c.phase).findings.isEmpty(), "kick+bas utan kollision: describePhase ger inga fynd");
    }

    // --- Kollision: samma signal i motfas (fas), samma band (spektrum).
    {
        auto c = makeCapture();
        c.phase = measurePhase (makeSignals (-1.0f, 50.0));
        const auto report = evaluate (c, options, nullptr);
        check (hasTitle (report, "partly cancel") && severityOf (report, "partly cancel") == Severity::problem, "kick och bas i motfas: fasfynd, allvarligt");
        check (hasId (describePhase (c.phase), "kick_bass_cancel"), "kick och bas i motfas: describePhase ger kick_bass_cancel");

        auto spectral = makeCapture();
        spectral.present[(size_t) firstInstrument + 1] = true;
        spectral.names[(size_t) firstInstrument + 1] = "B";
        setBand (spectral, firstInstrument, 6, -10.0f);
        setBand (spectral, firstInstrument + 1, 6, -10.0f);
        check (hasCategory (evaluate (spectral, options, nullptr), "collision"), "två instrument i samma band: krock");
        setBand (spectral, firstInstrument + 1, 8, -10.0f);
        setBand (spectral, firstInstrument + 1, 6, -120.0f);
        check (! hasCategory (evaluate (spectral, options, nullptr), "collision"), "två instrument i olika band: ingen krock");
    }

    // --- Fas: trösklar på mätvärdena (30-150 Hz, medelkorrelation -0,3 / -0,6, summa -3 / -6 dB, minst 4 hörbara sextondelar).
    {
        auto run = [&options] (const PhaseMetrics& m, bool& problem)
        {
            auto c = makeCapture();
            c.phase = m;
            const auto r = evaluate (c, options, nullptr);
            problem = severityOf (r, "partly cancel") == Severity::problem;
            return hasTitle (r, "partly cancel") && hasId (describePhase (m), "kick_bass_cancel");
        };
        bool problem = false;
        check (! run (makeMetrics (-0.29f, 8), problem), "fas: medelkorrelation -0,29 slår inte till");
        check (run (makeMetrics (-0.31f, 8), problem) && ! problem, "fas: medelkorrelation -0,31 ger en varning");
        check (run (makeMetrics (-0.59f, 8), problem) && ! problem, "fas: medelkorrelation -0,59 är fortfarande en varning");
        check (run (makeMetrics (-0.61f, 8), problem) && problem, "fas: medelkorrelation -0,61 är ett problem");
        check (! run (makeMetrics (0.0f, 8, -2.9f), problem), "fas: summan -2,9 dB slår inte till");
        check (run (makeMetrics (0.0f, 8, -3.1f), problem) && ! problem, "fas: summan -3,1 dB ger en varning");
        check (run (makeMetrics (0.0f, 8, -6.1f), problem) && problem, "fas: summan -6,1 dB är ett problem");
        check (! run (makeMetrics (-1.0f, 3), problem), "fas: tre hörbara sextondelar räcker inte, hur dålig korrelationen än är");
        check (run (makeMetrics (-1.0f, 4), problem), "fas: fyra hörbara sextondelar räcker");
        check (! run (makeMetrics (-1.0f, 0), problem), "fas: inga hörbara sextondelar ger inget fynd");
    }

    // --- Klick, toppspridning, stereo: på mätvärdena och på signaler.
    {
        auto evaluateMetrics = [&options] (const PhaseMetrics& m)
        {
            auto c = makeCapture();
            c.phase = m;
            return evaluate (c, options, nullptr);
        };
        auto m = makeMetrics (0.0f, 0);
        m.hits.resize (4);
        m.clickHits = 1;
        check (hasTitle (evaluateMetrics (m), "may click") && hasId (describePhase (m), "kick_click"), "klick: ett slag som slutar över -60 dB ger fynd");
        m.clickHits = 0;
        check (! hasTitle (evaluateMetrics (m), "may click") && ! hasId (describePhase (m), "kick_click"), "klick: inga klickslag ger inget fynd");

        m.peakSpreadDb = 1.0f;
        check (! hasTitle (evaluateMetrics (m), "peak level varies") && ! hasId (describePhase (m), "kick_peak_spread"), "toppspridning: exakt 1,0 dB slår inte till (gränsen är > 1)");
        m.peakSpreadDb = 1.1f;
        check (hasTitle (evaluateMetrics (m), "peak level varies") && hasId (describePhase (m), "kick_peak_spread"), "toppspridning: 1,1 dB ger ett info-fynd");
        m.hits.resize (1);
        check (! hasTitle (evaluateMetrics (m), "peak level varies"), "toppspridning: ett enda slag ger inget fynd");

        auto stereo = makeMetrics (0.0f, 0);
        stereo.stereoCorrelation[1][0] = 0.31f;
        check (! hasCategory (evaluateMetrics (stereo), "stereo") && ! hasId (describePhase (stereo), "low_end_wide"), "stereo: korrelation 0,31 under 120 Hz slår inte till");
        stereo.stereoCorrelation[1][0] = 0.29f;
        check (hasCategory (evaluateMetrics (stereo), "stereo") && hasId (describePhase (stereo), "low_end_wide"), "stereo: korrelation 0,29 ger en varning");
        stereo.stereoCorrelation[1][0] = 1.0f;
        stereo.stereoCorrelation[1][2] = -1.0f; // bara de högre banden
        check (! hasCategory (evaluateMetrics (stereo), "stereo"), "stereo: breda mellan- och diskantband räknas inte (regeln gäller basen)");

        // På riktiga signaler: kick med varierande slag, kick som klipps av, bred bas.
        check (measurePhase (makeSignals (1.0f, 100.0)).peakSpreadDb < 0.1f, "toppspridning: lika hårda slag ger 0 dB");
        check (measurePhase (makeSignals (1.0f, 100.0, { 1.0f, 1.0f, 0.5f, 1.0f })).peakSpreadDb > 5.0f, "toppspridning: ett slag på halva nivån ger ca 6 dB");
        const auto mild = measurePhase (makeSignals (1.0f, 100.0, { 1.0f, 0.9f, 1.0f, 1.0f })).peakSpreadDb;
        check (mild > 0.5f && mild < 1.0f, "toppspridning: ett slag 0,9 gånger så starkt (" + juce::String (mild, 2) + " dB) ligger under gränsen 1 dB");
        const auto stronger = measurePhase (makeSignals (1.0f, 100.0, { 1.0f, 0.85f, 1.0f, 1.0f })).peakSpreadDb;
        check (stronger > 1.0f, "toppspridning: ett slag 0,85 gånger så starkt (" + juce::String (stronger, 2) + " dB) ligger över gränsen");

        check (measurePhase (makeSignals (1.0f, 100.0, { 1.0f, 1.0f, 1.0f, 1.0f }, -50.0)).clickHits >= 3, "klick: ett slag som slutar på -50 dB ger klick");
        check (measurePhase (makeSignals (1.0f, 100.0, { 1.0f, 1.0f, 1.0f, 1.0f }, -70.0)).clickHits == 0, "klick: ett slag som slutar på -70 dB ger inget klick");

        auto wideInput = makeStereo (5.0f), mildInput = makeStereo (1.5f);
        const auto wide = measurePhase (wideInput), mono = measurePhase (makeSignals (1.0f, 100.0)), mildWide = measurePhase (mildInput);
        check (wide.stereoCorrelation[0][0] < 0.3f, "stereo: höger = vänster + 5 gånger en ortogonal ton ger korrelation " + juce::String (wide.stereoCorrelation[0][0], 2) + " (< 0,3)");
        check (mildWide.stereoCorrelation[0][0] > 0.3f, "stereo: 1,5 gånger ger korrelation " + juce::String (mildWide.stereoCorrelation[0][0], 2) + " (> 0,3)");
        check (mono.stereoCorrelation[0][0] > 0.99f, "stereo: mono ger korrelation 1");
        auto c = makeCapture();
        setBand (c, firstInstrument, 5, -10.0f); // evaluate avbryter med "Nothing plays" om ingen källa är starkare än -75 dB
        c.phase = wide;
        check (hasCategory (evaluate (c, options, nullptr), "stereo"), "stereo: bred bas ger fynd i evaluate");
    }

    // --- Limitern: 95:e percentilen över ramarna, gränsen är 6 dB (varning) och 9 dB (problem). Utan ramvärden gäller maxet.
    {
        auto run = [&options] (float db)
        {
            auto c = makeCapture();
            setBand (c, firstInstrument, 5, -10.0f); // se stereo ovan: tyst underlag ger bara ett felmeddelande
            c.limiterMaxReductionDb = db;
            return evaluate (c, options, nullptr);
        };
        check (! hasCategory (run (0.0f), "master"), "limiter: ingen sänkning ger inget fynd");
        check (! hasCategory (run (5.9f), "master"), "limiter: 5,9 dB slår inte till");
        check (hasCategory (run (6.0f), "master") && severityOf (run (6.0f), "limiter") == Severity::warning, "limiter: 6,0 dB ger en varning");
        check (severityOf (run (8.9f), "limiter") == Severity::warning, "limiter: 8,9 dB är fortfarande en varning");
        check (severityOf (run (9.0f), "limiter") == Severity::problem, "limiter: 9,0 dB är ett problem");

        // Percentil: ett enda ramvärde på 12 dB (av 32) räcker inte, flera höga ramar gör det.
        auto runFrames = [&options] (int highFrames)
        {
            auto c = makeCapture();
            setBand (c, firstInstrument, 5, -10.0f);
            c.frameLimiterDb.assign ((size_t) c.frames, 0.0f);
            for (int i = 0; i < highFrames; ++i)
                c.frameLimiterDb[(size_t) i] = 12.0f;
            c.limiterMaxReductionDb = 12.0f;
            return evaluate (c, options, nullptr);
        };
        check (! hasCategory (runFrames (1), "master"), "limiter: en enda hög ram (maxet 12 dB) ger inget fynd");
        check (severityOf (runFrames (4), "limiter") == Severity::problem, "limiter: fyra av 32 ramar på 12 dB ger ett problem");
    }

    // --- Rumlande djupbas under 30 Hz.
    {
        auto run = [&options] (float subDb)
        {
            auto c = makeCapture();
            setBand (c, firstInstrument, 5, -10.0f);
            setBand (c, firstInstrument, 0, subDb);
            return evaluate (c, options, nullptr);
        };
        check (! hasCategory (run (-120.0f), "sub"), "sub: ingenting under 30 Hz ger inget fynd");
        check (hasCategory (run (-10.0f), "sub"), "sub: lika mycket under 30 Hz som i basen ger fynd");
        check (hasCategory (run (-16.0f), "sub"), "sub: 6 dB under toppen räknas (gränsen är toppen - 6 dB)");
        check (! hasCategory (run (-17.0f), "sub"), "sub: 7 dB under toppen räknas inte");
        // Bara en liten del av tiden: två ramar av 32.
        auto c = makeCapture();
        setBand (c, firstInstrument, 5, -10.0f);
        c.band (firstInstrument, 3, 0) = c.band (firstInstrument, 4, 0) = -10.0f;
        check (! hasCategory (evaluate (c, options, nullptr), "sub"), "sub: två ramar av 32 räcker inte (minst 2 x minFrames)");
    }

    // --- RAND-fas på en bas: RAND >= 0,1 och minst hälften av energin under 150 Hz.
    {
        auto run = [&options] (float rand, float highDb)
        {
            auto c = makeCapture();
            setBand (c, firstInstrument, 5, -10.0f);   // 80-100 Hz
            setBand (c, firstInstrument, 16, highDb);  // 1 kHz
            c.randomPhase[0] = rand;
            return hasTitle (evaluate (c, options, nullptr), "random phase");
        };
        check (! run (0.0f, -120.0f), "RAND: 0 på en bas ger ingen varning");
        check (! run (0.09f, -120.0f), "RAND: 0,09 på en bas slår inte till");
        check (run (0.1f, -120.0f), "RAND: 0,10 på en bas ger en varning");
        check (run (0.8f, -120.0f), "RAND: 0,80 på en bas ger en varning");
        check (run (0.8f, -10.86f), "RAND: basandel ca 55 % ger en varning (gränsen är 50 %)");
        check (! run (0.8f, -9.14f), "RAND: basandel ca 45 % slår inte till (A är inte en bas)");
        check (! run (0.8f, -10.0f + 3.0f), "RAND: bas och diskant där diskanten är dubbelt så stark slår inte till");
        // Instrumentet finns inte: ingen varning, även om RAND står kvar.
        auto c = makeCapture();
        setBand (c, firstInstrument, 5, -10.0f);
        c.randomPhase[0] = 0.8f;
        c.present[(size_t) firstInstrument] = false;
        check (! hasTitle (evaluate (c, options, nullptr), "random phase"), "RAND: ett instrument som inte finns ger ingen varning");
    }

    // --- Lera: 160-250 Hz mot 40-150 Hz i mastern, gränsen är 0,6 gånger effekten.
    {
        auto run = [&options] (float ratio, bool masterPresent = true)
        {
            auto c = makeCapture();
            c.present[(size_t) masterSource] = masterPresent;
            setBand (c, kickSource, 5, -10.0f); // mastern räknas inte in i gmax; någon annan källa måste spela
            setBand (c, masterSource, 6, -10.0f);                                               // 100-125 Hz
            setBand (c, masterSource, 8, -10.0f + 10.0f * std::log10 (ratio));                  // 160-200 Hz
            return hasTitle (evaluate (c, options, nullptr), "around 200 Hz");
        };
        check (! run (0.1f), "lera: 0,1 gånger slår inte till");
        check (! run (0.5f), "lera: 0,5 gånger slår inte till");
        check (! run (0.59f), "lera: 0,59 gånger slår inte till");
        check (run (0.61f), "lera: 0,61 gånger ger en varning");
        check (run (1.0f), "lera: lika mycket ger en varning");
        check (! run (1.0f, false), "lera: utan master ger ingen varning");

        // Nästan tyst mix: golvnivån (-120 dB) har fler basband (5) än lerband (2) och ger kvoten 0,4, alltså under gränsen.
        auto quiet = makeCapture();
        check (! hasTitle (evaluate (quiet, options, nullptr), "around 200 Hz"), "lera: en helt tyst master slår inte till");
        // Golv: en master på -90 dB i lerbandet och -100 dB i basen är för tyst för att bedömas (medeleffekt i basen under -40 dB).
        auto faint = makeCapture();
        setBand (faint, kickSource, 5, -10.0f);
        setBand (faint, masterSource, 6, -100.0f);
        setBand (faint, masterSource, 8, -90.0f);
        check (! hasTitle (evaluate (faint, options, nullptr), "around 200 Hz"), "lera: en master under -40 dB i basen bedöms inte");
    }

    std::puts (failed ? "FEL: antest" : "AN-testet gick igenom");
    return failed ? 1 : 0;
}
