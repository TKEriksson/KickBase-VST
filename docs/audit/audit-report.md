# Granskning av Kickbase: rapport (första körningen)

Gren `audit` (från `dev` 08dedcf). Skrivs löpande. Allvarlighet: **krasch**, **fel ljud**, **data** (något går förlorat eller sparas fel),
**prestanda**, **förvirrande**, **kosmetiskt**. Status: *rättad (commit)*, *fråga N* (se [questions-for-tobias.md](questions-for-tobias.md)),
*föreslagen*, *ok* (granskat, inget fel).

## Sammanfattning

**Rättat (allt testat; ljudet bit-identiskt om inget annat står, se avsnitt 0 för hur det kontrolleras):**
- *Krascher och häng:* minne som ljudtråden läste kunde frigöras när samplingar/wavetables byttes snabbt (avsnitt 1); fönstret hängde
  upp till 15 s om det stängdes under ett AI-anrop; ett enda NaN gjorde pluginet tyst tills det laddades om.
- *Knaster:* ~700 minnesallokeringar per ljudblock (EQ-läsningen) och nya filterobjekt på ljudtråden vid varje EQ-ändring.
- *Sparande:* egen wavetable på OSC 3 lästes inte tillbaka; presets sparade inte egna wavetables; gamla tabeller/kicksampling blev kvar
  när ett annat tillstånd laddades; preset-byte kunde spela ett block med standardvärden; testerna skrev i din presetmapp.
- *Prestanda:* fönstret tog ~½ processorkärna bara av att vara öppet (SETTINGS räknade 4000 filer 10 gånger/s), mönstret ritades om helt
  10 gånger/s, ljudtråden ~⅓ snabbare, analysen ~35 % snabbare och dess kopia 561 → 123 ms.
- *Texter:* varje parameter och effektratt har en förklaring (AI:n får dem också), dubbla och missvisande DAW-namn, README/DESIGN/AGENT_GUIDE.
- *Design:* klippta etiketter med 10 instrument, Kickbase FX:s huvud och SETTINGS, LENGTH-ratten, vyer som visade fel första bilden.
- *Kod:* 0 kompilatorvarningar (15), död kod, `static_assert`, namnbytena bass → instrument (C++) och MyOwnFruit → Kickbase (mål/skript),
  plugin-identiteten och alla ID:n orörda.

**Behöver ditt beslut:** [questions-for-tobias.md](questions-for-tobias.md): 1 (moduleringen beror på DAW:ens buffertstorlek: hörbart),
2 och 3 (spektrumet och analysen ännu lättare), 4 (sidechain-fabriksljudens djup), 7 (gamla filer i repots rot), 8 (mappen/GitHub).
5 och 6 är gjorda, säg till om du vill ha dem som förut.

**Inte gjort / inte granskat på djupet:** inget är provat i FL Studio (bara i testvärden `KickbaseHostTest`); pluginval är inte kört;
länkade presets och deras dialoger är bara täckta av de befintliga testerna; varje parameters intervall och standardvärde är genomläst i
listorna men inte provlyssnat ett och ett; förslagen i 4b och avsnitt 5.

## 0. Inventering och verktyg

| Vad | Resultat |
|---|---|
| Bygge RelWithDebInfo (alla mål) | ok |
| Alla tester (`MyOwnFruitTests`, `eqtest`, `rolltest`, `lengthtest`, `instrumenttest`, `presettest`, `auditiontest`, `settingstest`, `chattest`, `listentest`, `perftest`, `--selftest-link`, `KickbaseHostTest` på båda VST3) | alla gick igenom redan före granskningen, också efter Sonnets `preferCodeInit` |
| `rendersig` (kod-Init) | identisk med Sonnets baslinje före Init-bytet → [baseline-kod.txt](baseline-kod.txt) |
| `rendersigreal` (inbakat Init) | → [baseline-init.txt](baseline-init.txt) |
| Parameterregistret | [params-instrument.txt](params-instrument.txt) (2052), [params-fx.txt](params-fx.txt) (841) |

Nya verktyg och tester (alla i `KickbaseMcp tool ...`, körs av hela sviten):

| Kommando | Vad det gör |
|---|---|
| `rendersigwide` | Fingeravtryck av ett "tungt" läge ([Tools/Scenarios.h](../../Tools/Scenarios.h): 10 instrument med tre oscillatorer, unison, alla warp-lägen, filter, sub, brus, drive, GATE, swing/humanize/delay, glide, samplingar, egna wavetables även på OSC 3; alla effekttyper, EQ och kompressor på alla kanaler; LFO:er, makron, ENV 2/3; kickens alla lager; masterns EQ/bredd) i båda pluginsen, med blockstorlek 37–1024 och 44,1–96 kHz. `rendersig` täckte bara Init och fabriksljuden, så en optimering kunde ändra ljudet i effekter, FX-versionen eller modulering utan att det syntes. |
| `audiotest` | Räknar minnesallokeringar och frigöranden *på ljudtråden* (programmets `operator new` byts mot en räknare som bara är på i den tråd som mäts) i tunga lägen, också medan rattar, EQ, effekttyper och modulering ändras mellan blocken; gränsfall (block på 1 sample, ett block större än utlovat, byte av samplingsfrekvens); NaN/inf in och NaN från DAW:en. `KB_ALLOC_TRACE=1` visar varifrån en allokering kommer. |
| `persisttest` | Sparande/laddning av egna wavetables och samplingar, presets, att preset-laddning sätter varje parameter en gång, att ersatta data inte frigörs för tidigt, kick-presets och `kickOn`. |
| `blocksizetest` | Hur mycket ljudet beror på DAW:ens blockstorlek (fråga 1). |
| `profile <init\|heavy\|fx\|analysis-init> [instrument] [s]` | Samplande profilerare (DbgHelp, PDB): var ljudtråden eller analysen lägger tiden, per funktion och källrad. |
| `uiprofile <flik> <init\|heavy> [instrument] [s]` | Samma för gränssnittet: editorn i ett riktigt fönster, ljudet i realtid; CPU för meddelandetråden och ljudtråden. |

Testerna som sparar filer körs nu i en tillfällig mapp (`Scenarios::TemporaryUserFolders`), aldrig i `Dokument\Kickbase` (förut sparade
`presettest` testfiler bland dina presets och raderade dem efteråt: en krasch mitt i hade lämnat dem kvar).

Varje ändring nedan är kontrollerad med hela sviten + `rendersig`, `rendersigreal` och `rendersigwide` (bit-identiska om inget annat
står) + `repairpresets` (mätning) på alla dina 165 presets (identiska) + skärmbilder av gränssnittet mot det gamla bygget (pixel-identiska
om inget annat står).

## 1. Ljudtråd, krascher och trådsäkerhet

| Var | Vad | Allvar | Status |
|---|---|---|---|
| [Effects.cpp:1438](../../Source/Core/Effects.cpp#L1438) (gamla raden) | `Effects::readEq` byggde parameter-ID:n som text för varje upptagen FX-kanal i varje block: **~700 minnesallokeringar per block** på ljudtråden (en allokering kan vänta på heapens lås: knaster vid hög last). Nu cachade pekare (`Effects::EqRefs`). | prestanda/knaster | rättad ee9a052, `audiotest` |
| [SourceEq.cpp](../../Source/Core/SourceEq.cpp) | En ändrad EQ (kick, instrument, master) skapade nya koefficientobjekt på ljudtråden (allokering + frigörande). Nu skapas de i `prepare` och värdena skrivs på plats (`eqArrayCoefficients`; JUCE bygger `Coefficients::make*` av `ArrayCoefficients` på samma sätt, så värdena är identiska). | knaster | rättad ee9a052, `audiotest` |
| [Master.cpp:154](../../Source/Core/Master.cpp#L154), effekterna | Ett enda NaN/inf (från en synth före Kickbase FX, eller trasig automation) fastnade för alltid i masterns DC-filter och effekternas återkopplingar: pluginet var tyst/trasigt tills det laddades om. FX-insignalen tvättas nu, och båda pluginsen nollställer sina DSP-tillstånd om ett block ändå blir icke-ändligt (`PluginBase::allFinite`, `nonFiniteRecoveries`). | fel ljud | rättad ee9a052, `audiotest` |
| [SampleSlot.cpp:77](../../Source/Core/SampleSlot.cpp#L77), [PluginBase.cpp](../../Source/Core/PluginBase.cpp) | En sampling eller egen wavetable som byttes ut sparade bara *den närmast föregående*. Två byten inom samma ljudblock (presets eller AI-anrop i rad; eller att ta bort flera instrument med egna tabeller på en gång, där bara 4 sparades och antalet instrument sänktes *efter* rensningen) frigjorde minne som ljudtråden läste: **möjlig krasch**. Nu `DeferredRelease` (frigörs efter 2 s). | krasch | rättad ee9a052, `persisttest` |
| [AiChat.cpp](../../Source/Core/AiChat.cpp) | Stängdes fönstret (pluginet togs bort, projektet stängdes) mitt i ett AI-anrop väntade destruktorn upp till 15 s och dödade sedan tråden (anropet hade 240 s timeout). Nu en `WebInputStream` som avbryts direkt. | häng | rättad f2272d1, `chattest` (0,01 s) |
| [PluginBase.cpp](../../Source/Core/PluginBase.cpp) `loadPresetXml`, [Presets.cpp](../../Source/Core/Presets.cpp) `applyFactory` | En preset-laddning satte först hela delen till standardvärden och sedan filens värden: ljudtråden kunde spela ett block med standardvärden mitt i (ett glapp när man byter preset under spelning). Nu räknas slutvärdet ut och varje parameter sätts en gång. **JUCE-detalj** värd att kunna: APVTS:s råa värde (det ljudtråden läser) uppdateras inte om det nya värdet är `approximatelyEqual` med det gamla, och dess startvärde är inte avrundat till parameterns steg ([juce_AudioProcessorValueTreeState.cpp:148-156](../../build/RelWithDebInfo/_deps/juce-src/modules/juce_audio_processors/utilities/juce_AudioProcessorValueTreeState.cpp)). Min första version hoppade över parametrar som redan hade rätt värde och ändrade då det råa värdet på 135 parametrar med några ULP (t.ex. 59.9999962 i stället för 60), vilket `rendersigreal` fångade (−102 dB). Därför sätts alla. | glapp | rättad ee9a052/f2272d1, `persisttest` |
| [InstrumentProcessor.cpp](../../Source/Instrument/InstrumentProcessor.cpp) `ensureScratchSize` | Ett block större än det DAW:en lovade i `prepareToPlay` allokerar (en gång) och fungerar. | – | ok (`audiotest`) |
| Mönstret, envelopes, sidechain-kurvan, LFO-kurvor, `modRoutes` | Alla fält som ljudtråden läser är atomiska (per ton, per tabellvärde, per koppling, och `source` sätts sist). En laddning under spelning kan ge ett block med en blandning av gammalt och nytt, men aldrig en krasch. | – | ok |
| `ensureTablesInUse` | Blockerar meddelandetråden medan tabeller byggs (några hundra ms första gången). Ljudtråden spelar "Basic" tills en tabell är klar. | – | ok (känd avgränsning) |
| [Params.cpp:105](../../Source/Core/Params.cpp#L105) `rowNames` | En statisk `std::map` som ljudtråden läser (via `ParamID::rowBars`). Fylls redan i konstruktorn, så det första (skrivande) anropet sker aldrig samtidigt med ljudtråden i praktiken. | – | ok, noterad |
| Moduleringen | Räknas en gång per block: ljudet beror på DAW:ens buffertstorlek. | fel ljud | **fråga 1** |

## 2. Sparande och bakåtkompatibilitet

| Var | Vad | Allvar | Status |
|---|---|---|---|
| [PluginBase.cpp:379](../../Source/Core/PluginBase.cpp) (gamla raden) | En egen wavetable på **OSC 3** lästes inte tillbaka från projektet (`osc < 2` i stället för `oscsPerLayer`). Samma sak i varje kopia (förhandslyssning, export, analys, AI:ns rendering). | data | rättad ee9a052, `persisttest` |
| `setStateInformation` | Ett tillstånd utan egna tabeller (eller kicksampling) lämnade de gamla kvar (t.ex. AI:ns ångra, eller en värd som laddar ett annat tillstånd i samma instans). | data | rättad ee9a052 |
| Full- och instrument-presets | Sparade inte oscillatorernas egna tabeller, bara valet "User WAV": presetet laddades med fel ljud. | data | rättad ee9a052 (fråga 6) |
| Kick-presets | Sparade och laddade `kickOn` (radens M), till skillnad från instrument-presets. | förvirrande | rättad f2272d1 (fråga 5) |
| Sidechain-fabriksljuden | Sätter `duckAmount`, som inte finns längre. | fel ljud | fråga 4 |
| `repairpresets` | Mätning (utan `apply`) på dina 165 presets med gammal och ny kod: identiska fingeravtryck. | – | ok |

## 3. Prestanda

Mätt med de nya profilerarna. Alla siffror RelWithDebInfo (Release är snabbare); jämförelser i samma byggtyp.

| Var | Vad | Före → efter | Status |
|---|---|---|---|
| [SettingsPage.cpp](../../Source/Ui/SettingsPage.cpp) | SETTINGS-sidans timer gick 10 gånger/s även när fliken var dold, och varje gång räknades AKWF-filerna på disken (`WavetablePack::installedCount`, rekursivt över 4000+ filer) och `settings.xml` lästes och tolkades flera gånger. **Ungefär en halv processorkärna** så länge fönstret var öppet. | standardvyn, 3 instrument: meddelandetråden 92 % → 22 % av en kärna (tillsammans med nästa rad) | rättad f6885fe |
| [StepSequencer.cpp](../../Source/Ui/StepSequencer.cpp) | Hela rutnätet (alla rader och celler, var och en med en egen Direct2D-gradient som byggs om) ritades om varje gång en markör flyttades, ~10 gånger/s. Nu ritas bara cellerna där markören var och hamnar, och celler utanför ritområdet hoppas över. | (se ovan) | rättad f6885fe; 13 flikar pixel-identiska |
| [Pattern.cpp:253](../../Source/Core/Pattern.cpp#L253) | GATE-stegen (4096 tecken per lager) lästes med `String::operator[]` och `length()`, som båda går igenom UTF-8-texten från början: kvadratiskt, ~80 miljoner teckensteg vid varje projekt-, preset-, analys- och renderingskopia. | kopia för analys/rendering 561 → 123 ms | rättad f6885fe |
| [Params.cpp:393](../../Source/Core/Params.cpp) | `ParamRefs` jämförde varje instrumentparameter med alla ~2000 ID:n (två gånger per instans och för varje kopia). | ingår ovan; ny instans ~80 ms med Init | rättad f6885fe |
| [MixAnalysis.cpp](../../Source/Core/MixAnalysis.cpp) `periodHz` | Kickens svans: energierna räknades om för varje fördröjning. | en analys 1,93 → 1,28 s, rapporterna identiska | rättad f6885fe |
| [SpectrumPage.cpp](../../Source/Ui/SpectrumPage.cpp) | Tysta källor hoppar över FFT:n, kurvorna kopieras inte varje bildruta. Resten är själva ritningen. | ~44 % av en kärna med 10 tunga instrument | delvis rättad f2272d1; fråga 2 |
| ANALYSE | Kör med låg prioritet; kan göras ännu snällare mot DAW:en. | | fråga 3 |
| Ljudtråden, tungt läge med 10 instrument | 58 % av en kärna (RelWithDebInfo). Största posterna: JUCE:s IIR-filter (EQ:erna) ~20 %, oscillatorerna och wavetables ~17 %, effektkanalerna ~10 %, filtrens koefficientomräkning (`tan`) 5–15 %. | | se 3b |

### 3b. Ljudtråden (commit 05c999e)

Profilerat med `tool profile` (riktiga Init med 3 instrument, och det tunga läget med 10). Allt nedan är **bit-identiskt** (`rendersig`,
`rendersigreal`, `rendersigwide`, dina 165 presets).

| Var | Vad | Status |
|---|---|---|
| [Biquad.h](../../Source/Core/Biquad.h), [SourceEq.cpp](../../Source/Core/SourceEq.cpp), [Effects.cpp](../../Source/Core/Effects.cpp) | EQ:erna (kick, instrument, master, FX-kanalerna, EQ-effekten) använde `juce::dsp::IIR::Filter::processSample`, som för *varje sample* anropar `check()` och `getFilterOrder()` och kör en generell slinga. `Biquad` gör samma räkneoperationer i samma ordning, med samma normalisering av koefficienterna som JUCE:s `Coefficients::assignImpl` (därför identiska bitar). | rättad |
| [InstrumentSynth.cpp](../../Source/Core/InstrumentSynth.cpp) (förr BassSynth.cpp) | Cutoff *och* resonans sattes på alla fyra filter varje gång filterenvelopen rörde sig: 8 `tan` per sample och röst. `StateVariableTPTFilter::update` räknar om från sina medlemmar, så nu anropas bara den setter vars värde ändrats. | rättad |
| [KickEngine.cpp](../../Source/Core/KickEngine.cpp) | Kickens low/high cut-, brus- och klickfilter sattes varje sample; nu bara vid ändring. Övertonernas `n^-tilt` sparas per tilt. | rättad |
| Drive (instrument och kick) | `tanh(gain)` (och `tanh(bias)`, `exp2`) räknades två gånger per sample; nu en gång per ändrad drive/läge. | rättad |
| [Master.cpp](../../Source/Core/Master.cpp) | Limiterns fönsterminimum (72 värden vid 48 kHz, varje sample) är nu en monoton kö (O(1)). CLIP på 0 hoppar över `tanh` (x + 0 ger samma bitar, också −0). | rättad |

CPU per sekund ljud (RelWithDebInfo): riktiga Init med 3 instrument 0,056 → 0,036 s, tungt läge med 10 instrument 0,583 → 0,383 s,
`perftest` 0,022/0,041/0,133 → 0,018/0,036/0,100 s. Alltså ungefär en tredjedel snabbare.

Kvar (inte gjort, skulle ändra ljudet en aning eller kräver mer arbete): oscillatorernas wavetable-läsning (~17 %) och
effektkanalernas reverb/delay. Ingen av dem är ett problem i dag.

## 4. Texter, namn och dokumentation

| Var | Vad | Allvar | Status |
|---|---|---|---|
| [Params.cpp:141, 214](../../Source/Core/Params.cpp) | Två parametrar hette "Kick Length" (`rowBarsKick`, `kickLength`) och två "Instr N Length" (`rowBarsBassN`, `bassNGate`) i DAW:ens automationslista. Nu "Kick Row Length", "Instr N Row Length", "Instr N Note Length". ID:n orörda. | förvirrande | rättad f2272d1 |
| [Effects.cpp](../../Source/Core/Effects.cpp) | Effektplatsernas namn i DAW:en, "FX1 1 Type", såg ut som kanal FX11. Nu "FX1 Slot 1 Type". | förvirrande | rättad 312d572 |
| [Params.cpp](../../Source/Core/Params.cpp) | Oscillatorerna hette A/B/C i DAW:en men OSC 1–3 i gränssnittet; mixern "Bass N"; `bassNGlide` (används inte längre) såg ut som en vanlig ratt och fanns bland MATRIX-målen; Kick Low Cut visade 20 Hz när den är av. Nu "Osc 1–3", "Mixer Kick/Instr N", "Glide (unused)" (borttagen ur MATRIX), "Off". | förvirrande | rättad 312d572 |
| [Help.cpp](../../Source/Ui/Help.cpp) | Varje parameter och varje effektratt har nu en förklaring (`tool helpcheck`: 0 saknas). Fel texter rättade: Glide, Kick Clip Mode (Fold saknades), OSC A/B, och sidechain-effektens rattar visade kompressorns THRESHOLD/ATTACK/RELEASE-texter (`forEffectKnob` får nu effekttypen). Döda poster borttagna. | förvirrande | rättad cb75289 |
| [ToolLayer.cpp](../../Source/Core/ToolLayer.cpp) | `list_parameters` och `list_effects` ger AI:n förklaringen (`help`), som [sound-design-roadmap.md](../sound-design-roadmap.md) ber om för registret. | – | rättad cb75289 |
| README.md | Raden `.\build.bat` var trasig (ett backsteg i filen), 13 effekttyper (15 finns), fel exe-namn, presets-beskrivningen gammal, `createVst.bat` nämndes inte. | förvirrande | rättad dd9c596 |
| docs/DESIGN.md | Hette annat än länkarna i CLAUDE.md och README; beskrev tre baslager och FX A–E. Nu upp till tio instrument, FX1–FX11 (ID:n `fxA`–`fxK`). | förvirrande | rättad dd9c596 |
| docs/AGENT_GUIDE.md, [AnalysisAbout.h](../../Source/Core/AnalysisAbout.h) | AI:ns guide: fältet `help`, vad presets sparar, att kicken inte har DUCK. Analysens "om"-text: kickens svans mäts i tidsdomänen. | förvirrande | rättad dd9c596 |

### 4b. Design och användbarhet

Skärmbilder av alla flikar med 1, 3 och 10 instrument, och av Kickbase FX. Allt utom raderna nedan var pixel-identiskt mot det gamla bygget.

| Var | Vad | Allvar | Status |
|---|---|---|---|
| [OscPanel.cpp](../../Source/Ui/OscPanel.cpp) | Wavetable-bilden visade "Basic" tills timern gått en gång, och ritades inte om när en tabell blev klar i bakgrunden. | kosmetiskt | rättad b6d3abb |
| [LfoPage.cpp](../../Source/Ui/LfoPage.cpp) | "→ Instr 2 Cutoff +26 %"-raden under en LFO syntes först efter en stund. | kosmetiskt | rättad b6d3abb |
| [Scopes.cpp](../../Source/Ui/Scopes.cpp) | Sidechain-förklaringen (DUCK 1–10) klipptes med många instrument: nu radbryts den. SOUNDSTAGE: "INSTRUMENTS" krockade med "MASTER". | kosmetiskt | rättad b6d3abb, a8553d5 |
| [EnvelopeView.cpp](../../Source/Ui/EnvelopeView.cpp) | Tidsetiketterna (10ms/50ms) skrevs ovanpå varandra i smala envelopes. | kosmetiskt | rättad b6d3abb |
| [Modules.cpp](../../Source/Ui/Modules.cpp) | LAYER OUT-ratten hette GATE men är notlängden (GATE är hackrytmen i mönstret). Nu LENGTH. | förvirrande | rättad b6d3abb |
| [Modules.h](../../Source/Ui/Modules.h) | Kickbase FX: OUT-mätaren låg över "140.0 BPM" (den placerades efter EXPORT, som inte finns i FX). | kosmetiskt | rättad a8553d5 |
| [SettingsPage.cpp](../../Source/Ui/SettingsPage.cpp) | Kickbase FX: WAVETABLE PACK-kolumnen klipptes (och FX har inga oscillatorer); hjälptexter syntes som halva rader. Nu utan vågformspaketet och wavetable-mappen i FX. | kosmetiskt | rättad a8553d5 |
| [SidechainPage.cpp](../../Source/Ui/SidechainPage.cpp) | Kickbase FX: "Set MODE to Curve to draw your o…" klipptes. | kosmetiskt | rättad a8553d5 |
| SETTINGS | "INSTALL AKWF PACK" står kvar när paketet redan finns (knappen installerar om). | kosmetiskt | föreslagen |
| Kickbase FX | Sidechain-förklaringen säger "INSTRUMENTS DUCKED"/"DUCK 1" fast det i FX är ljudet som kommer in. | förvirrande | föreslagen |
| KICK-fliken | 22 rattar i två rader; grupperna är tydliga men BODY/CLICK/TOP/SAMPLE kunde ha en egen färgnyans per grupp. | – | föreslagen (smak) |

## 5. Kodkvalitet, städning och namnbyten

| Var | Vad | Status |
|---|---|---|
| Kompilatorvarningar | En ren ombyggnad gav 15 varningar i vår kod (0 i JUCE) med JUCE:s rekommenderade nivå (`/W4`): skuggade namn (C4456/C4458), `double` → `float` (C4244), utfasade JUCE-anrop (C4996: `addFormat(new ...)`, `createEditorIfNeeded`, `ZipFile::Builder::addEntry(new ...)`) och ett kastat returvärde (C4858). Nu 0. `float *= double` skrevs om till `(float) (f * d)`, alltså samma avrundning; de utfasade anropen har direkta ersättare med samma beteende i 8.0.15 (`createEditorIfNeeded` anropar bara `createEditorAndMakeActive`). | rättad 840083d |
| Död kod | Oåtkomliga hjälptexter (`rowLoopBass1-3`, `rowBarsBass1-3`: fångades redan av en regel ovanför), oanvända `apvtsPointer` i effektracket, kontrollen av `duckAmount` (finns inte) i `Modulation::isModulatable`. | rättad 840083d |
| [PluginBase.cpp:9-14](../../Source/Core/PluginBase.cpp#L9-L14) | `static_assert` för talen som sparade projekt bygger på: kicken är mönsterlager 10, FX-kanalerna fxA–fxK med fem platser, modulationskällornas nummer (LFO 0–2, makro 3–6, sedan fyra per instrument), tre oscillatorer per instrument. | rättad 840083d |
| **bass → instrument** | 57 C++-namn bytta mekaniskt (`BassSynth` → `InstrumentSynth`, även filen; `BassField` → `InstrumentField`, `BassGrid`, `numBassLayers` → `numInstrumentLayers`, `bassLayerOf`, `setBass`, …). Ett skript bytte bara namn *utanför* strängar och teckenkonstanter, och den sorterade listan över **alla** strängkonstanter i `Source`, `Tools` och `Tests` är identisk före och efter: parameter-ID:n (`bass1Cutoff`), XML-taggar (`BassSample`), presetmappen `Bass`, `Section::bass` och AI-verktygens namn är orörda. Kvar med "bass": där det betyder basljud (fabriksljuden `bassAcid`, `bassReese`, analysen av en referenslåt `fitbass`), JUCE:s genererade BinaryData-namn, och fem namn där det nya namnet redan fanns (`bass`, `addBass`, `bassPreset(s)`, `firstBassSource`). | rättad 656a526 |
| **MyOwnFruit → Kickbase** | CMake-projektet och målen (`Kickbase_VST3`, `KickbaseFX_VST3`, `Kickbase_Standalone`, `KickbaseTests`, `KickbaseTool.exe`), `build.bat`, `install.ps1` (städningen av gamla `MyOwnFruit.vst3` i FL:s mapp är kvar), README, DESIGN, kommentarer, `FruitLookAndFeel` → `KickbaseLookAndFeel`. **Plugin-identiteten är orörd:** i JUCE 8.0.15 räknas VST3-klassens id ur `PLUGIN_MANUFACTURER_CODE` och `PLUGIN_CODE` ([juce_VST3ModuleInfo.h:54-62](../../build/RelWithDebInfo/_deps/juce-src/modules/juce_audio_plugin_client/VST3/juce_VST3ModuleInfo.h), `JUCE_VST3_CAN_REPLACE_VST2=0`), inte ur målets namn; `BUNDLE_ID` står kvar som `com.tobiaseriksson.myownfruit(fx)`. Bevis: `moduleinfo.json` (klass-id:na) är identisk före och efter, och `KickbaseHostTest <vst3> state save` med det gamla bygget följt av `state load` med det nya ger samma id och ett tillstånd som sparas byte för byte identiskt (båda pluginsen). | rättad 8f5a670 |
| Mappen och GitHub-repot | Kan inte bytas inifrån en session. | fråga 8 |
| Stora filer (`PluginBase.cpp`, `Modules.cpp`, `Tool.cpp`) | Inte uppdelade: det skulle ge en stor diff utan att ändra något, och krocka med annat arbete på `dev`. | föreslagen (vid tillfälle) |

## 6. Efter dina svar (18 okt)

Tillagt efter svaren i [questions-for-tobias.md](questions-for-tobias.md); inget ovan är ändrat.

| Fråga | Vad | Status |
|---|---|---|
| 7 | De sex gamla filerna i repots rot är borttagna (de var aldrig incheckade), `createVst.bat` är incheckad. | gjort 97681d1 |
| 4 | Sidechain-fabriksljudens `duckAmount` sätts nu på alla instruments DUCK ([Presets.cpp](../../Source/Core/Presets.cpp), `applyFactory`). `persisttest`: Classic Pump ger 0,9 på alla tio, Follow Kick standarddjupet. | gjort |
| 3 | Analysen (ANALYSE och AI:ns `analyse_mix`) körs med `Thread::Priority::background`, som JUCE 8.0.15 översätter till Windows `THREAD_PRIORITY_IDLE` ([juce_ThreadPriorities_native.h:90](../../build/RelWithDebInfo/_deps/juce-src/modules/juce_core/native/juce_ThreadPriorities_native.h)): den får bara CPU som annars står oanvänd (förut `low` = `THREAD_PRIORITY_LOWEST`). Rapporterna är desamma; den tar längre tid om datorn är upptagen. | gjort |
| 2 | **SETTINGS > OPTIMIZATIONS** (knappen i WINDOW SIZE-rutan, där REDUCE ANIMATIONS satt): GLOBAL (REDUCE ANIMATIONS, halverar allt) och en rad per vy med tydligt namn: ANALYZER (bildtakt, analyser/s, FILLED), SIDECHAIN, MASTER: maximizer, SOUNDSTAGE, EFFECTS (bildtakt, FILLED), MASTER: mixer meters, TOP BAR, EVERYTHING ELSE. Fem steg: 5/10/20/30/40 bilder/s (analyser: 4/8/15/30/40). Standard: 30 bilder/s som förut, ANALYZER 20; analyser 15 som förut. SPEED på ANALYZER-fliken är samma inställning som analyser/s och sparas nu. Ändringar slår igenom direkt i alla öppna fönster (ny [VisualTimer.h](../../Source/Ui/VisualTimer.h)), också REDUCE ANIMATIONS, som förut krävde att fönstret öppnades igen. | gjort |

Mätt med `tool uiprofile 7 heavy 10 8` (10 tunga instrument, ANALYZER-spektrumet på, meddelandetråden, samma tillfälle): 30 bilder/s
42 % av en kärna, 20 bilder/s (nya standarden) 40 %, 20 utan fyllning 35 %, 5 bilder/s 27 %, 40 bilder/s 47 %; spektrumet av 24 %.
Spektrumets egen kostnad blir alltså ~16 procentenheter i stället för ~18, ~11 utan fyllning och ~3 vid 5 bilder/s.
`KB_SPECTRUM_FPS` och `KB_SPECTRUM_FILL` ställer in det för mätningen (i en tillfällig inställningsfil).

### Fråga 1: kontrolltakten (gjord)

Varje DAW-block delas i bitar på ett rutnät av 32 samples räknat från `prepareToPlay` (`PluginBase::processInControlChunks`), och hela
blockbehandlingen körs per bit. Moduleringen, parameterläsningen och det som räknas per fönster uppdateras bara på rutnätet. Resultat:

- **Ljudet är bit för bit detsamma vid alla blockstorlekar**, också udda (37, 64, 100, 256, 512, 1024, 2048 mot 32) och i båda
  pluginsen. `blocksizetest` är nu ett test i sviten som fallerar vid minsta skillnad. Före: upp till -1,7 dBFS skillnad.
- Fel som det skarpa testet hittade och som är rättade: moduleringen och en hel parameterläsning gjordes också i bitar som började mitt i
  ett fönster; filtereffektens utjämnade cutoff stegades med bitens längd (5 + 27 samples gav andra värden än 32); auto-filtret räknade
  svepet i bitens mitt i stället för fönstrets.
- Ljudet mot förut (det riktiga Init, block 512, gammalt mot nytt Release): skillnad topp -22 dBFS, 53 dB under signalen. Snabba LFO:er
  och ENV 2/3 rör sig nu var 32:a sample i stället för en gång per block.
- CPU (Release, gammalt och nytt omväxlande): 1 instrument 0,013 -> 0,016 s per sekund ljud, 3 instrument 0,029 -> 0,033, 10 instrument
  0,095 -> 0,10 (ungefär 0,4 % av en kärna mer). För att komma dit, utan att ändra något tal: parametrar som inte kan moduleras läses en
  gång per DAW-block; mellan läsningarna läses bara de instrument och den kick som moduleras; `ParamRefs::get` har en trådsäker cache på
  ID-strängens adress; mixerns dB -> gain och effektrattarnas omräkning görs bara vid ändring.
- Nya baslinjer: `baseline-kod-kontrolltakt.txt`, `baseline-init-kontrolltakt.txt`, `baseline-wide-kontrolltakt.txt`.
- Ny guide för framtida arbete: [docs/ENGINEERING.md](../ENGINEERING.md).
