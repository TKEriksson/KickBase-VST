#pragma once

#include <array>
#include <atomic>
#include <vector>

// Ett "avlyssningsuttag" för gränssnittet: ljudtråden skriver de senaste samplesen i en
// ringbuffert, gränssnittet läser dem när det ritar vågformer, spektrum och stereobild.
// Ingen låsning: varje sample är en atomic och skrivpositionen publiceras sist.
template <int Capacity>
class BasicSignalTap
{
public:
    static constexpr int capacity = Capacity; // standard: 1 << 17 = ~3 s vid 44,1 kHz

    // Ljudtråden.
    void push (float left, float right)
    {
        const auto index = writeIndex.load (std::memory_order_relaxed);
        const auto slot = (size_t) (index & (capacity - 1));
        l[slot].store (left, std::memory_order_relaxed);
        r[slot].store (right, std::memory_order_relaxed);
        writeIndex.store (index + 1, std::memory_order_release);
    }

    void push (float mono) { push (mono, mono); }

    // Gränssnittet: de senaste `count` samplesen (äldst först). Kan hoppa lite om ljudtråden
    // skriver samtidigt, vilket inte syns i en bild.
    void copyLatest (int count, std::vector<float>& left, std::vector<float>& right) const
    {
        count = std::min (count, capacity);
        left.resize ((size_t) count);
        right.resize ((size_t) count);
        const auto end = writeIndex.load (std::memory_order_acquire);
        for (int i = 0; i < count; ++i)
        {
            const auto slot = (size_t) ((end - count + i) & (capacity - 1));
            left[(size_t) i] = l[slot].load (std::memory_order_relaxed);
            right[(size_t) i] = r[slot].load (std::memory_order_relaxed);
        }
    }

    long long position() const { return writeIndex.load (std::memory_order_acquire); }

private:
    std::array<std::atomic<float>, capacity> l {}, r {};
    std::atomic<long long> writeIndex { 0 };
};

using SignalTap = BasicSignalTap<(1 << 17)>;
using ShortSignalTap = BasicSignalTap<(1 << 14)>; // ~0,37 s: räcker för spektrumet, kostar en åttondel
