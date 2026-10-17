#include "PluginBase.h"
#include "AiChat.h"
#include "OfflineRender.h"
#include "Mixer.h"

namespace
{
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
    modulator.setLfoCurves (&lfoCurves);
    presetNames.fill ("Default");
    bassPresetNames.fill ("Default");
    presetNames[(size_t) Presets::Section::full] = "Init";
    presetNames[(size_t) Presets::Section::pattern] = "Rolling";
}

PluginBase::~PluginBase() = default;

AiChat& PluginBase::getAiChat()
{
    if (aiChat == nullptr)
        aiChat = std::make_unique<AiChat> (*this);
    return *aiChat;
}

void PluginBase::prepareBase (double sampleRate)
{
    currentSampleRate = sampleRate;
    clock.prepare (sampleRate);
    kickSample.prepare (sampleRate);
    for (auto& slot : bassSamples)
        slot.prepare (sampleRate);
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
BassParams PluginBase::readModulatedBass (int layer, int rootMidiNote) const
{
    auto bass = params.readBass (layer, rootMidiNote);
    bass.oscA.user = userTable (layer, 0);
    bass.oscB.user = userTable (layer, 1);
    bass.oscC.user = userTable (layer, 2);
    bass.sampleData = getActiveBassSample (layer);
    return bass;
}

void PluginBase::setUserTable (int slot, std::unique_ptr<Wavetable> table, std::vector<float> frames, const juce::String& name)
{
    // Den gamla tabellen kan läsas av ljudtråden ett block till: den sparas undan en stund.
    const auto* newTable = table.get();
    auto old = std::move (userOwned[(size_t) slot]);
    userOwned[(size_t) slot] = std::move (table);
    userTables[(size_t) slot].store (newTable);
    if (old != nullptr)
        retiredTables.push_back (std::move (old));
    while (retiredTables.size() > 4)
        retiredTables.erase (retiredTables.begin());

    userFrames[(size_t) slot] = std::move (frames);
    userNames[(size_t) slot] = name;
}

bool PluginBase::loadUserTable (int layer, int osc, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (layer, ParamID::numBassLayers) || ! juce::isPositiveAndBelow (osc, ParamID::oscsPerLayer))
        return false;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    auto readMono = [&formats] (const juce::File& source, std::vector<float>& mono, int maxLength)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));
        if (reader == nullptr || reader->lengthInSamples < 16)
            return false;
        const auto length = (int) std::min<juce::int64> (reader->lengthInSamples, maxLength);
        juce::AudioBuffer<float> audio ((int) reader->numChannels, length);
        reader->read (&audio, 0, length, 0, true, true);
        mono.assign ((size_t) length, 0.0f);
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            for (int i = 0; i < length; ++i)
                mono[(size_t) i] += audio.getSample (ch, i) / (float) audio.getNumChannels();
        return true;
    };

    std::vector<float> mono;
    if (file.isDirectory())
    {
        // En mapp med enkelcykel-filer (som AKWF): varje fil sträcks till en cykel om 2048 samples, och högst 16 av dem väljs jämnt.
        auto files = file.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        if (files.size() < 2)
            return false;

        const auto count = std::min (16, files.size());
        for (int i = 0; i < count; ++i)
        {
            std::vector<float> one;
            if (! readMono (files[count == 1 ? 0 : (int) std::lround ((double) i * (files.size() - 1) / (count - 1))], one, 2048 * 8))
                one.assign (2048, 0.0f);
            for (int s = 0; s < 2048; ++s)
            {
                const auto position = (double) s * (double) one.size() / 2048.0;
                const auto i0 = std::min ((int) one.size() - 1, (int) position), i1 = (i0 + 1) % (int) one.size();
                const auto fraction = (float) (position - i0);
                mono.push_back (one[(size_t) i0] + (one[(size_t) i1] - one[(size_t) i0]) * fraction);
            }
        }
    }
    else if (! readMono (file, mono, 2048 * 256)) // högst 256 cykler om 2048 samples räcker för 16 frames
        return false;

    std::vector<float> frames;
    auto table = Wavetables::fromAudio (mono, file.isDirectory() ? file.getFileName() : file.getFileNameWithoutExtension(), frames);
    if (table == nullptr)
        return false;

    setUserTable (layer * ParamID::oscsPerLayer + osc, std::move (table), std::move (frames), file.isDirectory() ? file.getFileName() : file.getFileNameWithoutExtension());

    // Välj tabellen i oscillatorn.
    if (auto* parameter = apvts.getParameter (ParamID::bass (layer, ParamID::oscField (osc, BassField::aTable))))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) Wavetables::userIndex()));
        parameter->endChangeGesture();
    }
    markStateChanged();
    return true;
}

bool PluginBase::instrumentHasContent (int layer, juce::String* what) const
{
    juce::StringArray parts;

    auto notes = 0;
    for (int tick = 0; tick < Pattern::maxTicks; ++tick)
        if (pattern.note (layer, tick).on.load())
            ++notes;
    if (notes > 0)
        parts.add ((pattern.isCustom (layer) ? "a piano roll with " : "") + juce::String (notes) + (notes == 1 ? " note" : " notes"));

    auto changed = false;
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && Presets::bassLayerOf (ranged->getParameterID()) == layer
            && ranged->getParameterID() != ParamID::bass (layer, BassField::on) && std::abs (ranged->getValue() - ranged->getDefaultValue()) > 1.0e-4f)
        {
            changed = true;
            break;
        }
    if (changed)
        parts.add (getPresetName (Presets::Section::bass, layer) == "Default" || getPresetName (Presets::Section::bass, layer).isEmpty()
                       ? juce::String ("changed or unsaved settings")
                       : "settings from the preset '" + getPresetName (Presets::Section::bass, layer) + "' (changed or not, they are not saved anywhere else)");

    if (bassSamples[(size_t) layer].hasSample())
        parts.add ("a sample");
    for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
        if (userTable (layer, osc) != nullptr)
        {
            parts.add ("its own wavetable");
            break;
        }
    if (layerNames[(size_t) layer].isNotEmpty())
        parts.add ("the name '" + layerNames[(size_t) layer] + "'");

    auto routes = 0;
    const auto& parameters = getParameters();
    for (const auto& route : modRoutes.routes)
        if (route.source.load() >= 0 && juce::isPositiveAndBelow (route.target.load(), parameters.size()))
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[route.target.load()]); withId != nullptr && Presets::bassLayerOf (withId->paramID) == layer)
                ++routes;
    if (routes > 0)
        parts.add (juce::String (routes) + (routes == 1 ? " modulation route" : " modulation routes"));

    if (what != nullptr)
        *what = parts.joinIntoString (", ", 0, std::max (0, parts.size() - 1)) + (parts.size() > 1 ? " and " : "") + (parts.isEmpty() ? juce::String() : parts[parts.size() - 1]);
    return ! parts.isEmpty();
}

void PluginBase::wipeInstrument (int layer)
{
    if (! juce::isPositiveAndBelow (layer, ParamID::numBassLayers))
        return;

    // Mönster och gate.
    pattern.clearNotes (layer);
    pattern.setGate (layer, true);

    // Alla instrumentets parametrar (ljud, nivå, rutnät, duck, FX) och dess rad i mönstret och mixern tillbaka till standard.
    const auto row = 1 + layer;
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = ranged->getParameterID();
            if (Presets::bassLayerOf (id) == layer || id == ParamID::rowBars (row) || id == ParamID::rowLoop (row)
                || id == Mixer::paramID (Mixer::firstBassSource + layer, "Gain") || id == Mixer::paramID (Mixer::firstBassSource + layer, "Pan")
                || id == Mixer::paramID (Mixer::firstBassSource + layer, "Duck") || id == Mixer::paramID (Mixer::firstBassSource + layer, "Max"))
                Presets::setParameter (*ranged, ranged->getDefaultValue());
        }

    // Sampling, egna tabeller, namn.
    bassSamples[(size_t) layer].clear();
    for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
    {
        const auto slot = (size_t) (layer * ParamID::oscsPerLayer + osc);
        userTables[slot].store (nullptr);
        if (userOwned[slot] != nullptr)
            retiredTables.push_back (std::move (userOwned[slot]));
        userFrames[slot].clear();
        userFrames[slot].shrink_to_fit();
        userNames[slot] = {};
    }
    while (retiredTables.size() > 4)
        retiredTables.erase (retiredTables.begin());
    layerNames[(size_t) layer] = {};
    bassPresetNames[(size_t) layer] = "Default";
    ++layerNamesVersion;
    selectedBassSource[(size_t) layer] = 0;

    // LFO-kopplingar till instrumentets rattar.
    const auto& parameters = getParameters();
    for (int i = 0; i < Modulation::maxRoutes; ++i)
    {
        const auto target = modRoutes.routes[(size_t) i].target.load();
        if (modRoutes.routes[(size_t) i].source.load() >= 0 && juce::isPositiveAndBelow (target, parameters.size()))
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[target]); withId != nullptr && Presets::bassLayerOf (withId->paramID) == layer)
                modRoutes.remove (i);
    }

    patternChanged();
}

void PluginBase::setInstrumentCount (int count)
{
    if (effectVersion)
        return;

    count = juce::jlimit (1, ParamID::numBassLayers, count);
    const auto before = activeInstruments();
    for (int layer = count; layer < before; ++layer)
        wipeInstrument (layer);

    if (auto* parameter = apvts.getParameter (ParamID::instrumentCount))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) count));
        parameter->endChangeGesture();
    }
    markStateChanged();
}

void PluginBase::ensureTablesInUse()
{
    for (int layer = 0; layer < ParamID::numBassLayers; ++layer)
        for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
            if (auto* value = apvts.getRawParameterValue (ParamID::bass (layer, ParamID::oscField (osc, BassField::aTable))))
                Wavetables::ensure ((int) std::lround (value->load()));
}

juce::ValueTree PluginBase::userTablesToValueTree() const
{
    juce::ValueTree tree ("UserTables");
    for (int slot = 0; slot < numUserSlots; ++slot)
    {
        const auto& frames = userFrames[(size_t) slot];
        if (frames.empty())
            continue;

        // 16 bitar räcker för en tabell och ger 44 kB (base64) per oscillator i stället för 85 kB.
        juce::MemoryBlock block (frames.size() * sizeof (juce::int16));
        auto* out = static_cast<juce::int16*> (block.getData());
        for (size_t i = 0; i < frames.size(); ++i)
            out[i] = (juce::int16) juce::roundToInt (juce::jlimit (-1.0f, 1.0f, frames[i]) * 32767.0f);

        juce::ValueTree child ("Table");
        child.setProperty ("layer", slot / ParamID::oscsPerLayer, nullptr);
        child.setProperty ("osc", slot % ParamID::oscsPerLayer, nullptr);
        child.setProperty ("name", userNames[(size_t) slot], nullptr);
        child.setProperty ("data", juce::Base64::toBase64 (block.getData(), block.getSize()), nullptr);
        tree.appendChild (child, nullptr);
    }
    return tree;
}

void PluginBase::userTablesFromValueTree (const juce::ValueTree& tree)
{
    for (const auto& child : tree)
    {
        const auto layer = (int) child["layer"], osc = (int) child["osc"];
        if (! child.hasType ("Table") || ! juce::isPositiveAndBelow (layer, ParamID::numBassLayers) || ! juce::isPositiveAndBelow (osc, 2))
            continue;

        juce::MemoryOutputStream decoded;
        if (! juce::Base64::convertFromBase64 (decoded, child["data"].toString()))
            continue;

        const auto count = decoded.getDataSize() / sizeof (juce::int16);
        const auto* in = static_cast<const juce::int16*> (decoded.getData());
        std::vector<float> frames (count);
        for (size_t i = 0; i < count; ++i)
            frames[i] = (float) in[i] / 32767.0f;

        const auto name = child["name"].toString();
        if (auto table = Wavetables::fromFrames (name, frames))
            setUserTable (layer * ParamID::oscsPerLayer + osc, std::move (table), std::move (frames), name);
    }
}

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

void PluginBase::setBridgeEnabled (bool enabled)
{
    if (enabled)
    {
        if (bridge == nullptr)
            bridge = std::make_unique<BridgeLink::Server> (*this);
        bridge->start();
    }
    else if (bridge != nullptr)
        bridge->stop();
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
    state.setProperty ("previewNote", previewNote.load(), nullptr);
    for (int layer = 0; layer < ParamID::numBassLayers; ++layer)
        state.setProperty ("layerName" + juce::String (layer + 1), layerNames[(size_t) layer], nullptr);
    state.appendChild (pattern.toValueTree(), nullptr);
    state.appendChild (kickSample.toValueTree (clickTreeType), nullptr);
    state.appendChild (bassSamplesToValueTree(), nullptr);
    state.appendChild (userTablesToValueTree(), nullptr);
    state.appendChild (sidechainCurve.toValueTree(), nullptr);
    state.appendChild (modRoutes.toValueTree (getParameters()), nullptr);
    for (int i = 0; i < Modulation::numLfos; ++i)
        state.appendChild (lfoCurves[(size_t) i].toValueTree ("LfoCurve" + juce::String (i + 1)), nullptr);
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
    previewNote = juce::jlimit (0, 127, (int) state.getProperty ("previewNote", 60));
    for (int layer = 0; layer < ParamID::numBassLayers; ++layer)
        layerNames[(size_t) layer] = state.getProperty ("layerName" + juce::String (layer + 1), juce::String()).toString();
    ++layerNamesVersion;

    if (auto patternTree = state.getChildWithName (Pattern::treeType); patternTree.isValid())
    {
        pattern.fromValueTree (patternTree);
        state.removeChild (patternTree, nullptr);
        ++patternVersion;
    }

    if (auto clickTree = state.getChildWithName (clickTreeType); clickTree.isValid())
    {
        kickSample.fromValueTree (clickTree);
        state.removeChild (clickTree, nullptr);
    }

    if (auto samplesTree = state.getChildWithName ("BassSamples"); samplesTree.isValid())
    {
        bassSamplesFromValueTree (samplesTree);
        state.removeChild (samplesTree, nullptr);
    }
    else
    {
        for (auto& slot : bassSamples)
            slot.clear();
    }

    if (auto userTree = state.getChildWithName ("UserTables"); userTree.isValid())
    {
        userTablesFromValueTree (userTree);
        state.removeChild (userTree, nullptr);
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

    for (int i = 0; i < Modulation::numLfos; ++i)
    {
        const juce::Identifier type ("LfoCurve" + juce::String (i + 1));
        if (auto tree = state.getChildWithName (type); tree.isValid())
        {
            lfoCurves[(size_t) i].fromValueTree (tree);
            state.removeChild (tree, nullptr);
        }
        else
            lfoCurves[(size_t) i].reset();
    }

    modRoutes.fromValueTree (state.getChildWithName (ModRoutes::treeType), getParameters());
    if (auto routesTree = state.getChildWithName (ModRoutes::treeType); routesTree.isValid())
        state.removeChild (routesTree, nullptr);

    apvts.replaceState (state);
    ensureTablesInUse();
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
    {
        modRoutes.clear();
        for (auto& curve : lfoCurves)
            curve.reset();
    }

    if (section == Presets::Section::full || section == Presets::Section::kick)
    {
        kickPitchEnv.reset();
        kickAmpEnv.reset();
    }

    // Kickens envelopes och LFO-kopplingar som presetet har med sig.
    if (const auto* extras = Presets::factoryExtras (section, effectVersion, index);
        extras != nullptr && (section == Presets::Section::full || section == Presets::Section::kick))
    {
        if (! extras->pitchEnv.empty())
            kickPitchEnv.setPoints (extras->pitchEnv);
        if (! extras->ampEnv.empty())
            kickAmpEnv.setPoints (extras->ampEnv);

        if (section == Presets::Section::full)
        {
            const auto& parameters = getParameters();
            for (const auto& route : extras->routes)
                for (int i = 0; i < parameters.size(); ++i)
                    if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]); withId != nullptr && withId->paramID == route.paramID)
                    {
                        modRoutes.add (route.source, i, route.amount);
                        break;
                    }
        }
    }

    if (section == Presets::Section::bass)
        clearBassSample (layer); // fabriksljuden har ingen sampling

    if (section == Presets::Section::full)
    {
        clearClickSample();
        for (int l = 0; l < ParamID::numBassLayers; ++l)
            clearBassSample (l);
        presetNames.fill ({}); // delarna kommer inte längre från sina egna presets
        bassPresetNames.fill ({});
    }

    presetNameSlot (section, layer) = Presets::factoryName (section, effectVersion, index);
    ensureTablesInUse();
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
        root.addChildElement (kickSample.toValueTree (clickTreeType).createXml().release());
        root.addChildElement (kickPitchEnv.toValueTree (kickPitchType).createXml().release());
        root.addChildElement (kickAmpEnv.toValueTree (kickAmpType).createXml().release());
    }

    if (section == Presets::Section::full)
        root.addChildElement (bassSamplesToValueTree().createXml().release());
    if (section == Presets::Section::bass && bassSamples[(size_t) layer].hasSample())
        root.addChildElement (bassSamples[(size_t) layer].toValueTree ("BassSample").createXml().release());

    if (section == Presets::Section::full || section == Presets::Section::sidechain)
        root.addChildElement (sidechainCurve.toValueTree().createXml().release());

    if (section == Presets::Section::full || section == Presets::Section::lfo)
    {
        root.addChildElement (modRoutes.toValueTree (getParameters()).createXml().release());
        for (int i = 0; i < Modulation::numLfos; ++i)
            root.addChildElement (lfoCurves[(size_t) i].toValueTree ("LfoCurve" + juce::String (i + 1)).createXml().release());
    }

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
            kickSample.fromValueTree (juce::ValueTree::fromXml (*clickXml));
        else
            kickSample.clear();

        for (auto [envelope, type] : { std::pair<NodeEnvelope*, juce::Identifier> { &kickPitchEnv, kickPitchType }, { &kickAmpEnv, kickAmpType } })
        {
            if (const auto* envelopeXml = xml->getChildByName (type))
                envelope->fromValueTree (juce::ValueTree::fromXml (*envelopeXml));
            else
                envelope->reset();
        }
    }

    if (section == Presets::Section::full)
    {
        if (const auto* samplesXml = xml->getChildByName ("BassSamples"))
            bassSamplesFromValueTree (juce::ValueTree::fromXml (*samplesXml));
        else
            for (auto& slot : bassSamples)
                slot.clear();
    }
    else if (section == Presets::Section::bass)
    {
        if (const auto* sampleXml = xml->getChildByName ("BassSample"))
            bassSamples[(size_t) layer].fromValueTree (juce::ValueTree::fromXml (*sampleXml));
        else
            bassSamples[(size_t) layer].clear();
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
        for (int i = 0; i < Modulation::numLfos; ++i)
        {
            if (const auto* curveXml = xml->getChildByName ("LfoCurve" + juce::String (i + 1)))
                lfoCurves[(size_t) i].fromValueTree (juce::ValueTree::fromXml (*curveXml));
            else
                lfoCurves[(size_t) i].reset();
        }
    }

    if (section == Presets::Section::full)
    {
        presetNames.fill ({});
        bassPresetNames.fill ({});
    }

    presetNameSlot (section, layer) = file.getFileNameWithoutExtension();
    ensureTablesInUse();
    patternChanged();
    return true;
}

juce::AudioBuffer<float> PluginBase::renderOffline (const OfflineRender::Settings& settings, ExportPart part)
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

    copy->ensureTablesInUse();
    return OfflineRender::render (*copy, settings);
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

    copy->ensureTablesInUse();

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
    if (! kickSample.loadFile (file))
        return false;
    patternChanged();
    return true;
}

void PluginBase::clearClickSample()
{
    kickSample.clear();
    patternChanged();
}

bool PluginBase::loadBassSample (int layer, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (layer, ParamID::numBassLayers) || ! bassSamples[(size_t) layer].loadFile (file))
        return false;
    patternChanged();
    return true;
}

void PluginBase::clearBassSample (int layer)
{
    if (juce::isPositiveAndBelow (layer, ParamID::numBassLayers))
        bassSamples[(size_t) layer].clear();
    patternChanged();
}

juce::ValueTree PluginBase::bassSamplesToValueTree() const
{
    juce::ValueTree tree ("BassSamples");
    for (int layer = 0; layer < ParamID::numBassLayers; ++layer)
        if (bassSamples[(size_t) layer].hasSample())
        {
            auto child = bassSamples[(size_t) layer].toValueTree ("BassSample");
            child.setProperty ("layer", layer, nullptr);
            tree.appendChild (child, nullptr);
        }
    return tree;
}

void PluginBase::bassSamplesFromValueTree (const juce::ValueTree& tree)
{
    std::array<bool, ParamID::numBassLayers> found {};
    for (const auto& child : tree)
        if (const auto layer = (int) child.getProperty ("layer", -1); juce::isPositiveAndBelow (layer, ParamID::numBassLayers))
        {
            bassSamples[(size_t) layer].fromValueTree (child);
            found[(size_t) layer] = true;
        }
    for (int layer = 0; layer < ParamID::numBassLayers; ++layer)
        if (! found[(size_t) layer])
            bassSamples[(size_t) layer].clear();
}
