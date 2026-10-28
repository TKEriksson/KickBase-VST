# Arbetsfördelning rel-0.3 (projektledaren, 28 okt 2026)

Regler för båda: nya parametrar sist, ID/preset-index/hash oförändrade, standard av, aldrig push, committa ofta, logga i egen PROGRESS-fil, `build.bat Debug all` grönt före commit, "DAW" (inte FL) i testrader. Rapportera bara när klar eller blockerad. Mergen till main gör bara projektledaren, efter Tobias test och ja.

## Arbetare 1 (UI och motor, branch play-pack)
1. Slutför multisampling (ocommittade ändringar i `MultiSample.*`), committa grönt.
2. ANALYZER-mätningarna (`plan-v2.md:258-271`): kontrollera vilka som finns, komplettera, test.
3. FUNDAMENTAL DUCK-visning i motorns vågvisning och ANALYZER (ljudtråd till UI via atomics/FIFO).
4. Övertonsredigerare, Serum-stil, etapp 1-3.
5. Planer, sedan bygge: sample-editor (AMP/PITCH, på/av), klick-lager (3) och sampleverktyg, reverse per not, parameterlanes under pianorollen (resizebar).
6. Kvarvarande nav/preset-punkter ur masterplanens rel-0.3-lista (kick SYNTH/SAMPLE MIX, TOP DECAY, FX multiselect, ghost notes, pop-out, glide-plus, SOUNDSTAGE).
7. AUTO FIX sist (troligen rel-0.4).

## Arbetare 2 (preset och verktyg, branch w2)
1. Merga play-pack i w2, bygg grönt.
2. SFZ-delmängd i KickbaseTool (multisampling etapp 4) + tester.
3. Slå på FUNDAMENTAL DUCK i basar som krockar med kicken (PSY ROLLING BASS, GLIDE BASSES m.fl.), mät före/efter med audit, räkna om rendersig med avsikt, recept i `docs/school/`.
4. Slutför pågående presets (`task-w2-kick-rollbas.md`), max 3 per återstående kategori.
5. New Init 1-5 med fabriks-Init som bas.

## Gränser
Arbetare 2 rör inte UI. Arbetare 1 rör inte `PresetData.inl` och KickbaseTool utan att säga till.

## Därefter (projektledaren)
Rel-0.3-build, testlista (PDF i `docs/releases/`), Tobias testar i DAW, fixar, merge först efter ja.
