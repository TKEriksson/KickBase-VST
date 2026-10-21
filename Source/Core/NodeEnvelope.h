#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>
#include <vector>

// En envelope av fria punkter (som i Kick 3): x = tid i sekunder, y = värde (t.ex. halvtoner
// eller volym 0..1), tension = hur segmentet fram till nästa punkt böjer sig.
//
// Punkterna ägs av message thread; ljudtråden läser en uppslagstabell som byggs om när
// punkterna ändras.
class NodeEnvelope
{
public:
    struct Point
    {
        float x = 0.0f, y = 0.0f;
        float tension = 0.0f; // -1..1: 0 = rak linje, negativ = snabbt fall först
        // Bézierläge (BZ): handtag relativt punkten. out styr segmentet till nästa punkt, in segmentet från förra.
        // x begränsas i setPoints så att kurvan förblir en funktion av tiden. Tension-läget ignorerar dem.
        float outDx = 0.0f, outDy = 0.0f, inDx = 0.0f, inDy = 0.0f;
    };

    enum class Mode { tension, bezier }; // per kurva; tension = som förut

    static constexpr int maxPoints = 24;
    static constexpr int tableSize = 4096;

    NodeEnvelope (float maxSeconds, float maxValue, std::vector<Point> defaults);

    void reset(); // till standardpunkterna

    // Bara message thread.
    const std::vector<Point>& getPoints() const { return points; }
    void setPoints (std::vector<Point> newPoints);
    float evaluate (float seconds) const; // från punkterna
    Mode getMode() const { return mode; }
    void setMode (Mode newMode);
    // Handtag så att Bézierkurvan följer nuvarande tension-form (x linjärt, y matchad vid 1/3 och 2/3).
    static std::vector<Point> withHandlesFromTension (std::vector<Point> pts);

    // Ljudtråden: värdet vid `seconds` (tabell med interpolation); efter sista punkten dess värde.
    float lookup (double seconds) const;
    float endSeconds() const { return end.load(); } // sista punktens tid

    float getMaxSeconds() const { return maxSeconds; }
    float getMaxValue() const { return maxValue; }

    juce::ValueTree toValueTree (const juce::Identifier& type) const;
    void fromValueTree (const juce::ValueTree& tree);

    std::atomic<int> version { 0 }; // ökas vid varje ändring, så att gränssnittet ritar om

    static float shape (float t, float tension);

private:
    void rebuildTable();

    const float maxSeconds, maxValue;
    std::vector<Point> defaults, points;
    Mode mode = Mode::tension;
    std::array<std::atomic<float>, tableSize> table {};
    std::atomic<float> end { 0.1f };
};
