#pragma once

// AUTO FIX, mätdelen (masterplan.md:135-136): deterministiska mått per instrument mot kicken
// och en lista med föreslagna åtgärder (före/efter). Ren funktion: bara std, inget UI,
// ingenting ändras. Arbetare 1 kopplar listan till UI och APPLY.
// Plan: docs/work/autofix-matdel-plan.md.

#include <string>
#include <vector>

namespace AutoFix
{
    constexpr float defaultWindowCents = 100.0f;   // samma som FUNDAMENTAL DUCK WINDOW
    constexpr float lowCorrelationLimit = -0.3f;   // under detta släcker kick och bas ut varandra
    constexpr float excess200LimitDb = 3.0f;       // överskott runt 200 Hz som flaggas
    constexpr float levelMarginDb = 0.0f;          // bas över kick i lågbandet med mer än detta flaggas

    struct Note
    {
        double startSeconds = 0.0, lengthSeconds = 0.0;
        float hz = 0.0f;           // grundton (nominell, utan glide/LFO)
        float velocity = 0.8f;     // 0..1
    };

    // Måtten från en analyskörning (MixAnalysis::PhaseMetrics) i den form som behövs här.
    struct PhaseInfo
    {
        bool valid = false;
        float meanCorrelation = 0.0f;   // viktad Pearson kick/bas, 30-150 Hz
        float negativeShare = 0.0f;     // andel aktiva 16-delar med korrelation under lowCorrelationLimit (0..1)
        float bassMinusKickDb = 0.0f;   // bas minus kick, RMS 30-150 Hz
        float excess200Db = 0.0f;       // effekt runt 200 Hz mot grannbanden
    };

    struct InstrumentInput
    {
        std::string name;
        int index = 0;                  // instrumentets plats (0-baserad)
        std::vector<Note> notes;
        PhaseInfo phase;
        bool duckOn = false;            // vanlig sidechain
        float duckDepthDb = 0.0f;
        bool fundamentalDuckOn = false; // FUNDAMENTAL DUCK
        float fundamentalDuckDepthDb = 0.0f;
        float fundamentalDuckWindowCents = defaultWindowCents;
    };

    struct Input
    {
        float kickTailHz = 0.0f;        // uppmätt, annars design (0 = okänd, då görs inga fundamental-mått)
        std::vector<InstrumentInput> instruments;
    };

    struct NoteMeasure
    {
        int noteIndex = 0;
        float distanceCents = 0.0f;     // grundton minus kickens svans, i cent (tecken = riktning)
        bool collides = false;          // |avstånd| <= fönster
    };

    struct Measurement
    {
        std::string name;
        int index = 0;
        std::vector<NoteMeasure> notes;
        int collidingNotes = 0;
        float worstCents = 0.0f;        // minsta |avstånd| bland tonerna (0 om inga toner)
        float spreadCents = 0.0f;       // största minus minsta avstånd bland krockande toner
        PhaseInfo phase;
        bool levelProblem = false;      // bas över kick i lågbandet
        bool phaseProblem = false;      // korrelation under gränsen
        bool excess200Problem = false;
    };

    enum class Severity { info, warning, problem };

    // Ett förslag: ingenting tillämpas här. `param` är ett symboliskt namn (UI-lagret mappar det).
    struct Action
    {
        std::string id;                 // stabilt, t.ex. "inst2.fundamentalDuck"
        int instrument = 0;
        std::string param;              // "fundamentalDuckOn", "fundamentalDuckDepthDb", "fundamentalDuckWindowCents",
                                        // "noteVelocity", "phaseRotation", "eq200Db", "duckDepthDb"
        int note = -1;                  // -1 = hela instrumentet
        float before = 0.0f, after = 0.0f;
        std::string reason;             // engelska
        Severity severity = Severity::info;
    };

    struct Result
    {
        std::vector<Measurement> measurements;
        std::vector<Action> actions;    // allvarligast först, sedan instrumentordning, sedan id
    };

    // Avstånd mellan två frekvenser i cent (a relativt b). 0 om någon är <= 0.
    float centsBetween (float hz, float referenceHz);

    Result analyse (const Input& input);
}
