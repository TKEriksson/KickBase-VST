# Fas M: 10 LFO:er – spec

Skriven på branch `spec-m` (från content-r 64d6aa8). Endast kodläsning, inget byggt eller testat. Radnummer gäller content-r 64d6aa8 och kan glida om worker 1 committar i Source/.

## 0. Beslut som Opus måste ta först

1. **Kickbase FX**: `LfoPage`, `Modulator` och `Modulation::addParameters` delas av instrumentet och FX (FxEditor.h:43, FxProcessor.cpp:16, FxProcessor.cpp:79). Planen (rad ~167) säger "FX opåverkad om inget annat bestäms". Rekommendation: **FX behåller 3 LFO:er** (`maxLfos` blir en körtidsparameter på Modulator, FX anropar inte `addExtraLfoParameters`, FX-editorn visar inte "+ ADD LFO"). Då är params-fx.txt och FX-rendering orörda. Alternativ (10 i båda) ändrar FX-parameterlistan och dubblar testytan.
2. **Får LFO 1–3 tas bort?** Rekommendation: ja, alla 10 kan tas bort (enklast och enhetligast; ingen "minst en kvar"-regel). Default är 1–3 aktiva.
3. **Fabrikspreset som använder lfo1/2/3** (PresetData.inl:222, :238): vid laddning aktiveras alla LFO:er presetet har rutter/parametrar för (se 3.5).

## 1. Inventering: allt som antar 3 LFO:er

### Kärna
| Plats | Antagande |
|---|---|
| Modulation.h `numLfos = 3`, `firstMacro = numLfos`, `firstInstrumentSource = numLfos + numMacros` (=7), `numSources = 7 + maxInstruments*4` (=47) | källnummer 0–2 = LFO, 3–6 makro, 7+ instrument. **Sparas som tal i ModRoutes** |
| Modulation.h: `sourceOn`, `sourceValues` (`std::array<…, numSources>`), `sourceDisplay`, `phase`, `value`, `snapNote`, `snapLocked`, `phaseOffset`, `lfos[]`, `lfoCurves` | alla dimensionerade med `numLfos` |
| Modulation.cpp `addParameters` | lfo1..3 `{On,Shape,Rate,Mode,Hz}` + `{Snap,SnapRange}` i separat block, sedan macro1..4 |
| Modulation.cpp:254-361 `Modulator::update` | loop över `numLfos`: fas (phaseOffset/lastCyclesPerQuarter/lastPpq), SNAP, form (Custom via `lfoCurves[i].lookup`) |
| Modulation.cpp `sourceName` (`"LFO " + (source+1)`), `sourceColour` (`source<3`, `source<7`, `(source-7)/4`), `isLfo`-liknande jämförelser | hårdkodade trösklar |
| PluginBase.cpp:9-14 | `static_assert(Modulation::numLfos == 3 && … firstInstrumentSource == 7 …)` |
| PluginBase.cpp:32 | `modulator.setLfoCurves(&lfoCurves)` |
| PluginBase.cpp:537-539 / :620-630 | spara/ladda `modRoutes` och `LfoCurve1..N`; saknad kurva återställs |
| PluginBase.cpp:665-669 | fabrikspreset Section::full/lfo rensar rutter och återställer kurvor |
| PluginBase.cpp:821-825, :968-979 | användarpreset-XML: rutter + `LfoCurve`-barn |
| PluginBase.h:246 | `lfoCurves` initieras med exakt 3 `NodeEnvelope(1,1,sineCurve())` |
| InstrumentProcessor.cpp:15-34 `createLayout` | `Modulation::addParameters` mitt i layouten (efter Sidechain, före MasterSection/Mixer/addMasterParams); :149 anropar `update` |
| FxProcessor.cpp:14-17, :79 | samma, efter Sidechain före MasterSection |
| Presets.cpp:292-293 | `Section::lfo: id.startsWith("lfo")` |
| Presets.h:55 | kommentar "LFO 0-2" |
| PresetData.inl:222, :238 | fabrikspreset använder lfo1On/Shape/Rate |

### MCP (ToolLayer.cpp)
- :113-119 `modulationSource`: `route["lfo"]-1`, accepterar 1–3.
- :209 verktygsbeskrivning `set_modulation` ("lfo 1-3").
- :514-525 `get_state` curves-loop `for lfo<3`.
- :586 `lfo_routes`: `put(entry,"lfo",source+1)`.
- :905-975 `setModulation`: felet `"lfo must be 1-3"` (:919), problem-strängen (:968), sätter `paramID(index,…)` och `lfoCurves[index]`.
- Det finns **inget** `set_lfo`-verktyg; LFO:er sätts via `set_modulation.lfos`. Planens "set_lfo" = `set_modulation.lfos`.
- docs/AGENT_GUIDE.md:56-58, docs/work/preset-checklist.md:8 ska uppdateras.

### UI
- LfoPage.cpp:544-562: konstruktorn bygger `numLfos` `LfoPanel`; `resized` lägger ut horisontellt, bredd = (w − gap*(numLfos−1))/numLfos.
- LfoPanel (LfoPage.cpp ~415-540): form/mode/rate/hz/snap/display/handtag/mål. LfoTargets ~495-515 (högerklick tar bort rutter).
- Theme.h:43-47 `lfoColour(index)` har 3 färger, klampar på 2. Theme.h:49-59 `sourceColour`.
- Components.cpp:312 drop `"mod:<source>"` → `jlimit(0, numSources-1, source)`. :257, :267, :288 använder `sourceName`.
- MatrixPage.cpp:19-20 källmeny loopar `lfo<numLfos`; :283-287 makroknappar.
- Befintligt scrollmönster: `juce::Viewport` i MatrixPage.h:35 och MixerPage.h:71.
- LfoPage används av InstrumentEditor.h:47 och FxEditor.h:43.

### Övrigt
- MixAnalysis.cpp:1115 (`id.startsWith("lfo")||"macro"` → modulationsgrupp), :840, :2036 (`sourceName`; rutter fångas dynamiskt – ok).
- Help.cpp:55-61, :244-248 (hjälptexter per lfo1/2/3, `rawID.startsWith("lfo")`) – behöver lfo4–10.
- Tools/Scenarios.h:186-194 (rendersigwide: lfo1–3-former, `firstMacro`-rutt). **`firstMacro` måste förbli 3 – se 2.**
- Tools/Tool.cpp:502 (lfo1On), :913-931 (set_modulation lfo 2 + curves persist), :1509-1519 (KB_LFOCUSTOM).
- Tests/ModulationTests.cpp (~20 refs, lfo1-tester ~72-210), Tests/EngineTests.cpp:523-524, Tools/AudioTests.cpp:326.
- docs/audit/baseline-*.txt (hash-baser) – ska vara oförändrade.

## 2. Parametrar och källnummer

**Parametrar LFO 4–10**: `lfo{4..10}{On,Shape,Rate,Mode,Hz,Snap,SnapRange}`, samma typer/intervall/default som LFO 1–3 (On=true, Rate-index 8 "1 bar", Hz 1.0, Snap=false, SnapRange=5). 49 nya parametrar. Läggs i en **ny funktion `Modulation::addExtraLfoParameters(layout)`** som anropas **sist** i layouten (efter `Params::addMasterParams` i InstrumentProcessor.cpp:createLayout). LFO 1–3:s ordning/ID:n rörs inte. ENGINEERING.md:118 (nya parametrar sist), :114 (nya källor sist).

**Källnummer – viktigaste designbeslutet.** Att bara höja `numLfos` till 10 skjuter makron (3–6) och instrumentkällor (7+) → sparade projekt/presets bryts. Därför:
- `numLfos` (=3) och `firstMacro` (=3), `firstInstrumentSource` (=7) **förblir oförändrade** (gamla LFO 1–3 = källa 0–2).
- Ny konstant `maxLfos = 10`, `firstExtraLfoSource = firstInstrumentSource + maxInstruments*instrumentSources` (=47), `numSources` = 47 + 7 = **54**.
- Hjälpare: `lfoSource(i)` (i<3 → i, annars 47+i−3), `lfoIndexOfSource(s)` (−1 om ej LFO), `isLfoSource(s)`. Alla ställen som idag jämför `source < numLfos`/`source<3` byts mot dessa (sourceName, sourceColour, MatrixPage, ToolLayer, Components.cpp:312 jlimit-intervallet funkar redan).
- `static_assert` PluginBase.cpp:9-14 utökas: `firstExtraLfoSource == 47`, `numSources == 54`.
- Array-storlekar som är indexerade på LFO-index (phase, value, snapNote, snapLocked, phaseOffset, lfos, lfoCurves) blir `maxLfos`. Arrayer indexerade på källa (`sourceValues`, `sourceOn`) blir `numSources`. Loop i `update` går 0..maxLfos−1 och skriver till `sourceValues[lfoSource(i)]`.

## 3. Aktiv-läge

### 3.1 Sparande
Ett heltal `activeLfos` (bitmask, bit i = LFO i+1) som **property på state-trädet** (som `previewNote`/`layerName`; ej automatiserbar parameter). Saknas den vid laddning ⇒ `0b111` (gamla projekt identiska). I användarpreset-XML ett rotattribut `activeLfos` med samma regel. `Modulator` har `std::atomic<uint32_t> activeMask` (default 7); `PluginBase` skriver den vid set/load. UI-trådens och MCP-trådens ändringar går via en funktion `PluginBase::setLfoActive(int index, bool)`.

### 3.2 0 CPU för inaktiva
I `Modulator::update` läses masken **en gång** till en lokal (LESSONS R7). Loopen: `if (!(mask & (1u<<i))) { sourceOn[lfoSource(i)] = false; sourceValues[…] = 0; value[i]=0; phase[i]=0; snapNote[i]=-1; snapLocked[i]=false; phaseOffset-fälten nollställs; continue; }`. Ingen form-/fasberäkning. Aktiva LFO:er går exakt samma kod som idag. Matrisapplikationen itererar rutter (inaktiva har inga) så den påverkas inte.

Obs: skilj från befintliga `lfoNOn` (automatiserbar på/av) – oförändrad och orelaterad.

### 3.3 Aktivera
`setLfoActive(i,true)`: bara `activeMask.fetch_or(1u<<i)`. Den får **inte** skriva `phaseOffset`/`lastCyclesPerQuarter`/`lastPpq` från message-/MCP-tråden: de är vanliga `double` som ägs av ljudtråden (datarace). Inaktiva-grenen i `update()` nollställer dem redan varje block, så fasen startar rent ändå. Parametrarna har redan default.

### 3.4 Ta bort (inaktivera)
`setLfoActive(i,false)`, i denna ordning: (1) UI: om LFO:n har rutter → bekräftelsedialog ("LFO 5 styr N rattar. Ta bort ändå?"), MCP: tar bort utan dialog men rapporterar antal borttagna rutter; (2) ta bort alla rutter med den källan med `ModRoutes::remove` (Modulation.cpp:155-163); (3) återställ lfoN-parametrarna till default via `Presets::setParameter` (message-tråd); (4) återställ `lfoCurves[i]` till sinus; (5) **sist** `activeMask.fetch_and(~(1u<<i))`. Ingen tråd skriver ljudtrådens `double`-fält (se 3.3).

Mekanismen mot ljudtråden för rutter är densamma som idag: `ModRoutes::routes` är en fast array av atomer utan lås; `remove` sätter `source=-1` först (så kopplingen slutar verka direkt), sedan target/amount, och `add` sätter `source` sist (Modulation.cpp:140-163). `update()` läser `source` atomiskt per rutt (Modulation.cpp:346-356) och hoppar över `<0`. Ingen swap/kopia av arrayen behövs.

### 3.5 Laddning
`apvts.replaceState` återställer inte frånvarande parametrar. Därför: vid state-/presetladdning, efter `replaceState`, för varje LFO som är inaktiv i det inladdade: återställ dess parametrar+kurva till default. För fabrikspreset (Section::full/lfo, Presets.cpp:292): bitmasken sätts till `7 | (LFO:er som presetet rör)`; LFO 4–10 som presetet inte rör avaktiveras (fullständig sektion återställer även dem – `startsWith("lfo")` täcker lfo4–10 automatiskt).

### 3.6 Kurvor
State: `LfoCurve1..3` skrivs som idag; `LfoCurve4..10` skrivs endast för aktiva (eller alla med icke-default kurva). Laddning: saknad kurva → default (som idag, PluginBase.cpp:620-630). Gamla filer har bara 1–3 ⇒ identiskt.

## 4. UI

- LFO-fliken: `juce::Viewport` (mönster MatrixPage.h:35) med vertikalt scroll. En kort-komponent per aktiv LFO, staplade lodrätt (idag horisontellt, LfoPage.cpp:544-562). Ordning: efter LFO-nummer.
- **Hopfällbart kort**: hopfälld rad = namn, minikurva, rate, DRAG-handtag, pil. Utfällt = dagens LfoPanel. Hopfällt-läge sparas i UI-state (ny int-bitmask bredvid `selectedTab`, PluginBase.h:319; sparas som state-property `lfoFolded`, påverkar inte ljud). Default: utfällda LFO 1–3? (förslag: LFO 1 utfälld, övriga hopfällda när >3 aktiva).
- **"+ ADD LFO"**-knapp under sista kortet; lägger till lägsta lediga index, disabled vid 10 aktiva. Rensar default (3.3).
- **Ta bort**: × i kortets rubrik + högerklick → 3.4.
- **"Visa bara vald"**: toggle överst; visar bara det kort man senast klickade/valde, övriga göms (UI-state, ej sparat i ljud).
- `Theme::lfoColour(index)` utökas till 10 färger (index klampar vid 9); `sourceColour` använder `lfoIndexOfSource`.
- **MATRIX/DRAG/MCP-listor** visar bara aktiva LFO:er: MatrixPage.cpp:19-20 källmenyn loopar aktiva index och använder `lfoSource(i)` som menu-ID/värde (obs `menu.addItem(1+lfo…)`: ID-schemat måste ändras till källa+1). DRAG-handtag finns bara i synliga kort.
- Fliken måste lyssna på `activeLfos` (ChangeBroadcaster/timer som MatrixPage) så MCP-ändringar syns.
- FX-editorn: se beslut 1.

## 5. MCP

- `modulationSource` (ToolLayer.cpp:113-119): `lfo` 1–10 → `lfoSource(lfo-1)`; ger fel om LFO:n är inaktiv? **Nej**: att sätta rutt eller `lfos[]` på en inaktiv LFO **aktiverar** den (planen).
- `set_modulation.lfos[].lfo` 1–10 (felet :919 → "lfo must be 1-10"), `routes[].lfo` 1–10. Parameter-ID: `paramID(index,…)` ska ge `lfo{index+1}…` för alla; kurvor `lfoCurves[index]`.
- Ny valfri `remove: true` på `lfos[]` (ta bort LFO, ger `removed_routes: N`) – så MCP kan avaktivera.
- `get_state`: `curves` (:514-525) och `lfo_routes` (:586, `lfoIndexOfSource(source)+1`) listar bara aktiva; ny nyckel `active_lfos: [1,2,3,…]`.
- Verktygsbeskrivning :209, docs/AGENT_GUIDE.md:56-58, docs/work/preset-checklist.md:8, Help.cpp:55-61/:244-248 uppdateras.
- MCP kallas från annan tråd än audio; ändringar går via `setLfoActive` (atomisk mask + message-trådsarbete för parametrar – följ hur `setModulation` gör idag).

## 6. Bit-identity (rendersig oförändrad med LFO 1–3)

- Källnummer 0–46 oförändrade ⇒ befintliga rutter/Scenarios (`firstMacro`) identiska.
- `Modulator::update`: ingen omskrivning av fas-/formuttrycken för i<3; endast `continue` för inaktiva och indexmappning till `sourceValues`. Behåll float/double-former exakt; ändra inte beräkningsordning. `sourceValues` är större (54) men iterationsordningen över rutter oförändrad.
- Källloopar i ljudtråden: verifiera i steg 1 att ingen loop över `numSources` gör något källberoende som ändrar ordning eller summering. Granskat hittills i `Modulation.cpp`: :335 kopierar bara `sourceValues`→`sourceDisplay` (ordningsoberoende), :346 summerar per rutt i ruttordning (inte källordning). `MixAnalysis`/`Mixer` har egna, orelaterade `numSources` (mixerkanaler). Resten kontrolleras med grep före första ändringen.
- Parameter-ID-hashar: LFO 1–3-ID:n oförändrade. Nya ID:n sist ⇒ params-*.txt får bara tillägg i slutet (rendersig läser parametrar via ID, inte via ordning – verifiera).
- `rendersigwide` sätter `lfo1–3`-former; måste ge samma hash.
- Default activeMask=7 ⇒ varje gammal rendering har exakt samma aktiva LFO:er.
- Ingen ny ljudtrådsallokering: alla arrayer är fixstorlek.

## 7. Testlista

Befintliga (ska vara oförändrade): `rendersig`, `rendersigreal`, `rendersigwide`, `rendersigtb303`, `blocksizetest`, `audiotest`, `persisttest`, ModulationTests (lfo1-tester), KickbaseTests.

Nya (Tests/ModulationTests.cpp + Tools):
1. activeMask default 7; gammalt state utan `activeLfos` laddas = 7.
2. LFO 4 aktiv ger värden; inaktiv ger `sourceOn=false`, `value=0` och rör inte `sourceValues`.
3. LFO 4–10 form/rate/mode/hz/snap-tester (spegla lfo1-tester :72-210), inkl. Custom-kurva.
4. Ta bort LFO med rutter: rutter borta, parametrar default, kurva sinus. Ta bort utan rutter.
5. State round-trip (persisttest): lägg till 5 och 9, rutter från båda, spara, ladda, lika; ta bort 5, spara, ladda; användarpreset round-trip med `activeLfos`.
6. Källnummer: `lfoSource`/`lfoIndexOfSource` bijektiva; `sourceName(47)=="LFO 4"`; gammal rutt på källa 3 = makro 1.
7. Fabrikspreset med lfo1 laddas med LFO 1 aktiv även om den var borttagen.
8. MCP: `set_modulation` på lfo 8 aktiverar; `lfo 11` ger fel; `get_state` listar bara aktiva; remove ger `removed_routes`.
9. `perftest` med 10 aktiva LFO:er (jämför mot 3; budget i LESSONS) och 3 aktiva + 7 inaktiva (ska ≈ som idag).
10. `blocksizetest` med LFO 4–10 aktiva (chunkgränser/ppq-kontinuitet för de nya).
11. Parameterlista: params-instrument.txt får endast tillägg sist; params-fx.txt oförändrad (om FX klarar sig med 3).
13. Aktivera/inaktivera LFO 4–10 medan rendering pågår (blocksizetest-variant, växla t.ex. varje block): inga krascher, och LFO 1–3 bit-identiska oavsett vad som händer med LFO 4–10.
12. Mänsklig UI-test (Tobias): add/remove/fold/scroll/show-only-selected, DRAG från hopfälld rad.

Testregler enligt LESSONS: rendersig-trion + blocksizetest + audiotest en gång per delsteg som rör Modulation; persisttest när state ändras; hela sviten bara vid fasens slut.

## 8. Arbetsdelning (4 steg, ett i taget, eget commit)

1. **Modulation-kärnan**: `maxLfos`, `lfoSource`-hjälpare, `numSources=54`, arrayer, `addExtraLfoParameters` (anropas sist i instrument-layouten), `activeMask` + `continue` i `update`, static_asserts, ModulationTests 1–3, 6, 9–11. *Ljudkritiskt: rendersig-trio + blocksizetest + audiotest + perftest.*
2. **State och presets**: `activeLfos`-property, `LfoCurve4..10`, `setLfoActive` (aktivera/ta bort inkl. rutter/återställning), laddning 3.5, Presets.cpp Section::lfo, fabrikspreset-regeln. persisttest + test 4, 5, 7.
3. **MCP**: ToolLayer-ändringarna (sektion 5), `active_lfos`, `remove`, AGENT_GUIDE, preset-checklist, Help. Test 8.
4. **UI**: LfoPage med Viewport, hopfällbara kort, ADD/×/bekräftelse, visa bara vald, Theme-färger, MatrixPage-meny + DRAG-lista, UI-state. Mänsklig test 12. (Kan delas i 4a layout/kort och 4b ADD/ta bort/matrix om stort.)

## 9. De fem största riskerna

1. **Källnummer-skift** bryter sparade projekt/presets om LFO 4–10 inte läggs sist (47–53). Alla `source<3`/`numLfos`-jämförelser måste hittas (sourceName, sourceColour, MatrixPage menu-ID `1+lfo`, ToolLayer, Components).
2. **Bit-identity i `Modulator::update`**: varje omskrivning av LFO 1–3-loopen kan ändra rendersig (float/double, ordning, atomics lästa flera gånger). Håll ändringen till `continue` + indexmappning.
3. **Parameterlayout/FX**: nya parametrar måste sist i layouten; FX delar `Modulation::addParameters` och `LfoPage`. Fel beslut ändrar FX-parameterlistan och DAW-automation.
4. **State-migrering och rester**: `replaceState` återställer inte frånvarande parametrar ⇒ borttagna LFO:er kan "spöka" (parametrar/kurvor/rutter) efter preset/laddning; fabrikspreset som använder lfo1 när LFO 1 är borttagen.
5. **Trådsäkerhet och UI-komplexitet**: UI/MCP/audio rör `activeMask`, rutter och parametrar samtidigt; Viewport + hopfällbara kort + omlayout av ett redan komplext LfoPage; hårdkodade 3:or i MixAnalysis/Help/Tests missas.
