#include "AutoFix.h"

#include <algorithm>
#include <cmath>

namespace AutoFix
{
    namespace
    {
        constexpr float maxDuckDepthDb = 24.0f;

        // Djup efter hur nära grundtonen ligger kickens svans: mitt på 12 dB, i fönsterkanten 4 dB.
        float depthFor (float worstAbsCents, float windowCents)
        {
            const auto closeness = windowCents > 0.0f ? 1.0f - std::min (1.0f, worstAbsCents / windowCents) : 1.0f;
            return 4.0f + 8.0f * closeness;
        }

        float roundTo (float v, float step) { return std::round (v / step) * step; }

        void add (Result& r, int instrument, const std::string& param, int note, float before, float after,
                  const std::string& reason, Severity severity)
        {
            Action a;
            a.instrument = instrument;
            a.param = param;
            a.note = note;
            a.before = before;
            a.after = after;
            a.reason = reason;
            a.severity = severity;
            a.id = "inst" + std::to_string (instrument) + "." + param + (note >= 0 ? "." + std::to_string (note) : std::string());
            r.actions.push_back (std::move (a));
        }
    }

    float centsBetween (float hz, float referenceHz)
    {
        if (hz <= 0.0f || referenceHz <= 0.0f)
            return 0.0f;
        return 1200.0f * std::log2 (hz / referenceHz);
    }

    Result analyse (const Input& input)
    {
        Result result;

        for (const auto& inst : input.instruments)
        {
            Measurement m;
            m.name = inst.name;
            m.index = inst.index;
            m.phase = inst.phase;

            const auto window = std::max (0.0f, inst.fundamentalDuckWindowCents);
            float minAbs = 0.0f, minC = 0.0f, maxC = 0.0f;
            bool anyColliding = false, anyMeasured = false;

            if (input.kickTailHz > 0.0f)
                for (int i = 0; i < (int) inst.notes.size(); ++i)
                {
                    const auto& n = inst.notes[(size_t) i];
                    if (n.hz <= 0.0f)
                        continue;
                    NoteMeasure nm;
                    nm.noteIndex = i;
                    nm.distanceCents = centsBetween (n.hz, input.kickTailHz);
                    nm.collides = std::abs (nm.distanceCents) <= defaultWindowCents + 0.01f; // marginal för flyttalsfel i kanten
                    m.notes.push_back (nm);

                    const auto a = std::abs (nm.distanceCents);
                    minAbs = anyMeasured ? std::min (minAbs, a) : a;
                    anyMeasured = true;
                    if (nm.collides)
                    {
                        ++m.collidingNotes;
                        minC = anyColliding ? std::min (minC, nm.distanceCents) : nm.distanceCents;
                        maxC = anyColliding ? std::max (maxC, nm.distanceCents) : nm.distanceCents;
                        anyColliding = true;
                    }
                }

            m.worstCents = minAbs;
            m.spreadCents = anyColliding ? maxC - minC : 0.0f;

            if (inst.phase.valid)
            {
                m.levelProblem = inst.phase.bassMinusKickDb > levelMarginDb;
                m.phaseProblem = inst.phase.meanCorrelation < lowCorrelationLimit;
                m.excess200Problem = inst.phase.excess200Db > excess200LimitDb;
            }

            // Åtgärder.
            if (m.collidingNotes > 0)
            {
                // Fönstret ska täcka de krockande tonerna med marginal, minst standardfönstret.
                const auto needWindow = std::max (defaultWindowCents, roundTo (std::max (std::abs (minC), std::abs (maxC)) + 25.0f, 25.0f));
                const auto depth = depthFor (minAbs, defaultWindowCents);
                const auto sev = m.worstCents <= defaultWindowCents * 0.5f ? Severity::problem : Severity::warning;
                const auto text = std::to_string (m.collidingNotes) + " note(s) have their fundamental within "
                                  + std::to_string ((int) std::lround (m.worstCents)) + " cents of the kick tail";

                if (! inst.fundamentalDuckOn)
                {
                    add (result, inst.index, "fundamentalDuckOn", -1, 0.0f, 1.0f,
                         text + "; enable FUNDAMENTAL DUCK so the kick envelope dips only that fundamental.", sev);
                    add (result, inst.index, "fundamentalDuckDepthDb", -1, inst.fundamentalDuckDepthDb, depth,
                         "Depth scaled to how close the closest fundamental is to the kick tail.", sev);
                }
                else if (inst.fundamentalDuckDepthDb < depth - 0.5f)
                    add (result, inst.index, "fundamentalDuckDepthDb", -1, inst.fundamentalDuckDepthDb, depth,
                         text + "; the current FUNDAMENTAL DUCK depth is too shallow.", sev);

                if (inst.fundamentalDuckWindowCents + 0.5f < needWindow)
                    add (result, inst.index, "fundamentalDuckWindowCents", -1, inst.fundamentalDuckWindowCents, needWindow,
                         "Widen the window so it covers every colliding note.", Severity::warning);
            }

            if (m.levelProblem && m.collidingNotes > 0)
            {
                const auto cut = std::min (maxDuckDepthDb, roundTo (inst.phase.bassMinusKickDb + 1.0f, 0.5f));
                for (const auto& nm : m.notes)
                    if (nm.collides)
                    {
                        const auto& n = inst.notes[(size_t) nm.noteIndex];
                        const auto after = std::max (0.0f, n.velocity * std::pow (10.0f, -cut / 20.0f));
                        add (result, inst.index, "noteVelocity", nm.noteIndex, n.velocity, after,
                             "The bass is " + std::to_string ((int) std::lround (inst.phase.bassMinusKickDb))
                                 + " dB over the kick in 30-150 Hz; lower the colliding note's velocity.", Severity::warning);
                    }
            }

            if (m.phaseProblem)
                add (result, inst.index, "phaseRotation", -1, 0.0f, 90.0f,
                     "Kick and bass cancel in 30-150 Hz (correlation " + std::to_string (inst.phase.meanCorrelation).substr (0, 5)
                         + "); try PHASE ROTATION 90 degrees.", Severity::problem);

            if (m.excess200Problem)
                add (result, inst.index, "eq200Db", -1, 0.0f, -std::min (6.0f, roundTo (inst.phase.excess200Db, 0.5f)),
                     "Energy around 200 Hz sticks out by " + std::to_string ((int) std::lround (inst.phase.excess200Db))
                         + " dB; a narrow cut there.", Severity::warning);

            if (m.levelProblem && ! inst.duckOn)
                add (result, inst.index, "duckDepthDb", -1, inst.duckDepthDb, std::max (inst.duckDepthDb, 6.0f),
                     "Bass is louder than the kick in 30-150 Hz and sidechain is off; turn on at least 6 dB.", Severity::warning);

            result.measurements.push_back (std::move (m));
        }

        std::stable_sort (result.actions.begin(), result.actions.end(), [] (const Action& a, const Action& b)
        {
            if (a.severity != b.severity)
                return (int) a.severity > (int) b.severity;
            if (a.instrument != b.instrument)
                return a.instrument < b.instrument;
            return a.id < b.id;
        });

        return result;
    }
}
