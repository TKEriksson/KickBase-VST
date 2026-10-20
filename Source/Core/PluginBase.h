#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Effects.h"
#include "KickEngine.h"
#include "Master.h"
#include "Mixer.h"
#include "OfflineRender.h"
#include "Modulation.h"
#include "Params.h"
#include "Pattern.h"
#include "Presets.h"
#include "Sidechain.h"
#include "SignalTap.h"
#include "TransportStatus.h"
#include "SampleSlot.h"
#include "BridgeLink.h"
#include "Settings.h"
#include "DeferredRelease.h"

// Det som instrumentet och FX-versionen har gemensamt: parametrar, mönster, klicksampling,
// sparat läge och klockan.
class AiChat;
class MixAnalyser;

class PluginBase : public juce::AudioProcessor
{
public:
    PluginBase (const BusesProperties& buses, juce::AudioProcessorValueTreeState::ParameterLayout layout, bool effectVersion);
    ~PluginBase() override;

    bool isEffectVersion() const { return effectVersion; }

    // AI-chatten i synten (skapas första gången den behövs; samtalet finns kvar medan pluginet lever). Message thread.
    AiChat& getAiChat();
    MixAnalyser& getMixAnalyser();

    // Hur länge en typisk ton i instrumentets mönster hålls, i sekunder (mediannotens längd gånger lagrets LENGTH, vid DAW:ens tempo); 0 om lagret
    // inte har några toner. Meddelandetråden. Räknas om högst fyra gånger per sekund, så att diagram kan fråga ofta.
    double typicalNoteSeconds (int layer);

    // Vilka effektkanaler som används (bit n = FX n+1): något skickas dit (kicken eller ett instrument), en effekt ligger på någon plats eller kanalens EQ är inställd.
    // Gränssnittet döljer de andra. Meddelandetråden.
    int fxChannelUsageMask();

    // SOLO i mönstret: bit n = instrument n+1 (bit 10 = kicken). Är något soloat spelar bara de soloade. Inget som sparas med projektet:
    // solo är ett lyssningshjälpmedel, och ett projekt ska inte öppnas med ett instrument tyst.
    std::atomic<int> soloMask { 0 };
    std::atomic<int> nonFiniteRecoveries { 0 }; // hur många gånger ljudtråden tömt sitt tillstånd efter NaN/inf (se allFinite)
    static constexpr int kickSoloBit = 1 << 10; // en enda analys per instans: samma som visas i ANALYZER-fliken och som en AI hämtar med get_mix_report

    // Mastersektionen ägs av underklassen (den behöver bara finnas där ljudet bearbetas).
    virtual const MasterSection& getMasterSection() const = 0;

    // Effektkanalerna ägs också av underklassen; gränssnittet läser deras uttag (EQ, kompressor).
    virtual const EffectRack& getEffectRack() const = 0;

    // Mixern (bara instrumentet): gain, panorering, mätare och bypass per ljudkälla.
    virtual const Mixer* getMixer() const { return nullptr; }

    void releaseResources() override {}
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Anropas av gränssnittet när mönstret ändrats, så att DAW:en vet att projektet ändrats.
    void patternChanged();

    // Som patternChanged, för annat som sparas utan att vara en parameter (t.ex. effektordningen).
    void markStateChanged();
    int clearLfo (int index);                  // tar bort kopplingarna, återställer parametrar och kurva, sist biten i masken
    void applyLoadedLfos (std::uint32_t mask); // efter laddning: LFO:er med kopplingar räknas som aktiva, övriga återställs

    // Presets per del (se Presets.h). Bara i plugin:ets egen meny, inte som VST3-"programs":
    // en värd kan sätta program-parametern när ett projekt laddas och skulle då skriva över
    // det sparade. Anropas från gränssnittet (message thread).
    // `layer` gäller Bass (vilket baslager presetet laddas i eller sparas från).
    void loadFactoryPreset (Presets::Section section, int index, int layer = 0);
    // Länkade presets: ett instrument- eller kick-preset kan peka på ett FX-preset (kanalen instrumentet skickar till), och ett pattern-preset på ett
    // instrument- eller kick-preset. Länken är ett namn (filen i respektive mapp), inte en kopia. kind = "effect", "instrument" eller "kick".
    struct PresetLink
    {
        juce::String kind, name;
        float amount = 1.0f; // bara "effect": hur mycket som skickas till effekten
        bool valid() const { return name.isNotEmpty(); }
    };
    static PresetLink readPresetLink (const juce::File& presetFile);
    juce::File findUserPreset (Presets::Section section, const juce::String& name) const;
    int sendChannelOf (Presets::Section section, int layer) const;      // effektkanalen (0-10) som kicken (Section::kick) eller instrumentet skickar till, eller -1
    bool fxChannelHasEffects (int channel) const;                        // någon plats på kanalen har en effekt
    int firstFreeFxChannel();                                            // första kanalen utan sändningar, effekter eller EQ, eller -1

    // Sparar ett instrument-/kick-preset; med fxName också effektkanalen det skickar till som FX-preset med det namnet, och en länk dit.
    bool saveInstrumentPreset (Presets::Section section, int layer, const juce::String& name, const juce::String& fxName = {});
    // Sparar en rads mönster-preset; med instrumentName också instrumentet (kicken för kickraden) som eget preset, med sin effekt om fxName anges.
    bool savePatternPreset (int layer, const juce::String& name, const juce::String& instrumentName = {}, const juce::String& fxName = {});

    // fxChannel: kanalen den länkade effekten läggs på; -2 = första lediga (finns ingen hoppas effekten över), -1 = ingen effekt, annars den kanalen.
    struct LinkedLoad
    {
        bool ok = false;
        bool effectMissing = false;    // presetet länkade en effekt som inte finns längre
        bool instrumentMissing = false;
        int fxChannel = -1;            // kanalen effekten lades på, eller -1
    };
    LinkedLoad loadInstrumentPreset (Presets::Section section, int layer, const juce::File& file, int fxChannel = -2);
    LinkedLoad loadPatternPreset (int layer, const juce::File& file, bool withInstrument, int fxChannel = -2);

    bool saveUserPreset (Presets::Section section, const juce::String& name, int layer = 0, const PresetLink* link = nullptr);
    std::unique_ptr<juce::XmlElement> buildPresetXml (Presets::Section section, int layer, const PresetLink* link = nullptr); // samma innehåll som filen

    // Spår = ett instrument och dess rad i mönstret. Instrumenten ligger på fasta platser (0-9), så ett spår flyttas, dupliceras och raderas genom
    // att innehållet kopieras mellan platserna. copyTrack ger en klipp (ljud, mönster, på/av, duck, FX-skick, mixer, LFO-kopplingar, namn, färg)
    // som pasteTrack skriver över ett annat instrument med. Bara meddelandetråden.
    std::unique_ptr<juce::XmlElement> copyTrack (int layer);
    bool pasteTrack (int layer, const juce::XmlElement& clip);
    std::function<void()> beforeTrackChange; // sätts av InstrumentHost: river panelerna innan ett instrument skrivs över
    bool duplicateTrack (int layer);        // en kopia på platsen efter, resten flyttas ner (kräver ett ledigt instrument)
    bool moveTrack (int layer, int delta);  // byter plats med grannen (delta -1 eller +1)
    bool deleteTrack (int layer);           // resten flyttas upp och sista instrumentet töms (minst ett instrument kvar)
    bool loadUserPreset (Presets::Section section, const juce::File& file, int layer = 0);
    bool loadPresetXml (const juce::XmlElement& xml, Presets::Section section, int layer, const juce::String& presetName); // själva laddningen (fil eller inbyggt)
    bool loadEmbeddedInit(); // instrumentets fabrikspreset Init: ett Full-preset inbakat i programmet

    // Bara för utvecklingsverktygen och testerna: använd Init från fabrikslistan i koden (PresetData.inl) i stället för den inbakade filen, så att testerna
    // får samma startläge som förr oavsett vilket ljud Init har. Aldrig satt i pluginet självt.
    static inline bool preferCodeInit = false;
    juce::Array<juce::File> getUserPresets (Presets::Section section, int layer = -1) const; // Mönster: bara rad-presets, av rätt sort om `layer` anges
    juce::String getPresetName (Presets::Section section, int layer = 0) const;

    // Renderar ljudet offline till en WAV-fil som loopar: bars takter i DAW:ens senaste tempo och
    // med den senaste tonen. Kick eller bas ensam stänger av den andra. Anropas från gränssnittet.
    // `instrument` (0-9, bara med ExportPart::instrument): ett enda instrument ensamt, utan kicken. `withoutEffects`: kickens och instrumentens
    // sändningar till FX-kanalerna nollas, så att bara det torra ljudet (med master) skrivs.
    enum class ExportPart { full, kick, bass, instrument };
    bool exportWav (const juce::File& file, ExportPart part, int bars, int instrument = -1, bool withoutEffects = false);

    // Renderar en kopia av det nuvarande tillståndet offline (den levande ljudtråden rörs inte). Kick eller bas
    // ensam stänger av den andra. Används av MCP-bryggans render_preview.
    juce::AudioBuffer<float> renderOffline (const OfflineRender::Settings& settings, ExportPart part);

    // Egna wavetables från WAV-filer, en per oscillator (lager 0-2, OSC A/B). Anropas från gränssnittet
    // (message thread); ljudtråden läser tabellerna via userTable. Sparas i projektet.
    // `file` kan också vara en mapp: dess WAV-filer (högst 16, jämnt utvalda, i namnordning) blir tabellens vågformer.
    bool loadUserTable (int layer, int osc, const juce::File& file);
    void ensureTablesInUse(); // bygger de inbyggda tabeller som oscillatorerna använder nu (blockerar; inte på ljudtråden)
    const Wavetable* userTable (int layer, int osc) const { return userTables[(size_t) (layer * ParamID::oscsPerLayer + osc)].load(); }
    juce::String getUserTableName (int layer, int osc) const { return userNames[(size_t) (layer * ParamID::oscsPerLayer + osc)]; }
    size_t retiredTableCount() const { return retiredTables.size(); } // ersatta tabeller som ännu inte frigjorts (för testerna)

    // Instrumenten (baslager 1-10). Antalet sätts här: tas ett instrument bort rensas det helt (mönster, alla inställningar, sampling,
    // egna tabeller, namn, mixer och LFO-kopplingar) så att inget ligger kvar i minnet. `instrumentHasContent` säger om något går
    // förlorat (för bekräftelsen) och beskriver vad i `what`. Anropas från gränssnittet (message thread).
    void setInstrumentCount (int count);
    void wipeInstrument (int layer);
    bool instrumentHasContent (int layer, juce::String* what = nullptr) const;

    // AI-länken till MCP-bryggan (se BridgeLink). Av som standard; användaren slår på den med AI-knappen. Message thread.
    void setBridgeEnabled (bool enabled);
    bool isBridgeEnabled() const { return bridge != nullptr && bridge->isRunning(); }
    juce::String getBridgeId() const { return bridge != nullptr ? bridge->getId() : juce::String(); }
    std::unique_ptr<PluginBase> cloneForOffline() const { return createOfflineCopy(); } // en egen instans för analys och rendering
    // Anteckningar från en AI (via verktyget add_report_note, eller chatten i synten) som visas i ANALYZER-flikens rapport: förslag och det som redan är fixat.
    struct AiNote
    {
        juce::String title, text;
        int finding = 0;       // 1-baserat nummer på fyndet anteckningen gäller (0 = inget särskilt)
        bool applied = false;  // redan genomfört (annars ett förslag)
    };
    void addAiNote (AiNote note)
    {
        const juce::ScopedLock lock (aiNotesLock);
        aiNotes.push_back (std::move (note));
        if (aiNotes.size() > 40)
            aiNotes.erase (aiNotes.begin());
        ++aiNotesVersion;
    }
    void clearAiNotes()
    {
        const juce::ScopedLock lock (aiNotesLock);
        aiNotes.clear();
        ++aiNotesVersion;
    }
    std::vector<AiNote> getAiNotes() const
    {
        const juce::ScopedLock lock (aiNotesLock);
        return aiNotes;
    }
    juce::var getAiNotesVar() const
    {
        juce::Array<juce::var> list;
        for (const auto& note : getAiNotes())
        {
            auto* entry = new juce::DynamicObject();
            entry->setProperty ("title", note.title);
            entry->setProperty ("text", note.text);
            entry->setProperty ("finding", note.finding);
            entry->setProperty ("applied", note.applied);
            list.add (juce::var (entry));
        }
        return list;
    }
    std::atomic<int> aiNotesVersion { 0 };
    int getBridgeConnections() const { return bridge != nullptr ? bridge->getConnectedCount() : 0; }

    // Kickens sampling (klick eller hel kick) och baslagrens samplingar. Anropas från gränssnittet (message thread).
    bool loadClickSample (const juce::File& file);
    void clearClickSample();
    juce::String getClickSampleName() const { return kickSample.name(); }
    const ClickSample* clickForDisplay() const { return getActiveClickSample(); } // message thread

    bool loadInstrumentSample (int layer, const juce::File& file);
    void clearInstrumentSample (int layer);
    juce::String getInstrumentSampleName (int layer) const { return instrumentSamples[(size_t) juce::jlimit (0, ParamID::numInstrumentLayers - 1, layer)].name(); }
    const ClickSample* getActiveInstrumentSample (int layer) const { return instrumentSamples[(size_t) juce::jlimit (0, ParamID::numInstrumentLayers - 1, layer)].active(); }

    juce::AudioProcessorValueTreeState apvts;
    Pattern pattern;
    SidechainCurve sidechainCurve;

    // Vad gränssnittet ritar i realtid: kicken, och basen före och efter duckningen under kicken.
    SignalTap kickTap, instrumentBeforeDuckTap, instrumentAfterDuckTap;

    // Varje instruments egen signal (efter duckning, före effekter) till ANALYZER-fliken. Skrivs bara när fliken är öppen och analysen
    // påslagen (`instrumentTapsOn`), så att den inte kostar något annars.
    std::array<ShortSignalTap, ParamID::numInstrumentLayers> instrumentTaps;
    std::atomic<bool> instrumentTapsOn { false };

    // Hur mycket varje baslager duckas just nu: förstärkningen (1 = orörd) sample för sample, och
    // det senaste värdet som minskning 0..1 (för ringen runt DUCK-ratten).
    std::array<SignalTap, ParamID::numInstrumentLayers> duckGainTaps;
    std::array<std::atomic<float>, ParamID::numInstrumentLayers> duckReduction {};

    // Kickens pitch-envelope (halvtoner över svansen, upp till 0,5 s) och volym-envelope
    // (upp till 2 s), redigeras i KICK-fliken.
    NodeEnvelope kickPitchEnv { 0.5f, 72.0f, { { 0.0f, 42.0f, -0.6f }, { 0.045f, 0.0f, 0.0f } } };
    NodeEnvelope kickAmpEnv { 2.0f, 1.0f, { { 0.0f, 0.0f, 0.0f }, { 0.0004f, 1.0f, 0.0f }, { 0.015f, 1.0f, -0.5f }, { 0.155f, 0.0f, 0.0f } } };

    // LFO:ernas egna former (shape Custom): fas 0..1 i x, 0..1 i y (uppåt = positivt). Startar som en sinus; redigeras på LFO-fliken.
    static std::vector<NodeEnvelope::Point> sineCurve()
    {
        std::vector<NodeEnvelope::Point> points;
        for (int i = 0; i <= 8; ++i)
            points.push_back ({ (float) i / 8.0f, 0.5f - 0.5f * std::sin (juce::MathConstants<float>::twoPi * (float) i / 8.0f), 0.0f });
        return points;
    }
    std::array<NodeEnvelope, Modulation::maxLfos> lfoCurves { NodeEnvelope (1.0f, 1.0f, sineCurve()), NodeEnvelope (1.0f, 1.0f, sineCurve()),
                                                              NodeEnvelope (1.0f, 1.0f, sineCurve()), NodeEnvelope (1.0f, 1.0f, sineCurve()),
                                                              NodeEnvelope (1.0f, 1.0f, sineCurve()), NodeEnvelope (1.0f, 1.0f, sineCurve()),
                                                              NodeEnvelope (1.0f, 1.0f, sineCurve()), NodeEnvelope (1.0f, 1.0f, sineCurve()),
                                                              NodeEnvelope (1.0f, 1.0f, sineCurve()), NodeEnvelope (1.0f, 1.0f, sineCurve()) };
    static_assert (Modulation::maxLfos == 10, "lfoCurves ovan har en sinus per LFO");

    // Aktiva LFO:er (fas M). Instrumentet har 10, FX 3 (saknar parametrarna för LFO 4-10). LFO 1-3 är aktiva som standard.
    int availableLfos() const;
    bool isLfoActive (int index) const;
    int lfoRouteCount (int index) const;
    // Lägger till eller tar bort en LFO (UI- och MCP-tråden). Borttagen: kopplingarna försvinner och parametrar och kurva återställs.
    // Ger antalet borttagna kopplingar.
    int setLfoActive (int index, bool active);

    // Kickens inställningar som de är just nu, utan LFO (för gränssnittets bilder).
    KickParams kickForDisplay (int keyPitchClass) const;
    TransportStatus transportStatus;

    // Provlyssning från gränssnittet (en klick på kickens, wavetablens eller samplingens ruta, eller en LISTEN-knapp). Ljudtråden
    // slår an ljudet nästa block: 0 = kicken, 200 + lager = instrumentets hela ton en sekund, 300 + lager * 10 + källa = bara en
    // källa i ett lager (källa 0-2 = OSC 1-3, 3 = samplingen). 5 = bara kickens sampling (oavsett läge).
    void requestAudition (int what) { auditionRequest = what; }
    static int layerAudition (int layer) { return 200 + layer; }
    static int soloAudition (int layer, int source) { return 300 + layer * 10 + source; }

    // Provlyssning som hålls: tonen klingar så länge musen hålls ner och startar om när ljudet tar slut (loop); `auditionHoldStop` släpper den.
    // 400 + lager = instrumentets hela ton, 500 + lager * 10 + källa = bara en källa (som soloAudition).
    static constexpr int auditionHoldStop = 98;
    static int heldLayerAudition (int layer) { return 400 + layer; }
    static int heldSoloAudition (int layer, int source) { return 500 + layer * 10 + source; }

    // Förhandsspelning av en enskild rad (en pianorulle): spelar raden en gång med den egna klockan, utan DAW-ton (100 + notlager,
    // där 0-9 = instrument och 10 = kicken; samma anrop igen avbryter). `previewingLayer` visar vilken rad som spelas, -1 = ingen.
    // Raden efter kicken är hela synthen (alla rader). Förhandsspelningen loopar tills `previewStop` skickas, och ett nytt anrop börjar om från början.
    static constexpr int previewAllRows = Pattern::numNoteLayers;
    static constexpr int previewStop = 99;
    static int previewRequest (int noteLayer) { return 100 + noteLayer; }

    // Börjar om räknaren och mönstret vid 1.1 (egen klocka; med DAW-synk styr DAW:en).
    void requestRestart() { restartRequest = true; }
    std::atomic<bool> restartRequest { false };
    std::atomic<int> previewingLayer { -1 };

    // Pianorullens urklipp (gränssnittet): kopierade toner, relativt den första, och var nästa inklistring börjar som standard
    // (där de kopierade slutar, och efter en inklistring där den slutar).
    struct NoteClipboard
    {
        struct Note { int dt = 0, length = 24, pitch = 0; float velocity = 0.8f; bool slide = false; Pattern::Chord chord {}; };
        std::vector<Note> notes;
        int span = 0;          // hela de kopierade tonernas längd i ticks
        int nextStart = -1;    // var nästa inklistring börjar, -1 = vid vyns början
    } noteClipboard;
    bool snapshotSpectrum = false; // bara för skärmbildsverktyget: SPECTRUM-analysen är på (även när fönstret inte visas)
    bool snapshotPaste = false; // bara för skärmbildsverktyget: pianorullen startar med en inklistring
    std::atomic<int> previewNote { 60 }; // tonen som LISTEN och förhandsspelningen spelar (MIDI, C5 = 60 som i FL)

    // Instrumentets tangentbord (för LISTEN-knappen som spelar C5), eller nullptr i FX-versionen.
    virtual juce::MidiKeyboardState* getKeyboardState() { return nullptr; }

    // Baslagrens namn: standard BASS 1-3, men man kan döpa dem själv. `layerNamesVersion` ökas vid ändring, så att
    // gränssnittet kan uppdatera sina texter.
    juce::String getLayerName (int layer) const
    {
        const auto index = (size_t) juce::jlimit (0, ParamID::numInstrumentLayers - 1, layer);
        return layerNames[index].isEmpty() ? "INSTR " + juce::String (layer + 1) : layerNames[index];
    }
    void setLayerName (int layer, const juce::String& name)
    {
        layerNames[(size_t) juce::jlimit (0, ParamID::numInstrumentLayers - 1, layer)] = name.trim().substring (0, 10);
        ++layerNamesVersion;
    }
    // Eget färgval per instrument (0 = standardfärgen), sparas i projektet. Ökar `layerNamesVersion`.
    juce::uint32 getLayerColour (int layer) const { return layerColours[(size_t) juce::jlimit (0, ParamID::numInstrumentLayers - 1, layer)]; }
    void setLayerColour (int layer, juce::uint32 argb)
    {
        layerColours[(size_t) juce::jlimit (0, ParamID::numInstrumentLayers - 1, layer)] = argb;
        ++layerNamesVersion;
        ++layerStructureVersion;
    }
    std::atomic<int> layerNamesVersion { 0 };
    std::atomic<int> layerStructureVersion { 0 }; // ökas när en färg ändras eller ett instrument klistras in: panelerna byggs om (InstrumentHost)

    // Hur många instrument som används (1-10; FX-versionen: 1). Läses av gränssnittet.
    int activeInstruments() const
    {
        if (effectVersion)
            return 1;
        return juce::jlimit (1, ParamID::numInstrumentLayers, (int) std::lround (apvts.getRawParameterValue (ParamID::instrumentCount)->load()));
    }
    std::atomic<int> auditionRequest { -1 };

    std::atomic<bool> followPlayhead { true }; // pianorullen och stegvyn bläddrar med uppspelningen
    std::atomic<int> lfoActiveVersion { 0 }; // ökas när en LFO läggs till eller tas bort (fas M), så att gränssnittet ritar om
    std::atomic<int> patternVersion { 0 }; // ökas när mönstret ändras, så att gränssnittet ritar om
    std::atomic<int> liveCodeLocks { 0 };  // Live Code: en bit per rad (0-9 = instrument, 10 = kicken) som koden inte får skriva över
    std::atomic<int> liveCodeLoads { 0 };  // ökar när ett projekt eller en Full-preset laddats; LIVE CODE-sidan stoppar då LIVE
    std::atomic<int> liveCodeOwned { 0 };  // rader som Live Code skrev senast; andra rader med toner räknas som handritade
    // Live Code, steg D: kodtexten och låsen sparas i projektet och i Full-presets (element <LiveCode>, sist, bara när något är satt).
    // LIVE, CODE ONLY och liveCodeOwned sparas aldrig: inget startar av sig självt vid laddning, och rader med toner är handritade tills koden körs.
    juce::String getLiveCodeText() const { const juce::ScopedLock lock (liveCodeTextLock); return liveCodeText; }
    void setLiveCodeText (const juce::String& text) { const juce::ScopedLock lock (liveCodeTextLock); liveCodeText = text; }
    juce::ValueTree liveCodeToValueTree() const; // ogiltigt träd = inget att spara
    void liveCodeFromValueTree (const juce::ValueTree& tree);
    // Live Code, steg E (.lfo/.env): meddelandetråden håller en post per (rad, parameter, sort) med sin CODE-plats. Kopplingarna från
    // CODE-källor sparas aldrig (Modulation::isCodeSource) och rensas vid laddning, stopp och ny körning utan .lfo/.env.
    struct CodeMod { juce::String key; int slot = 0; };
    std::vector<CodeMod> codeMods;
    void clearCodeMods (bool dropRoutes = true); // stänger alla platser och tar bort alla kopplingar från CODE-källor direkt
    void sweepCodeRoutes(); // tar bort kopplingar vars plats hunnit tona ut (anropas av meddelandetråden då och då)
    int selectedTab = 0;                  // gränssnittets valda flikar (nedre och övre panelen), så att de
    int selectedSynthTab = 0;              // finns kvar när fönstret öppnas igen
    int selectedPatternMode = 0;           // PATTERN-flikens läge: 0 PATTERN (toner), 1 GATE
    std::array<int, ParamID::numInstrumentLayers> selectedInstrumentSource {}; // instrumentets valda flik: INSTR, OSC 1-3, SAMPLE eller FILTER
    std::atomic<int> kickTab { 0 };                // kickens vyflik: 0 SYNTH, 1 SAMPLE (bara gränssnittet, sparas inte och ändrar inget ljud)
    int selectedPatternLayer = 0;          // PATTERN-flikens valda lager: 0-9 instrument, 10 kick
    int selectedInstrument = 0;            // INSTRUMENT-flikens valda instrument (0-9)
    int lfoFolded = -1;                    // LFO-flikens fällda kort (en bit per LFO); -1 = inte bestämt ännu (se LfoPage)
    int selectedLfo = 0;                   // LFO-flikens valda kort (SHOW ONLY SELECTED)
    int lfoRemoveAsk = -1;                 // LFO-kortet som frågar om borttagning (-1 = inget)
    int sequencerRowHeight = 0;            // radhöjden i mönstret (zoom): 0 = automatisk
    // Fönstrets skala (1 = fullstor). Gäller alla fönster och sparas i inställningarna (Settings), inte i projektet. Editorn sätter
    // `applyEditorScale` så att inställningssidan kan ändra storleken på det öppna fönstret.
    float editorScale = 0.0f;
    std::function<void (float)> applyEditorScale;
    void requestEditorScale (float scale)
    {
        editorScale = juce::jlimit (0.5f, 1.6f, scale);
        Settings::setUiScale (editorScale);
        if (applyEditorScale)
            applyEditorScale (editorScale);
    }
    bool pianoRollOpen = false;            // PATTERN-flikens vy: stegen eller pianorullen
    int selectedFxView = 2;                // EFFECTS-sidans vy: 0 EQ, 1 COMP, 2 FX

protected:
    // En ny processor av samma slag, som exporten spelar med en kopia av inställningarna.
    virtual std::unique_ptr<PluginBase> createOfflineCopy() const = 0;

    // Anropas i prepareToPlay i underklasserna.
    void prepareBase (double sampleRate);

    // Läser DAW:ens position. Får bara anropas från processBlock (i en bit: positionen vid bitens början).
    juce::Optional<juce::AudioPlayHead::PositionInfo> getHostPosition() const;

    // Kontrolltakten (fråga 1 i granskningen): ett DAW-block delas i bitar som börjar på en multipel av 32 samples, räknat från
    // prepareToPlay, och hela blockbehandlingen (modulering, parametrar, effekter, master) körs per bit. Då blir ljudet detsamma
    // vilken buffertstorlek DAW:en än använder (FL Studio byter den dessutom under spelningen), och LFO:er och ENV 2/3 rör sig var
    // 32:a sample i stället för en gång per block. `process (bit, bitensMidi)` anropas för varje bit; MIDI-händelserna flyttas till bitens tid.
    static constexpr int controlInterval = Modulation::controlInterval;
    template <typename Process>
    void processInControlChunks (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, Process&& process)
    {
        const auto numSamples = buffer.getNumSamples();
        blockPosition = readHostPosition();
        for (int start = 0; start < numSamples;)
        {
            const auto length = std::min (numSamples - start, controlInterval - (int) (controlCounter % controlInterval));
            juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, length);
            chunkMidi.clear();
            chunkMidi.addEvents (midi, start, length, -start);
            chunkOffset = start;
            controlPhase = (int) (controlCounter % controlInterval);
            process (chunk, chunkMidi);
            controlCounter += length;
            start += length;
        }
        chunkOffset = 0;
        controlPhase = 0;
    }

    // Skydd mot NaN och oändliga värden (ljudtråden): ett enda sådant värde fastnar annars för alltid i filtrens och effekternas tillstånd,
    // och pluginet blir tyst eller knastrar tills det laddas om. `allFinite` kollar ett block; hittas något töms blocket och underklassen
    // nollställer sitt DSP-tillstånd. `nonFiniteRecoveries` räknar hur ofta det hänt (för tester och felsökning).
    static bool allFinite (const juce::AudioBuffer<float>& buffer, int numSamples);
    static void replaceNonFinite (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples); // icke-ändliga värden blir 0

    juce::Optional<juce::AudioPlayHead::PositionInfo> readHostPosition() const;
    juce::Optional<juce::AudioPlayHead::PositionInfo> blockPosition; // vid DAW-blockets början
    int chunkOffset = 0;               // bitens början i DAW-blocket
    int controlPhase = 0;              // hur långt in i sitt 32-fönster biten börjar (0 = på rutnätet: moduleringen uppdateras)
    juce::int64 controlCounter = 0;    // samples sedan prepareToPlay (bitarnas gränser)
    juce::MidiBuffer chunkMidi;        // bitens MIDI (plats reserveras i prepareBase)

    // Klicksamplingen som ljudtråden ska använda just nu (kan vara nullptr).
    const ClickSample* getActiveClickSample() const { return kickSample.active(); }

    ParamRefs params;
    ParamRefs displayParams { apvts }; // utan modulering, för gränssnittet
    TransportClock clock;

public:
    // LFO-kopplingarna (gränssnittet lägger till och tar bort) och modulatorn som räknar ut dem.
    ModRoutes modRoutes;
    Modulator modulator { apvts, modRoutes };

protected:
    // Läser kickens och basens parametrar, med LFO-modulering.
    KickParams readModulatedKick (int keyPitchClass) const;
    InstrumentParams readModulatedInstrument (int layer, int rootMidiNote) const;

private:
    juce::ValueTree instrumentSamplesToValueTree() const;
    void instrumentSamplesFromValueTree (const juce::ValueTree& tree);

    // Originalet sparas i projektet; en omsamplad kopia spelas. Bara message thread och
    // prepareToPlay rör originalet, därför låset (aldrig på ljudtråden).
    static constexpr int numUserSlots = ParamID::numInstrumentLayers * ParamID::oscsPerLayer;
    void setUserTable (int slot, std::unique_ptr<Wavetable> table, std::vector<float> frames, const juce::String& name);
    juce::ValueTree userTablesToValueTree (int onlyLayer = -1) const;              // onlyLayer >= 0: bara det instrumentet, sparat som lager 0
    void userTablesFromValueTree (const juce::ValueTree& tree, int intoLayer = -1); // intoLayer >= 0: ett instrument-preset i det lagret
    void clearUserTable (int slot);

    std::array<std::atomic<const Wavetable*>, numUserSlots> userTables {};
    std::array<std::unique_ptr<Wavetable>, numUserSlots> userOwned;
    DeferredRelease<Wavetable> retiredTables; // ljudtråden kan läsa en ersatt tabell ett block till: den frigörs först efter en stund
    std::array<std::vector<float>, numUserSlots> userFrames;
    std::array<juce::String, numUserSlots> userNames;

    juce::SharedResourcePointer<TableBuilder> tableBuilder; // bygger tabeller som ljudtråden bett om
    std::unique_ptr<BridgeLink::Server> bridge;

    SampleSlot kickSample { 2.0 };
    std::array<SampleSlot, ParamID::numInstrumentLayers> instrumentSamples; // 12 s var (minne används först när en sampling laddats)

    juce::String& presetNameSlot (Presets::Section section, int layer);

    const bool effectVersion;
    std::array<juce::String, Presets::numSections> presetNames; // bara message thread
    std::array<juce::String, ParamID::numInstrumentLayers> instrumentPresetNames;
    std::array<juce::String, numEffectChannels> fxPresetNames;       // FX-presets: ett namn per effektkanal
    std::array<juce::String, Pattern::numNoteLayers> rowPresetNames; // mönster-presets: ett namn per rad (instrument 0-9, kicken sist)
    std::array<juce::String, ParamID::numInstrumentLayers> layerNames;
    std::array<juce::uint32, ParamID::numInstrumentLayers> layerColours {};
    juce::String liveCodeText;
    mutable juce::CriticalSection liveCodeTextLock;

    double currentSampleRate = 44100.0;

    mutable juce::CriticalSection aiNotesLock;
    std::vector<AiNote> aiNotes;
    std::array<double, Pattern::numLayers> noteSecondsCache {};
    std::array<juce::uint32, Pattern::numLayers> noteSecondsStamp {};
    std::unique_ptr<MixAnalyser> mixAnalyser;
    std::unique_ptr<AiChat> aiChat; // sist: rivs först, så att dess tråd är klar innan resten försvinner
};
