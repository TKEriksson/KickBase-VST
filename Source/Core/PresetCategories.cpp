#include "PresetCategories.h"

namespace PresetCategories
{
    const juce::StringArray& names()
    {
        static const juce::StringArray list { "PSY CLEAN", "PSY ROLLING BASS", "PSY ROLLING", "MINIMAL", "GOA", "GLIDE BASSES",
                                              "SYNTH", "MID BASS", "HI BASS", "DARK PSY", "DANCE", "TRANCE", "TECHNO", "EMPIRIKAL",
                                              "GAMLA DEFAULT" };
        return list;
    }

    juce::String defaultName() { return "GAMLA DEFAULT"; }

    bool usedBy (Presets::Section section)
    {
        return section == Presets::Section::kick || section == Presets::Section::bass;
    }

    juce::String factoryCategory (Presets::Section section, const juce::String& presetName)
    {
        // Regler på namnets början. Nya fabrikspresets läggs till här; utan regel hamnar de i GAMLA DEFAULT.
        if (section == Presets::Section::kick && presetName.startsWith ("Clean "))
            return "PSY CLEAN";
        if (section == Presets::Section::bass && presetName.startsWith ("Rolling ") && presetName != "Rolling Classic")
            return "PSY ROLLING BASS";
        if (section == Presets::Section::bass && presetName.startsWith ("Mid "))
            return "MID BASS";
        if (section == Presets::Section::bass && presetName.startsWith ("Hi "))
            return "HI BASS";
        if (section == Presets::Section::bass && presetName.startsWith ("Synth "))
            return "SYNTH";
        if (section == Presets::Section::kick || section == Presets::Section::bass)
        {
            static const char* const genres[][2] = { { "Minimal ", "MINIMAL" }, { "Goa ", "GOA" }, { "Dark Psy ", "DARK PSY" }, { "Dance ", "DANCE" },
                                                     { "Trance ", "TRANCE" }, { "Techno ", "TECHNO" }, { "Empirikal ", "EMPIRIKAL" } };
            for (const auto& genre : genres)
                if (presetName.startsWith (genre[0]))
                    return genre[1];
        }
        return defaultName();
    }

    juce::String normalise (const juce::String& text)
    {
        const auto upper = text.trim().toUpperCase();
        for (const auto& name : names())
            if (name.toUpperCase() == upper)
                return name;
        return {};
    }
}
