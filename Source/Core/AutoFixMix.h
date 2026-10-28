#pragma once

// Bro mellan MixAnalysis och AutoFix: gör PhaseMetrics (kick mot bas, 30-150 Hz) till AutoFix::PhaseInfo.
// Header-only så att AutoFix.h förblir ren std. PhaseMetrics gäller kick mot basen som helhet; 200 Hz-överskottet
// mäts i bandanalysen (Finding "mud"), så anroparen skickar in det.

#include "AutoFix.h"
#include "MixAnalysis.h"
#include <cmath>

namespace AutoFix
{
    constexpr int minActiveSlots = 4;   // färre aktiva 16-delar än så ger inget tillförlitligt mått

    inline PhaseInfo fromPhaseMetrics (const MixAnalysis::PhaseMetrics& p, float excess200Db = 0.0f)
    {
        PhaseInfo info;
        info.excess200Db = excess200Db;

        const auto slots = p.lowCorrelation.size();
        const auto has = [slots] (size_t n) { return n == slots; };
        if (! p.valid || slots == 0 || ! has (p.lowActive.size()))
            return info;

        const auto weighted = has (p.lowWeight.size());
        const auto levels = has (p.lowKickDb.size()) && has (p.lowBassDb.size());
        const auto hits = has (p.lowKickHit.size());

        int active = 0, negative = 0, levelSlots = 0;
        double sumWeight = 0.0, sumCorr = 0.0, sumDiff = 0.0;
        int hitSlots = 0;
        double sumDiffHit = 0.0;
        for (size_t i = 0; i < slots; ++i)
        {
            if (p.lowActive[i] == 0)
                continue;
            ++active;
            const auto w = weighted ? (double) p.lowWeight[i] : 1.0;
            sumWeight += w;
            sumCorr += w * p.lowCorrelation[i];
            if (p.lowCorrelation[i] < lowCorrelationLimit)
                ++negative;
            if (levels)
            {
                const auto d = (double) (p.lowBassDb[i] - p.lowKickDb[i]);
                sumDiff += d;
                ++levelSlots;
                if (hits && p.lowKickHit[i] != 0)
                {
                    sumDiffHit += d;
                    ++hitSlots;
                }
            }
        }
        if (active < minActiveSlots)
            return info;

        info.valid = true;
        info.meanCorrelation = sumWeight > 0.0 ? (float) (sumCorr / sumWeight) : 0.0f;
        info.negativeShare = (float) negative / (float) active;
        // Nivån räknas på kickslagen (där kicken ska vinna); finns inga används alla aktiva 16-delar.
        if (hitSlots > 0)
            info.bassMinusKickDb = (float) (sumDiffHit / hitSlots);
        else if (levelSlots > 0)
            info.bassMinusKickDb = (float) (sumDiff / levelSlots);
        return info;
    }
}
