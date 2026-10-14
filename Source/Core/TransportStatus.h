#pragma once

#include <atomic>
#include <cmath>
#include "TransportClock.h"

// Det gränssnittet visar om uppspelningen. Skrivs från ljudtråden och läses av editorn,
// därför atomics: editorn får aldrig låsa eller vänta på ljudtråden.
struct TransportStatus
{
    std::atomic<double> bpm { TransportClock::fallbackBpm };
    std::atomic<bool> followingHost { false };
    std::atomic<int> bar { 1 };
    std::atomic<int> beat { 1 };
    std::atomic<int> keyNote { -1 };      // tonen som bestämmer tonarten, -1 = ingen
    std::atomic<bool> patternActive { false };
    std::atomic<int> kickStep { -1 };     // steg i mönstret som spelas nu, för markören
    std::atomic<int> bassStep { -1 };

    void publish (const TransportClock& clock)
    {
        const auto ppq = clock.getPpqAt (0);

        bpm = clock.getBpm();
        followingHost = clock.isFollowingHost();
        bar = (int) clock.getBarIndex() + 1;
        beat = (int) std::floor (ppq - clock.getBarStartPpq()) + 1;
    }
};
