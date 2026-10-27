#include <set>
#include "PluginBase.h"
#include <BinaryData.h>
#include "AiChat.h"
#include "MixAnalysis.h"
#include "OfflineRender.h"
#include "Mixer.h"
#include "WavetableEdit.h"
#include <map>

// Tal som sparade projekt och presets bygger på: ändras de läses gamla filer fel (eller måste migreras).
static_assert (Pattern::numLayers == 10 && Pattern::kickLayer == 10, "mönstrets lager sparas med index; kicken är lager 10 sedan version 6");
static_assert (numEffectChannels == 11 && slotsPerChannel == 5, "FX-kanalernas id:n är fxA-fxK med fem platser");
static_assert (Modulation::numLfos == 3 && Modulation::numMacros == 4 && Modulation::firstInstrumentSource == 7
                   && Modulation::instrumentSources == 4 && Modulation::maxLfos == 10 && Modulation::firstExtraLfoSource == 47
                   && Modulation::firstEnvSource == 54 && Modulation::envSources == 2 && Modulation::firstCodeSource == 74
                   && Modulation::numCodeSources == 8 && Modulation::numSources == 82,
               "modulationskällorna sparas som tal i modRoutes (CODE 1-8 är sist, källa 74-81)");
static_assert (ParamID::oscsPerLayer == 3, "projektens egna wavetables sparas med lager och oscillator (OSC 1-3)");

namespace
{
    const juce::Identifier clickTreeType { "ClickSample" };
    const juce::String presetFileExtension { ".mofp" };

    juce::String presetProperty (Presets::Section section) { return "preset" + Presets::sectionName (section); }
    const juce::Identifier kickPitchType { "KickPitchEnvelope" }, kickAmpType { "KickAmpEnvelope" };
}

PluginBase::PluginBase (const BusesProperties& buses, juce::AudioProcessorValueTreeState::ParameterLayout layout, bool isEffect)
    : AudioProcessor (buses),
      undoManager (30000, 200),
      apvts (*this, &undoManager, "Parameters", std::move (layout)),
      params (apvts),
      effectVersion (isEffect)
{
    // APVTS skriver värdet till trädet utan undo första gången (flushToTree, juce_AudioProcessorValueTreeState.cpp:135-138); fyll i det nu så att första gesten går att ångra
    for (auto child : apvts.state)
        if (auto* prm = apvts.getParameter (child.getProperty ("id").toString()))
            if (! child.hasProperty ("value"))
                child.setProperty ("value", prm->convertFrom0to1 (prm->getValue()), nullptr);

    params.setModulator (&modulator); // ljudtrådens parametrar läses med LFO-modulering
    modulator.setLfoCurves (&lfoCurves);
    presetNames.fill ("Default");
    instrumentPresetNames.fill ("Default");
    rowPresetNames.fill ({});
    fxPresetNames.fill ({});
    presetNames[(size_t) Presets::Section::full] = "Init";
}

PluginBase::~PluginBase() = default;

MixAnalyser& PluginBase::getMixAnalyser()
{
    if (mixAnalyser == nullptr)
        mixAnalyser = std::make_unique<MixAnalyser>();
    return *mixAnalyser;
}

double PluginBase::typicalNoteSeconds (int layer)
{
    layer = juce::jlimit (0, Pattern::numLayers - 1, layer);
    const auto now = juce::Time::getMillisecondCounter();
    if (noteSecondsStamp[(size_t) layer] != 0 && now - noteSecondsStamp[(size_t) layer] < 250)
        return noteSecondsCache[(size_t) layer];

    const auto ticks = pattern.lengthBars (layer) * Pattern::ticksPerBar;
    std::vector<int> noteLengths;
    pattern.forEachNote (layer, [&] (int tick, const Pattern::Note& note)
    {
        if (tick < ticks)
            noteLengths.push_back (note.length.load());
    });

    auto seconds = 0.0;
    if (! noteLengths.empty())
    {
        std::nth_element (noteLengths.begin(), noteLengths.begin() + (long) noteLengths.size() / 2, noteLengths.end());
        const auto scale = apvts.getRawParameterValue (ParamID::bass (layer, InstrumentField::gate))->load();
        const auto bpm = std::max (40.0, (double) transportStatus.bpm.load());
        seconds = std::max (1.0, (double) noteLengths[noteLengths.size() / 2] * (double) scale) * 60.0 / bpm / Pattern::ticksPerQuarter;
    }
    noteSecondsCache[(size_t) layer] = seconds;
    noteSecondsStamp[(size_t) layer] = std::max<juce::uint32> (1, now);
    return seconds;
}

int PluginBase::fxChannelUsageMask()
{
    auto mask = 0;
    auto sentTo = [this, &mask] (const char* channelID, const char* amountID)
    {
        const auto channel = (int) apvts.getRawParameterValue (channelID)->load() - 1;
        if (channel >= 0 && channel < numEffectChannels && apvts.getRawParameterValue (amountID)->load() > 0.0f)
            mask |= 1 << channel;
    };
    sentTo (ParamID::kickFxChannel, ParamID::kickFxAmount);
    for (int layer = 0; layer < activeInstruments(); ++layer)
        sentTo (ParamID::bass (layer, InstrumentField::fxChannel).toRawUTF8(), ParamID::bass (layer, InstrumentField::fxAmount).toRawUTF8());

    for (int channel = 0; channel < numEffectChannels; ++channel)
    {
        if ((mask & (1 << channel)) != 0)
            continue;
        for (int slot = 0; slot < slotsPerChannel; ++slot)
            if (Effects::slotType (apvts, channel, slot) != EffectType::off)
                mask |= 1 << channel;
        if ((mask & (1 << channel)) == 0 && ! Effects::readEq (apvts, channel).isNeutral())
            mask |= 1 << channel;
    }
    return mask;
}

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
    controlCounter = 0;
    chunkMidi.ensureSize (8192); // (ljudtråden ska inte behöva allokera för bitarnas MIDI)
    kickSample.prepare (sampleRate);
    for (auto& slot : instrumentSamples)
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
InstrumentParams PluginBase::readModulatedInstrument (int layer, int rootMidiNote) const
{
    auto bass = params.readInstrument (layer, rootMidiNote);
    bass.oscA.user = userTable (layer, 0);
    bass.oscB.user = userTable (layer, 1);
    bass.oscC.user = userTable (layer, 2);
    bass.sampleData = getActiveInstrumentSample (layer);
    return bass;
}

void PluginBase::setUserTable (int slot, std::unique_ptr<Wavetable> table, std::vector<float> frames, const juce::String& name)
{
    // Den gamla tabellen kan läsas av ljudtråden ett block till: den sparas undan en stund.
    const auto* newTable = table.get();
    auto old = std::move (userOwned[(size_t) slot]);
    userOwned[(size_t) slot] = std::move (table);
    userTables[(size_t) slot].store (newTable);
    retiredTables.retire (std::move (old));

    userFrames[(size_t) slot] = std::move (frames);
    userNames[(size_t) slot] = name;
}

bool PluginBase::loadUserTable (int layer, int osc, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (layer, ParamID::numInstrumentLayers) || ! juce::isPositiveAndBelow (osc, ParamID::oscsPerLayer))
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
    if (auto* parameter = apvts.getParameter (ParamID::bass (layer, ParamID::oscField (osc, InstrumentField::aTable))))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) Wavetables::userIndex()));
        parameter->endChangeGesture();
    }
    markStateChanged();
    return true;
}

bool PluginBase::wavetableSource (int layer, int osc, std::vector<float>& frames, int& steps, juce::String& name) const
{
    if (! juce::isPositiveAndBelow (layer, ParamID::numInstrumentLayers) || ! juce::isPositiveAndBelow (osc, ParamID::oscsPerLayer))
        return false;
    auto* parameter = apvts.getParameter (ParamID::bass (layer, ParamID::oscField (osc, InstrumentField::aTable)));
    if (parameter == nullptr)
        return false;

    // Oscillatorns egen User-tabell om den är vald, annars den inbyggda som är vald (installWavetable gör den till en redigerad kopia i User).
    const auto slot = layer * ParamID::oscsPerLayer + osc;
    const auto choice = (int) std::lround (parameter->convertFrom0to1 (parameter->getValue()));
    if (choice == Wavetables::userIndex())
    {
        if (userTable (layer, osc) == nullptr || userFrames[(size_t) slot].size() != (size_t) Wavetable::numFrames * Wavetable::frameSize)
            return false;
        frames = userFrames[(size_t) slot];
        steps = userTable (layer, osc)->getSteps();
        name = userNames[(size_t) slot];
        return true;
    }

    Wavetables::ensure (choice);
    const auto& source = Wavetables::get (choice);
    frames = WavetableEdit::framesOf (source);
    steps = source.getSteps();
    name = Wavetables::names()[choice] + " (edited)";
    return true;
}

bool PluginBase::installWavetable (int layer, int osc, std::vector<float> frames, int steps, const juce::String& name)
{
    auto* parameter = juce::isPositiveAndBelow (layer, ParamID::numInstrumentLayers) && juce::isPositiveAndBelow (osc, ParamID::oscsPerLayer)
                          ? apvts.getParameter (ParamID::bass (layer, ParamID::oscField (osc, InstrumentField::aTable)))
                          : nullptr;
    if (parameter == nullptr)
        return false;
    auto table = Wavetables::fromFrames (name, frames, steps);
    if (table == nullptr)
        return false;

    setUserTable (layer * ParamID::oscsPerLayer + osc, std::move (table), std::move (frames), name);
    if ((int) std::lround (parameter->convertFrom0to1 (parameter->getValue())) != Wavetables::userIndex())
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) Wavetables::userIndex()));
        parameter->endChangeGesture();
    }
    markStateChanged();
    return true;
}

bool PluginBase::editWavetable (int layer, int osc, int frame, const std::vector<WavetableEdit::Edit>& edits, bool normalise)
{
    std::vector<float> frames;
    auto steps = Wavetable::numFrames;
    juce::String name;
    if (edits.empty() || ! wavetableSource (layer, osc, frames, steps, name))
        return false;

    WavetableEdit::apply (frames, frame, edits, normalise);
    return installWavetable (layer, osc, std::move (frames), steps, name);
}

bool PluginBase::instrumentHasContent (int layer, juce::String* what) const
{
    juce::StringArray parts;

    auto notes = 0;
    pattern.forEachNote (layer, [&notes] (int, const Pattern::Note&) { ++notes; });
    if (notes > 0)
        parts.add ((pattern.isCustom (layer) ? "a piano roll with " : "") + juce::String (notes) + (notes == 1 ? " note" : " notes"));

    auto changed = false;
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && Presets::instrumentLayerOf (ranged->getParameterID()) == layer
            && ranged->getParameterID() != ParamID::bass (layer, InstrumentField::on) && std::abs (ranged->getValue() - ranged->getDefaultValue()) > 1.0e-4f)
        {
            changed = true;
            break;
        }
    if (changed)
        parts.add (getPresetName (Presets::Section::bass, layer) == "Default" || getPresetName (Presets::Section::bass, layer).isEmpty()
                       ? juce::String ("changed or unsaved settings")
                       : "settings from the preset '" + getPresetName (Presets::Section::bass, layer) + "' (changed or not, they are not saved anywhere else)");

    if (instrumentSamples[(size_t) layer].hasSample())
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
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[route.target.load()]); withId != nullptr && Presets::instrumentLayerOf (withId->paramID) == layer)
                ++routes;
    if (routes > 0)
        parts.add (juce::String (routes) + (routes == 1 ? " modulation route" : " modulation routes"));

    if (what != nullptr)
        *what = parts.joinIntoString (", ", 0, std::max (0, parts.size() - 1)) + (parts.size() > 1 ? " and " : "") + (parts.isEmpty() ? juce::String() : parts[parts.size() - 1]);
    return ! parts.isEmpty();
}

void PluginBase::wipeInstrument (int layer)
{
    if (! juce::isPositiveAndBelow (layer, ParamID::numInstrumentLayers))
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
            if (Presets::instrumentLayerOf (id) == layer || id == ParamID::rowBars (row) || id == ParamID::rowLoop (row)
                || id == Mixer::paramID (Mixer::firstBassSource + layer, "Gain") || id == Mixer::paramID (Mixer::firstBassSource + layer, "Pan")
                || id == Mixer::paramID (Mixer::firstBassSource + layer, "Duck") || id == Mixer::paramID (Mixer::firstBassSource + layer, "Max"))
                Presets::setParameter (*ranged, ranged->getDefaultValue());
        }

    // Sampling, egna tabeller, namn.
    instrumentSamples[(size_t) layer].clear();
    for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
        clearUserTable (layer * ParamID::oscsPerLayer + osc);
    layerNames[(size_t) layer] = {};
    layerColours[(size_t) layer] = 0;
    instrumentPresetNames[(size_t) layer] = "Default";
    ++layerNamesVersion;
    selectedInstrumentSource[(size_t) layer] = 0;

    // LFO-kopplingar till instrumentets rattar.
    const auto& parameters = getParameters();
    for (int i = 0; i < Modulation::maxRoutes; ++i)
    {
        const auto target = modRoutes.routes[(size_t) i].target.load();
        if (modRoutes.routes[(size_t) i].source.load() >= 0 && juce::isPositiveAndBelow (target, parameters.size()))
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[target]); withId != nullptr && Presets::instrumentLayerOf (withId->paramID) == layer)
                modRoutes.remove (i);
    }

    patternChanged();
}

void PluginBase::setInstrumentCount (int count)
{
    if (effectVersion)
        return;

    count = juce::jlimit (1, ParamID::numInstrumentLayers, count);
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
    for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
        for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
            if (auto* value = apvts.getRawParameterValue (ParamID::bass (layer, ParamID::oscField (osc, InstrumentField::aTable))))
                Wavetables::ensure ((int) std::lround (value->load()));
}

juce::ValueTree PluginBase::userTablesToValueTree (int onlyLayer) const
{
    juce::ValueTree tree ("UserTables");
    for (int slot = 0; slot < numUserSlots; ++slot)
    {
        const auto& frames = userFrames[(size_t) slot];
        if (frames.empty() || (onlyLayer >= 0 && slot / ParamID::oscsPerLayer != onlyLayer))
            continue;

        // 16 bitar räcker för en tabell och ger 44 kB (base64) per oscillator i stället för 85 kB.
        juce::MemoryBlock block (frames.size() * sizeof (juce::int16));
        auto* out = static_cast<juce::int16*> (block.getData());
        for (size_t i = 0; i < frames.size(); ++i)
            out[i] = (juce::int16) juce::roundToInt (juce::jlimit (-1.0f, 1.0f, frames[i]) * 32767.0f);

        juce::ValueTree child ("Table");
        child.setProperty ("layer", onlyLayer >= 0 ? 0 : slot / ParamID::oscsPerLayer, nullptr); // ett instrument-preset sparas som lager 1
        child.setProperty ("osc", slot % ParamID::oscsPerLayer, nullptr);
        child.setProperty ("name", userNames[(size_t) slot], nullptr);
        if (const auto* table = userTables[(size_t) slot].load())
            child.setProperty ("steps", table->getSteps(), nullptr); // så många vågformer Morph av hoppar mellan; saknas = 16 (äldre projekt)
        child.setProperty ("data", juce::Base64::toBase64 (block.getData(), block.getSize()), nullptr);
        tree.appendChild (child, nullptr);
    }
    return tree;
}

void PluginBase::clearUserTable (int slot)
{
    userTables[(size_t) slot].store (nullptr);
    retiredTables.retire (std::move (userOwned[(size_t) slot]));
    userFrames[(size_t) slot].clear();
    userFrames[(size_t) slot].shrink_to_fit();
    userNames[(size_t) slot] = {};
}

void PluginBase::userTablesFromValueTree (const juce::ValueTree& tree, int intoLayer)
{
    // Trädet är hela sanningen för de lager det gäller: en plats som inte finns i det töms (annars ligger en gammal tabell kvar när ett
    // projekt, ett preset eller en ångring laddas i en instans som redan har tabeller). `intoLayer` >= 0: ett instrument-preset (lager 0 i
    // trädet) laddas i det lagret, och bara dess platser rörs.
    std::array<bool, numUserSlots> found {};
    for (const auto& child : tree)
    {
        auto layer = (int) child["layer"];
        const auto osc = (int) child["osc"];
        if (intoLayer >= 0)
        {
            if (layer != 0)
                continue;
            layer = intoLayer;
        }
        if (! child.hasType ("Table") || ! juce::isPositiveAndBelow (layer, ParamID::numInstrumentLayers) || ! juce::isPositiveAndBelow (osc, ParamID::oscsPerLayer))
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
        if (auto table = Wavetables::fromFrames (name, frames, (int) child.getProperty ("steps", Wavetable::numFrames)))
        {
            setUserTable (layer * ParamID::oscsPerLayer + osc, std::move (table), std::move (frames), name);
            found[(size_t) (layer * ParamID::oscsPerLayer + osc)] = true;
        }
    }

    for (int slot = 0; slot < numUserSlots; ++slot)
        if (! found[(size_t) slot] && (intoLayer < 0 || slot / ParamID::oscsPerLayer == intoLayer) && userOwned[(size_t) slot] != nullptr)
            clearUserTable (slot);
}

juce::String& PluginBase::presetNameSlot (Presets::Section section, int layer)
{
    if (section == Presets::Section::pattern)
        return rowPresetNames[(size_t) juce::jlimit (0, Pattern::numNoteLayers - 1, layer)];
    if (section == Presets::Section::effects)
        return fxPresetNames[(size_t) juce::jlimit (0, numEffectChannels - 1, layer)];
    return section == Presets::Section::bass ? instrumentPresetNames[(size_t) juce::jlimit (0, ParamID::numInstrumentLayers - 1, layer)]
                                             : presetNames[(size_t) section];
}

juce::String PluginBase::getPresetName (Presets::Section section, int layer) const
{
    if (section == Presets::Section::pattern)
        return rowPresetNames[(size_t) juce::jlimit (0, Pattern::numNoteLayers - 1, layer)];
    if (section == Presets::Section::effects)
        return fxPresetNames[(size_t) juce::jlimit (0, numEffectChannels - 1, layer)];
    return section == Presets::Section::bass ? instrumentPresetNames[(size_t) juce::jlimit (0, ParamID::numInstrumentLayers - 1, layer)]
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

juce::Optional<juce::AudioPlayHead::PositionInfo> PluginBase::readHostPosition() const
{
    if (auto* hostPlayHead = getPlayHead())
        return hostPlayHead->getPosition();

    return juce::nullopt;
}

juce::Optional<juce::AudioPlayHead::PositionInfo> PluginBase::getHostPosition() const
{
    // Positionen vid DAW-blockets början, framflyttad till biten som spelas nu (samma tempo hela blocket, som DAW:en anger).
    auto position = blockPosition;
    if (! position.hasValue() || chunkOffset == 0)
        return position;

    const auto rate = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    if (const auto samples = position->getTimeInSamples(); samples.hasValue())
        position->setTimeInSamples (*samples + chunkOffset);
    if (const auto seconds = position->getTimeInSeconds(); seconds.hasValue())
        position->setTimeInSeconds (*seconds + chunkOffset / rate);
    const auto bpm = position->getBpm();
    if (const auto ppq = position->getPpqPosition(); ppq.hasValue() && bpm.hasValue() && *bpm > 0.0)
    {
        const auto movedPpq = *ppq + chunkOffset * *bpm / 60.0 / rate;
        position->setPpqPosition (movedPpq);
        // Passerar biten en taktgräns flyttas taktens början med.
        const auto sig = position->getTimeSignature();
        if (const auto barStart = position->getPpqPositionOfLastBarStart(); barStart.hasValue() && sig.hasValue() && sig->numerator > 0 && sig->denominator > 0)
        {
            const auto barLength = sig->numerator * 4.0 / sig->denominator;
            auto start = *barStart;
            while (movedPpq >= start + barLength - 1.0e-9)
                start += barLength;
            position->setPpqPositionOfLastBarStart (start);
        }
    }
    return position;
}

bool PluginBase::allFinite (const juce::AudioBuffer<float>& buffer, int numSamples)
{
    // x - x är 0 för alla ändliga värden och NaN för NaN och ±inf, så summan är 0 precis när allt är ändligt.
    auto sum = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            sum += data[i] - data[i];
    }
    return sum == 0.0f;
}

void PluginBase::replaceNonFinite (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
{
    for (int ch = 0; ch < std::min (numChannels, buffer.getNumChannels()); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            if (! std::isfinite (data[i]))
                data[i] = 0.0f;
    }
}

void PluginBase::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int i = 0; i < Presets::numSections; ++i)
        state.setProperty (presetProperty ((Presets::Section) i), presetNames[(size_t) i], nullptr);
    for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
        state.setProperty ("presetBass" + juce::String (layer + 1), instrumentPresetNames[(size_t) layer], nullptr);
    for (int row = 0; row < Pattern::numNoteLayers; ++row)
        state.setProperty ("presetRow" + juce::String (row + 1), rowPresetNames[(size_t) row], nullptr);
    for (int channel = 0; channel < numEffectChannels; ++channel)
        state.setProperty ("presetFx" + juce::String (channel + 1), fxPresetNames[(size_t) channel], nullptr);
    state.setProperty ("previewNote", previewNote.load(), nullptr);
    state.setProperty ("fxKnobsV", Effects::knobsVersion, nullptr);
    for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
        state.setProperty ("layerName" + juce::String (layer + 1), layerNames[(size_t) layer], nullptr);
    for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
        if (layerColours[(size_t) layer] != 0) // bara de som ändrats: gamla projekt och standardfärger får ingen ny egenskap
            state.setProperty ("layerColour" + juce::String (layer + 1), (juce::int64) layerColours[(size_t) layer], nullptr);
    state.appendChild (pattern.toValueTree(), nullptr);
    state.appendChild (kickSample.toValueTree (clickTreeType), nullptr);
    state.appendChild (instrumentSamplesToValueTree(), nullptr);
    state.appendChild (userTablesToValueTree(), nullptr);
    state.appendChild (sidechainCurve.toValueTree(), nullptr);
    state.appendChild (modRoutes.toValueTree (getParameters()), nullptr);
    state.setProperty ("activeLfos", (int) modulator.activeMask.load(), nullptr);
    for (int i = 0; i < Modulation::maxLfos; ++i)
        if (i < Modulation::numLfos || isLfoActive (i)) // LFO 1-3 alltid (som förut)
            state.appendChild (lfoCurves[(size_t) i].toValueTree ("LfoCurve" + juce::String (i + 1)), nullptr);
    state.appendChild (kickPitchEnv.toValueTree (kickPitchType), nullptr);
    state.appendChild (kickAmpEnv.toValueTree (kickAmpType), nullptr);

    if (const auto liveCode = liveCodeToValueTree(); liveCode.isValid())
        state.appendChild (liveCode, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

juce::ValueTree PluginBase::liveCodeToValueTree() const
{
    const auto text = getLiveCodeText();
    const auto locks = liveCodeLocks.load();
    if (text.isEmpty() && (locks & 0x7ff) == 0)
        return {};
    juce::ValueTree tree ("LiveCode");
    tree.setProperty ("code", text, nullptr);
    tree.setProperty ("locks", (int) (locks & 0x7ff), nullptr); // filformatet är oförändrat: bit 0-10 (underraders lås sparas inte)
    return tree;
}

void PluginBase::liveCodeFromValueTree (const juce::ValueTree& tree)
{
    setLiveCodeText (tree.getProperty ("code").toString());
    liveCodeLocks = (juce::uint64) ((int) tree.getProperty ("locks", 0) & 0x7ff); // bit 0-9 instrument, 10 kicken
    liveCodeOwned = 0;
    liveCodeCodeLocks = 0; // lås från koden sparas aldrig
    liveCodeCodeNoteLocks = 0;
}

void PluginBase::clearCodeMods (bool dropRoutes)
{
    for (auto& slot : modulator.codeSlots)
        slot.kind = 0;
    if (dropRoutes)
        for (int i = 0; i < (int) modRoutes.routes.size(); ++i)
            if (Modulation::isCodeSource (modRoutes.routes[(size_t) i].source.load()))
                modRoutes.remove (i);
    codeMods.clear();
    if (! dropRoutes)
        startCodeSweep();
}

void PluginBase::startCodeSweep()
{
    // Utan meddelandehanterare (verktygstester) finns ingen timer; sweepCodeRoutes anropas då av applyCodeMods och direkt.
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr && ! codeSweeper.isTimerRunning())
        codeSweeper.startTimer (100);
}

void PluginBase::CodeSweeper::timerCallback()
{
    owner.sweepCodeRoutes();
    for (const auto& route : owner.modRoutes.routes)
    {
        const auto source = route.source.load();
        if (Modulation::isCodeSource (source) && owner.modulator.codeSlots[(size_t) (source - Modulation::firstCodeSource)].kind.load() == 0)
            return; // en stängd plats tonar fortfarande ut: titta igen om 100 ms
    }
    stopTimer();
}

void PluginBase::sweepCodeRoutes()
{
    for (int i = 0; i < (int) modRoutes.routes.size(); ++i)
    {
        const auto source = modRoutes.routes[(size_t) i].source.load();
        if (! Modulation::isCodeSource (source))
            continue;
        const auto& slot = modulator.codeSlots[(size_t) (source - Modulation::firstCodeSource)];
        if (slot.kind.load() == 0 && slot.idle.load())
            modRoutes.remove (i);
    }
}

void PluginBase::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    clearHistory ("project loaded"); // (replaceState tömmer också, men utan anteckning)
    clearCodeMods(); // kopplingarna ersätts nedan; poster och platser får inte peka på dem

    auto state = juce::ValueTree::fromXml (*xml);
    for (int i = 0; i < Presets::numSections; ++i)
        presetNames[(size_t) i] = state.getProperty (presetProperty ((Presets::Section) i), presetNames[(size_t) i]).toString();
    for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
        instrumentPresetNames[(size_t) layer] = state.getProperty ("presetBass" + juce::String (layer + 1), instrumentPresetNames[(size_t) layer]).toString();
    for (int row = 0; row < Pattern::numNoteLayers; ++row)
        rowPresetNames[(size_t) row] = state.getProperty ("presetRow" + juce::String (row + 1), rowPresetNames[(size_t) row]).toString();
    for (int channel = 0; channel < numEffectChannels; ++channel)
        fxPresetNames[(size_t) channel] = state.getProperty ("presetFx" + juce::String (channel + 1), fxPresetNames[(size_t) channel]).toString();
    previewNote = juce::jlimit (0, 127, (int) state.getProperty ("previewNote", 60));
    for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
        layerNames[(size_t) layer] = state.getProperty ("layerName" + juce::String (layer + 1), juce::String()).toString();
    for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
        layerColours[(size_t) layer] = (juce::uint32) (juce::int64) state.getProperty ("layerColour" + juce::String (layer + 1), 0);
    ++layerNamesVersion;
    if (auto liveCode = state.getChildWithName ("LiveCode"); liveCode.isValid()) // äldre projekt har inget: koden och låsen lämnas som de är
    {
        liveCodeFromValueTree (liveCode);
        state.removeChild (liveCode, nullptr); // hör inte hemma i apvts-tillståndet
    }
    ++liveCodeLoads; // sidan LIVE CODE stoppar LIVE och tar in den nya koden

    auto legacyLengths = false; // projekt före mönsterversion 7: längderna låg i parametrarna (patternBars, rowBars)
    if (auto patternTree = state.getChildWithName (Pattern::treeType); patternTree.isValid())
    {
        legacyLengths = (int) patternTree.getProperty ("version", 1) < 7;
        pattern.fromValueTree (patternTree);
        state.removeChild (patternTree, nullptr);
        ++patternVersion;
    }

    if (auto clickTree = state.getChildWithName (clickTreeType); clickTree.isValid())
    {
        kickSample.fromValueTree (clickTree);
        state.removeChild (clickTree, nullptr);
    }
    else
        kickSample.clear(); // (sparas alltid, men ett tillstånd utan den ska inte ärva förra ljudets sampling)

    if (auto samplesTree = state.getChildWithName ("BassSamples"); samplesTree.isValid())
    {
        instrumentSamplesFromValueTree (samplesTree);
        state.removeChild (samplesTree, nullptr);
    }
    else
    {
        for (auto& slot : instrumentSamples)
            slot.clear();
    }

    if (auto userTree = state.getChildWithName ("UserTables"); userTree.isValid())
    {
        userTablesFromValueTree (userTree);
        state.removeChild (userTree, nullptr);
    }
    else
        userTablesFromValueTree (juce::ValueTree ("UserTables")); // inga tabeller i tillståndet: inga kvar från förut

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

    for (int i = 0; i < Modulation::maxLfos; ++i)
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

    const auto lfoMask = (std::uint32_t) (int) state.getProperty ("activeLfos", 7); // gamla projekt: LFO 1-3
    state.removeProperty ("activeLfos", nullptr);
    Effects::migrateLegacyKnobs (state); // gamla projekt: nya effektrattar får sitt neutrala standardvärde
    apvts.replaceState (state);
    applyLoadedLfos (lfoMask); // (efter replaceState: den återställer inte parametrar som saknas i projektet)
    if (legacyLengths)
    {
        Params::applyLegacyLengthParams (apvts, pattern); // (efter parametrarna: de bär de gamla längderna)
        ++patternVersion;
    }
    ensureTablesInUse();
}

void PluginBase::patternChanged()
{
    pattern.refreshLengths(); // radernas längd följer innehållet (AUTO); tomma block frigörs
    ++patternVersion;
    markStateChanged();
}

namespace
{
// Ett steg i ångra-historiken: raden före och efter. perform/undo skriver tillbaka hela raden och meddelar mönstret
// (patternChanged skriver inget nytt steg: inget i gränssnittet skriver tillbaka därifrån).
class PatternRowAction : public juce::UndoableAction
{
public:
    PatternRowAction (PluginBase& p, int l, juce::ValueTree b, juce::ValueTree a) : owner (p), layer (l), before (std::move (b)), after (std::move (a))
    {
        units = 1 + (int) ((estimate (before) + estimate (after)) / 1024); // KB, så att historiens tak räknar minne och inte bara antal
    }

    bool perform() override
    {
        if (skipFirst) // ändringen är redan gjord när steget läggs till
        {
            skipFirst = false;
            return true;
        }
        apply (after);
        return true;
    }

    bool undo() override
    {
        apply (before);
        return true;
    }

    int getSizeInUnits() override { return units; }

private:
    static size_t estimate (const juce::ValueTree& t) { return (size_t) t.createXml()->createDocument ({}, true, false).length(); }

    void apply (const juce::ValueTree& tree)
    {
        owner.pattern.rowFromValueTree (layer, tree);
        owner.patternChanged();
    }

    PluginBase& owner;
    int layer;
    juce::ValueTree before, after;
    bool skipFirst = true;
    int units = 1;
};
}

namespace {
// Allmän ångra-post: före/efter som träd och en funktion som lägger tillbaka ett träd. Första perform hoppas över (ändringen är redan gjord).
class TreeAction : public juce::UndoableAction
{
public:
    TreeAction (juce::ValueTree b, juce::ValueTree a, std::function<void (const juce::ValueTree&)> f)
        : before (std::move (b)), after (std::move (a)), apply (std::move (f)) {}

    bool perform() override
    {
        if (skipFirst)
            skipFirst = false;
        else
            apply (after);
        return true;
    }
    bool undo() override { apply (before); return true; }
    int getSizeInUnits() override { return 2; }

private:
    juce::ValueTree before, after;
    std::function<void (const juce::ValueTree&)> apply;
    bool skipFirst = true;
};
}

juce::ValueTree PluginBase::snapshotMods() const { return modRoutes.toValueTree (getParameters()); }

void PluginBase::recordModEdit (const juce::ValueTree& before, const juce::String& name)
{
    auto after = snapshotMods();
    if (! before.isValid() || before.isEquivalentTo (after))
        return;
    undoManager.beginNewTransaction (stampedName (name));
    undoManager.perform (new TreeAction (before, std::move (after), [this] (const juce::ValueTree& t)
    {
        modRoutes.fromValueTree (t, getParameters());
        markStateChanged();
    }));
}

juce::ValueTree PluginBase::snapshotAux() const
{
    juce::ValueTree aux ("Aux");
    aux.appendChild (pattern.toValueTree(), nullptr);
    aux.appendChild (snapshotMods(), nullptr);
    return aux;
}

void PluginBase::applyAux (const juce::ValueTree& aux)
{
    pattern.fromValueTree (aux.getChild (0), true);
    modRoutes.fromValueTree (aux.getChild (1), getParameters());
    patternChanged();
    markStateChanged();
}

namespace {
// Runt en förinställning: huvudpreset (Full) tömmer historiken, en del blir ett enda steg.
class PresetUndoScope
{
public:
    PresetUndoScope (PluginBase& o, Presets::Section s, const juce::String& n) : owner (o), full (s == Presets::Section::full), name (n)
    {
        if (full)
            return;
        owner.apvts.copyState(); // väntande rattändringar hör inte till presetet
        owner.undoManager.beginNewTransaction (PluginBase::stampedName ("Load preset: " + name));
        before = owner.snapshotAux();
    }
    ~PresetUndoScope()
    {
        if (full)
        {
            owner.clearHistory (name);
            return;
        }
        owner.apvts.copyState();
        auto after = owner.snapshotAux();
        if (! before.isEquivalentTo (after))
            owner.undoManager.perform (new TreeAction (before, std::move (after), [&o = owner] (const juce::ValueTree& t) { o.applyAux (t); }));
    }

private:
    PluginBase& owner;
    bool full;
    juce::String name;
    juce::ValueTree before;
};
}

juce::ValueTree PluginBase::snapshotRow (int layer) const
{
    return pattern.rowToValueTree (layer);
}

juce::String PluginBase::stampedName (const juce::String& what)
{
    return juce::Time::getCurrentTime().formatted ("%H:%M:%S") + " " + what;
}

void PluginBase::nameParameterGesture (const juce::String& paramName)
{
    apvts.copyState(); // flushParameterValuesToValueTree
    undoManager.setCurrentTransactionName (stampedName ("Knob: " + paramName));
}

void PluginBase::clearHistory (const juce::String& why)
{
    apvts.copyState(); // väntande rattändringar skrivs till trädet nu, annars hamnar de som ett namnlöst steg efter rensningen
    undoManager.clearUndoHistory();
    historyClearedNote = why.isEmpty() ? juce::String() : "History cleared: " + why;
    undoManager.sendChangeMessage();
}

void PluginBase::recordRowEdit (int layer, const juce::ValueTree& before, const juce::String& name)
{
    auto after = pattern.rowToValueTree (layer);
    if (! before.isValid() || before.isEquivalentTo (after))
        return;
    undoManager.beginNewTransaction (stampedName (name + ": " + getLayerName (layer)));
    undoManager.perform (new PatternRowAction (*this, layer, before, std::move (after)));
}

void PluginBase::markStateChanged()
{
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

int PluginBase::availableLfos() const
{
    return apvts.getParameter (Modulation::paramID (Modulation::numLfos, "On")) != nullptr ? Modulation::maxLfos : Modulation::numLfos;
}

bool PluginBase::isLfoActive (int index) const
{
    return juce::isPositiveAndBelow (index, Modulation::maxLfos) && (modulator.activeMask.load() & (1u << index)) != 0;
}

int PluginBase::lfoRouteCount (int index) const
{
    const auto source = Modulation::lfoSource (index);
    auto count = 0;
    for (const auto& route : modRoutes.routes)
        count += route.source.load() == source ? 1 : 0;
    return count;
}

int PluginBase::setLfoActive (int index, bool active)
{
    if (! juce::isPositiveAndBelow (index, availableLfos()) || isLfoActive (index) == active)
        return 0;
    auto removed = 0;
    if (active)
        modulator.activeMask.fetch_or (1u << index);
    else
        removed = clearLfo (index);
    ++lfoActiveVersion;
    markStateChanged();
    return removed;
}

int PluginBase::clearLfo (int index)
{
    // Ordningen: kopplingarna först (slutar verka direkt), sist biten i masken. Ljudtråden nollställer sina egna fält (Modulator::update).
    const auto source = Modulation::lfoSource (index);
    auto removed = 0;
    for (int i = 0; i < (int) modRoutes.routes.size(); ++i)
        if (modRoutes.routes[(size_t) i].source.load() == source)
        {
            modRoutes.remove (i);
            ++removed;
        }
    for (const auto* suffix : { "On", "Shape", "Rate", "Mode", "Hz", "Snap", "SnapRange" }) // (exakta id:n: "lfo1" är början på "lfo10")
        if (auto* parameter = apvts.getParameter (Modulation::paramID (index, suffix)))
            Presets::setParameter (*parameter, parameter->getDefaultValue());
    lfoCurves[(size_t) index].reset();
    modulator.activeMask.fetch_and (~(1u << index));
    return removed;
}

void PluginBase::applyLoadedLfos (std::uint32_t mask)
{
    for (int i = 0; i < Modulation::maxLfos; ++i)
        if (lfoRouteCount (i) > 0)
            mask |= 1u << i; // en koppling från en borttagen LFO ska inte tyst sluta verka
    mask &= (1u << availableLfos()) - 1u;
    for (int i = 0; i < Modulation::maxLfos; ++i)
        if ((mask & (1u << i)) == 0)
            clearLfo (i);
    modulator.activeMask = mask;
    ++lfoActiveVersion;
}

void PluginBase::loadFactoryPreset (Presets::Section section, int index, int layer)
{
    PresetUndoScope undoScope (*this, section, Presets::factoryName (section, effectVersion, index));
    // Instrumentets Init är ett vanligt Full-preset (Resources/InitPreset.mofp); FX-versionen och övriga delar använder fabrikslistorna i koden.
    if (section == Presets::Section::full && index == 0 && ! preferCodeInit && loadEmbeddedInit())
        return;

    Presets::applyFactory (section, effectVersion, index, apvts, pattern, sidechainCurve, layer);

    if (section == Presets::Section::full || section == Presets::Section::lfo)
    {
        clearCodeMods();
        modRoutes.clear();
        for (auto& curve : lfoCurves)
            curve.reset();
    }

    if (section == Presets::Section::full || section == Presets::Section::kick)
    {
        kickPitchEnv.setMode (NodeEnvelope::Mode::tension); // reset() rör inte läget
        kickAmpEnv.setMode (NodeEnvelope::Mode::tension);
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

    if (section == Presets::Section::full || section == Presets::Section::lfo)
        applyLoadedLfos (7u); // LFO 1-3 och de LFO:er presetets kopplingar använder

    if (section == Presets::Section::bass)
    {
        clearInstrumentSample (layer); // fabriksljuden har ingen sampling och inga egna tabeller
        for (int osc = 0; osc < ParamID::oscsPerLayer; ++osc)
            clearUserTable (layer * ParamID::oscsPerLayer + osc);
    }

    if (section == Presets::Section::full)
    {
        clearClickSample();
        for (int l = 0; l < ParamID::numInstrumentLayers; ++l)
            clearInstrumentSample (l);
        for (int slot = 0; slot < numUserSlots; ++slot)
            clearUserTable (slot);
        layerNames.fill ({}); // som den inbakade Init-filen: standardnamn och -färger
        layerColours.fill (0);
        ++layerNamesVersion;
        presetNames.fill ({}); // delarna kommer inte längre från sina egna presets
        instrumentPresetNames.fill ({});
        rowPresetNames.fill ({});
        fxPresetNames.fill ({});
    }

    presetNameSlot (section, layer) = Presets::factoryName (section, effectVersion, index);
    ensureTablesInUse();
    patternChanged();
}

juce::Array<juce::File> PluginBase::getUserPresets (Presets::Section section, int layer) const
{
    auto files = Presets::folder (section, effectVersion).findChildFiles (juce::File::findFiles, false, "*" + presetFileExtension);
    files.sort();
    if (section == Presets::Section::effects)
    {
        // FX Preset: bara kanal-presets (äldre filer med alla kanaler visas inte).
        juce::Array<juce::File> channels;
        for (const auto& file : files)
            if (auto xml = juce::XmlDocument::parse (file); xml != nullptr && xml->getStringAttribute ("scope") == "channel")
                channels.add (file);
        return channels;
    }
    if (section != Presets::Section::pattern)
        return files;

    // Mönster: bara rad-presets (äldre filer med hela mönstret visas inte), och av rätt sort: kickens rad eller ett instruments.
    juce::Array<juce::File> rows;
    for (const auto& file : files)
        if (auto xml = juce::XmlDocument::parse (file); xml != nullptr && xml->getStringAttribute ("scope") == "row"
                                                       && (layer < 0 || xml->getStringAttribute ("rowKind") == (layer == Pattern::kickLayer ? "kick" : "instrument")))
            rows.add (file);
    return rows;
}

std::unique_ptr<juce::XmlElement> PluginBase::buildPresetXml (Presets::Section section, int layer, const PresetLink* link)
{
    auto rootPtr = std::make_unique<juce::XmlElement> ("KickbasePreset");
    auto& root = *rootPtr;
    root.setAttribute ("section", Presets::sectionName (section));
    root.setAttribute ("plugin", effectVersion ? "fx" : "instrument");
    root.setAttribute ("version", 2); // 2: Full har <LayerNames>; äldre filer (saknas eller 1) ger standardnamn och -färger
    if (section == Presets::Section::effects)
        root.setAttribute ("scope", "channel"); // en effektkanal
    if (section == Presets::Section::pattern)
    {
        root.setAttribute ("scope", "row"); // en rad: en kick- eller instrumentrad, med sina toner, GATE och inställningar
        root.setAttribute ("rowKind", layer == Pattern::kickLayer ? "kick" : "instrument");
    }

    if (link != nullptr && link->valid())
    {
        auto* linkXml = root.createNewChildElement ("Link");
        linkXml->setAttribute ("kind", link->kind);
        linkXml->setAttribute ("name", link->name);
        linkXml->setAttribute ("amount", (double) link->amount);
    }

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
        entry->setAttribute ("id", section == Presets::Section::bass ? Presets::toInstrumentLayer (ranged->getParameterID(), 0)
                                   : section == Presets::Section::pattern ? Presets::patternIdToCanonical (ranged->getParameterID(), layer)
                                   : section == Presets::Section::effects ? Presets::effectIdToCanonical (ranged->getParameterID(), layer)
                                                                          : ranged->getParameterID());
        entry->setAttribute ("value", (double) ranged->convertFrom0to1 (ranged->getValue()));
    }

    if (section == Presets::Section::full)
    {
        auto* namesXml = root.createNewChildElement ("LayerNames");
        for (int i = 0; i < ParamID::numInstrumentLayers; ++i)
        {
            namesXml->setAttribute ("name" + juce::String (i + 1), layerNames[(size_t) i]);
            if (layerColours[(size_t) i] != 0) // som i projekttillståndet: bara ändrade färger
                namesXml->setAttribute ("colour" + juce::String (i + 1), juce::String ((juce::int64) layerColours[(size_t) i])); // String: setAttribute(int64) är tvetydig (C2668); läses med getLargeIntValue (:1312)
        }
    }

    if (section == Presets::Section::full)
        root.addChildElement (pattern.toValueTree().createXml().release());
    if (section == Presets::Section::pattern)
        root.addChildElement (pattern.rowToValueTree (layer).createXml().release());

    if (section == Presets::Section::full || section == Presets::Section::kick)
    {
        root.addChildElement (kickSample.toValueTree (clickTreeType).createXml().release());
        root.addChildElement (kickPitchEnv.toValueTree (kickPitchType).createXml().release());
        root.addChildElement (kickAmpEnv.toValueTree (kickAmpType).createXml().release());
    }

    if (section == Presets::Section::full)
        root.addChildElement (instrumentSamplesToValueTree().createXml().release());
    if (section == Presets::Section::bass && instrumentSamples[(size_t) layer].hasSample())
        root.addChildElement (instrumentSamples[(size_t) layer].toValueTree ("BassSample").createXml().release());

    // Oscillatorernas egna wavetables (User WAV): annars laddas presetet med fel tabell (den som råkar finnas, eller Basic).
    if (section == Presets::Section::full)
        root.addChildElement (userTablesToValueTree().createXml().release());
    if (section == Presets::Section::bass)
        root.addChildElement (userTablesToValueTree (layer).createXml().release());

    if (section == Presets::Section::full || section == Presets::Section::sidechain)
        root.addChildElement (sidechainCurve.toValueTree().createXml().release());

    if (section == Presets::Section::full || section == Presets::Section::lfo)
    {
        root.addChildElement (modRoutes.toValueTree (getParameters()).createXml().release());
        root.setAttribute ("activeLfos", (int) modulator.activeMask.load());
        for (int i = 0; i < Modulation::maxLfos; ++i)
            if (i < Modulation::numLfos || isLfoActive (i))
                root.addChildElement (lfoCurves[(size_t) i].toValueTree ("LfoCurve" + juce::String (i + 1)).createXml().release());
    }

    if (section == Presets::Section::full)
        if (const auto liveCode = liveCodeToValueTree(); liveCode.isValid())
            root.addChildElement (liveCode.createXml().release());

    return rootPtr;
}

bool PluginBase::saveUserPreset (Presets::Section section, const juce::String& name, int layer, const PresetLink* link)
{
    const auto fileName = juce::File::createLegalFileName (name.trim());
    const auto directory = Presets::folder (section, effectVersion);
    if (fileName.isEmpty() || directory.createDirectory().failed())
        return false;

    const auto root = buildPresetXml (section, layer, link);
    if (! root->writeTo (directory.getChildFile (fileName + presetFileExtension)))
        return false;

    presetNameSlot (section, layer) = fileName;
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    return true;
}

namespace
{
    // Det i ett spår som varken ligger i instrument- eller mönsterdelen av ett preset: på/av, duck, FX-skick och mixern.
    std::vector<std::pair<juce::String, juce::String>> trackExtraParameters (int layer)
    {
        const auto mixer = [layer] (const char* field) { return Mixer::paramID (Mixer::firstBassSource + layer, field); };
        return { { "on", ParamID::bass (layer, InstrumentField::on) },
                 { "duck", ParamID::bass (layer, InstrumentField::duck) },
                 { "fxChannel", ParamID::bass (layer, InstrumentField::fxChannel) },
                 { "fxAmount", ParamID::bass (layer, InstrumentField::fxAmount) },
                 { "fxSend", ParamID::bass (layer, InstrumentField::fxSend) },
                 { "mixGain", mixer ("Gain") }, { "mixPan", mixer ("Pan") }, { "mixDuck", mixer ("Duck") }, { "mixMax", mixer ("Max") } };
    }

    // Instrumentets egna modulationskällor följer med spåret till ett annat instrument; LFO:er, makron och andras källor lämnas.
    int sourceInLayer (int source, int layer)
    {
        return Modulation::isInstrumentSource (source) ? Modulation::instrumentSource (layer, Modulation::kindOfSource (source)) : source;
    }
}

std::unique_ptr<juce::XmlElement> PluginBase::copyTrack (int layer)
{
    if (effectVersion || ! juce::isPositiveAndBelow (layer, ParamID::numInstrumentLayers))
        return nullptr;

    auto clip = std::make_unique<juce::XmlElement> ("KickbaseTrack");
    clip->setAttribute ("name", layerNames[(size_t) layer]);
    clip->setAttribute ("colour", (int) layerColours[(size_t) layer]);
    clip->addChildElement (buildPresetXml (Presets::Section::bass, layer).release());
    clip->addChildElement (buildPresetXml (Presets::Section::pattern, layer).release());

    auto* extras = clip->createNewChildElement ("Extras");
    for (const auto& [key, id] : trackExtraParameters (layer))
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            auto* entry = extras->createNewChildElement ("Param");
            entry->setAttribute ("key", key);
            entry->setAttribute ("value", (double) ranged->convertFrom0to1 (ranged->getValue()));
        }

    // LFO-kopplingar till instrumentets rattar, sparade med parameterns ID som för lager 1 (som i presets) och källor som för lager 1.
    auto* routes = clip->createNewChildElement ("Routes");
    const auto& parameters = getParameters();
    for (const auto& route : modRoutes.routes)
    {
        const auto source = route.source.load(), target = route.target.load();
        if (source < 0 || ! juce::isPositiveAndBelow (target, parameters.size()))
            continue;
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[target]); withId != nullptr && Presets::instrumentLayerOf (withId->paramID) == layer)
        {
            auto* entry = routes->createNewChildElement ("Route");
            entry->setAttribute ("source", sourceInLayer (source, 0));
            entry->setAttribute ("target", Presets::toInstrumentLayer (withId->paramID, 0));
            entry->setAttribute ("amount", (double) route.amount.load());
        }
    }
    return clip;
}

bool PluginBase::pasteTrack (int layer, const juce::XmlElement& clip)
{
    if (beforeTrackChange)
        beforeTrackChange();
    if (effectVersion || ! clip.hasTagName ("KickbaseTrack") || ! juce::isPositiveAndBelow (layer, ParamID::numInstrumentLayers))
        return false;

    const juce::XmlElement* bassXml = nullptr;
    const juce::XmlElement* patternXml = nullptr;
    for (const auto* child : clip.getChildWithTagNameIterator ("KickbasePreset"))
        (child->getStringAttribute ("section") == Presets::sectionName (Presets::Section::pattern) ? patternXml : bassXml) = child;
    if (bassXml == nullptr || patternXml == nullptr)
        return false;

    // Det gamla innehållet (och dess kopplingar) rensas först, så att inget blir kvar av det som skrivs över.
    wipeInstrument (layer);
    loadPresetXml (*bassXml, Presets::Section::bass, layer, {});
    loadPresetXml (*patternXml, Presets::Section::pattern, layer, {});

    if (const auto* extras = clip.getChildByName ("Extras"))
        for (const auto& [key, id] : trackExtraParameters (layer))
            for (const auto* entry : extras->getChildIterator())
                if (entry->getStringAttribute ("key") == key)
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
                        Presets::setParameter (*ranged, ranged->convertTo0to1 ((float) entry->getDoubleAttribute ("value")));

    if (const auto* routes = clip.getChildByName ("Routes"))
    {
        const auto& parameters = getParameters();
        for (const auto* entry : routes->getChildIterator())
        {
            const auto targetId = Presets::toInstrumentLayer (entry->getStringAttribute ("target"), layer);
            for (int i = 0; i < parameters.size(); ++i)
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]); withId != nullptr && withId->paramID == targetId)
                {
                    modRoutes.add (sourceInLayer (entry->getIntAttribute ("source"), layer), i, (float) entry->getDoubleAttribute ("amount"));
                    break;
                }
        }
    }

    layerNames[(size_t) layer] = clip.getStringAttribute ("name");
    layerColours[(size_t) layer] = (juce::uint32) clip.getIntAttribute ("colour");
    ++layerNamesVersion;
    ++layerStructureVersion;
    patternChanged();
    markStateChanged();
    return true;
}

bool PluginBase::duplicateTrack (int layer)
{
    const auto count = activeInstruments();
    if (effectVersion || ! juce::isPositiveAndBelow (layer, count) || count >= ParamID::numInstrumentLayers)
        return false;

    std::vector<std::unique_ptr<juce::XmlElement>> clips;
    for (int i = layer; i < count; ++i)
        clips.push_back (copyTrack (i));

    setInstrumentCount (count + 1);
    for (int i = count - 1; i > layer; --i)
        pasteTrack (i + 1, *clips[(size_t) (i - layer)]);
    pasteTrack (layer + 1, *clips[0]);
    selectedInstrument = layer + 1;
    return true;
}

bool PluginBase::moveTrack (int layer, int delta)
{
    const auto other = layer + delta;
    const auto count = activeInstruments();
    if (effectVersion || std::abs (delta) != 1 || ! juce::isPositiveAndBelow (layer, count) || ! juce::isPositiveAndBelow (other, count))
        return false;

    const auto a = copyTrack (layer), b = copyTrack (other);
    pasteTrack (layer, *b);
    pasteTrack (other, *a);
    selectedInstrument = other;
    return true;
}

bool PluginBase::deleteTrack (int layer)
{
    if (beforeTrackChange)
        beforeTrackChange();
    const auto count = activeInstruments();
    if (effectVersion || count <= 1 || ! juce::isPositiveAndBelow (layer, count))
        return false;

    for (int i = layer; i < count - 1; ++i)
        if (const auto next = copyTrack (i + 1))
            pasteTrack (i, *next);
    setInstrumentCount (count - 1); // tömmer det sista instrumentet
    selectedInstrument = juce::jmin (selectedInstrument, count - 2);
    return true;
}

bool PluginBase::loadUserPreset (Presets::Section section, const juce::File& file, int layer)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("KickbasePreset")
        || xml->getStringAttribute ("section") != Presets::sectionName (section))
        return false;
    if (section == Presets::Section::effects && xml->getStringAttribute ("scope") != "channel")
        return false; // ett äldre FX-preset med alla kanaler
    if (section == Presets::Section::pattern
        && (xml->getStringAttribute ("scope") != "row" || xml->getStringAttribute ("rowKind") != (layer == Pattern::kickLayer ? "kick" : "instrument")))
        return false; // ett äldre preset med hela mönstret, eller en kickrad i ett instrument (eller tvärtom)

    return loadPresetXml (*xml, section, layer, file.getFileNameWithoutExtension());
}

bool PluginBase::loadEmbeddedInit()
{
    if (effectVersion)
        return false;
    const auto xml = juce::XmlDocument::parse (juce::String::fromUTF8 (BinaryData::InitPreset_mofp, BinaryData::InitPreset_mofpSize));
    return xml != nullptr && loadPresetXml (*xml, Presets::Section::full, 0, "Init");
}

bool PluginBase::loadPresetXml (const juce::XmlElement& xmlElement, Presets::Section section, int layer, const juce::String& presetName)
{
    PresetUndoScope undoScope (*this, section, presetName);
    const auto* xml = &xmlElement;

    // Delens parametrar får filens värden; de som saknas i filen (t.ex. nya sedan presetet sparades) får sitt standardvärde i stället för att
    // ärvas från förra ljudet. Slutvärdet räknas ut först och varje parameter sätts en gång: ljudtråden spelar under tiden, och med standard
    // först och filens värde sedan kunde den spela ett block med delen på standardvärden mitt i laddningen.
    std::vector<juce::RangedAudioParameter*> sectionParameters;
    std::vector<float> targets;
    std::map<juce::String, size_t> indexOfId;
    std::set<juce::String> fileIds; // parametrar som filen har
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p); ranged != nullptr && Presets::belongsTo (section, ranged->getParameterID(), layer))
        {
            indexOfId[ranged->getParameterID()] = sectionParameters.size();
            sectionParameters.push_back (ranged);
            targets.push_back (ranged->getDefaultValue());
        }

    if (const auto* paramsXml = xml->getChildByName ("Params"))
    {
        for (const auto* entry : paramsXml->getChildIterator())
        {
            auto id = entry->getStringAttribute ("id");
            if (section == Presets::Section::bass)
                id = Presets::toInstrumentLayer (id, layer);
            else if (section == Presets::Section::pattern)
                id = Presets::patternIdFromCanonical (id, layer);
            else if (section == Presets::Section::effects)
                id = Presets::effectIdFromCanonical (id, layer);
            if (const auto found = indexOfId.find (id); found != indexOfId.end())
            {
                fileIds.insert (id);
                targets[found->second] = sectionParameters[found->second]->convertTo0to1 ((float) entry->getDoubleAttribute ("value"));
            }
        }
    }

    // Effektrattar som tillkommit efter att presetet sparades: neutralt standardvärde i stället för 0.5.
    for (int channel = 0; channel < numEffectChannels; ++channel)
        for (int slot = 0; slot < slotsPerChannel; ++slot)
            if (const auto type = indexOfId.find (Effects::slotParamID (channel, slot, "Type")); type != indexOfId.end())
            {
                const auto effectType = (EffectType) juce::jlimit (0, numEffectTypes - 1, juce::roundToInt (sectionParameters[type->second]->convertFrom0to1 (targets[type->second])));
                for (int k = 0; k < knobsPerSlot; ++k)
                    if (float neutral = 0.0f; ! fileIds.count (Effects::knobParamID (channel, slot, k)) && Effects::newKnobDefault (effectType, k, neutral))
                        if (const auto knob = indexOfId.find (Effects::knobParamID (channel, slot, k)); knob != indexOfId.end())
                            targets[knob->second] = neutral;
            }

    // Också parametrar som redan har rätt värde sätts: APVTS råa värde (det ljudtråden läser) avrundas till parameterns steg först när den
    // sätts, och dess startvärde är inte avrundat (t.ex. 59.9999962 i stället för 60).
    for (size_t i = 0; i < sectionParameters.size(); ++i)
        Presets::setParameter (*sectionParameters[i], targets[i]);

    if (section == Presets::Section::full)
    {
        // Ett Full-preset ersätter alla namn och färger; filer utan <LayerNames> (version 1) ger standard.
        const auto* namesXml = xml->getChildByName ("LayerNames");
        for (int i = 0; i < ParamID::numInstrumentLayers; ++i)
        {
            layerNames[(size_t) i] = namesXml != nullptr ? namesXml->getStringAttribute ("name" + juce::String (i + 1)) : juce::String();
            layerColours[(size_t) i] = namesXml != nullptr ? (juce::uint32) namesXml->getStringAttribute ("colour" + juce::String (i + 1), "0").getLargeIntValue() : 0;
        }
        ++layerNamesVersion;
    }

    if (section == Presets::Section::full)
        if (const auto* patternXml = xml->getChildByName (Pattern::treeType))
        {
            pattern.fromValueTree (juce::ValueTree::fromXml (*patternXml));
            if (patternXml->getIntAttribute ("version", 1) < 7)
                Params::applyLegacyLengthParams (apvts, pattern);
        }
    if (section == Presets::Section::pattern)
        if (const auto* patternXml = xml->getChildByName (Pattern::treeType))
        {
            pattern.rowFromValueTree (layer, juce::ValueTree::fromXml (*patternXml));
            if (patternXml->getIntAttribute ("version", 1) < 7)
                Params::applyLegacyLengthParams (apvts, pattern, layer);
        }

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
            instrumentSamplesFromValueTree (juce::ValueTree::fromXml (*samplesXml));
        else
            for (auto& slot : instrumentSamples)
                slot.clear();
    }
    else if (section == Presets::Section::bass)
    {
        if (const auto* sampleXml = xml->getChildByName ("BassSample"))
            instrumentSamples[(size_t) layer].fromValueTree (juce::ValueTree::fromXml (*sampleXml));
        else
            instrumentSamples[(size_t) layer].clear();
    }

    // Egna wavetables. Ett Full-preset ersätter alla (äldre filer utan dem: inga); ett instrument-preset bara sitt lagers, och bara om
    // filen har dem med (äldre instrument-presets rör inte lagrets tabeller, som förut).
    if (section == Presets::Section::full)
    {
        if (const auto* tablesXml = xml->getChildByName ("UserTables"))
            userTablesFromValueTree (juce::ValueTree::fromXml (*tablesXml));
        else
            userTablesFromValueTree (juce::ValueTree ("UserTables"));
    }
    else if (section == Presets::Section::bass)
    {
        if (const auto* tablesXml = xml->getChildByName ("UserTables"))
            userTablesFromValueTree (juce::ValueTree::fromXml (*tablesXml), layer);
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
        clearCodeMods();
        if (const auto* routesXml = xml->getChildByName (ModRoutes::treeType))
            modRoutes.fromValueTree (juce::ValueTree::fromXml (*routesXml), getParameters());
        else
            modRoutes.clear();
        for (int i = 0; i < Modulation::maxLfos; ++i)
        {
            if (const auto* curveXml = xml->getChildByName ("LfoCurve" + juce::String (i + 1)))
                lfoCurves[(size_t) i].fromValueTree (juce::ValueTree::fromXml (*curveXml));
            else
                lfoCurves[(size_t) i].reset();
        }
        applyLoadedLfos ((std::uint32_t) xml->getIntAttribute ("activeLfos", 7)); // (efter parametrarna ovan; gamla presets: LFO 1-3)
    }

    if (section == Presets::Section::full)
    {
        presetNames.fill ({});
        instrumentPresetNames.fill ({});
        rowPresetNames.fill ({});
        fxPresetNames.fill ({});
    }

    if (section == Presets::Section::full) // filer utan <LiveCode> (äldre, Init) lämnar användarens kod och lås som de är
        if (const auto* liveCodeXml = xml->getChildByName ("LiveCode"))
            liveCodeFromValueTree (juce::ValueTree::fromXml (*liveCodeXml));
    if (section == Presets::Section::full)
        ++liveCodeLoads;

    presetNameSlot (section, layer) = presetName;
    ensureTablesInUse();
    patternChanged();
    return true;
}

PluginBase::PresetLink PluginBase::readPresetLink (const juce::File& presetFile)
{
    PresetLink link;
    if (const auto xml = juce::XmlDocument::parse (presetFile))
        if (const auto* linkXml = xml->getChildByName ("Link"))
        {
            link.kind = linkXml->getStringAttribute ("kind");
            link.name = linkXml->getStringAttribute ("name");
            link.amount = (float) linkXml->getDoubleAttribute ("amount", 1.0);
        }
    return link;
}

juce::File PluginBase::findUserPreset (Presets::Section section, const juce::String& name) const
{
    if (name.isEmpty())
        return {};
    return Presets::folder (section, effectVersion).getChildFile (juce::File::createLegalFileName (name) + presetFileExtension);
}

int PluginBase::sendChannelOf (Presets::Section section, int layer) const
{
    const auto kick = section == Presets::Section::kick;
    const auto channelID = kick ? juce::String (ParamID::kickFxChannel) : ParamID::bass (layer, InstrumentField::fxChannel);
    const auto amountID = kick ? juce::String (ParamID::kickFxAmount) : ParamID::bass (layer, InstrumentField::fxAmount);
    const auto channel = (int) apvts.getRawParameterValue (channelID)->load() - 1;
    if (channel < 0 || channel >= numEffectChannels || apvts.getRawParameterValue (amountID)->load() <= 0.0f)
        return -1;
    return channel;
}

bool PluginBase::fxChannelHasEffects (int channel) const
{
    for (int slot = 0; slot < slotsPerChannel; ++slot)
        if (Effects::slotType (apvts, channel, slot) != EffectType::off)
            return true;
    return false;
}

int PluginBase::firstFreeFxChannel()
{
    const auto mask = fxChannelUsageMask();
    for (int channel = 0; channel < numEffectChannels; ++channel)
        if ((mask & (1 << channel)) == 0)
            return channel;
    return -1;
}

bool PluginBase::saveInstrumentPreset (Presets::Section section, int layer, const juce::String& name, const juce::String& fxName)
{
    PresetLink link;
    const auto channel = sendChannelOf (section, layer);
    if (fxName.trim().isNotEmpty() && channel >= 0)
    {
        if (! saveUserPreset (Presets::Section::effects, fxName, channel))
            return false;
        link.kind = "effect";
        link.name = juce::File::createLegalFileName (fxName.trim());
        link.amount = apvts.getRawParameterValue (section == Presets::Section::kick ? juce::String (ParamID::kickFxAmount)
                                                                                     : ParamID::bass (layer, InstrumentField::fxAmount))->load();
    }
    return saveUserPreset (section, name, layer, link.valid() ? &link : nullptr);
}

bool PluginBase::savePatternPreset (int layer, const juce::String& name, const juce::String& instrumentName, const juce::String& fxName)
{
    PresetLink link;
    if (instrumentName.trim().isNotEmpty())
    {
        const auto kick = layer == Pattern::kickLayer;
        if (! saveInstrumentPreset (kick ? Presets::Section::kick : Presets::Section::bass, kick ? 0 : layer, instrumentName, fxName))
            return false;
        link.kind = kick ? "kick" : "instrument";
        link.name = juce::File::createLegalFileName (instrumentName.trim());
    }
    return saveUserPreset (Presets::Section::pattern, name, layer, link.valid() ? &link : nullptr);
}

PluginBase::LinkedLoad PluginBase::loadInstrumentPreset (Presets::Section section, int layer, const juce::File& file, int fxChannel)
{
    LinkedLoad result;
    const auto link = readPresetLink (file);
    const auto effectFile = link.kind == "effect" ? findUserPreset (Presets::Section::effects, link.name) : juce::File();
    result.effectMissing = link.kind == "effect" && link.valid() && ! effectFile.existsAsFile();

    if (! loadUserPreset (section, file, layer))
        return result;
    result.ok = true;

    if (! link.valid() || result.effectMissing || fxChannel == -1)
        return result;
    if (fxChannel == -2)
        fxChannel = firstFreeFxChannel();
    if (fxChannel < 0 || fxChannel >= numEffectChannels || ! loadUserPreset (Presets::Section::effects, effectFile, fxChannel))
        return result;

    // Instrumentet skickar till kanalen med den mängd det hade när presetet sparades.
    const auto kick = section == Presets::Section::kick;
    for (const auto& [id, value] : { std::pair<juce::String, float> { kick ? juce::String (ParamID::kickFxChannel) : ParamID::bass (layer, InstrumentField::fxChannel), (float) (fxChannel + 1) },
                                     std::pair<juce::String, float> { kick ? juce::String (ParamID::kickFxAmount) : ParamID::bass (layer, InstrumentField::fxAmount), link.amount } })
        if (auto* parameter = apvts.getParameter (id))
            Presets::setParameter (*parameter, parameter->convertTo0to1 (value));
    result.fxChannel = fxChannel;
    return result;
}

PluginBase::LinkedLoad PluginBase::loadPatternPreset (int layer, const juce::File& file, bool withInstrument, int fxChannel)
{
    LinkedLoad result;
    const auto link = readPresetLink (file);
    const auto kick = layer == Pattern::kickLayer;
    const auto instrumentFile = link.valid() ? findUserPreset (kick ? Presets::Section::kick : Presets::Section::bass, link.name) : juce::File();
    result.instrumentMissing = link.valid() && ! instrumentFile.existsAsFile();

    if (! loadUserPreset (Presets::Section::pattern, file, layer))
        return result;
    result.ok = true;

    if (withInstrument && link.valid() && ! result.instrumentMissing)
    {
        const auto instrument = loadInstrumentPreset (kick ? Presets::Section::kick : Presets::Section::bass, kick ? 0 : layer, instrumentFile, fxChannel);
        result.effectMissing = instrument.effectMissing;
        result.fxChannel = instrument.fxChannel;
    }
    return result;
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
        for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
            turnOff (ParamID::bass (layer, InstrumentField::on).toRawUTF8());
    if (part == ExportPart::bass)
        turnOff (ParamID::kickOn);

    copy->ensureTablesInUse();
    return OfflineRender::render (*copy, settings);
}

bool PluginBase::exportWav (const juce::File& file, ExportPart part, int bars, int instrument, bool withoutEffects)
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
        for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
            turnOff (ParamID::bass (layer, InstrumentField::on).toRawUTF8());
    if (part == ExportPart::bass || part == ExportPart::instrument)
        turnOff (ParamID::kickOn);
    if (part == ExportPart::instrument)
        for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
            if (layer != instrument)
                turnOff (ParamID::bass (layer, InstrumentField::on).toRawUTF8());
    if (withoutEffects)
    {
        turnOff (ParamID::kickFxAmount);
        for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
            turnOff (ParamID::bass (layer, InstrumentField::fxAmount).toRawUTF8());
    }

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

bool PluginBase::loadInstrumentSample (int layer, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (layer, ParamID::numInstrumentLayers) || ! instrumentSamples[(size_t) layer].loadFile (file))
        return false;
    patternChanged();
    return true;
}

void PluginBase::clearInstrumentSample (int layer)
{
    if (juce::isPositiveAndBelow (layer, ParamID::numInstrumentLayers))
        instrumentSamples[(size_t) layer].clear();
    patternChanged();
}

juce::ValueTree PluginBase::instrumentSamplesToValueTree() const
{
    juce::ValueTree tree ("BassSamples");
    for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
        if (instrumentSamples[(size_t) layer].hasSample())
        {
            auto child = instrumentSamples[(size_t) layer].toValueTree ("BassSample");
            child.setProperty ("layer", layer, nullptr);
            tree.appendChild (child, nullptr);
        }
    return tree;
}

void PluginBase::instrumentSamplesFromValueTree (const juce::ValueTree& tree)
{
    std::array<bool, ParamID::numInstrumentLayers> found {};
    for (const auto& child : tree)
        if (const auto layer = (int) child.getProperty ("layer", -1); juce::isPositiveAndBelow (layer, ParamID::numInstrumentLayers))
        {
            instrumentSamples[(size_t) layer].fromValueTree (child);
            found[(size_t) layer] = true;
        }
    for (int layer = 0; layer < ParamID::numInstrumentLayers; ++layer)
        if (! found[(size_t) layer])
            instrumentSamples[(size_t) layer].clear();
}

int PluginBase::rowRootNote (int noteLayer) const
{
    const auto key = transportStatus.keyNote.load();
    if (noteLayer == Pattern::kickLayer)
    {
        // Kickens tonhöjd 0 är dess svans: på tonarten (Follow) eller en fast ton (Fixed).
        const auto follow = (int) apvts.getRawParameterValue (ParamID::kickKeyFollow)->load() == 1;
        return follow ? Params::kickTailMidiNote ((key >= 0 ? key : 60) % 12) : 24;
    }
    return Params::instrumentRootMidiNote (key >= 0 ? key : 60);
}
