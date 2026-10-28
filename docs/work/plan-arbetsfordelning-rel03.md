# Arbetsfördelning rel-0.3 (projektledaren, 28 okt 2026)

Regler för båda: nya parametrar sist, ID/preset-index/hash oförändrade, standard av, aldrig push, committa ofta, logga i egen PROGRESS-fil, `build.bat Debug all` grönt före commit, "DAW" (inte FL) i testrader. Rapportera bara när klar eller blockerad. Mergen till main gör bara projektledaren, efter Tobias test och ja.

En enda release (Tobias 28 okt): allt som tidigare låg i rel-0.4 ingår nu i rel-0.3, inget mellanbygge. Arbetsordningen nedan gäller: tidiga punkter först, så att en testbar bas finns tidigt. Releasen kan delas först om Tobias begär det.

## Arbetare 1 (UI och motor, branch play-pack)
### Först
1. Slutför multisampling (ocommittade ändringar i `MultiSample.*`, SFZ-kärnan som egen commit), committa grönt.
2. ANALYZER-mätningarna (`plan-v2.md:258-271`): kontrollera vilka som finns, komplettera, test.
3. FUNDAMENTAL DUCK-visning i motorns vågvisning och ANALYZER (ljudtråd till UI via atomics/FIFO).
4. Broms B (BRAKE trigger, godkänd 27 okt): kontrollera om den redan är byggd, annars bygg. Sedan JOG-resten ur rel-0.3-listan.
5. Kvarvarande nav/preset-punkter (kick SYNTH/SAMPLE MIX, TOP DECAY, FX multiselect, ghost notes, pop-out, glide-plus, SOUNDSTAGE).
### Därefter (planer först)
6. Övertonsredigerare, Serum-stil, etapp 1-3.
7. Sample-editor (AMP/PITCH, på/av), klick-lager (3) och sampleverktyg, reverse per not, parameterlanes under pianorollen (resizebar).
8. RING, Echo/Flanger och autosave/IO (Tobias 28 okt: lägg in i planen): plan först, sedan bygge. Nya parametrar sist, standard av.
9. AUTO FIX sist.

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
