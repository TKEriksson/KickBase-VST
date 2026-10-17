#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <memory>
#include <vector>

// Data som ljudtråden kan läsa (en sampling, en wavetable) får inte frigöras i samma ögonblick som den byts ut: ljudtråden kan vara mitt i
// ett block som läser den gamla. Den läggs därför här och frigörs först när den legat minst `holdMs` (långt mer än ett block), även om den
// byts ut många gånger i snabb följd. Bara meddelandetråden (den som byter) använder klassen.
template <typename T>
class DeferredRelease
{
public:
    explicit DeferredRelease (juce::uint32 holdMilliseconds = 2000) : holdMs (holdMilliseconds) {}

    void retire (std::unique_ptr<T> object)
    {
        prune();
        if (object != nullptr)
            retired.push_back ({ juce::Time::getMillisecondCounter(), std::move (object) });
    }

    // Frigör det som legat tillräckligt länge.
    void prune()
    {
        const auto now = juce::Time::getMillisecondCounter();
        retired.erase (std::remove_if (retired.begin(), retired.end(), [this, now] (const Entry& entry) { return now - entry.when >= holdMs; }),
                       retired.end());
    }

    size_t size() const { return retired.size(); }

private:
    struct Entry
    {
        juce::uint32 when;
        std::unique_ptr<T> object;
    };

    const juce::uint32 holdMs;
    std::vector<Entry> retired;
};
