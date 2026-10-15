#include "NodeEnvelope.h"

NodeEnvelope::NodeEnvelope (float maxSecondsIn, float maxValueIn, std::vector<Point> defaultsIn)
    : maxSeconds (maxSecondsIn), maxValue (maxValueIn), defaults (std::move (defaultsIn))
{
    reset();
}

void NodeEnvelope::reset()
{
    setPoints (defaults);
}

void NodeEnvelope::setPoints (std::vector<Point> newPoints)
{
    for (auto& p : newPoints)
    {
        p.x = juce::jlimit (0.0f, maxSeconds, p.x);
        p.y = juce::jlimit (0.0f, maxValue, p.y);
        p.tension = juce::jlimit (-1.0f, 1.0f, p.tension);
    }
    std::stable_sort (newPoints.begin(), newPoints.end(), [] (const Point& a, const Point& b) { return a.x < b.x; });

    if (newPoints.size() < 2)
        newPoints = defaults;
    if (newPoints.size() > (size_t) maxPoints)
        newPoints.resize ((size_t) maxPoints);

    newPoints.front().x = 0.0f;
    newPoints.back().x = std::max (newPoints.back().x, 0.005f);

    points = std::move (newPoints);
    rebuildTable();
    ++version;
}

float NodeEnvelope::shape (float t, float tension)
{
    // Samma form som duckkurvan: exponent 4^tension (0 = rak linje).
    if (std::abs (tension) < 1.0e-3f)
        return t;
    return std::pow (t, std::pow (4.0f, tension));
}

float NodeEnvelope::evaluate (float seconds) const
{
    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        const auto& a = points[i];
        const auto& b = points[i + 1];
        if (seconds <= b.x)
        {
            const auto width = b.x - a.x;
            const auto t = width > 0.0f ? juce::jlimit (0.0f, 1.0f, (seconds - a.x) / width) : 1.0f;
            return a.y + (b.y - a.y) * shape (t, a.tension);
        }
    }
    return points.back().y;
}

void NodeEnvelope::rebuildTable()
{
    const auto length = points.back().x;
    for (int i = 0; i < tableSize; ++i)
        table[(size_t) i] = evaluate (length * (float) i / (float) (tableSize - 1));
    end = length;
}

float NodeEnvelope::lookup (double seconds) const
{
    const auto length = end.load();
    if (seconds >= length)
        return table[tableSize - 1].load();

    const auto position = (float) (std::max (0.0, seconds) / length) * (float) (tableSize - 1);
    const auto index = std::min (tableSize - 2, (int) position);
    const auto fraction = position - (float) index;
    const auto a = table[(size_t) index].load();
    return a + (table[(size_t) index + 1].load() - a) * fraction;
}

juce::ValueTree NodeEnvelope::toValueTree (const juce::Identifier& type) const
{
    juce::ValueTree tree (type);
    for (const auto& p : points)
    {
        juce::ValueTree point ("Point");
        point.setProperty ("x", p.x, nullptr);
        point.setProperty ("y", p.y, nullptr);
        point.setProperty ("tension", p.tension, nullptr);
        tree.appendChild (point, nullptr);
    }
    return tree;
}

void NodeEnvelope::fromValueTree (const juce::ValueTree& tree)
{
    std::vector<Point> loaded;
    for (const auto& child : tree)
        loaded.push_back ({ (float) child["x"], (float) child["y"], (float) child["tension"] });
    if (loaded.size() >= 2)
        setPoints (std::move (loaded));
    else
        reset();
}
