#include "ProjectFile.h"

namespace
{
    const juce::String versionAttribute ("kbprojVersion");
    const juce::String pluginAttribute ("pluginVersion");
}

namespace ProjectFile
{
    bool save (juce::AudioProcessor& processor, const juce::File& file, const juce::String& pluginVersion, juce::String& error)
    {
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        auto xml = processor.getXmlFromBinary (state.getData(), (int) state.getSize());
        if (xml == nullptr)
        {
            error = "Kunde inte läsa projektets tillstånd.";
            return false;
        }
        xml->setAttribute (versionAttribute, formatVersion);
        xml->setAttribute (pluginAttribute, pluginVersion);

        juce::TemporaryFile temporary (file);
        if (! temporary.getFile().replaceWithText (xml->toString(), false, false)) // UTF-8, utan BOM
        {
            error = "Kunde inte skriva " + file.getFullPathName() + ".";
            return false;
        }
        if (! temporary.overwriteTargetFileWithTemporary())
        {
            error = "Kunde inte ersätta " + file.getFullPathName() + ".";
            return false;
        }
        return true;
    }

    bool load (juce::AudioProcessor& processor, const juce::Identifier& rootType, const juce::File& file, juce::String& error)
    {
        if (! file.existsAsFile())
        {
            error = "Filen finns inte: " + file.getFullPathName();
            return false;
        }
        if (file.getSize() > maxFileBytes)
        {
            error = "Filen är för stor för ett projekt.";
            return false;
        }

        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr)
        {
            error = "Filen är inte ett projekt (ingen giltig XML).";
            return false;
        }
        if (! xml->hasTagName (rootType.toString()))
        {
            error = "Filen är inte ett Kickbase-projekt.";
            return false;
        }
        const auto version = xml->getIntAttribute (versionAttribute, 0);
        if (version < 1)
        {
            error = "Filen saknar projektversion.";
            return false;
        }
        if (version > formatVersion)
        {
            error = "Projektet är sparat med en nyare version (" + juce::String (version) + "). Uppdatera Kickbase.";
            return false;
        }

        // Attributen hör till filen, inte till tillståndet: tas bort så att de inte hamnar i parameterträdet och sparas om.
        xml->removeAttribute (versionAttribute);
        xml->removeAttribute (pluginAttribute);
        juce::MemoryBlock state;
        juce::AudioProcessor::copyXmlToBinary (*xml, state);
        processor.setStateInformation (state.getData(), (int) state.getSize());
        return true;
    }
}
