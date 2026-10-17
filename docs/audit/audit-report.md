# Granskning av Kickbase: rapport (första körningen)

Gren `audit` (från `dev` 08dedcf). Skrivs löpande. Allvarlighet: **krasch**, **fel ljud**, **data** (något går förlorat eller sparas fel),
**prestanda**, **förvirrande**, **kosmetiskt**. Status: *rättad (commit)*, *fråga N* (se [questions-for-tobias.md](questions-for-tobias.md)),
*föreslagen*, *ok* (granskat, inget fel).

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
| Ljudtråden, tungt läge med 10 instrument | 58 % av en kärna (RelWithDebInfo). Största posterna: JUCE:s IIR-filter (EQ:erna) ~20 %, oscillatorerna och wavetables ~17 %, effektkanalerna ~10 %, filtrens koefficientomräkning (`tan`) 5–15 %. | | se avsnitt 3b (fortsätter) |

## 4. Texter, namn och dokumentation

| Var | Vad | Allvar | Status |
|---|---|---|---|
| [Params.cpp:141, 214](../../Source/Core/Params.cpp) | Två parametrar hette "Kick Length" (`rowBarsKick`, `kickLength`) och två "Instr N Length" (`rowBarsBassN`, `bassNGate`) i DAW:ens automationslista. Nu "Kick Row Length", "Instr N Row Length", "Instr N Note Length". ID:n orörda. | förvirrande | rättad f2272d1 |
