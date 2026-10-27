#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <vector>
#include "SampleData.h"
#include "DeferredRelease.h"

// Multisampling: flera samplingar (zoner) på olika tangenter och anslagsstyrkor. Zonvalet sker vid note-on och är billigt (ingen allokering).
namespace MultiSampling
{
    constexpr int maxZones = 64;
    constexpr double maxSecondsPerZone = 8.0;
    constexpr double maxTotalSeconds = 120.0; // summan av originalen; skyddar minnet (originalet och den omsamplade kopian ryms båda)

    // Tonnamn (C4 = 60, A4 = 69) eller ett heltal 0..127 sist i namnet, t.ex. "Piano_C#3" eller "bass_048"; -1 om inget hittas.
    int parseRootKey (const juce::String& name);
}

// Zonens mappning: vilka toner och anslagsstyrkor den svarar på, och vilken ton samplingen själv har.
struct MultiSampleMapping
{
    int rootKey = 60;
    int keyLow = 0, keyHigh = 127;
    int velocityLow = 1, velocityHigh = 127;
    float tuneCents = 0.0f;
};

// Det ljudtråden läser: oföränderlig när den väl publicerats. Zonerna är mono och omsamplade till aktuell samplingsfrekvens.
struct MultiSampleBank
{
    struct Zone
    {
        ClickSample sample;
        MultiSampleMapping mapping;
        float rootHz = 261.63f; // samplingens egen tonhöjd inklusive finstämning
    };

    std::vector<Zone> zones;

    // Bästa zon för tonen (0..127) och anslaget (1..127), eller -1 om banken är tom. Ligger tonen eller anslaget utanför alla zoner väljs den
    // närmaste (tonavstånd väger tyngst), så ett glest ifyllt set tystnar inte. Lika bra zoner: den första.
    int select (int key, int velocity) const noexcept;

    const Zone* zone (int index) const noexcept { return index >= 0 && (size_t) index < zones.size() ? &zones[(size_t) index] : nullptr; }
};

// Ägaren av ett set: originalen sparas, en omsamplad bank publiceras atomiskt till ljudtråden (som SampleSlot).
class MultiSampleSet
{
public:
    bool addFile (const juce::File& file);                   // message thread; rotton tas ur filnamnet om möjligt
    bool addZone (juce::AudioBuffer<float> mono, double rate, const juce::String& name, const MultiSampleMapping& mapping);
    void removeZone (int index);
    void setMapping (int index, const MultiSampleMapping& mapping);
    void clear();
    void prepare (double sampleRate);                        // bygger om banken för samplingsfrekvensen

    // Sprider tangentområdena så att zonerna täcker 0..127 utan hål eller överlapp, efter rottonerna (gränsen halvvägs mellan två rottoner).
    void autoMapKeyRanges();

    const MultiSampleBank* active() const { return activeBank.load(); }
    bool hasZones() const { return active() != nullptr && ! active()->zones.empty(); }
    int zoneCount() const;
    juce::String zoneName (int index) const;
    MultiSampleMapping mapping (int index) const;

    // Sparas som ett barn i projektets (eller presetets) träd; originalen ligger kvar, så en annan samplingsfrekvens ger en ren omsampling.
    juce::ValueTree toValueTree (const juce::Identifier& type) const;
    void fromValueTree (const juce::ValueTree& tree);

    size_t retiredCount() const { return retired.size(); } // för testerna

private:
    struct Original
    {
        juce::AudioBuffer<float> mono;
        double rate = 0.0;
        juce::String name;
        MultiSampleMapping mapping;
    };

    void rebuild();
    double totalSecondsLocked() const;

    double currentSampleRate = 44100.0;
    mutable juce::CriticalSection lock;
    std::vector<Original> originals;

    std::atomic<MultiSampleBank*> activeBank { nullptr };
    std::unique_ptr<MultiSampleBank> owned;
    DeferredRelease<MultiSampleBank> retired;
};
