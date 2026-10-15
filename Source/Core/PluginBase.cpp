#include "PluginBase.h"
#include "OfflineRender.h"

namespace
{
    constexpr double maxClickSeconds = 2.0;
    const juce::Identifier clickTreeType { "ClickSample" };
    const juce::String presetFileExtension { ".mofp" };

    juce::String presetProperty (Presets::Section section) { return "preset" + Presets::sectionName (section); }
    const juce::Identifier kickPitchType { "KickPitchEnvelope" }, kickAmpType { "KickAmpEnvelope" };
}

PluginBase::PluginBase (const BusesProperties& buses, juce::AudioProcessorValueTreeState::ParameterLayout layout, bool isEffect)
    : AudioProcessor (buses),
      apvts (*this, nullptr, "Parameters", std::move (layout)),
      params (apvts),
      effectVersion (isEffect)
{
    params.setModulator (&modulator); // ljudtrådens parametrar läses med LFO-modulering
    presetNames.fill ("Default");
    bassPresetNames.fill ("Default");
    presetNames[(size_t) Presets::Section::full] = "Init";
    presetNames[(size_t) Presets::Section::pattern] = "Rolling";
}

void PluginBase::prepareBase (double sampleRate)
{
    currentSampleRate = sampleRate;
    clock.prepare (sampleRate);
    rebuildClickForSampleRate();
}

// params läser med modulering (se konstruktorn), så kick och bas får LFO:erna härifrån.
KickParams PluginBase::readModulatedKick (int keyPitchClass) const
{
    auto p = params.readKick (keyPitchClass);
    p.pitchEnv = &kickPitchEnv;
    p.ampEnv = &kickAmpEnv;
    return p;
}

KickParams PluginBase::kickForDisplay (int keyPitchClass) const
{
    auto p = displayParams.readKick (keyPitchClass);
    p.pitchEnv = &kickPitchEnv;
    p.ampEnv = &kickAmpEnv;
    return p;
}
BassParams PluginBase::readModulatedBass (int layer, int rootMidiNote) const { return params.readBass (layer, rootMidiNote); }

juce::String& PluginBase::presetNameSlot (Presets::Section section, int layer)
{
    return section == Presets::Section::bass ? bassPresetNames[(size_t) juce::jlimit (0, ParamID::numBassLayers - 1, layer)]
                                             : presetNames[(size_t) section];
}

juce::String PluginBase::getPresetName (Presets::Section section, int layer) const
{
    return section == Presets::Section::bass ? bassPresetNames[(size_t) juce::jlimit (0, ParamID::numBassLayers - 1, layer)]
                                             : presetNames[(size_t) section];
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
    for (int i = 0; i < Presets::numSections; ++i)
        state.setProperty (presetProperty ((Presets::Section) i), presetNames[(size_t) i], nullptr);
    for (int layer = 0; layer < ParamID::numBassLayers; ++layer)
        state.setProperty ("presetBass" + juce::String (layer + 1), bassPresetNames[(size_t) layer], nullptr);
    state.appendChild (pattern.toValueTree(), nullptr);
    state.appendChild (clickToValueTree(), nullptr);
    state.appendChild (sidechainCurve.toValueTree(), nullptr);
    state.appendChild (modRoutes.toValueTree (getParameters()), nullptr);
    state.appendChild (kickPitchEnv.toValueTree (kickPitchType), nullptr);
    state.appendChild (kickAmpEnv.toValueTree (kickAmpType), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginBase::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    for (int i = 0; i < Presets::numSections; ++i)
        presetNames[(size_t) i] = state.getProperty (presetProperty ((Presets::Section) i), presetNames[(size_t) i]).toString();
    for (int layer = 0; layer < ParamID::numBassLayers; ++layer)
        bassPresetNames[(size_t) layer] = state.getProperty ("presetBass" + juce::String (layer + 1), bassPresetNames[(size_t) layer]).toString();

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

    for (auto [envelope, type] : { std::pair<NodeEnvelope*, juce::Identifier> { &kickPitchEnv, kickPitchType }, { &kickAmpEnv, kickAmpType } })
    {
        if (auto tree = state.getChildWithName (type); tree.isValid())
        {
            envelope->fromValueTree (tree);
            state.removeChild (tree, nullptr);
        }
        else
            envelope->reset();
    }

    modRoutes.fromValueTree (state.getChildWithName (ModRoutes::treeType), getParameters());
    if (auto routesTree = state.getChildWithName (ModRoutes::treeType); routesTree.isValid())
        state.removeChild (routesTree, nullptr);

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

void PluginBase::resetSectionToDefaults (Presets::Section section, int layer)
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && Presets::belongsTo (section, ranged->getParameterID(), layer))
            Presets::setParameter (*ranged, ranged->getDefaultValue());
}

void PluginBase::loadFactoryPreset (Presets::Section section, int index, int layer)
{
    Presets::applyFactory (section, effectVersion, index, apvts, pattern, sidechainCurve, layer);

    if (section == Presets::Section::full || section == Presets::Section::lfo)
        modRoutes.clear();

    if (section == Presets::Section::full || section == Presets::Section::kick)
    {
        kickPitchEnv.reset();
        kickAmpEnv.reset();
    }

    if (section == Presets::Section::full)
    {
        clearClickSample();
        presetNames.fill ({}); // delarna kommer inte längre från sina egna presets
        bassPresetNames.fill ({});
    }

    presetNameSlot (section, layer) = Presets::factoryName (section, effectVersion, index);
    patternChanged();
}

juce::Array<juce::File> PluginBase::getUserPresets (Presets::Section section) const
{
    auto files = Presets::folder (section, effectVersion).findChildFiles (juce::File::findFiles, false, "*" + presetFileExtension);
    files.sort();
    return files;
}

bool PluginBase::saveUserPreset (Presets::Section section, const juce::String& name, int layer)
{
    const auto fileName = juce::File::createLegalFileName (name.trim());
    const auto directory = Presets::folder (section, effectVersion);
    if (fileName.isEmpty() || directory.createDirectory().failed())
        return false;

    juce::XmlElement root ("KickbasePreset");
    root.setAttribute ("section", Presets::sectionName (section));
    root.setAttribute ("plugin", effectVersion ? "fx" : "instrument");
    root.setAttribute ("version", 1);

    // Värden sparas i parameterns egen enhet (ms, Hz ...) i stället för 0..1, så att filen
    // fortfarande stämmer om ett omfång ändras senare.
    auto* paramsXml = root.createNewChildElement ("Params");
    for (auto* p : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr || ! Presets::belongsTo (section, ranged->getParameterID(), layer))
            continue;

        // Basens presets sparas som lager 1, så att de kan laddas i vilket lager som helst.
        auto* entry = paramsXml->createNewChildElement ("Param");
        entry->setAttribute ("id", section == Presets::Section::bass ? Presets::toBassLayer (ranged->getParameterID(), 0)
                                                                     : ranged->getParameterID());
        entry->setAttribute ("value", (double) ranged->convertFrom0to1 (ranged->getValue()));
    }

    if (section == Presets::Section::full || section == Presets::Section::pattern)
        root.addChildElement (pattern.toValueTree().createXml().release());

    if (section == Presets::Section::full || section == Presets::Section::kick)
    {
        root.addChildElement (clickToValueTree().createXml().release());
        root.addChildElement (kickPitchEnv.toValueTree (kickPitchType).createXml().release());
        root.addChildElement (kickAmpEnv.toValueTree (kickAmpType).createXml().release());
    }

    if (section == Presets::Section::full || section == Presets::Section::sidechain)
        root.addChildElement (sidechainCurve.toValueTree().createXml().release());

    if (section == Presets::Section::full || section == Presets::Section::lfo)
        root.addChildElement (modRoutes.toValueTree (getParameters()).createXml().release());

    if (! root.writeTo (directory.getChildFile (fileName + presetFileExtension)))
        return false;

    presetNameSlot (section, layer) = fileName;
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    return true;
}

bool PluginBase::loadUserPreset (Presets::Section section, const juce::File& file, int layer)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("KickbasePreset")
        || xml->getStringAttribute ("section") != Presets::sectionName (section))
        return false;

    // Delen till standard först: parametrar som saknas i filen (t.ex. nya sedan presetet
    // sparades) får sitt standardvärde i stället för att ärvas från förra ljudet.
    resetSectionToDefaults (section, layer);

    if (const auto* paramsXml = xml->getChildByName ("Params"))
    {
        for (const auto* entry : paramsXml->getChildIterator())
        {
            auto id = entry->getStringAttribute ("id");
            if (section == Presets::Section::bass)
                id = Presets::toBassLayer (id, layer);
            if (auto* parameter = apvts.getParameter (id); parameter != nullptr && Presets::belongsTo (section, id, layer))
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

        for (auto [envelope, type] : { std::pair<NodeEnvelope*, juce::Identifier> { &kickPitchEnv, kickPitchType }, { &kickAmpEnv, kickAmpType } })
        {
            if (const auto* envelopeXml = xml->getChildByName (type))
                envelope->fromValueTree (juce::ValueTree::fromXml (*envelopeXml));
            else
                envelope->reset();
        }
    }

    if (section == Presets::Section::full || section == Presets::Section::sidechain)
    {
        if (const auto* curveXml = xml->getChildByName (SidechainCurve::treeType))
            sidechainCurve.fromValueTree (juce::ValueTree::fromXml (*curveXml));
        else
            sidechainCurve.reset();
    }

    if (section == Presets::Section::full || section == Presets::Section::lfo)
    {
        if (const auto* routesXml = xml->getChildByName (ModRoutes::treeType))
            modRoutes.fromValueTree (juce::ValueTree::fromXml (*routesXml), getParameters());
        else
            modRoutes.clear();
    }

    if (section == Presets::Section::full)
    {
        presetNames.fill ({});
        bassPresetNames.fill ({});
    }

    presetNameSlot (section, layer) = file.getFileNameWithoutExtension();
    patternChanged();
    return true;
}

bool PluginBase::exportWav (const juce::File& file, ExportPart part, int bars)
{
    auto copy = createOfflineCopy();
    juce::MemoryBlock state;
    getStateInformation (state);
    copy->setStateInformation (state.getData(), (int) state.getSize());

    auto turnOff = [&copy] (const char* id)
    {
        if (auto* parameter = copy->apvts.getParameter (id))
            parameter->setValueNotifyingHost (0.0f);
    };
    if (part == ExportPart::kick)
        for (int layer = 0; layer < ParamID::numBassLayers; ++layer)
            turnOff (ParamID::bass (layer, BassField::on).toRawUTF8());
    if (part == ExportPart::bass)
        turnOff (ParamID::kickOn);

    // Fyra takters förrulle: mönstret och LFO:erna börjar om var fjärde takt, så loopen börjar
    // på mönstrets början med förra varvets svansar. En takt extra täcker masterns latens.
    constexpr int preRoll = 4;
    OfflineRender::Settings settings;
    settings.sampleRate = currentSampleRate > 0.0 ? currentSampleRate : 48000.0;
    settings.bpm = transportStatus.bpm.load();
    const auto note = transportStatus.keyNote.load();
    settings.notes = { note >= 0 ? note : 60 };
    settings.bars = preRoll + bars + 1;
    const auto audio = OfflineRender::render (*copy, settings);

    const auto samplesPerBar = settings.sampleRate * 60.0 / settings.bpm * 4.0;
    const auto start = (int) std::round (preRoll * samplesPerBar) + copy->getLatencySamples();
    const auto length = (int) std::round (bars * samplesPerBar);
    if (start + length > audio.getNumSamples())
        return false;

    juce::AudioBuffer<float> loop (audio.getNumChannels(), length);
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        loop.copyFrom (ch, 0, audio, ch, start, length);
    return OfflineRender::writeWav (loop, settings.sampleRate, file);
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
