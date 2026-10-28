# Arbetsfördelning rel-0.3 (projektledaren, 28 okt 2026)

Regler för båda: nya parametrar sist, ID/preset-index/hash oförändrade, standard av, aldrig push, committa ofta, logga i egen PROGRESS-fil, `build.bat Debug all` grönt före commit, "DAW" (inte FL) i testrader. Rapportera bara när klar eller blockerad. Mergen till main gör bara projektledaren, efter Tobias test och ja.

En enda release (Tobias 28 okt): allt som tidigare låg i rel-0.4 ingår nu i rel-0.3, inget mellanbygge. Arbetsordningen nedan gäller: tidiga punkter först, så att en testbar bas finns tidigt. Releasen kan delas först om Tobias begär det.

## Arbetare 1 (UI och motor, branch play-pack)
### Först
1. Slutför multisampling (ocommittade ändringar i `MultiSample.*`, SFZ-kärnan som egen commit), committa grönt.
2. ANALYZER-mätningarna (`plan-v2.md:258-271`): kontrollera vilka som finns, komplettera, test.
3. FUNDAMENTAL DUCK-visning i motorns vågvisning och ANALYZER (ljudtråd till UI via atomics/FIFO).
4. Broms B (BRAKE trigger, godkänd 27 okt): kontrollera om den redan är byggd, annars bygg. Sedan JOG-resten ur rel-0.3-listan.
5. Kvarvarande nav/preset-punkter (kick SYNTH/SAMPLE MIX, TOP DECAY (= kickNoiseDecay, intervall 5–400 ms utökas), FX multiselect, ghost notes, pop-out, glide-plus). Köordning (28 okt): färgväljare + TOP DECAY, LIVE CODE autocomplete (fungerar inte idag), SOUNDSTAGE med bussuppdelning och flerval av källor (Tobias vill ha det nu; plan först), gemensam MD-hjälpdialog (HJÄLP-knappar: pianorullen och LIVE CODE först, sedan en flik/motor i taget; färre notstips), resten.
5b. START per motor i MIX-fliken (Tobias 28 okt): tidsförskjutning i ms per motor (osc 1-3, sample, noise) så att lager kan komma in efter varandra. Skevt intervall 0-4000 ms, standard 0 (gamla presets oförändrade), nya parametrar sist. Ska även kunna synkas till takten (börja efter t.ex. 1 takt, steg i beats/takter). Plan först; fråga om max ska vara inställbart. Tillägg (Tobias 28 okt): varje motor ska även ha egen längd och kurva på sin envelope, så att start, längd och kurva för hela ljudet går att bestämma per motor (utred vad som redan finns i ENV-fliken och i kickens amp-env). MED I rel-0.3 (Tobias 28 okt).
### Därefter (planer först)
6. Övertonsredigerare, Serum-stil, etapp 1-3.
7. Sample-editor (AMP/PITCH, på/av), klick-lager (3) och sampleverktyg, reverse per not, parameterlanes under pianorollen (resizebar).
8. RING, Echo/Flanger och autosave/IO (Tobias 28 okt: lägg in i planen): plan först, sedan bygge. Nya parametrar sist, standard av.
9. AUTO FIX hör till rel-0.4 (Tobias 28 okt); görs inte nu. Mätkärnan som Arbetare 2 redan byggt får ligga kvar.

## Arbetare 2 (preset och verktyg, branch w2)
### Först
1. Merga play-pack i w2 (klart), bygg grönt.
2. Slå på FUNDAMENTAL DUCK i basar som krockar med kicken (PSY ROLLING BASS, GLIDE BASSES m.fl.), mät före/efter med audit, räkna om rendersig med avsikt, recept i `docs/school/`.
3. SFZ i KickbaseTool (multisampling etapp 4): merga Arbetare 1:s SFZ-commit, lägg bara kommando + tester.
4. Slutför pågående presets (`task-w2-kick-rollbas.md`: Psy Clean-kickar, PSY ROLLING BASS, PSY ROLLING, GLIDE BASSES).
5. New Init 1-5 med fabriks-Init som bas.
### Därefter
6. Resterande presets: SYNTH, MID BASS, HI BASS och genrer (MINIMAL, GOA, DARK PSY, DANCE, TRANCE, TECHNO, EMPIRIKAL), max 3 per kategori. Mer i taget om tiden medger det i rel-0.3.
7. Fas J: preset-skola i `docs/school/`, därefter skill för AI-presetbyggare.

## Gränser
Arbetare 2 rör inte UI. Arbetare 1 rör inte `PresetData.inl` och KickbaseTool utan att säga till.

## Projektledaren
- rel-0.3-build (allt ovan), testlista (PDF i `docs/releases/`) med egen sektion för hörtester som saknas i DAW: MIX-fliken, AP/START, HALFTIME, KEEP/COMPARE, shimmer, Maximizer MODE, Low Multiplier, autocomplete. Tobias testar i DAW, fixar, merge först efter ja.
- Skjuts till senare: multiband och sidechain i två band som FX-slot, AI-leverantörer.

## Tillägg 28 okt (Tobias: allt från 0.4 in i 0.3)
- FX send-matris + 12 FX-kanaler + M-knapp per kanal: `docs/work/plan-fx-send-matrix.md`. Arbetare 2 bygger kärnan (steg 1-3, branch fx-matrix, ingen UI); popup-UI tas av Arbetare 1 när kärnan är mergad, eller av Arbetare 2 om Arbetare 1 är full.
- Huvudpreset-kategorier ("Gamla" factory + user, egna kategorier vid spara, kategori lagras separat, preset-index/hash oförändrade): projektledaren, i `KickBase-VST-pl`.
- TIMING-fliken per motor (DELAY MODE, LENGTH SYNC, ×-antal 1-8, TO BEAT), kick-UI, semiton-snap, glasdisplay, glide-bugg, DETUNE-standard 0, JOG-förenkling (ETT läge, STEP 1/4-1/64, loop i 1/64 takt, TEST BPM): Arbetare 1.
- Rendersig-jämförelse mot main behövs inte (Tobias bygger nya presets).
- USER är en egen kategori i alla presetväljare (Tobias 28 okt), vid sidan av factory-kategorier och "Gamla".

## Läge 28 okt kväll (projektledaren)
- AUTO FIX stannar i rel-0.4 (Tobias). USER-kategori klar (42fd650 i `pl-presets`).
- Harmonics-editorn v2, etapp 1-3 klara i `pl-presets` (b3ddf32, 675213c, 5557277): större fönster och levande vågform, zoom och penslar, presets.
- Parameterlås: data (Arbetare 2, 807486b), ljud (Arbetare 2, 94ce1f6), UI och test (Arbetare 1, 42b093f, 096abb9). Klart.
- Grenkedja: jog-simple → fx-echo-steps → slice-roll (innehåller fx-matrix). Slås ihop med `pl-presets` och Arbetare 2:s fix före release.
- Blockerar rel-0.3: blocksizetest slumpfel (fanns före dessa ändringar, även på 57ddb1a). Orsak ej funnen; Arbetare 2 och 1 kör 20 körningar vardera. Opus bara efter besked från Tobias.
