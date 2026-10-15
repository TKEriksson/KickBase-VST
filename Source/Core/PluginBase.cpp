#include "PluginBase.h"

namespace
{
    constexpr double maxClickSeconds = 2.0;
    const juce::Identifier clickTreeType { "ClickSample" };
    const juce::String presetFileExtension { ".mofp" };

    juce::String presetProperty (Presets::Section section) { return "preset" + Presets::sectionName (section); }
}

PluginBase::PluginBase (const BusesProperties& buses, juce::AudioProcessorValueTreeState::ParameterLayout layout, bool isEffect)
    : AudioProcessor (buses),
      apvts (*this, nullptr, "Parameters", std::move (layout)),
      params (apvts),
      effectVersion (isEffect)
{
    presetNames.fill ("Default");
    presetNames[(size_t) Presets::Section::full] = "Init";
    presetNames[(size_t) Presets::Section::pattern] = "Rolling";
}

void PluginBase::prepareBase (double sampleRate)
{
    currentSampleRate = sampleRate;
    clock.prepare (sampleRate);
    rebuildClickForSampleRate();
}

KickParams PluginBase::readModulatedKick (int keyPitchClass) const
{
    using Dest = Modulation::Destination;
    auto p = params.readKick (keyPitchClass);
    p.pitchSemis = modulator.apply (Dest::kickPitch, p.pitchSemis);
    p.decayMs = modulator.apply (Dest::kickDecay, p.decayMs);
    return p;
}

BassParams PluginBase::readModulatedBass (int keyPitchClass) const
{
    using Dest = Modulation::Destination;
    auto p = params.readBass (keyPitchClass);
    p.cutoffHz = modulator.apply (Dest::bassCutoff, p.cutoffHz);
    p.resonance = modulator.apply (Dest::bassResonance, p.resonance);
    p.envAmount = modulator.apply (Dest::bassEnvAmount, p.envAmount);
    p.drive = modulator.apply (Dest::bassDrive, p.drive);
    p.level = modulator.apply (Dest::bassLevel, p.level);

    // Tune ingår redan i rootHz; lägg bara på skillnaden.
    if (modulator.isModulated (Dest::bassTune))
    {
        const auto tune = params.get (ParamID::bassTune);
        p.rootHz *= std::exp2 ((modulator.apply (Dest::bassTune, tune) - tune) / 12.0f);
    }
    return p;
}

juce::Optional<juce::AudioPlayHead::PositionInfo> PluginBase::getHostPosition() const
{
    if (auto* hostPlayHead = getPlayHead())
        return hostPlayHead->getPosition();

    return juce::nullopt;
}

void PluginBase::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("effectOrder", effectOrder.toString(), nullptr);
    for (int i = 0; i < Presets::numSections; ++i)
        state.setProperty (presetProperty ((Presets::Section) i), presetNames[(size_t) i], nullptr);
    state.appendChild (pattern.toValueTree(), nullptr);
    state.appendChild (clickToValueTree(), nullptr);
    state.appendChild (sidechainCurve.toValueTree(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginBase::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    effectOrder.fromString (state.getProperty ("effectOrder").toString());
    for (int i = 0; i < Presets::numSections; ++i)
        presetNames[(size_t) i] = state.getProperty (presetProperty ((Presets::Section) i), presetNames[(size_t) i]).toString();

    if (auto patternTree = state.getChildWithName (Pattern::treeType); patternTree.isValid())
    {
        pattern.fromValueTree (patternTree);
        state.removeChild (patternTree, nullptr);
        ++patternVersion;
    }

    if (auto clickTree = state.getChildWithName (clickTreeType); clickTree.isValid())
    {
        clickFromValueTree (clickTree);
        state.removeChild (clickTree, nullptr);
    }

    if (auto curveTree = state.getChildWithName (SidechainCurve::treeType); curveTree.isValid())
    {
        sidechainCurve.fromValueTree (curveTree);
        state.removeChild (curveTree, nullptr);
    }

    apvts.replaceState (state);
}

void PluginBase::patternChanged()
{
    ++patternVersion;
    markStateChanged();
}

void PluginBase::markStateChanged()
{
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void PluginBase::resetSectionToDefaults (Presets::Section section)
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && Presets::belongsTo (section, ranged->getParameterID()))
            Presets::setParameter (*ranged, ranged->getDefaultValue());
}

void PluginBase::loadFactoryPreset (Presets::Section section, int index)
{
    Presets::applyFactory (section, effectVersion, index, apvts, pattern, sidechainCurve);

    if (section == Presets::Section::full || section == Presets::Section::effects)
        effectOrder.reset();

    if (section == Presets::Section::full)
    {
        clearClickSample();
        presetNames.fill ({}); // delarna kommer inte längre från sina egna presets
    }

    presetNames[(size_t) section] = Presets::factoryName (section, effectVersion, index);
    patternChanged();
}

juce::Array<juce::File> PluginBase::getUserPresets (Presets::Section section) const
{
    auto files = Presets::folder (section, effectVersion).findChildFiles (juce::File::findFiles, false, "*" + presetFileExtension);
    files.sort();
    return files;
}

bool PluginBase::saveUserPreset (Presets::Section section, const juce::String& name)
{
    const auto fileName = juce::File::createLegalFileName (name.trim());
    const auto directory = Presets::folder (section, effectVersion);
    if (fileName.isEmpty() || directory.createDirectory().failed())
        return false;

    juce::XmlElement root ("MyOwnFruitPreset");
    root.setAttribute ("section", Presets::sectionName (section));
    root.setAttribute ("plugin", effectVersion ? "fx" : "instrument");
    root.setAttribute ("version", 1);

    if (section == Presets::Section::full || section == Presets::Section::effects)
        root.setAttribute ("effectOrder", effectOrder.toString());

    // Värden sparas i parameterns egen enhet (ms, Hz ...) i stället för 0..1, så att filen
    // fortfarande stämmer om ett omfång ändras senare.
    auto* paramsXml = root.createNewChildElement ("Params");
    for (auto* p : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr || ! Presets::belongsTo (section, ranged->getParameterID()))
            continue;

        auto* entry = paramsXml->createNewChildElement ("Param");
        entry->setAttribute ("id", ranged->getParameterID());
        entry->setAttribute ("value", (double) ranged->convertFrom0to1 (ranged->getValue()));
    }

    if (section == Presets::Section::full || section == Presets::Section::pattern)
        root.addChildElement (pattern.toValueTree().createXml().release());

    if (section == Presets::Section::full || section == Presets::Section::kick)
        root.addChildElement (clickToValueTree().createXml().release());

    if (section == Presets::Section::full || section == Presets::Section::sidechain)
        root.addChildElement (sidechainCurve.toValueTree().createXml().release());

    if (! root.writeTo (directory.getChildFile (fileName + presetFileExtension)))
        return false;

    presetNames[(size_t) section] = fileName;
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    return true;
}

bool PluginBase::loadUserPreset (Presets::Section section, const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("MyOwnFruitPreset")
        || xml->getStringAttribute ("section") != Presets::sectionName (section))
        return false;

    // Delen till standard först: parametrar som saknas i filen (t.ex. nya sedan presetet
    // sparades) får sitt standardvärde i stället för att ärvas från förra ljudet.
    resetSectionToDefaults (section);

    if (section == Presets::Section::full || section == Presets::Section::effects)
        effectOrder.fromString (xml->getStringAttribute ("effectOrder"));

    if (const auto* paramsXml = xml->getChildByName ("Params"))
    {
        for (const auto* entry : paramsXml->getChildIterator())
        {
            const auto id = entry->getStringAttribute ("id");
            if (auto* parameter = apvts.getParameter (id); parameter != nullptr && Presets::belongsTo (section, id))
                Presets::setParameter (*parameter, parameter->convertTo0to1 ((float) entry->getDoubleAttribute ("value")));
        }
    }

    if (section == Presets::Section::full || section == Presets::Section::pattern)
        if (const auto* patternXml = xml->getChildByName (Pattern::treeType))
            pattern.fromValueTree (juce::ValueTree::fromXml (*patternXml));

    if (section == Presets::Section::full || section == Presets::Section::kick)
    {
        if (const auto* clickXml = xml->getChildByName (clickTreeType))
            clickFromValueTree (juce::ValueTree::fromXml (*clickXml));
        else
            setClickOriginal ({}, 0.0, {});
    }

    if (section == Presets::Section::full || section == Presets::Section::sidechain)
    {
        if (const auto* curveXml = xml->getChildByName (SidechainCurve::treeType))
            sidechainCurve.fromValueTree (juce::ValueTree::fromXml (*curveXml));
        else
            sidechainCurve.reset();
    }

    if (section == Presets::Section::full)
        presetNames.fill ({});

    presetNames[(size_t) section] = file.getFileNameWithoutExtension();
    patternChanged();
    return true;
}

bool PluginBase::loadClickSample (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0)
        return false;

    const auto length = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (maxClickSeconds * reader->sampleRate));
    juce::AudioBuffer<float> fileAudio ((int) reader->numChannels, length);
    reader->read (&fileAudio, 0, length, 0, true, true);

    // Mixa ner till mono.
    juce::AudioBuffer<float> mono (1, length);
    mono.clear();
    for (int ch = 0; ch < fileAudio.getNumChannels(); ++ch)
        mono.addFrom (0, 0, fileAudio, ch, 0, length, 1.0f / (float) fileAudio.getNumChannels());

    setClickOriginal (std::move (mono), reader->sampleRate, file.getFileNameWithoutExtension());
    patternChanged();
    return true;
}

void PluginBase::clearClickSample()
{
    setClickOriginal ({}, 0.0, {});
    patternChanged();
}

juce::String PluginBase::getClickSampleName() const
{
    const juce::ScopedLock lock (clickLock);
    return clickName;
}

void PluginBase::setClickOriginal (juce::AudioBuffer<float> audio, double rate, const juce::String& name)
{
    {
        const juce::ScopedLock lock (clickLock);
        clickOriginal = std::move (audio);
        clickOriginalRate = rate;
        clickName = name;
    }
    rebuildClickForSampleRate();
}

void PluginBase::rebuildClickForSampleRate()
{
    std::unique_ptr<ClickSample> fresh;

    {
        const juce::ScopedLock lock (clickLock);

        if (clickOriginal.getNumSamples() > 0 && clickOriginalRate > 0.0)
        {
            fresh = std::make_unique<ClickSample>();
            fresh->name = clickName;

            const auto ratio = clickOriginalRate / currentSampleRate;
            const auto outLength = (int) (clickOriginal.getNumSamples() / ratio);
            fresh->audio.setSize (1, outLength);

            juce::LagrangeInterpolator interpolator;
            interpolator.process (ratio, clickOriginal.getReadPointer (0), fresh->audio.getWritePointer (0), outLength);
        }
    }

    // Byt atomiskt. Den gamla samplingen hålls kvar tills nästa byte, eftersom ljudtråden
    // kan vara mitt i ett block som läser den.
    activeClick.store (fresh.get());
    retiredClick = std::move (ownedClick);
    ownedClick = std::move (fresh);
}

juce::ValueTree PluginBase::clickToValueTree() const
{
    juce::ValueTree tree (clickTreeType);
    const juce::ScopedLock lock (clickLock);

    if (clickOriginal.getNumSamples() > 0)
    {
        juce::MemoryBlock block (clickOriginal.getReadPointer (0), (size_t) clickOriginal.getNumSamples() * sizeof (float));
        tree.setProperty ("name", clickName, nullptr);
        tree.setProperty ("rate", clickOriginalRate, nullptr);
        tree.setProperty ("data", block.toBase64Encoding(), nullptr);
    }

    return tree;
}

void PluginBase::clickFromValueTree (const juce::ValueTree& tree)
{
    juce::MemoryBlock block;
    if (! tree.hasProperty ("data") || ! block.fromBase64Encoding (tree["data"].toString()))
    {
        setClickOriginal ({}, 0.0, {});
        return;
    }

    const auto numSamples = (int) (block.getSize() / sizeof (float));
    juce::AudioBuffer<float> audio (1, numSamples);
    std::memcpy (audio.getWritePointer (0), block.getData(), (size_t) numSamples * sizeof (float));

    setClickOriginal (std::move (audio), (double) tree["rate"], tree["name"].toString());
}
