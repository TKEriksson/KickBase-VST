#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include "../Core/InstrumentSynth.h"
#include "../Core/Wavetable.h"

// Delad renderhjälp (0.4 h): motorns utsignal ur samma funktioner som synten (warpPhase, Wavetable::sample, unison-spridningen i
// InstrumentSynth::Oscillator::next), för UI-diagram. Mono, utan nivå, pan, filter och envelopes. Återanvänds av INSTR-diagrammet (f steg 2).
namespace OscRender
{
struct Settings
{
    float position = 0.0f;   // 0..1
    bool morph = true;
    WarpMode warp = WarpMode::off;
    float warpAmount = 0.0f; // 0..1
    float startPhase = 0.0f; // 0..1 (OSC:ns PHASE, plus ev. gemensam startfas)
    int unison = 1;          // 1..maxUnison
    float detuneCents = 0.0f;
    float blend = 0.5f;      // sidostämmornas nivå
};

// `periods` perioder av grundtonen i `out.size()` punkter. Med unison > 1 glider stämmorna isär över perioderna, så fler perioder syns som svävning.
// FM (kräver en modulator) och RING (kräver andra oscillatorer) visas som torr signal.
inline void render (const Wavetable& table, const Settings& s, std::vector<float>& out, double periods)
{
    const auto n = (int) out.size();
    if (n == 0)
        return;
    const auto voices = std::clamp (s.unison, 1, (int) InstrumentSynth::maxUnison);
    const auto warp = (s.warp == WarpMode::fm || s.warp == WarpMode::ring) ? WarpMode::off : s.warp;
    const auto depth = std::clamp (s.warpAmount, 0.0f, 1.0f);
    constexpr auto maxHarmonics = 512.0; // som diagrammens övriga vyer: alla övertoner syns

    std::fill (out.begin(), out.end(), 0.0f);
    auto gainSum = 0.0f;
    for (int v = 0; v < voices; ++v)
    {
        const auto spread = voices > 1 ? (double) v / (voices - 1) * 2.0 - 1.0 : 0.0;
        const auto ratio = std::exp2 (spread * s.detuneCents / 1200.0);
        const auto centre = voices == 1 || std::abs (spread) < 1.0 / voices + 1.0e-6;
        const auto gain = centre ? 1.0f : s.blend;
        gainSum += gain * gain;
        for (int i = 0; i < n; ++i)
        {
            auto phase = s.startPhase + periods * ratio * (double) i / (double) n;
            phase -= std::floor (phase);
            const auto warped = InstrumentSynth::warpPhase (warp, depth, phase, 0.0f);
            auto value = gain * table.sample (s.position, warped, maxHarmonics, s.morph);
            if (s.warp == WarpMode::am && depth > 0.0f)
                value *= 1.0f - depth + depth * (0.5f + 0.5f * (float) std::sin (6.283185307179586 * 2.0 * phase));
            out[(size_t) i] += value;
        }
    }
    const auto normal = 1.0f / std::sqrt (std::max (1.0f, gainSum));
    for (auto& x : out)
        x *= normal;
}
}
