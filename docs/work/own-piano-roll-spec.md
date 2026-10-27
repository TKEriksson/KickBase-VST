# E2 steg 2: egen pianorulle per skiva ("Slice N") – datamodell

Beslut (PL/Tobias 27 okt): en rad per skiva i samma mönster, inga nya ljudparametrar, ingen migrering, gamla modellen (SLICER-rad, MAP TO + index) tas bort, undo via UndoManager, inga ändrade parameter-ID:n.

## Läget idag (fil:rad)
- Underrader (`Kind::sample`, `Kind::slicer`) har bara längd och loop: `Params.h:188-197`. `StepScheduler::collect` går bara över instrumentens NOTE-rader och kicken (`StepScheduler.cpp:10-23`), så underradernas noter spelas inte.
- Slicern spelas via notens tonhöjd: `index = semitones - slicerMapTo` (`InstrumentSynth.cpp:172-184`). Skivorna ligger i `sampleData->slices`, högst 128.
- RowRegistry: pool om 64 id, underrader från id 11, slot `(instrument<<4)|kind` (`RowRegistry.h:191`).

## Ny modell
1. **Radsort** `Kind::slice = 3` (ersätter `Kind::slicer`, `Kind::sample` lämnas orörd). Slot: `(skiva<<12)|(instrument<<4)|kind`. Nyckel `i3.slice1` (1-baserad, samma som namnet i gränssnittet). Visningsnamn "Slice N". `removeInstrument` jämför instrumentet ur bitarna 4-11.
2. **Tak**: poolen är 64 (53 underrader). Raderna skapas för skiva 1..N där N = `min(slices.size(), 16)`; skivor över 16 saknar rad och är tysta (tidigare tak 128 via MAP TO). Vill PL ha fler: höj `maxRows`, men det ändrar lagringens storlek (pointers per rad x 1024 takter).
3. **Ljud (ny ljudtrådskod)**: `StepScheduler` får en händelsetyp `slice` (instrument, skivindex). I `collect`/`collectJustPassed` går den även över aktiva rader av sort `slice` (ur `rows`, bara atomläsning), med radens egen längd/loop (`lengths.barsOf(lengthRowOf(id))`) och `pattern.note(id, tick).on` som trigger; tonhöjden i raden används inte. `InstrumentProcessor` triggar instrumentets röst med skivindex i stället för `semitones - mapTo`. `InstrumentSynth::trigger` får ett skivargument (−1 = ingen skiva); ljudet för en vald skiva är identiskt med dagens (`sliceBegin/sliceEnd/samplePosition/slicerReverse` oförändrade).
4. **Skapa/ta bort rader (meddelandetråden)**: när samplet slicas (CREATE/auto/manuellt) och antalet skivor ändras synkas raderna: lägg till saknade, ta bort överflödiga (noterna i borttagna rader försvinner, som idag vid borttagen underrad). Knappen SLICER (på/av) försvinner: skivrader finns så länge samplet har skivor och instrumentets sample-läge är på.
5. **Gammal modell borttagen**: `Kind::slicer`, `slicerOn`/`slicerMapTo`-UI (`Modules.cpp:1222,1496,1534,1544`), `InstrumentSynth.cpp:172-184` ersätts, `Help.cpp:244`, MCP-argumenten `slicer`/`map_to` (`ToolLayer.cpp:2550`), `RollFiles.cpp:171`, `StepSequencer.cpp:1068`, testerna `EngineTests.cpp:934`, `StateTests.cpp:221-244`.
6. **Parameter-ID:n**: `slicerOn` och `slicerMapTo` ligger kvar i enum och layout (`Params.h:116`, `Params.cpp:415`) men läses inte; markeras "oanvänd, ID kvar". `slicerReverse` används som förut. Inga nya parametrar.
7. **Pianorulle**: en slice-rad är en enkel rad (en ton, bara "på/av" per steg, som en trigger-rad), med namnet "Slice N" i radlistan och i pianorullens rubrik. Alla ändringar går redan genom `Pattern`-API:t som UndoManager lyssnar på; de nya radoperationerna (4) görs i en undo-transaktion ("Slice rows") så att Ctrl+Z tar tillbaka både raderna och noterna.
8. **OWN PIANO ROLL-kryssrutan** blir överflödig (Tobias: gamla modellen bort): ingen ruta, skivrader finns alltid när samplet har skivor.

## Test
- Ljudtrådskod ändras: rendersig-trion + blocksizetest + audiotest. Rendersig-baslinjerna har ingen slicer i mönstret, så de ska vara oförändrade.
- Nytt test (EngineTests): en not i raden "Slice 3" ger samma ljud som dagens MAP TO-väg för skiva 3 (render jämförs mot en referens från `trigger` med skivindex 2).
- persisttest för raderna (sparas/laddas via befintligt R9b-format), undotest för rad + not.

## Filer att radera (gamla slicer-formatet)
Genomgång (27 okt): inga versionshanterade projekt-, preset- eller testfiler använder den gamla slicermodellen (`git ls-files` har inga .kbproj/.kbpreset, bara `docs/work/slicer-ui-spec.md` som är en spec och behålls). Ingenting `git rm`-ades. Två utdaterade docrader rättades: `docs/live-code-exempel.md:230`, `docs/synth-och-livecode-oversikt.md:29`.
