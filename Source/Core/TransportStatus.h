#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include "Pattern.h"
#include "TransportClock.h"

// Det gränssnittet visar om uppspelningen. Skrivs från ljudtråden och läses av editorn,
// därför atomics: editorn får aldrig låsa eller vänta på ljudtråden.
struct TransportStatus
{
    std::atomic<double> bpm { TransportClock::fallbackBpm };
    std::atomic<bool> followingHost { false };
    std::atomic<int> bar { 1 };
    std::atomic<int> beat { 1 };
    std::atomic<int> patternBar { 1 };    // takten i mönstret (börjar om när hela mönstret, den längsta raden, är klart)
    std::atomic<int> keyNote { -1 };      // tonen som bestämmer tonarten, -1 = ingen
    std::atomic<bool> patternActive { false };
    std::atomic<int> kickStep { -1 };     // steg i mönstret som spelas nu, för markören
    std::array<std::atomic<int>, Pattern::numLayers> bassSteps { -1, -1, -1 };

    // Kicken och baslagrens samplingar just nu, för markörerna i gränssnittet.
    std::atomic<float> kickPlayTime { -1.0f };     // sekunder sedan senaste kicken slog an, -1 = tyst
    std::atomic<int> kickTriggers { 0 };           // räknas upp för varje ny kick
    std::array<std::atomic<float>, Pattern::numLayers> bassSamplePosition { -1.0f, -1.0f, -1.0f }; // 0..1 i samplingen, -1 = spelar inte

    void publish (const TransportClock& clock)
    {
        const auto ppq = clock.getPpqAt (0);

        bpm = clock.getBpm();
        followingHost = clock.isFollowingHost();
        bar = (int) clock.getBarIndex() + 1;
        beat = (int) std::floor (ppq - clock.getBarStartPpq()) + 1;
    }
};
