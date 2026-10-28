# Plan: slicer-pianorulle (Arbetare 1, 28 okt)

Mål: skivorna får EN egen pianorulle där tangenterna är skivor ("Slice 1, 2 …"). En not på raden Slice N spelar skiva N. Samma PianoRoll och verktyg, bara tangentaxeln byts.

## Dagens modell
- En underrad per skiva: `RowRegistry::Kind::slice`, högst 16 (`RowRegistry.h:25`), `PluginBase::syncSliceRows` (`PluginBase.cpp:1928`). Not på raden = spela skivan (`StepScheduler.cpp:78-90`, `InstrumentProcessor.cpp:1109-1142`).
- PianoRoll är bunden till EN mönsterrad (`layer`), noter + staplar (`maxStack` 4) via `liveNote/readNotes/applyNotes` (`PianoRoll.cpp:1026-1207`).

## Datamodell (förslag)
- EN rad per instrument, ny `Kind::slices` (nyckel `i3.slices`, nytt radid i registret, sparas som övriga underrader). Not: `pitch` = skivindex 0..127 (rullen klämmer till 0..127 i skivläge, inte ±96), `length/velocity/slide` som vanligt. Samtidiga skivor på samma tick = staplar (upp till 4).
- Raden följer instrumentets markör som dagens skivrader (egen längd/loop).
- Gamla projekt: vid laddning flyttas noter från gamla `i3.sliceN`-rader in i `i3.slices` (pitch = N-1; fler än 4 på samma tick kasseras), gamla raderna tas bort. Test: gammal sparad slice-rad -> rätt not i nya raden.
- Inga parameter-id/presetindex ändras (bara mönsterträdet får en rad-nyckel, ingen version bump behövs: okänd nyckel ignoreras av äldre, gamla nycklar läses av nya).
- Ljud: StepScheduler skickar en `slice`-händelse per stapelnot i raden; `InstrumentProcessor` spelar skiva = pitch.

## Rullen
- `PianoRoll` får `sliceMode` (sätts när lagret är en `Kind::slices`-rad): tangentaxeln ritas som "Slice N" (färg som i samplevyn), bara de skivor som finns visas (≤128), skrollning/zoom som vanligt, ingen svart/vit-tangent, ingen skala/ackord/slide; `auditionStart` spelar skivan (`previewSlice`). Pennan/markera/sudda/flytta/kopiera/undo/snap oförändrade.
- Öppnas: (a) klick på radens miniatyr i mönstret, (b) knapp "ROLL" i SAMPLE/SLICER-sektionen (`Modules.cpp` ~1368).

## Etapper
1. Registry `Kind::slices` + syncSliceRows skapar en rad + migrering + scheduler/processor (tester a och b). Commit.
2. PianoRoll skivläge (tangentaxel, audition, tooltip/hjälp). Commit tidigt (Arbetare 2 bygger lanes ovanpå).
3. Mönster-UI: ta bort Slice N-raderna, en "Slices"-rad som öppnar rullen; knapp i SAMPLE-sektionen; snapshot till Tobias.
