#include "Settings.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <wincrypt.h>
 #pragma comment (lib, "crypt32.lib")
#endif

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

    // Nyckeln krypteras med Windows DPAPI (annars bara base64: det finns ingen nyckelring att luta sig mot).
    static juce::String protect (const juce::String& plain)
    {
       #if JUCE_WINDOWS
        const auto utf8 = plain.toStdString();
        DATA_BLOB in { (DWORD) utf8.size(), (BYTE*) utf8.data() }, out {};
        if (CryptProtectData (&in, L"Kickbase", nullptr, nullptr, nullptr, 0, &out))
        {
            const auto text = "dpapi:" + juce::Base64::toBase64 (out.pbData, out.cbData);
            LocalFree (out.pbData);
            return text;
        }
       #endif
        return "plain:" + juce::Base64::toBase64 (plain);
    }

    static juce::String unprotect (const juce::String& stored)
    {
        juce::MemoryOutputStream decoded;
        if (stored.startsWith ("plain:") && juce::Base64::convertFromBase64 (decoded, stored.fromFirstOccurrenceOf (":", false, false)))
            return decoded.toString();
       #if JUCE_WINDOWS
        if (stored.startsWith ("dpapi:") && juce::Base64::convertFromBase64 (decoded, stored.fromFirstOccurrenceOf (":", false, false)))
        {
            DATA_BLOB in { (DWORD) decoded.getDataSize(), (BYTE*) decoded.getData() }, out {};
            if (CryptUnprotectData (&in, nullptr, nullptr, nullptr, nullptr, 0, &out))
            {
                const auto text = juce::String::fromUTF8 ((const char*) out.pbData, (int) out.cbData);
                LocalFree (out.pbData);
                return text;
            }
        }
       #endif
        return {};
    }

    juce::String chatApiKey()
    {
        const auto stored = get ("chatApiKey");
        return stored.isEmpty() ? juce::String() : unprotect (stored).trim();
    }

    void setChatApiKey (const juce::String& key)
    {
        set ("chatApiKey", key.trim().isEmpty() ? juce::String() : protect (key.trim()));
    }

    juce::String chatModel()
    {
        const auto model = get ("chatModel").trim();
        return model.isEmpty() ? juce::String ("claude-sonnet-5-5") : model;
    }

    void setChatModel (const juce::String& model)
    {
        set ("chatModel", model == "claude-sonnet-5-5" ? juce::String() : model);
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
