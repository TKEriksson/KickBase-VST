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

    // Handtagens x ligger inom segmentet: då är x(t) monoton och kurvan en funktion av tiden.
    for (size_t i = 0; i < newPoints.size(); ++i)
    {
        auto& p = newPoints[i];
        const auto toNext = i + 1 < newPoints.size() ? newPoints[i + 1].x - p.x : 0.0f;
        const auto fromPrevious = i > 0 ? p.x - newPoints[i - 1].x : 0.0f;
        p.outDx = juce::jlimit (0.0f, toNext, p.outDx);
        p.inDx = juce::jlimit (-fromPrevious, 0.0f, p.inDx);
        p.outDy = juce::jlimit (-maxValue, maxValue, p.outDy);
        p.inDy = juce::jlimit (-maxValue, maxValue, p.inDy);
    }

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
            if (mode == Mode::bezier && width > 0.0f)
            {
                // Kubisk Bézier a, a+out, b+in, b. x(t) är monoton (handtagens x ligger i segmentet), så t hittas med bisektion.
                const auto x1 = a.x + a.outDx, x2 = b.x + b.inDx;
                const auto y1 = a.y + a.outDy, y2 = b.y + b.inDy;
                const auto cubic = [] (float p0, float p1, float p2, float p3, float u)
                {
                    const auto v = 1.0f - u;
                    return v * v * v * p0 + 3.0f * v * v * u * p1 + 3.0f * v * u * u * p2 + u * u * u * p3;
                };
                auto low = 0.0f, high = 1.0f;
                for (int i = 0; i < 24; ++i)
                {
                    const auto mid = 0.5f * (low + high);
                    (cubic (a.x, x1, x2, b.x, mid) < seconds ? low : high) = mid;
                }
                return juce::jlimit (0.0f, maxValue, cubic (a.y, y1, y2, b.y, 0.5f * (low + high)));
            }
            const auto t =width > 0.0f ? juce::jlimit (0.0f, 1.0f, (seconds - a.x) / width) : 1.0f;
            return a.y + (b.y - a.y) * shape (t, a.tension);
        }
    }
    return points.back().y;
}

void NodeEnvelope::setMode (Mode newMode)
{
    if (newMode == mode)
        return;
    mode = newMode;
    rebuildTable();
    ++version;
}

std::vector<NodeEnvelope::Point> NodeEnvelope::withHandlesFromTension (std::vector<Point> pts)
{
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        auto& a = pts[i];
        auto& b = pts[i + 1];
        const auto width = b.x - a.x, rise = b.y - a.y;
        // Normaliserade y-handtag Y1, Y2 så att kubiken träffar shape() vid 1/3 och 2/3 (x-handtagen på width/3 ger linjär x).
        const auto s1 = shape (1.0f / 3.0f, a.tension), s2 = shape (2.0f / 3.0f, a.tension);
        const auto A = 9.0f * s1 - 1.0f / 3.0f, B = 9.0f * s2 - 8.0f / 3.0f;
        const auto y1 = (4.0f * A - 2.0f * B) / 12.0f, y2 = (4.0f * B - 2.0f * A) / 12.0f;
        a.outDx = width / 3.0f;
        a.outDy = rise * y1;
        b.inDx = -width / 3.0f;
        b.inDy = rise * (y2 - 1.0f);
    }
    return pts;
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
        if (mode == Mode::bezier)
        {
            point.setProperty ("outDx", p.outDx, nullptr);
            point.setProperty ("outDy", p.outDy, nullptr);
            point.setProperty ("inDx", p.inDx, nullptr);
            point.setProperty ("inDy", p.inDy, nullptr);
        }
        tree.appendChild (point, nullptr);
    }
    if (mode == Mode::bezier)
        tree.setProperty ("mode", "bezier", nullptr); // saknas = tension, så gamla projekt är orörda
    return tree;
}

void NodeEnvelope::fromValueTree (const juce::ValueTree& tree)
{
    std::vector<Point> loaded;
    for (const auto& child : tree)
        loaded.push_back ({ (float) child["x"], (float) child["y"], (float) child["tension"],
                            (float) child["outDx"], (float) child["outDy"], (float) child["inDx"], (float) child["inDy"] });
    mode = tree["mode"].toString() == "bezier" ? Mode::bezier : Mode::tension;
    if (loaded.size() >= 2)
        setPoints (std::move (loaded));
    else
    {
        mode = Mode::tension;
        reset();
    }
}
