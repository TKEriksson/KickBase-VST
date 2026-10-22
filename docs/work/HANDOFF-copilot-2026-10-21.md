> **Cursor kan ta över efter copilot.** Copilot skriver därför LÖPANDE i docs/work/PROGRESS-copilot-2026-10-22.md (efter varje steg: vad som gjorts, hash, kommando, resultat, nästa steg, vad som är obyggt/otestat). Skriv så att någon utan chatthistorik (Cursor) kan fortsätta direkt. Cursor: läs CLAUDE.md, denna handoff och den filen först.

> **OBS 22 okt: kvoten är delad av alla sessioner. När den är slut arbetar copilot (projektledaren) ENSAM. Arbetare 1 och 2 går inte att nå, delegera inget. Där handoffen nämner Arbetare 1/2 som utförare gör copilot jobbet själv.**

# HANDOFF till Copilot (projektledaren [e66748], 21 okt kväll)

> Gäller även Cursor: där det står "Copilot" kan du läsa ditt eget namn. Läs hela filen, `CLAUDE.md` och `docs/work/PROGRESS.md` först. Filen ligger i `docs/` som är gitignorerat, så öppna Cursor i huvudmappen `KickBase-VST` (där docs/ finns), inte i en ren klon.

Skriven för Copilot eller annan AI som tar över när veckolimiten är slut. Ägaren heter Tobias, skriver svenska och vill vara insatt: korta svar, orsak före lösning, `fil:rad`, ett kort tips om att läsa dokumentationen när ett fel rör ett bibliotek (länka sidan/filen). Använd rollnamn (Arbetare 1, Arbetare 2, specialisten, projektledaren), aldrig A1/A2/nummer.
Läs först: `CLAUDE.md`, `docs/work/PROGRESS.md`, `docs/work/LESSONS.md`, `docs/plan-v2.md`, `docs/ENGINEERING.md`, `docs/work/HANDOFF-release.md` (äldre, delvis passerad).

## Hårda regler
- Pusha ALDRIG. Mergea till main bara när allt är klart och testat OCH Tobias säger till (bara projektledaren/du, ingen annan agent).
- Ingen release/installation förrän Tobias säger till (se "Release" nedan: just nu ÄR den godkänd).
- Specialisten (kickbase-vst-41, worktree KickBase-VST-op) tar order bara av Tobias. Skicka den inget.
- Nya parametrar och modulationskällor SIST i listorna (`docs/ENGINEERING.md`). Standardvärden ska ge oförändrat ljud: rendersig-baslinjerna ska ge 0 diff.
- Inga nycklar i repot. `docs/` är gitignorerat. `installRel.bat/.ps1` är otrackade med flit.
- En build i taget. Commit-meddelanden slutar med `Co-Authored-By: <din modell>`.
- Backup av installerade plugins tas automatiskt av `installRel.ps1` (-> `C:\Users\hunga\Documents\KickBase-backup\<tid>\`).
- Fyll på loggen (`docs/work/PROGRESS.md` eller egen `PROGRESS-*.md`) vid varje delsteg: vad, av vem, resultat.
- Veckolimiten är nästan slut: välj det som kostar minst tokens men håller kvaliteten.

## Gren och worktrees (under C:\Users\hunga\Documents\GitHub\)
| Mapp | Gren | Roll |
|---|---|---|
| KickBase-VST-int | release-int | Integrationsgren, byggs av CI. Tip `b0be511`. Jobba här. |
| KickBase-VST-ci | (detached) | CI-bygge: `build\Release` (Ninja + MSVC via `ci-build.bat`) |
| KickBase-VST-rel | content-g | Arbetare 1 (AN, tool layer, R9b) |
| KickBase-VST-lce | content-lce | Arbetare 1 (Live Code E), tip `a0442ee` (INTE i release-int) |
| KickBase-VST-f2 | content-s2 | Arbetare 2 (FM/S2, slicer), tip `4dad3e2` |
| KickBase-VST-mx | fix-matrix | ingen ägare, `bc3e9f8` |
| KickBase-VST-fs | fix-slicer-test | `d254e2d` |
| KickBase-VST-op | fix-matrix-opus | specialisten |
Städa gärna gamla worktrees (-test, -live, -poly1, -lc, -lcui, -f) efter att Tobias godkänt.

release-int-kedjan (äldst -> nyast): … 1c3519c (/STACK-nödlösning), 2913acb, 23be047, cbab932, 4e035ed, 56eac62 (senast installerad), 458025b (Gemini-fix), 24d5ebd (Live Code tom + AUTO av vid start), a2ae6a7, e9c1ee0 (FILTER-sändning UI), 5a60d08 (mixerfix), b0be511 (livetest-förväntan).

## CI (hur man testar)
- Skript i `C:\Users\hunga\Documents\GitHub\`: `ci-run.ps1 -Revs rev1,rev2`, startas via `ci-startNN.ps1`. Skriver `ci-log.txt` (skrivs över: kopiera till `ci-log-runNN.txt` först). Full utdata i `ci-full-<rev>-<test>.txt`. Raden `== KLAR` avslutar en körning.
- Körs: KickbaseTests, eqtest, instrumenttest, livetest, helpcheck, persisttest, matrixtest, inittest, fulltest, antest, rowkeytest, paramtest, rendertooltest, filter2test, chattest, rendersig (Init, instr 1, tung instr 10, tb303), sedan HostTest.
- Fullt omabygge ~15 min, inkrementellt ~2 min.
- rendersig-baslinjer som ska vara oförändrade: Init `94f0a1b6bd8a3c7f`; instr 1 `f7139084f716c127`; tung instr 10 @48k/512 `6a3e65e696d14d10`; tb303 instr 10 `03fbeb761547e195`.
- Körning 20 (innan b0be511): allt grönt utom 4 livetest-kontroller (förväntade AUTO på vid start; ändrat avsiktligt i 24d5ebd). Rättat i b0be511 (`Tools/Tool.cpp:381-382`). Körning 21 (b0be511 + a0442ee) pågår/är klar: läs `ci-log.txt`/`ci-log-run21.txt`.

## RELEASE (godkänd av Tobias 21 okt kväll)
1. Vänta tills körning 21 är klar. Den bygger sist `a0442ee` (annan gren) som skriver över VST3:n i `KickBase-VST-ci\build\Release`.
2. Kör därför om ENBART `b0be511` (eller aktuell release-int-tip): `ci-run.ps1 -Revs @('b0be511')`. Då ligger rätt VST3 i CI-mappen.
3. Kontrollera: KickbaseTests, livetest (nya kontroller "AUTO: av när sidan skapas" och "tom kodruta"), instrumenttest, rendersig-baslinjerna identiska (mixerfixen är multiplikation med 1.0 vid standard), HostTest.
4. Be Tobias köra `.\installRel.bat` i `C:\Users\hunga\Documents\GitHub\KickBase-VST` och scanna om plugins i FL Studio.
Inte i denna release: Instr-mix-fliken, pan/bredd för SAMPLE/NOISE/SUB, sidechain-vyn.

## KLART sedan förra releasen (56eac62) och väntar på Tobias test
- Gemini-fix `458025b`: `WebInputStream(url, false)` (`AiProvider.cpp:495,531`, `AiChat.cpp:548`) eftersom POST med nyckel i URL gav HTTP 400. Retesta TEST & SAVE; om nyckeln "AQ.…" fortfarande avvisas, undersök nyckelformat (rimligen en Google AI Studio-nyckel börjar "AIza"). Rådgör: be Tobias skapa en ny nyckel (den har visats i chatt). Valfritt: maska `key=` i felutskrift (`AiChat.cpp:287`, `AiProvider.cpp:416-435`).
- Live Code `24d5ebd`: tom kodruta och AUTO av vid start. Fråga Tobias om ett sparat projekts Live Code också ska starta tomt.
- SUB-indikator (liten oscillator/scope som syns när SUB spelar): Tobias bad om det när sub låter fast alla OSC är av.
- FILTER-fliken `e9c1ee0`: de sex "SENT THROUGH FILTER"-knapparna ersatta av flervals-dropdown "THROUGH FILTER" (`Source/Ui/FilterPanel.cpp`), plus en "Send through filter"-knapp i varje källflik (OSC: `OscPanel` `filterSendButton`; SAMPLE: `Modules.cpp` `filterSendToggle` ~1015-1020; NOISE: `noiseFilterButton` ~745-749). Parametrar: `filterOn, filterOsc1..3, filterSample, filterNoise` (`Core/Params.h:110`), motor `InstrumentSynth.cpp:399-415`. OKOMPILERAT före körning 21: kolla att den gick igenom.
- Mixerfix `5a60d08`: Tobias klagade att INSTR-fadern/mätaren inte gjorde något när instrumentet skickades 100 % till FX-kanal. Orsak: stripens fader/pan/mätare täckte bara torr del (`dryGain = 1 - fxAmount`); den våta sändningen togs före stripen. Nu skalar stripen hela källan och sändbufferten tas efter fadern (`Mixer.cpp:108-135`, `InstrumentProcessor.cpp` ~795-817 och kicken ~886-888, `sendBus` i `InstrumentProcessor.h:99`). Ouppklarat: varför FX 6-fadern inte heller hördes (misstanke: instr 4 matar även en annan FX-kanal; FL separata utgångar går förbi mixern; installerat bygge äldre). Fråga Tobias efter nästa install. Lägg gärna ett test att INSTR-fadern skalar FX-sändningen och att mätaren visar den.

## TODO för Tobias önskemål (i prioritetsordning)
1. **Instr-mix-flik** (efter FILTER): mixer med en strip per källa (OSC 1-3, SAMPLE, NOISE, SUB, ev. FM) med gain, volym och pan. Befintliga nivåparametrar: `aLevel/bLevel/cLevel` (`Params.cpp:259`), `sampleLevel` (`Params.cpp:307`), `noise`, `sub`, `fmLevel`. Pan finns bara för OSC (`aPan/bPan/cPan`, `Params.cpp:274`, `aStereo` 277). Obs flikindex: INSTR, OSC 1-3, SAMPLE, FILTER, FM (`Modules.cpp` ~746-767, `addPower (3 + ParamID::oscsPerLayer, …)`): kontrollera att nya fliken inte förskjuter index.
2. **Pan + bredd (stereobreddare) per motor** (Tobias, "Japp"): nya parametrar SIST i `InstrumentField` (före `count`, `Params.h:116`) och i `Params.cpp`: `samplePan, noisePan, subPan` samt bredd per källa (`*Width`, 0..200 %, standard 100 % = oförändrat). Pan standard 0 (mitten) och bredd 100 % ska ge exakt samma signal, så rendersig ska ge 0 diff. Breddare = mid/side: skala side-delen. Håll bas/sub mono under ~120-150 Hz (låg-mono) så att subbasen inte blir luddig och mono-kompatibel; sub ska normalt vara mono. Läs in parametrarna i `InstrumentSynth` (`Params.cpp` ~679-684 är läsningen av instrumentets fält). Lägg även en `ToolLayer`/AGENT_GUIDE-notering och ett test.
3. **Sidechain-vy i Kickstart 2-stil** (Tobias: "ser väldigt smidig ut"): kick (vänster) och bas (höger) ritas i samma bild, med duck-kurvan ovanpå så man ser exakt vad kurvan gör med basen; måste kunna gå i halv hastighet (halftime). Befintlig sida: `Source/Ui/SidechainPage.h/.cpp` (Timer), `Core/Sidechain.h`, motorn har redan `duckReduction`/`duckGainTaps` i `InstrumentProcessor`. Kräver ingen motorändring. Läs SidechainPage.cpp först för vad som redan ritas. Be Arbetare 1 skriva en kort spec först (eller skriv den själv), sedan implementera efter releasen.
4. **Strudel-effekter** (väntar på Tobias OK på mappningen): `.lpf/.hpf/.lpq/.attack/.release` -> filter + amp-ADSR; `.distort/.shape/.crush` -> drive/bitcrusher-slot; `.room/.delay` -> send + reverb/echo; `.pan` -> mixerns pan; kick via `s("bd")`. Dessutom `fx1.set/get` osv. för alla parametrar. Arbetare 1 skriver kort spec: additivt, rendersig 0 diff utan effektkoder, nya parametrar sist.
5. **LC F** (Live Code-flik med sliders, kodblock i chatt med Play/Stop/Create, transpiler): `lc-f-design.md` ligger ocommittad i KickBase-VST-lcui; avgör `.lfo/.env`-namn. Arbetare 1.
6. Köas efter release: slicer SLICER-rad / MAP TO i MIDI-export (Arbetare 1); FM-preset-LFO:er (Tobias: ja, efter release); content-g `d6cbcba`, content-s2 tip (`4dad3e2`), fix-matrix `bc3e9f8` ska CI-testas och sedan ff-mergas av ägarna till testade tips; content-ph troligen ut (BZ-test faller på `3447d16`).
7. Ta bort `/STACK:8388608`-nödlösningen (`1c3519c`, KickbaseTests) när rotorsaken till stack overflow är funnen (misstanke: stora processorer på stacken i `Tools/ParamSweepTests.cpp:50,90`, `NodeEnvelope` kan inte kopieras `PersistFmTests.cpp:22`).
8. Flagga en gång för Tobias: antest-fixturen ändrad (`7ca1484`, bas x0,55); slicertestets fixtur (0,8 s WAV, 128-krav kvar); `Tool.cpp` tolerant konsolkontroll; be Arbetare 2 granska s2-konfliktlösningen; lägg ev. `instrN.X.set`/`fx1.slotN` i `docs/live-code-exempel.md`.
9. Städfrågor till Tobias: gamla worktrees; flytta `docs/AGENT_GUIDE.md` och `psytrance-kick-bass-reference.md` till `Resources/` (`CMakeLists.txt:138-140`); höja `maxExtraNotes`.
10. Specialisten: påminn Tobias att säga åt den att föra egen `PROGRESS-*.md`.

## Merge
Först när Tobias säger till och allt är testat: i huvudrepot `git merge release-int` (ingen push). Inkludera den ocommittade `CLAUDE.md`-ändringen. Uppdatera `docs/work/PROGRESS.md` före sessionens slut (körning 14-21, install, Gemini-fix, Live Code-fix, livetest-fix, SUB-indikator, FILTER-UI, mixerfix, önskemålen ovan).

## Tobias testlista efter nästa install
PROJECT-menyn, AI-server, Gemini TEST & SAVE, slicer (MIDI-toner 2-9), Live Code-exemplen i `docs/live-code-exempel.md` (inkl. 12a-12d och ackordexempel 7 med VOICES 1 vs 3-4), tom Live Code/AUTO av vid start, SUB-indikatorn, FILTER-dropdownen och per-flik-knapparna, mixern (INSTR-fader + FX-fader + dB-mätare).

## Tillägg: nästa release = release-int + HELA content-g + AN-resten
- AN-resten är specad i `docs/work/COPILOT-AN-kvar.md` (läs den). Rättelse: `an-threshold-review-A2.md` ligger i `KickBase-VST-mx/docs/work/`, inte i main.
- Ordning: (1) pågående release (release-int b0be511, utan content-g) körs klart och installeras; (2) projektledaren mergar content-g (d6cbcba: AN, R9b, kbproj, F2 steg 3, FX2) in i release-int och kör CI; (3) Copilot bygger AN-resten ovanpå den sammanslagna grenen; (4) CI på slutresultatet, sedan release. Merge till main görs bara av projektledaren efter Tobias go.

## Tobias beslut 21 okt kväll: Copilot får pusha, merga och bygga release om projektledaren nått limit
- Gäller BARA när Tobias uttryckligen säger till Copilot att projektledaren är slut. Annars gäller gamla regeln (bara projektledaren mergar, ingen push utan Tobias).
- Då får Copilot: köra `ci-run.ps1`, merga content-g -> release-int -> main när CI är grönt (KickbaseTests, alla verktygstester, rendersig-baseline oförändrade, HostTest), pusha, och bygga + installera releasen (`installRel.bat`, backup sker automatiskt).
- Gör i ordning: (1) kolla att release 21 (b0be511) redan är installerad, annars gör den först; (2) merga content-g in i release-int på testgren, lös konflikter, CI; (3) AN-resten enligt `COPILOT-AN-kvar.md` paket för paket, CI efter varje; (4) slutlig CI, merge till main, release.
- Committa också `CLAUDE.md` (osparad ändring i main) men INTE `installRel.bat/.ps1` (ska förbli ospårade).
- Uppdatera `docs/work/PROGRESS.md` + `PROGRESS-copilot-an.md` efter varje steg.

## UPPDATERING 21 okt sent kväll (projektledaren [e66748]) - gäller före äldre avsnitt ovan
Senaste fakta, åsidosätter äldre rader (t.ex. "release-int b0be511"):
- **Installerad release = `77b5ccd`** (release-int, inkl. a0442ee). Tobias har kört `installRel.bat`.
- **Arbetsgren i KickBase-VST-int = `fx-send-mode`** (ovanpå release-int): `ba917dd` + `0149478`. Inte mergad till release-int ännu.

### KLART nu (kodat och committat, väntar på CI/Tobias test)
- `ba917dd` FX SEND/INSERT per instrument + kick (nytt fält `InstrumentField::fxSend`, `ParamID::kickFxSend`, sist i listorna, standard INSERT = ljudet oförändrat). Orsak: Tobias märkte att FX6-faderna sänkte hela instrumentet (INSERT blandar bort torrt). SEND lämnar torrt orört. Se `InstrumentProcessor.cpp` ~795-816, `FxProcessor.cpp:146-151`, UI `FxSendControl` i `Components.cpp`, ny kontroll i `Tools/Tool.cpp` ~1198. Hjälp-nycklar `kickFxSend`/`bassFxSend` finns. CI körning 23 pågår.
- `0149478` pianorulle: Space = start/stopp av preview, Shift+Space = bara pianorullens rad, hint-rad längst ned, MIDI-tangentbord spelar bara den öppna radens instrument (dropdown "MIDI IN: THIS ROW / ALL"). Körtillstånd `PluginBase::midiInLayer` (ingen parameter), filter i `InstrumentProcessor.cpp:898-902`, UI `Ui/PianoRoll.cpp` (`applyMidiIn`, `keyPressed` ~1950). Ej byggd/CI-testad än (körning 24 ska köra `0149478`).

### VI JOBBAR MED NU
1. Vänta in CI 23 (`ba917dd`), spara `ci-log.txt` som `ci-log-run23.txt`, starta körning 24 (`ci-start24.ps1 -Revs @('0149478')`). rendersig-baslinjerna ska vara oförändrade.
2. Fas-verktyg (Tobias godkände "Ja, vi gör så", planen i `docs/plan-v2.md` "Tillägg 21 okt kväll"): (1) kickens PHASE-vy med spökkurva (`Modules.cpp:510`, bara UI), (2) kick/bas-fasvy i AN (återanvänd `lowCorrelation` i `Core/MixAnalysis.cpp`, plus antest-rad).

### BERÄKNAS HINNA (projektledaren, med minimal tokenanvändning)
- Säkert: CI 23 + 24 klara och rättade, rapport till Tobias om att installera och testa (Space/Shift+Space, MIDI IN-dropdown, FX SEND/INSERT).
- Troligt: fas-verktyg 1 (spökkurva) och 2 (AN-fasvy).
- Osäkert: mergen av content-g (`d6cbcba`) in i release-int + CI. Hinns den inte gör Copilot den (se avsnittet "Copilot får pusha ...").
- Efter release (görs av Copilot/nästa session): 3 allpass FREQ/AMOUNT (DSP, nya parametrar sist, AMOUNT 0 som standard), 4 start-offset per motor + sammanlagd vågform i INSTR-MIX-fliken (kolla flikindex `Modules.cpp` ~746-767), instr-mix pan/bredd, sidechain-vy, Strudel-effekter, LC F.

### Rekommenderad ordning för den som tar över
1. Klart `fx-send-mode` (CI-grönt) -> merga in i release-int. 2. Merga content-g (d6cbcba) i release-int, CI. 3. Ge Tobias hashen; han startar Copilot (`COPILOT-AN-kvar.md`). 4. Release + `installRel.bat` först när Tobias säger till.
- CI-arbetsrutin: kopiera `ci-log.txt` till `ci-log-runNN.txt` före varje ny körning; "== KLAR" avslutar körningen.
- RÄTTELSE (22 okt-natt): fas-verktyg 2 (kick/bas-fasvy i AN) FINNS REDAN: `Source/Ui/KickBassView.h` (ANALYZER > KICK+BASS: vågform för kick/bas/summa färgad efter fas, korrelation per 16-del, spektrum 20-300 Hz) och `MixAnalysis` `PhaseMetrics`. Inget mer att bygga där förutom ev. förslag på åtgärd (allpass, se punkt 3). Fas-verktyg 1 (PHASE-spökkurva i `Ui/KickDisplay.cpp` `renderWaveform`/`paint`) är kodat och committat: `82de7e9` (bara UI, rendersig opåverkad). CI-körning 24 ska köra `82de7e9` (innehåller ba917dd + 0149478).

## LIVE CODE-ARBETE EFTER RELEASEN (Tobias beslut 21 okt sent)
Veckokvoten är nästan slut. Ordningen nedan gäller; Arbetare 1 får uppdraget först (efter att projektledaren släppt releasen), och Copilot tar över det som inte hunnit bli klart.
1. **Strudel-effektmappning (godkänd av Tobias):** tabell från Strudels effektnamn till våra effekter (set_effect/FXn.slotN). Liten, rör inte ljudmotorn. Lägg i Live Code-dokumentationen (`live-code-exempel.md`) och i koden där Strudel-namn tolkas.
2. **LC F** (design: `KickBase-VST-lcui/docs/work/lc-f-design.md`, tillägg längst ned): mätarlista, `instr1.lock()`, `.set`/`.get`, `fx.slotN.setEffect`. Ordning F1 → F2 → F3 → F4, en commit per steg.
3. **LC G:** flytande Live Code-fönster, KONSOL/flikar, notmarkering. Mest UI, görs sist.
4. Därefter: MCP `live_code_run`/`live_code_stop`, AGPL-banderoll + THIRD-PARTY.md, helpcheck 21 poster (LFO 9/10), AN visuell koll KICK+BASS/COLLISIONS, slicer MAP TO.

Regler för den som tar över: kolla `docs/work/PROGRESS-lc.md` (Arbetare 1) för exakt hur långt F kommit innan du börjar; nya parametrar och modulationskällor sist (`docs/ENGINEERING.md`); rendersig-baslinjerna ska vara oförändrade (Init `94f0a1b6bd8a3c7f`, instr 1 `f7139084f716c127`, tung instr 10 `6a3e65e696d14d10`, tb303 `03fbeb761547e195`); logga i egen PROGRESS-fil; ingen push/merge utan Tobias (Copilot får bara pusha/merga/bygga release om Tobias uttryckligen säger att projektledaren nått sin gräns).

## Reservplan: Cursor (gratisversion) efter Copilot
Tobias startar Cursor om även Copilot når sin gräns. Därför ska den som slutar sist (Arbetare 1 eller Copilot) lämna:
- En ren lista överst i sin PROGRESS-fil: "KLART / PÅGÅR / EJ PÅBÖRJAT" med commit-hashar och gren per punkt.
- Små, avslutade steg (en commit per F-steg); inga halvfärdiga ändringar i working tree. Ofärdigt arbete på egen gren, aldrig på release-int.
- Exakta kommandon för bygg och test (`ci-run.ps1`, rendersig-baslinjerna ovan) så att Cursor kan verifiera utan att gissa.
Cursor (gratis, begränsad kontext): ge den EN punkt i taget ur ordningen ovan, be den läsa `CLAUDE.md`, `docs/ENGINEERING.md` och `lc-f-design.md` först, och granska resultatet innan något mergas. Merge till main sker bara med Tobias go.

## Loggregel för Copilot och Cursor (Tobias, 21 okt)
Alla som tar över för projektledaren och arbetarna för en egen fil: `docs/work/PROGRESS-copilot-<datum>.md` respektive `docs/work/PROGRESS-cursor-<datum>.md`. Skriv där: vad som gjorts (commit-hash, gren), hur det gick (tester, rendersig), vad som återstår och eventuella slarvfel (även i `docs/work/LESSONS.md`). Uppdatera filen efter varje avslutat steg, inte bara vid sessionens slut, och lägg en rad i `docs/work/PROGRESS.md` som pekar på den. Då kan nästa i kedjan (Copilot → Cursor → projektledaren) ta vid utan att fråga.

## Rättelse av Copilots frågor (21 okt sent)
- AUTO i stället för LIVE är KLART och ingår i installerade releasen 77b5ccd (Live Code tom/AUTO av vid start = 24d5ebd, livetest-kontroller i b0be511). Avsnittet "Nästa steg efter E och F1: AUTO" i PROGRESS-lc.md är historik.
- PROGRESS-lc.md:s status för E och F är gammal. Hur långt F kommit vet ingen säkert än: Arbetare 1 ska som första steg lägga en aktuell KLART/PÅGÅR/EJ PÅBÖRJAT-lista överst i PROGRESS-lc.md. Strudel-mappningen är EJ PÅBÖRJAD.
- Bas-hash (release-int med content-g och fx-send-mode) ges av Tobias när projektledaren är klar; tills dess ingen kod.

## RELEASE-KANDIDAT 21 okt natt (projektledaren [e66748], kvoten ~98 %) - GÄLLER FÖRE SLUTSTATUS NEDAN
- **Gren `release-cand` i KickBase-VST-int**: fx-send-mode `6fc8ce1` + content-g (merge `8dffe77`, konflikter i `Params.h/.cpp` lösta: ordning `... slicerReverse, fxSend, sampleMode` i enum, namnlista och layout) + chatt-hälsning `9bb2d8f`. **CI 28 körs på `9bb2d8f`** (se `ci-log.txt`, spara som `ci-log-run28.txt`; "== KLAR" avslutar; alla rader "exit 0"; rendersig Init `94f0a1b6bd8a3c7f`, instr 1 `f7139084f716c127`, tungt `6a3e65e696d14d10`, tb303 `03fbeb761547e195` ska vara oförändrade).
- Nytt i content-g (`6096db1`): klick på fynd i ANALYZER > REPORT markerar bara, dubbelklick öppnar spektrumet (`SpectrumPage.cpp`, `selectFinding(index, showSpectrum)`); varje färdig analys sparas som `Documents\Kickbase\Analysis\latest.json` (`MixAnalysis.cpp`, `MixAnalyser::run`), AI läser den med `get_mix_report`/filen; `AGENT_GUIDE.md` §9 beskriver det.
- Chatt-hälsning (`AiChatPage.cpp`, `refresh()`): i tom chatt med nyckel visas en engelsk hälsning (roll, vad den kan, fråga hur den kan hjälpa). Bara visning: sparas inte, skickas inte till modellen.
- **KLART 21 okt natt: CI 28 helt grön (`ci-log-run28.txt`), release-int = `9bb2d8f`, main = `3d3d5e6` (merge + CLAUDE.md-commit `fe643b8`). Inget pushat.** Kvar: Tobias kör `.\installRel.bat`, scannar om i FL och testar; push bara med Tobias.
- (Gammalt, utfört:) grön CI 28 -> Tobias flyttar `release-int` till `9bb2d8f` (`git branch -f release-int release-cand` i KickBase-VST-int) -> `.\installRel.bat` -> testa -> merge till main bara med hans go. Rött: rätta småfel (troligast Params/sampleMode-sammanslagningen).

## SLUTSTATUS 21 okt natt (projektledaren [e66748], veckokvoten ~97 %) - GÄLLER FÖRE ALLT OVAN
Läs detta avsnitt först. Det åsidosätter hashar och ordning i äldre avsnitt.

### Hashar och grenar
- **Installerad release = `77b5ccd`** (release-int, inkl. content-lce `a0442ee`).
- **fx-send-mode (worktree KickBase-VST-int) = `6fc8ce1`** (UI: FILTER-knapp med etikett i alla motorflikar + COLLISIONS-axeltitlar/legend). Föräldern **`c23467e` hade CI 26 helt grön** (FX SEND/INSERT, pianorulle Space/Shift+Space + hint-rad + MIDI IN INSTRUMENT, PHASE-spökkurva 82de7e9, Analyzer KICK+BASS-UX, eqtest-fix 9fba804).
- **CI 27 (6fc8ce1)** startades 2026-10-21T23:09:36 och var inte klar när detta skrevs. Läs `C:\Users\hunga\Documents\GitHub\ci-log.txt`; rad `== KLAR` avslutar. Alla testrader ska vara "exit 0". Spara loggen som `ci-log-run27.txt` före nästa körning. Grön -> målet för release-int är `6fc8ce1`; rött i bara UI-kod -> rätta (småfel), annars `c23467e`.
- **release-int har INTE flyttats**: `git branch -f release-int fx-send-mode` nekades av auto-läges-klassificeraren. Gå inte runt det. Tobias kör själv i `KickBase-VST-int` (`git branch -f release-int <hash>` eller `git checkout release-int && git merge --ff-only fx-send-mode`) eller tillåter det.
- **content-g `d6cbcba`** (AN, R9b, kbproj, F2 steg 3, FX2; worktree KickBase-VST-rel): INTE i release-int, och ej byggd efter F2 steg 3 (kompilfel möjliga).
- content-s2 (Arbetare 2, worktree -f2): `4df9d66` + `4dad3e2` (slicer-ljudtest 4a2; `4dad3e2` obyggd). content-lce `a0442ee` ligger redan i release-int.

### Nästa steg i ordning
1. Kolla CI 27. Spara loggen. Rätta ev. småfel.
2. Få release-int flyttat till den gröna hashen (Tobias, se ovan).
3. Merga content-g (`d6cbcba`) in på en testgren från release-int, lös konflikter, kör `ci-run.ps1 -Revs @('<hash>')` (fullt omabygge ~15 min). Se upp för kompilfel i F2 steg 3 (`StepSequencer`, `sampleMode`).
4. Ge Tobias bas-hashen; han startar Copilot (läs `COPILOT-AN-kvar.md`). Cursor är reserv (egen `PROGRESS-cursor-<datum>.md`).
5. När CI är grönt: be Tobias köra `.\installRel.bat` (i `C:\Users\hunga\Documents\GitHub\KickBase-VST`) och scanna om plugins i FL.
6. Merge release-int -> main bara med Tobias go (ta med CLAUDE.md-ändringen; `installRel.*` förblir ospårade). Aldrig push (utom enligt avsnittet "Copilot får pusha ..." och bara om Tobias säger att projektledaren nått limit).

### Tobias testlista efter nästa install
- Pianorulle: Space / Shift+Space, hint-raden, MIDI IN-dropdown (INSTRUMENT: spelade toner låter och tangenterna lyser).
- FX SEND/INSERT på FX6 (INSERT-fadern längst ned ska tysta).
- Kickens PHASE-spökkurva.
- Analyzer: färger/legend, SNAPSHOT/CLEAR-rutor, tips; COLLISIONS axeltitlar (FREQUENCY Hz, BAR) och färgförklaring.
- FILTER-knappen (etikett, uppe till höger) på OSC 1/2/3, SAMPLE, FM och NOISE (`Ui/KickbaseLookAndFeel.cpp:177`, läget "labelled"; `OscPanel.cpp` resized, `Modules.cpp` SAMPLE/NOISE, `FmPanel.cpp:77`).
- Äldre kvarstående: Gemini TEST & SAVE (nyckeln "AQ." avvisades; ny AI Studio-nyckel börjar "AIza"), Live Code tom/AUTO av, SUB-indikator/scope, sparat projekts Live Code startar tomt.

### Arbetare 1 status (kopia från toppen av KickBase-VST-lce/docs/work/PROGRESS-lc.md, 21 okt sent)
KLART:
- LC D (CODE-moduleringskällor 74-81, .lfo/.env) `4f40a85` (i release-int). LC E gap 1-6 + tak 32 kodrutter + sopning: `b6467df`, `56316bb`, `a0442ee` (content-lce, nu i release-int). LC F1 `.set/.get` `d58a0a0` (i release-int). Slider(v,min,max) `e2fbe0a`, `eb67ddd`. AUTO + tom start `24d5ebd`, `b0be511`. THIRD-PARTY.md + hjälptexter lfo4-10 `ce77236`.
- content-g: F2 steg 3 `d6cbcba` (obyggd); FX2 (Widener, Flanger, Transient, Dynamic EQ, Octaver, Band Ducker) `6350f47..c103834`.
- Slicer i samplerösten (Grid/Auto/Manual, REVERSE, MAP TO) `8c1ac04` + `df473db` (content-s2); persisttest 128 skivor `d254e2d` (fix-slicer-test).
PÅGÅR: slicer-ljudtest 4a2 (StateTests.cpp:173-251) tystnad med slicer PÅ, Arbetare 2 på content-s2 (`4dad3e2`, `4df9d66`).
EJ PÅBÖRJAT: LC F2 `instr1.lock()`, F3 `fx.slotN.setEffect`, F4 mätarlista (design `KickBase-VST-lcui/docs/work/lc-f-design.md`, ocheckad; en commit per steg); `.lfo/.env` skuggar Strudels egna (`glue.js:79,85`); LC G; Strudel-effektmappning (godkänd av Tobias); MCP `live_code_run/stop`; AGPL-banderoll i Live Code-fliken; helpcheck 21 poster (LFO 9/10); slicer-UI (parkerat, se nedan).
Ocommittat: KickBase-VST-lce `docs/work/PROGRESS-lc.md` (listan, tappa inte) och `lc-e-design.md` (stub, committa INTE).

### Parkerat tills ny veckolimit (starta INTE)
- Slicer-UI: `docs/work/slicer-ui-spec.md`.
- COLLISIONS "Gå till" + Quick-Fix: `docs/work/collisions-goto-quickfix-spec.md` (först Gå till, sedan Quick-Fix; samma godkännande/A-B/ångra-regel som fasfyndens "SUGGESTED FIXES").

### Efter release (kön)
Skicka Arbetare 1: Strudel-mappningen + LC F (F1->F4), sedan LC G, `.lfo/.env`-fix, MCP live_code_run/stop, AGPL-banderoll, helpcheck 21. Därefter: allpass FREQ/AMOUNT (nya parametrar sist, AMOUNT 0 standard), start-offset per motor, instr-mix pan/bredd, Kickstart-2-sidechain-vy, FM-preset-LFO:er. Frågor till Tobias: content-ph (troligen ut), städa worktrees, ta bort `/STACK:8388608` (`1c3519c`), flytta docs till `Resources/`, `maxExtraNotes`, påminn om att specialisten ska föra egen `PROGRESS-*.md`. Flagga en gång: antest-fixtur `7ca1484`, slicer-fixtur, `Tool.cpp` tolerant konsolkontroll, be Arbetare 2 granska s2-konfliktlösningen; nämn att projektledaren startade CI 19 på eget initiativ. Skicka CI-FEL-rader till Arbetare 2 när release-int är grönt.
- Notera: Edit kräver en Read i samma kontext (slarvfel i FmPanel.cpp loggat). Hjälp-tips till Tobias: läs `KickbaseLookAndFeel.cpp:177` (labelled-läget).


## PÅGÅENDE 22 okt: AI-chatt-polish (Arbetare 2, branch ai-polish, worktree KickBase-VST-ai)
Spec: docs/work/ai-chat-polish-spec.md. Mergas INTE förrän allt är CI-testat och Tobias säger go.
- STATUS 22 okt: punkt 1-5 är KLARA och CI-verifierade. ai-polish spets = 19c6afd, CI 30 grön (alla tester exit 0, rendersig-baslinjerna oförändrade, ci-log-run30.txt). Commits: e7bd9b1 (3+5), a4873ec (test), b5b99a4 (4), a6d2d95 (1, StopButton AiChatPage.h:37), c3b4026 (2, AiChat::fetchAnthropicModels AiChat.cpp:225), 19c6afd (dölj lifecycle deprecated).
- Testbygget ligger i KickBase-VST-ci\build\Release (= 19c6afd). Tobias installerar med installRel.bat och testar punkt 1, 2, 4 i FL. ai-polish är MERGAD till main (2ba981c, lokalt, Tobias go 22 okt; trädet = CI 30-bygget 19c6afd). Inget pushat.
- Kvar efter merge (ej påbörjade): punkt 6 (CONTINUE), punkt 7 (betyg 1-10 + AI-minne, ai-memory-spec.md), analysläge dry/efter sidechain/master (se sist i ai-chat-polish-spec.md).
- Arbetare 2:s egen logg: docs/work/PROGRESS-a2.md (i huvudrepots docs, inte i worktree).
- CI per hash: cd C:\Users\hunga\Documents\GitHub; .\ci-run.ps1 -Revs @('<hash>'); kopiera ci-log.txt till ci-log-runNN.txt; ok = alla "exit 0"; rendersig-baslinjer oförändrade (Init 94f0a1b6bd8a3c7f, instr1 f7139084f716c127, heavy instr10 6a3e65e696d14d10, tb303 instr10 03fbeb761547e195).
- Nya förslag (bara nedskrivna): docs/work/ai-memory-spec.md (felminne + uppföljningsdialog, chatt + MCP). Bygg efter polishen.
- Parkerat: Slicer UI, COLLISIONS Gå till/Quick-Fix, Strudel-mappning (Arbetare 1: AI la instrN.Param.set EFTER mönstret -> "expecting ')'", överväg tydligare guide eller mildare parse), LC F/G.
- Release: release-int = 9bb2d8f, main = 746f28f, inget pushat. Tobias kör .\installRel.bat och testlistan.

- Indata till Arbetare 1 (Strudel, Tobias skärmdumpar 22 okt, Trance Lead instr 2): "'fx1' is not defined", "not a function" (const direkt efter .set-rader utan tom rad? ej verifierat, guide :29), ".when() expects 2 inputs but got 1", "interrupted" med lång stack (BigInt/parse). AI:n fortsätter chansa i stället för att byta till set_pattern. Överväg guide-exempel och mildare parse.

### Senaste status (Copilot, 22 okt; ersätter äldre main-/release-rader ovan)
- `main` är lokalt `2ba981c` (ai-polish merge), `release-int` är `9bb2d8f`. Inget är pushat. `ai-polish` är mergat efter Tobias go.
- Tobias bekräftade att sidechainen nu syns i analysen; utredningen stängd utan kodändring.
- Punkt 6 CONTINUE finns i commit `a84d845` på `ai-extras`; bygget och `chattest` passerar, inklusive fortsättning efter `max_tokens`.
- Aktuell branch/worktree `ai-extras` (`KickBase-VST-ai`), HEAD `a84d845`, bas `2ba981c`. Punkt 7 + analyzerhistorik är ocommittade: lokalt feedbackminne, betyg/kommentar i AI-svar, opt-in promptminne (AV som standard), valbar lookback 1/5/8/10/20, lagringstak 1/10/50/100 MB (100 standard, äldsta först), MEMORY-popup, rensning av aktuell chatt eller delat minne och AI/MCP-verktyg `get_memory_settings`/`set_memory_settings`/`clear_memory`. `Resources/AGENT_GUIDE.md` uppdaterad.
- VERIFIERAT: RelWithDebInfo `KickbaseMcp` build + `tool chattest` exit 0; Release build av `Kickbase_VST3`, `KickbaseFX_VST3`, `Kickbase_Standalone`, `KickbaseTests` och `KickbaseMcp` exit 0; Release `KickbaseTests.exe` gav “Alla tester gick igenom”; Release `KickbaseMcp.exe tool chattest` exit 0. Releaseartefakter ligger i `KickBase-VST-ai\build\Release\...`; ingen installation, commit, merge eller push. AI-chattskärmbild bekräftar MEMORY-knappen ryms, men popupmenyns layout öppnades inte separat.
- Kvar: Tobias kan installera/testa bygget. Inget ska kopieras till CI-/installmappen utan instruktion. I FL, kör en riktig ANALYSE och verifiera att senaste analyzer-thumbnailen hamnar längst till höger och att äldre snapshots går att välja; den kontrollen är ännu inte gjord. Se `docs/work/PROGRESS-copilot-2026-10-22.md` för exakta kommandon och logg.
- `installRel.bat/.ps1` i huvudworktree är otrackade lokala skript och pekar på den testade Release-builden i `KickBase-VST-ai\build\Release` (branch `ai-extras`, inklusive de ocommittade feedbackminnes/MCP-ändringarna). De installerar Kickbase + Kickbase FX och uppdaterar `%LOCALAPPDATA%\Kickbase\KickbaseMcp.exe`, med backup. CI-worktree är äldre ai-polish och används inte för denna installation. Stäng FL Studio, Claude Desktop och KickbaseMcp helt innan `.\installRel.bat`; skriptet vägrar fortsätta om någon kör. Installationen körs av Tobias.

### Handoff 22 okt 14:46 (Copilot → Claude)
- Tobias körde installationen. SHA-256 för Kickbase VST3, Kickbase FX VST3 och MCP-exe matchar Release-artefakterna i `KickBase-VST-ai\build\Release`; backup ligger i `Documents\KickBase-backup\2026-10-22_144504`. Release `KickbaseTests` och MCP `tool chattest` gröna.
- Fortsätt på `KickBase-VST-ai` / `ai-extras`, HEAD `a84d845` med ocommittade feedbackminnes-, UI-, MCP- och analyzerhistorikändringar. Kvar för Tobias: testa i FL en riktig ANALYSE (senaste thumbnail sist och äldre valbara) samt MEMORY-popupens öppna layout.
- `installRel.bat/.ps1` i huvudworktree är maskinspecifika och ska förbli otrackade. CI-worktree är äldre ai-polish.
- GitHub secret-scanning-alerts 1–2 pekade på syntetiska Google-nycklar i test-fixtures i `Tools/Tool.cpp` vid `b2ad056` (`x.test`, A/B-testvärden); inga verkliga credentials identifierades. Alerts markerades false positive med förklaring. Nytt test bygger Google-liknande fixture dynamiskt, utan nyckelliteral i diffen.

## UPPDATERING 22 okt 16:25 (Copilot) - läs detta först
- main = `6cfc29e` (merge av `ai-extras` `07dee09`; CI-grön). Inkluderar punkt 6 CONTINUE, punkt 7 betyg + opt-in feedback-/arbetsminne (MEMORY-popup, `AiMemory.*`), samt säkerhetsrättningar: chattmodellen kan inte ändra minnesinställningar/radera, `save_work_lesson` kräver opt-in, review-läge nollas.
- Pushat till origin: se PROGRESS-copilot-2026-10-22.md (sista raden) för bekräftelse.
- Rutin som gäller: bygg/CI bara i `KickBase-VST-ci` med `ci-run.ps1 -Revs @('<hash>')`; mergea till main först efter grön CI; fullmakt från Tobias 22 okt 15:40 att commita/mergea/pusha.
- Nästa: Tobias FL-test (punkt 1, 2, 4, minnespopup) efter ny `installRel.bat`; punkt 8; AUTO i stället för LIVE.

## UPPDATERING 22 okt kväll
an-tap mergad (analysläge dry/as heard, AGENT_GUIDE §9 uppdaterad). Se PROGRESS-copilot-2026-10-22.md. Nästa: AUTO istället för LIVE, LC-G.
