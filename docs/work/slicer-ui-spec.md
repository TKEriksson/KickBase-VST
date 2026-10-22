# Slicer-UI: Tobias önskemål (2026-10-21)

Underlag: två skärmbilder av SAMPLE-fliken (instrument 1, sample "GOROVICH 1 ref"). Nuvarande vy visar vågformen, och i slice-läge siffror ovanför, men man ser inte tydligt slicen och knapparna saknar etiketter.

## Önskat

1. **SHOW: SAMPLE | SLICES** (växlare ovanför vågformen).
   - SAMPLE = hela samplet som nu.
   - SLICES = samma våg uppdelad: varje slice är en egen ruta/färgad region med tydliga gränser, titel "Slice 1, 2, 3 …" och tilldelad ton under sig.
   - Klick på en slice spelar bara den slicen (förhandslyssning, samma väg som HEAR). Då ser/hör man hur olika inställningar låter per slice.
2. **Dra i slicegränserna** för att flytta var en slice delar sig (manuellt). Kräver zoom in/ut (rullhjul/knappar) och scroll i vågformen.
3. **Lägen: AUTO SLICE | MANUAL**.
   - AUTO SLICE: alla dagens kontroller (knappar/knoppar/slidern med MAP TO, Auto, 0.62 osv.) hör hit och får **etiketter** (idag saknas de, det är otydligt vad de gör). Helst en dialog/panel med inställningarna och en **CREATE**-knapp som kör autoslicen. Efter CREATE kan man redigera resultatet (flytta/ta bort/lägga till gränser).
   - MANUAL: klicka i vågformen för att lägga en delning där man vill, dra för att flytta, högerklick/Delete för att ta bort.
4. **Namn och ton per slice**: slice 1, 2, 3 … Under varje slice står vilken ton den får tilldelad. Efter att samplet slicats får varje slice sin **egen pianorulle** (rad), och pianorullen visar samma namn ("Slice 3"), så man hittar rätt.
5. Alla knappar/rattar i fliken ska ha en synlig etikett och tooltip (helpcheck ska vara grön).

## Anmärkningar för projektledaren
- Beror på hur slice → ton → pianorulle fungerar i kärnan (Arbetare 2 vet; docs/work/m-spec.md + PROGRESS-9a.md). Om "en egen pianorulle per slice" kräver ny kärnstruktur: föreslå uppdelning i steg (1. SHOW-växlare + klick-för-att-lyssna + namn/ton under slice, 2. drag av gränser + zoom, 3. AUTO/MANUAL + CREATE-dialog, 4. pianorulle per slice).
- Nya parametrar sist (docs/ENGINEERING.md). Rendersig-baselines måste vara oförändrade.
- Vänta med bygget till release-int är grön (CI 26 pågår); arbeta i egen worktree.

## Tillägg (Tobias, samma dag)
- Stort bygge: dela upp, bara steg 1 nu, resten senare (efter release och LC). Men viktigt, eftersom slicern idag är obegriplig.
- MAP TO mappar idag bara slicerna till toner i **samma** pianorulle. Lägg en kryssruta, t.ex. "OWN PIANO ROLL" (av = som idag): när den är på får varje slice en egen pianorulle/rad ("Slice 1, 2 …").
- Ordning: 1) etiketter/tooltips + SHOW SAMPLE|SLICES med synliga slices + klick-för-att-lyssna. 2) Kryssruta OWN PIANO ROLL (kärnstruktur). 3) Dra gränser + zoom, MANUAL. 4) AUTO SLICE-dialog med CREATE.
