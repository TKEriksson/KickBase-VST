#pragma once

#include <atomic>
#include <cmath>
#include "TransportClock.h"

// Det gränssnittet visar om klockan. Skrivs från ljudtråden och läses av editorn,
// därför atomics: editorn får aldrig låsa eller vänta på ljudtråden.
struct TransportStatus
{
    std::atomic<double> bpm { TransportClock::fallbackBpm };
    std::atomic<bool> followingHost { false };
    std::atomic<int> bar { 1 };
    std::atomic<int> beat { 1 };
    std::atomic<int> heldNote { -1 }; // -1 = ingen ton hålls (används bara av instrumentet)

    void publish (const TransportClock& clock)
    {
        const auto ppq = clock.getPpqAt (0);
        const auto barLength = clock.getBarLengthPpq();

        bpm = clock.getBpm();
        followingHost = clock.isFollowingHost();
        bar = (int) std::floor (ppq / barLength) + 1;
        beat = (int) std::floor (ppq - clock.getBarStartPpq()) + 1;
    }
};
