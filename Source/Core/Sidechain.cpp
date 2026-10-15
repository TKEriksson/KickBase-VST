#include "Sidechain.h"

void SidechainCurve::reset()
{
    setPoints ({ { 0.0f, 1.0f, 0.0f }, { 0.08f, 1.0f, -0.6f }, { 1.0f, 0.0f, 0.0f } });
}

void SidechainCurve::setPoints (std::vector<Point> newPoints)
{
    // Sortera, håll inom 0..1 och se till att första och sista punkten ligger på kanterna.
    for (auto& p : newPoints)
    {
        p.x = juce::jlimit (0.0f, 1.0f, p.x);
        p.y = juce::jlimit (0.0f, 1.0f, p.y);
        p.tension = juce::jlimit (-1.0f, 1.0f, p.tension);
    }
    std::sort (newPoints.begin(), newPoints.end(), [] (const Point& a, const Point& b) { return a.x < b.x; });

    if (newPoints.size() < 2)
        newPoints = { { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
    if (newPoints.size() > (size_t) maxPoints)
        newPoints.resize ((size_t) maxPoints);

    newPoints.front().x = 0.0f;
    newPoints.back().x = 1.0f;

    points = std::move (newPoints);
    rebuildTable();
    ++version;
}

float SidechainCurve::shape (float t, float tension)
{
    // tension 0 = rak linje; positiv = långsam start, negativ = snabb start.
    if (std::abs (tension) < 1.0e-3f)
        return t;
    const auto exponent = std::pow (4.0f, tension);
    return std::pow (t, exponent);
}

float SidechainCurve::evaluate (float x) const
{
    for (size_t i = 0; i + 1 < points.size(); ++i)
    {
        const auto& a = points[i];
        const auto& b = points[i + 1];
        if (x <= b.x)
        {
            const auto width = b.x - a.x;
            const auto t = width > 0.0f ? (x - a.x) / width : 1.0f;
            return a.y + (b.y - a.y) * shape (t, a.tension);
        }
    }
    return points.back().y;
}

void SidechainCurve::rebuildTable()
{
    for (int i = 0; i < tableSize; ++i)
        table[(size_t) i] = evaluate ((float) i / (float) (tableSize - 1));
}

float SidechainCurve::lookup (float phase) const
{
    const auto position = juce::jlimit (0.0f, 1.0f, phase) * (float) (tableSize - 1);
    const auto index = (int) position;
    const auto next = std::min (index + 1, tableSize - 1);
    const auto fraction = position - (float) index;
    const auto a = table[(size_t) index].load();
    return a + (table[(size_t) next].load() - a) * fraction;
}

juce::ValueTree SidechainCurve::toValueTree() const
{
    juce::ValueTree tree (treeType);
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

void SidechainCurve::fromValueTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (treeType))
        return;

    std::vector<Point> loaded;
    for (const auto& child : tree)
        loaded.push_back ({ (float) child["x"], (float) child["y"], (float) child["tension"] });
    setPoints (std::move (loaded));
}

namespace Sidechain
{
    juce::StringArray lengthNames()
    {
        return { "Kick", "1/16", "1/8", "3/16", "1/4", "1/2" };
    }

    double lengthInQuarters (int index)
    {
        constexpr double quarters[] { 0.0, 0.25, 0.5, 0.75, 1.0, 2.0 };
        return quarters[juce::jlimit (0, (int) std::size (quarters) - 1, index)];
    }

    void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
    {
        using namespace juce;
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scMode", 1 }, "Sidechain Mode",
                                                            StringArray { "Kick envelope", "Curve" }, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scLength", 1 }, "Sidechain Length", lengthNames(), 0));
    }
}
