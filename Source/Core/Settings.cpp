#include "Settings.h"

namespace
{
    // Sökvägen (som vanlig text: ingen statisk JUCE-resurs som måste rivas när DLL:en laddas ur).
    std::string& overridePath()
    {
        static std::string path;
        return path;
    }

    std::unique_ptr<juce::XmlElement> readRoot()
    {
        if (auto xml = juce::XmlDocument::parse (Settings::file()); xml != nullptr && xml->hasTagName ("Settings"))
            return xml;
        return std::make_unique<juce::XmlElement> ("Settings");
    }
}

namespace Settings
{
    juce::File rootFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Kickbase");
    }

    juce::File file()
    {
        if (! overridePath().empty())
            return juce::File (juce::String::fromUTF8 (overridePath().c_str()));
        return rootFolder().getChildFile ("settings.xml");
    }

    void useFile (const juce::File& other)
    {
        overridePath() = other == juce::File() ? std::string() : std::string (other.getFullPathName().toRawUTF8());
    }

    juce::String get (const juce::String& key, const juce::String& fallback)
    {
        const auto root = readRoot();
        return root->hasAttribute (key) ? root->getStringAttribute (key) : fallback;
    }

    void set (const juce::String& key, const juce::String& value)
    {
        auto root = readRoot();
        if (value.isEmpty())
            root->removeAttribute (key);
        else
            root->setAttribute (key, value);

        file().getParentDirectory().createDirectory();
        root->writeTo (file());
    }

    juce::File defaultFolder (const juce::String& key)
    {
        const auto name = key == "presetsFolder" ? "Presets" : key == "exportsFolder" ? "Exports" : "Wavetables";
        return rootFolder().getChildFile (name);
    }

    static juce::File chosenOrDefault (const juce::String& key)
    {
        const auto chosen = get (key);
        if (chosen.isNotEmpty() && juce::File::isAbsolutePath (chosen))
            return juce::File (chosen);
        return defaultFolder (key);
    }

    juce::File presetsFolder()    { return chosenOrDefault ("presetsFolder"); }
    juce::File exportsFolder()    { return chosenOrDefault ("exportsFolder"); }
    juce::File wavetablesFolder() { return chosenOrDefault ("wavetablesFolder"); }

    void setFolder (const juce::String& key, const juce::File& folder)
    {
        set (key, folder == juce::File() || folder == defaultFolder (key) ? juce::String() : folder.getFullPathName());
    }

    juce::String aiKey()
    {
        return get ("aiKey").trim();
    }

    juce::String makeKey()
    {
        juce::String key;
        for (int i = 0; i < 4; ++i)
            key << juce::String::toHexString (juce::Random::getSystemRandom().nextInt (0x10000)).paddedLeft ('0', 4);
        return key.toLowerCase();
    }

    float uiScale()
    {
        const auto scale = (float) get ("uiScale").getDoubleValue();
        return scale >= 0.4f && scale <= 2.0f ? scale : 0.0f;
    }

    void setUiScale (float scale)
    {
        set ("uiScale", juce::String (scale, 3));
    }
}
