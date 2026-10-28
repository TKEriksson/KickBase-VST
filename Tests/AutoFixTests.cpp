// Tester för AUTO FIX-mätdelen (Source/Core/AutoFix.h) på påhittade data.

#include "../Source/Core/AutoFix.h"
#include "TestUtils.h"
#include <cmath>
#include <cstring>

namespace
{
    AutoFix::InstrumentInput bass (float hz)
    {
        AutoFix::InstrumentInput b;
        b.name = "bass";
        b.index = 1;
        AutoFix::Note n;
        n.hz = hz;
        b.notes.push_back (n);
        return b;
    }

    const AutoFix::Action* find (const AutoFix::Result& r, const char* param)
    {
        for (const auto& a : r.actions)
            if (a.param == param)
                return &a;
        return nullptr;
    }

    void testCents()
    {
        std::puts ("AutoFix: cent mellan frekvenser");
        check (std::abs (AutoFix::centsBetween (110.0f, 55.0f) - 1200.0f) < 0.01f, "en oktav = 1200 cent");
        check (AutoFix::centsBetween (0.0f, 55.0f) == 0.0f, "0 Hz ger 0");
    }

    void testCollision()
    {
        std::puts ("AutoFix: krock och ingen krock");
        AutoFix::Input in;
        in.kickTailHz = 45.0f;

        in.instruments = { bass (45.0f) };
        auto r = AutoFix::analyse (in);
        check (r.measurements.size() == 1 && r.measurements[0].collidingNotes == 1, "samma ton krockar");
        const auto* on = find (r, "fundamentalDuckOn");
        check (on != nullptr && on->before == 0.0f && on->after == 1.0f, "föreslår FUNDAMENTAL DUCK på");
        check (on != nullptr && on->severity == AutoFix::Severity::problem, "mitt i svansen är problem");

        in.instruments = { bass (45.0f * std::pow (2.0f, 100.0f / 1200.0f)) };
        r = AutoFix::analyse (in);
        check (r.measurements[0].collidingNotes == 1, "fönsterkanten (100 cent) räknas som krock");

        in.instruments = { bass (45.0f * std::pow (2.0f, 150.0f / 1200.0f)) };
        r = AutoFix::analyse (in);
        check (r.measurements[0].collidingNotes == 0 && r.actions.empty(), "150 cent bort: ingen krock, inga åtgärder");

        in.instruments = { bass (80.0f) };
        r = AutoFix::analyse (in);
        check (r.actions.empty(), "långt bort: inga åtgärder");
    }

    void testAlreadyOn()
    {
        std::puts ("AutoFix: redan påslagen duck föreslås inte igen");
        AutoFix::Input in;
        in.kickTailHz = 45.0f;
        auto b = bass (45.0f);
        b.fundamentalDuckOn = true;
        b.fundamentalDuckDepthDb = 12.0f;
        in.instruments = { b };
        auto r = AutoFix::analyse (in);
        check (find (r, "fundamentalDuckOn") == nullptr, "ingen on-åtgärd");
        check (find (r, "fundamentalDuckDepthDb") == nullptr, "djupet räcker");

        in.instruments[0].fundamentalDuckDepthDb = 2.0f;
        r = AutoFix::analyse (in);
        const auto* d = find (r, "fundamentalDuckDepthDb");
        check (d != nullptr && d->before == 2.0f && d->after > 2.0f, "för grunt djup höjs", d ? d->after : 0.0f);
    }

    void testWindowWidening()
    {
        std::puts ("AutoFix: fönstret vidgas bara när det behövs");
        AutoFix::Input in;
        in.kickTailHz = 45.0f;
        auto b = bass (45.0f);
        b.fundamentalDuckOn = true;
        b.fundamentalDuckDepthDb = 12.0f;
        b.fundamentalDuckWindowCents = 30.0f;
        in.instruments = { b };
        auto r = AutoFix::analyse (in);
        const auto* w = find (r, "fundamentalDuckWindowCents");
        check (w != nullptr && w->before == 30.0f && w->after >= 100.0f, "smalt fönster vidgas", w ? w->after : 0.0f);
    }

    void testPhaseLevelExcess()
    {
        std::puts ("AutoFix: fas, nivå och 200 Hz");
        AutoFix::Input in;
        in.kickTailHz = 45.0f;
        auto b = bass (45.0f);
        b.phase.valid = true;
        b.phase.meanCorrelation = -0.6f;
        b.phase.bassMinusKickDb = 4.0f;
        b.phase.excess200Db = 5.0f;
        in.instruments = { b };
        auto r = AutoFix::analyse (in);
        check (r.measurements[0].phaseProblem && r.measurements[0].levelProblem && r.measurements[0].excess200Problem, "alla tre flaggas");
        check (find (r, "phaseRotation") != nullptr, "phase rotation föreslås");
        const auto* e = find (r, "eq200Db");
        check (e != nullptr && e->after < 0.0f, "200 Hz-sänkning är negativ", e ? e->after : 0.0f);
        const auto* v = find (r, "noteVelocity");
        check (v != nullptr && v->after < v->before && v->note == 0, "velocity sänks på krockande ton");
        check (find (r, "duckDepthDb") != nullptr, "sidechain föreslås när den är av");
        check (r.actions.front().severity == AutoFix::Severity::problem, "allvarligast först");

        b.phase.valid = false;
        in.instruments = { b };
        r = AutoFix::analyse (in);
        check (! r.measurements[0].phaseProblem && find (r, "phaseRotation") == nullptr, "ogiltiga fasmått ger inga fasåtgärder");
    }

    void testEdgesAndDeterminism()
    {
        std::puts ("AutoFix: tomt och deterministiskt");
        AutoFix::Input in;
        auto r = AutoFix::analyse (in);
        check (r.measurements.empty() && r.actions.empty(), "tom indata");

        in.instruments = { bass (45.0f) };
        r = AutoFix::analyse (in);
        check (r.actions.empty() && r.measurements[0].notes.empty(), "okänd kicksvans (0 Hz): inga fundamental-mått");

        in.kickTailHz = 45.0f;
        auto b = bass (45.0f);
        b.phase = { true, -0.5f, 0.5f, 3.0f, 4.0f };
        in.instruments = { b, b };
        in.instruments[1].index = 2;
        const auto a = AutoFix::analyse (in);
        const auto c = AutoFix::analyse (in);
        bool same = a.actions.size() == c.actions.size();
        for (size_t i = 0; same && i < a.actions.size(); ++i)
            same = a.actions[i].id == c.actions[i].id && a.actions[i].after == c.actions[i].after;
        check (same, "samma indata ger samma lista");
        check (a.actions.size() > 0 && a.actions[0].instrument == 1, "instrumentordning inom samma allvar");
    }
}

void runAutoFixTests()
{
    testCents();
    testCollision();
    testAlreadyOn();
    testWindowWidening();
    testPhaseLevelExcess();
    testEdgesAndDeterminism();
}
