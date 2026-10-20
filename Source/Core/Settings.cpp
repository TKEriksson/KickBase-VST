#include "Settings.h"
#include <atomic>
#include <array>

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
        const auto name = key == "presetsFolder" ? "Presets" : key == "exportsFolder" ? "Exports" : key == "libraryFolder" ? "Library" : "Wavetables";
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
    juce::File libraryFolder()    { return chosenOrDefault ("libraryFolder"); }
    juce::File folderOf (const juce::String& key) { return chosenOrDefault (key); }

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
    juce::String protect (const juce::String& plain)
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

    juce::String unprotect (const juce::String& stored)
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

    juce::String chatProvider()
    {
        return get ("chatProvider") == "gemini" ? "gemini" : "anthropic";
    }

    void setChatProvider (const juce::String& provider)
    {
        set ("chatProvider", provider == "gemini" ? juce::String ("gemini") : juce::String());
    }

    juce::String geminiApiKey()
    {
        const auto stored = get ("geminiApiKey");
        return stored.isEmpty() ? juce::String() : unprotect (stored).trim();
    }

    void setGeminiApiKey (const juce::String& key)
    {
        set ("geminiApiKey", key.trim().isEmpty() ? juce::String() : protect (key.trim()));
    }

    juce::String geminiModel()
    {
        const auto model = get ("geminiModel").trim();
        return model.isEmpty() ? juce::String ("gemini-3.6-flash") : model;
    }

    void setGeminiModel (const juce::String& model)
    {
        set ("geminiModel", model.trim() == "gemini-3.6-flash" ? juce::String() : model.trim());
    }

    int geminiToolsCheck (const juce::String& model, const juce::String& key)
    {
        const auto value = get ("geminiTools:" + model.trim());
        const auto hash = juce::String::toHexString (key.trim().hashCode64());
        return value.startsWith (hash + ":") ? value.getTrailingIntValue() : -1;
    }

    void setGeminiToolsCheck (const juce::String& model, const juce::String& key, bool works)
    {
        set ("geminiTools:" + model.trim(), juce::String::toHexString (key.trim().hashCode64()) + ":" + (works ? "1" : "0"));
    }

    namespace
    {
        std::atomic<int> reduceState { -1 }; // -1 = inte läst än (filen tolkas inte vid varje fråga)
        std::atomic<int> analysisStepState { -1 };
        std::array<std::atomic<int>, numVisuals> fpsStepStates {}, fillStates {}; // 0 = inte läst; steg 1-5; fyllning 1 = av, 2 = på
        std::atomic<int> visualChanges { 0 };

        int defaultFpsStep (Visual visual) { return visual == Visual::analyzer ? 3 : 4; } // 20 respektive 30 bilder per sekund
    }

    int visualVersion() { return visualChanges.load(); }

    bool reduceAnimations()
    {
        auto state = reduceState.load();
        if (state < 0)
        {
            state = get ("reduceAnimations") == "1" ? 1 : 0;
            reduceState = state;
        }
        return state == 1;
    }

    void setReduceAnimations (bool reduce)
    {
        reduceState = reduce ? 1 : 0;
        set ("reduceAnimations", reduce ? "1" : juce::String());
        ++visualChanges;
    }

    const char* visualKey (Visual visual)
    {
        constexpr const char* keys[] { "Analyzer", "Sidechain", "Maximizer", "Soundstage", "Effects", "Mixer", "TopBar", "Other" };
        return keys[(int) visual];
    }

    int visualFpsStep (Visual visual)
    {
        auto& cached = fpsStepStates[(size_t) visual];
        auto step = cached.load();
        if (step == 0)
        {
            const auto saved = get (juce::String ("fps") + visualKey (visual)).getIntValue();
            step = saved >= 1 && saved <= 5 ? saved : defaultFpsStep (visual);
            cached = step;
        }
        return step;
    }

    void setVisualFpsStep (Visual visual, int step)
    {
        step = juce::jlimit (1, 5, step);
        fpsStepStates[(size_t) visual] = step;
        set (juce::String ("fps") + visualKey (visual), step == defaultFpsStep (visual) ? juce::String() : juce::String (step));
        ++visualChanges;
    }

    bool visualFill (Visual visual)
    {
        auto& cached = fillStates[(size_t) visual];
        auto state = cached.load();
        if (state == 0)
        {
            state = get (juce::String ("fill") + visualKey (visual)) == "0" ? 1 : 2;
            cached = state;
        }
        return state == 2;
    }

    void setVisualFill (Visual visual, bool fill)
    {
        fillStates[(size_t) visual] = fill ? 2 : 1;
        set (juce::String ("fill") + visualKey (visual), fill ? juce::String() : "0");
        ++visualChanges;
    }

    int visualHz (Visual visual, int baseHz)
    {
        // Vyns egen takt (baseHz, normalt 30) skalas med det valda steget: 30 bilder/s ger vyns vanliga takt.
        return animationHz (juce::jmax (1, (int) std::lround (baseHz * fpsForStep (visualFpsStep (visual)) / 30.0)));
    }

    int spectrumAnalysisStep()
    {
        auto step = analysisStepState.load();
        if (step < 0)
        {
            const auto saved = get ("spectrumAnalysisStep").getIntValue();
            step = saved >= 1 && saved <= 5 ? saved : 3;
            analysisStepState = step;
        }
        return step;
    }

    void setSpectrumAnalysisStep (int step)
    {
        step = juce::jlimit (1, 5, step);
        analysisStepState = step;
        set ("spectrumAnalysisStep", step == 3 ? juce::String() : juce::String (step));
        ++visualChanges;
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
