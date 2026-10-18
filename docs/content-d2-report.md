# Fas D2 och D3: filtermodeller, Filter 2 och FILTER-fliken (gren `content-d2`, från `content-e2`)

Skriven av Sonnet 18 okt 2026. D2 = tre nya filtermodeller (Ladder, Formant, Comb) med två generiska ratter P1/P2. D3 = ett andra filter (Serial/Parallel) och ett eget gränssnitt för filtren med frekvenssvarskurva.
**Med standardvärden (Kickbase-modellen, Filter 2 av) är ljudet bit för bit som förut:** `rendersig`, `rendersigreal`, `rendersigwide` och `rendersigtb303` är identiska med baslinjerna.

## 1. Vad som byggts

- **Modeller** (`Source/Core/FilterModels.h/.cpp`, `ModelFilter`): index 0 Kickbase, 1 TB-303 (som förut), 2 Ladder, 3 Formant, 4 Comb. `filterModel` fick tre nya val sist i listan (se avsnitt 4).
  - **Ladder:** egen Moog-stege av fyra TPT-ettpoliga lågpass med noll-fördröjnings-lösning av återkopplingen, tanh-mättnad (rationell Padé-approximation, ingen `std::tanh` på ljudtråden), 2x översampling (`Oversampler2x`, delad med TB-303). TYPE = LP 24, LP 12, Band, High. P1 = DRIVE (mättar ingången före slingan), RESO 100 % självsvänger.
  - **Formant:** fem parallella TPT-bandpass med basrösternas formanter (A E I O U, Csound Manual Appendix D, uppmätta värden, ingen kod kopierad). CUTOFF förskjuter formanterna, RESO smalnar bandbredden, P1 = MORPH (vokalen, kontinuerlig), P2 = BLEND (torr/våt).
  - **Comb:** kamfilter med återkoppling: `juce::dsp::DelayLine` (Lagrange 3:e ordningen) med en lågpass i slingan. CUTOFF = kamfrekvens, RESO = återkoppling (högst 0,97), P1 = DAMPING, P2 = BLEND.
  - **`FilterUnit`** (`FilterUnit.h/.cpp`): ett filter för en kanal med alla fem modellerna bakom ett gränssnitt. Kickbase-SVF:en och TB-303 flyttades hit ordagrant (utbrytningen ändrade inget i ljudet), så fas P kan ha ett filter per röst.
- **Filter 2** (`InstrumentSynth`): efter Filter 1, före drive. Parametrar sist i `InstrumentField`: `filter2On` (av), `filter2Model`, `filter2Type` (standard High), `filter2Cutoff` (120 Hz), `filter2Reso`, `filter2P1`, `filter2P2`, `filterRouting` (Serial/Parallel), `filterMix`. Allt utom On/Model/Type/Routing är modulerbart. Inget eget envelope eller keytrack (för sweep: ENV 2/3 eller LFO i matrisen).
  - **Serial:** Filter 2 tar Filter 1:s utsignal; `sample += MIX * (wet - sample)`, så MIX 0 = bara Filter 1. **Parallel:** båda får samma insignal; samma formel ger (1 − MIX)·F1 + MIX·F2.
  - Av: körs inte alls (ingen CPU). Av → på nollställer Filter 2 så att gammalt tillstånd inte hörs. Modellbyte nollställer filtret som tar över.
- **Gränssnitt (D3):** en ny flik **FILTER** bredvid OSC 1–3 och SAMPLE (`Source/Ui/FilterPanel.h/.cpp`): frekvenssvarskurva till vänster (Filter 1 i lagrets färg, Filter 2 orange, summan vit och ifylld), ROUTING under kurvan, och två rutor till höger (FILTER 1 med MODEL, TYPE, CUTOFF, RESO, P1, P2, KEYTRK; FILTER 2 med på/av, MODEL, TYPE, CUTOFF, RESO, P1, P2, MIX).
  P1 och P2 byter namn efter modellen (DRIVE, MORPH, DAMPING, BLEND) och tonas ned när modellen inte använder dem; TYPE tonas ned för TB-303, Formant och Comb; ett avstängt Filter 2 tonas ned. Filtergruppen i raden under källorna är borta (den flyttade till fliken); kvar där: PITCH, SUB, NOISE och FILTER ENVELOPE, som fyller bredden (luckan i raden är borta).
  - **Kurvan** (`FilterResponseView`) räknas på meddelandetråden ur en egen uppsättning `FilterUnit` (aldrig ljudtrådens): en impuls med liten amplitud (0,01) körs genom filtren med inställningarnas grundvärden (utan keytrack, envelope och modulering), FFT 16384 punkter vid 48 kHz, 240 punkter 20 Hz–20 kHz. Den räknas om bara när en inställning ändrats (kontroll 15 gånger i sekunden).
  - Summan räknas med samma formel som synten (Serial/Parallel/MIX), så kurvan visar vad som verkligen händer.
- **Hjälptexter** för alla nio nya parametrar (`helpcheck`: 0 utan förklaring), AGENT_GUIDE, `docs/audit/params-instrument.txt` (regenererad).

## 2. Ett fel som kurvan avslöjade (Ladder)

I första versionen låg drive-förstärkningen (1/√drive · drive vid liten signal = √drive) **inuti** återkopplingsslingan, så slingförstärkningen blev k·√drive: med DRIVE 40 % och RESO 55 % självsvängde filtret redan (kurvan visade +36 dB över hela området; en jämn insignal gav ett svängande utsignal i stället för en jämn nivå). Ladder-testet (`filtermodels`) mäter med DRIVE 0, där felet inte syns.
Rättat: DRIVE mättar nu **ingången** före slingan (`tanh(drive·x)/drive`, liten-signal-förstärkning 1), slingan har egen tanh med enhetslutning (begränsar amplituden över RESO 4), och utnivån vägs upp efter slingan med √drive. RESO ger nu samma resonans vid alla DRIVE. Ladder är en ny modell och finns inte i några baslinjer. **Lärdom:** testa nya filter med impuls/kurva vid flera inställningar, inte bara med standardvärdena.

## 3. Tester

- **`tool filtermodels`** (enhet): Ladder LP 24/LP 12/High mot analog förebild (störst avvikelse 0,01 dB, gräns 0,3), 24 dB/oktav-lutning som JUCE:s `LadderFilter` (−21,4 mot −21,6 dB), självsvängning vid 1 kHz, Formant och Comb (frekvenssvar, NaN och extrema värden, återställning), CPU per sampling.
- **`tool filter2test`** (nytt, riktiga renderingar av ett instrument, kicken och övriga instrument avstängda): MIX 0 ger *exakt* samma ljud som Filter 1 ensamt (Serial och Parallel); Serial LP 300 Hz → High 4 kHz tar bort nästan allt (rms 0,0005 mot 0,165); Parallel MIX 1 = bara Filter 2 (skillnad 1e-7 mot ett instrument med Filter 2:s inställning i Filter 1); alla 25 modellkombinationer vid resonans 100 %: inga NaN, topp 0,50.
- **`blocksizetest`** läge 5 (Ladder/Formant/Comb) och **läge 6** (tungt läge med Filter 2, Serial och Parallel, olika modeller): alla blockstorlekar identiska med block 32. **`audiotest`**: noll allokeringar på ljudtråden, också med alla fem modellerna som byts mellan blocken och med Filter 2 på alla tio instrument där modeller, routing, MIX och på/av ändras var femte block (resonans 100 %); Comb allokerar bara i `prepare`.
- **`persisttest`** 4d (modell, P1/P2) och **4e** (Filter 2: av som standard, alla nio fält följer med projektet; gamla projekt utan fälten = Filter 2 av).
- `rendersig`, `rendersigreal`, `rendersigwide`, `rendersigtb303` identiska; `includecheck` (Open303:s headers på rätt plats); `KickbaseTests` gröna.
- **CPU** (`tool perftest`, 10 instrument, värdena brusar några procent): standard 0,117 s/s; Ladder 0,130 (1,11x); Formant 0,104; Comb 0,115; Filter 2 Kickbase + Kickbase 0,102; **Ladder + Ladder 0,189 (1,62x)**; Comb + Comb 0,143 (1,23x). Det dyraste fallet är alltså knappt 2x standard.

## 4. Beslut och undantag att känna till

- **JUCE:s `dsp::LadderFilter` kunde inte användas:** cutoff och resonans går genom en `SmoothedValue` på 50 ms som sätts hårt i `setSampleRate` (`juce_LadderFilter.cpp` rad ca 103 och 166–171, smoothers privata i headern) och `reset()` nollställer tillståndet. En filter-envelope som ändrar cutoff varje sampling skulle fördröjas och slätas ut. Därför egen implementation; JUCE:s filter används bara som kontroll i testet (lutningen).
- **Zavalishin:** "The Art of VA Filter Design" (Native Instruments, rev. 2.1.2), kapitlet om transistorstegen; kapitelnumret är **inte kontrollerat** mot boken (kapitelnamnet gäller), står så i koden.
- **Undantag från regel 3 (flytta inte valslistor):** `filterModel`-listan fick tre val *sist*; TB-303 (index 1) ligger kvar. Befintlig DAW-automation av Filter Model från Tobias egna tester kan förskjutas eftersom parameterns steg ändrades (2 → 5 val). Engångsundantag, redovisat.
- **Comb-minne:** en fördröjningslinje på `combMaxDelaySamples` (2408 double per kanal vid 48 kHz, 9608 vid 192 kHz). Cirka 0,5 MB för 10 instrument; allokeras i `prepare`. Ska avgöras i fas P (per röst).
- **Kurvan visar inte** keytrack, filter-envelope och modulering (de flyttar det riktiga filtret runt det som ritas) och är en linjär approximation för Ladder och TB-303 (små signaler); en hård drivning ger lite annat i verkligheten. Det står i verktygstipset.
- **Layout:** Tobias bad om en "tydlig, snygg och enkel" layout utan att välja mellan alternativ ("jag litar på att du får till en okej layout"). Valt: filtren i en egen flik (fyller källpanelens höjd, plats för kurvan), inte i raderna (tre rader med reglage ryms inte i instrumentvyn utan att rattarna blir för små). Kan justeras efteråt.

## 5. Gränssnittskontroll (bildjämförelse mot `content-e2`)

Bilderna är tagna av båda grenarnas tippar (`e7a7475` och denna gren) med samma 42 anrop (`snapshot fx` flik 0–8; `snapshot instrument` med 1, 3 och 10 instrument × nedre flik 0–8; sex bilder med övre flik 0, 1 och 3) och jämförda pixel för pixel (zlib, ingen PIL).
**15 av 42 är identiska:** alla nio FX-bilder och de sex bilderna med övre flik PATTERN, KICK och AI. **Alla nedre flikar är identiska i varje INSTRUMENT-bild** (0 skilda pixlar från y 475 och neråt, och 0 i rubriken y 0–96).
De 27 INSTRUMENT-bilderna (3 antal × 9 nedre flikar) skiljer sig *exakt lika* (samma antal pixlar i alla 27), på tre ställen och inget annat:
1. **Källflikarna** (x 380–463, y 98–112): den nya FILTER-fliken.
2. **OSC-panelens formknappsrad** (x 45–257, y 172–186): MORPH och WAV har bytt plats (WAV hör ihop med SIN/TRI/SAW/SQR, MORPH längst till höger, Tobias önskemål). Den ändringen hamnade i D2-DSP-incheckningen (`34ff60f`) och nämndes inte där; den hör egentligen till fas W.
3. **Raden under källorna** (x 194–885, y 292–399): FILTER-gruppen är borta, övriga grupper fyller bredden.

Bilder av själva FILTER-fliken (`KickbaseTool snapshot instrument ... 20` med `KB_SOURCETAB=4`, ny miljövariabel; `KB_FILTER2=1` ställer in ett exempel med Ladder och Filter 2 på): `snap-f/filter_off.png` och `snap-f/filter_on.png` i scratchpad.
CUTOFF-rattens värde (en inbyggd display i ratten) krymper nu sin text så att den ryms i de mindre rattarna i fliken ("900 Hz" i stället för "900..").

## 6. Tillståndsladdning (regel 11, `KickbaseHostTest state load`)

Ett tillstånd sparat med VST3-bygget av `content-e2` (4253 parametrar, varje 97:e satt till 0,37 och körd 20 block) laddat i bygget av denna gren (4363 parametrar):
- **0 av 4253 gamla parametrar skiljer** (samma id, samma värde).
- **110 nya parametrar** (10 instrument × 11: Filter P1/P2 och de nio Filter 2-fälten): alla på sitt standardvärde (0 inte på standardvärdet). Filter 2 On: 10 st, 0 på. Filter P1: 10 st, 0 inte 0. Filter P2: 10 st, 0 inte 1. Filter Model: 10 st, alla på Kickbase.
- Tillståndet sparas inte identiskt igen (fler parametrar), vilket är väntat; testet godkänner det bara när inget gammalt värde ändrats. Kontrollen av nya parametrar är ny i `KickbaseHostTest` (`Tools/HostTest.cpp`).
