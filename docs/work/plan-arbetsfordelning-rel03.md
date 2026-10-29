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

## Till 0.4 (Tobias 28 okt kväll, efter test av rel-0.3 i Lemon)
- **Gammal FX-länk i send-matrisen:** preset med den gamla länken (kanal + belopp + INS/SEND) visar en extra "FX1"-rad längst ner, och FX1 får ljud via två vägar (`Components.cpp:609-646`). Tobias valde lösningen att flytta den gamla länken till matrisens rad vid laddning (ingen dubblettrad). Försiktigt: rör presetladdning, IDn och hash får inte ändras, och INS-läget (torrt minskar) har ingen motsvarighet i matrisen, så avgör först vad som händer med INS.
- **Lättare att ta bort en FX-kanal:** idag går det bara på FX-sidan (`EffectsPage.cpp:353`, bara sista kanalen). Ska också gå från send-popupen, med bekräftelse när kanalen har effekter eller sends (finns redan), och **ångra** efter borttagning (kanalens effekter, EQ, kompressor, nivå och sends ska kunna återställas).
- **Brödsmulan i displayen visar hela vägen** (Tobias 28 okt): idag `KICKBASE > INSTRUMENT` (`TabbedPanel.h:224-227`, bara huvudfliken). Ska bli `> INSTRUMENT > INSTR 3 > OSC 2`: ta bort ordet KICKBASE, behåll ">" först, och lägg till instrumentet och den inre fliken. Kräver att de inre flikarna (instrument, OSC/FILTER/FM/..) anmäler sitt namn uppåt till brödsmulan.
- **HELP-knappen ligger över tidsetiketterna** i INSTR-flikens envelope-diagram (Tobias 28 okt): `Modules.cpp:1957` placerar knappen 8 px under diagrammets överkant, 16 px hög, och `EnvelopeView.cpp:95` ritar etiketterna (10ms, 50ms ... 800ms) i samma rad. Flytta knappen utanför diagrammet eller under etikettraden.
- **Flikordning i instrumentet** (Tobias 28 okt, projektledaren håller med): `INSTR | OSC 1 OSC 2 OSC 3 SAMPLE FM | FILTER MIX TIMING`, med en enkel avdelare efter INSTR och efter FM. Inga gruppnamn (tar plats). Kontrollera att inget sparat flikindex eller hjälpavsnitt beror på ordningen.

(f) Ljudbild-diagram längst ner i INSTR (Tobias 28 okt kväll): ingen ny flik. Korta ENV-diagrammet i höjd, krymp Phase-raden, nytt diagram över hela bredden. Steg 1: tidslinje med en bana per motor (OSC 1-3, SUB, NOISE, SAMPLE, FM), linjal i ms och takt/beat (värdens tempo), gemensam kontur från Layer Out/volume envelope, allt ur parametrar. Steg 2: verklig sammanlagd vågform via offline-render i bakgrunden efter paus (finns som render_preview). Löser samtidigt HELP-knappen över tidsetiketterna (d).
Tillägg till (f): diagrammet ska visa fasen (Phase: polarity, start, nudge) så att man ser effekten direkt när man vrider på Phase-knapparna. I steg 1 syns det som förskjutning av kick-/motorbanornas start (nudge i ms) och inverterad kontur (polarity); i steg 2 syns det i den verkliga vågformen (start-vinkeln ändrar hur vågen börjar).
Ändrad prioritet för (f) (Tobias 28 okt kväll): verklig vågform först (steg 2), tidslinjen med banor efter. Fasen syns tydligast i vågformen (start-vinkel, nudge, polaritet). Kräver zoom: förvalt fönster första 20-50 ms, val 50 ms / 200 ms / hela, annars blir vågen en tät yta och fasen syns inte. Kort ton renderas på några ms; hela längden räknas om efter paus.

(g) Knappar i INSTR: etiketten ligger långt under ratten (Tobias 28 okt kväll). Orsak: `KnobComponent::resized` (`Components.cpp:104-110`) fäster värde- och namnetikett i cellens botten och centrerar ratten i resten, så extra cellhöjd blir glapp. Åtgärd: sätt ratt + etiketter som ett block (eller sänk radhöjden i INSTR-panelerna), så att höjden går till diagrammet (f). Kontrollera alla paneler som använder KnobComponent, ändra helst på ett ställe. Ingen ändring av parametrar.

(h) OSC-panelerna: växla diagrammet TABLE | OUTPUT (Tobias 28 okt kväll). TABLE = dagens wavetable-vy (förvald). OUTPUT = oscillatorns faktiska utsignal, två perioder, med Warp, Phase, Unison och Detune applicerade. Återanvänder per-motor-renderingen från (f), så bygg dem i samma pass; renderingen ska vara en delad hjälpfunktion, inte två kopior. Ingen ny parameter (vyvalet kan sparas som UI-läge, inte i presets).

(i) SAMPLE: rattarna i AMP- och PITCH-raderna är för små (Tobias 28 okt kväll). Orsak: `editRow` i `SamplePage::resized` (`Modules.cpp:1860-1866`) ger varje rad 54 px och 70 px per ratt, och 26 px av radhöjden går åt till etiketterna, så ratten blir ca 28 px. Åtgärd: samma rattstorlek som övriga paneler (minst ~48 px), t.ex. radhöjd ~80 px och mindre vågform, eller placera AMP och PITCH sida vid sida i högerkolumnen. Gör tillsammans med (g).
Preciserat (i) (Tobias 28 okt kväll): lägg om SAMPLE-redigeringen som tre rutor i INSTR-stil under vågformen, med rubrik och egen på/av-knapp: PITCH (semis, time, shape), AMP (time, shape), REVERSE (smal ruta, bara växel). Ordning efter ljudets väg. Rattar ~48 px med tät etikett (se g). Vågformen blir lägre. Hjälptexten behålls som verktygstips; rubrik med kort förklaring. Högerkolumnen (LEVEL/START/ROOT, LOOP/KEY, SLICES) oförändrad.

(j) FILTER-fliken (Tobias 28 okt kväll): (1) huvudströmbrytaren (`filterOnButton`, `FilterPanel.cpp:242-246`, placerad `:394`) ska flytta upp i flikknappen FILTER, som OSC/SAMPLE/FM har sin; samma parameter (F::filterOn), ingen ny. (2) Glappet mellan ratt och etikett (som g). (3) Rattar för små: P1 och P2 (och CUTOFF-rattens inre display) är mycket mindre än RESO/KEYTRK; alla rattar i ett filterblock ska ha samma storlek, ~48 px. (4) Filter 2-strömbrytaren (`block.power`, `FilterPanel.cpp:319`) är för liten: minst 22 px hög, som övriga växlar. (5) Rattar som är avstängda (P1/P2 för modeller som inte använder dem) får gärna vara nedtonade men lika stora. Gör i samma pass som (g) och (i).

(k) FM-fliken: diagram (Tobias 28 okt kväll). Under rattarna två diagram: (1) vågformen (OUTPUT, två perioder) med Index/Ratio applicerade, delar renderingen med (f) och (h); (2) Index över tid: kurva från INDEX + ENV AMT som faller över ENV TIME, räknas ur parametrarna (ingen rendering, kan göras först). Rattstorlek enligt (g). Hjälptexten längst ner kan bli verktygstips.

(l) TIMING-fliken (Tobias 28 okt kväll): rattar för små och göms bakom andra objekt (7 rutor x 6 kontroller). Förslag, "ett diagram, en vald motor": stort diagram överst med en färgad bana per motor (OSC 1-3, SAMPLE, NOISE, SUB, FM) och linjal i ms/takt; klick på en bana väljer motorn; under diagrammet visas bara den motorns kontroller (MODE, POS DELAY, LENGTH, 2 x SYNC, CURVE) med stora rattar. Dra i banan: startkant = POS DELAY, slutkant = LENGTH (rattarna finns kvar för exakta värden). Samma diagramkomponent som (f) steg 1: bygg den en gång, TIMING = redigerbar, INSTR = skrivskyddad med klick som hoppar till TIMING. Banorna ritas ur parametrarna (ingen rendering). Inga nya parametrar; befintliga timing-parametrar och deras ID:n oförändrade (hash!).

(m) MIX-fliken kompaktare (Tobias 28 okt kväll): kombinerad våg som alltid är bred längst upp men lägre, knapparna under; ersätter WIDE/NARROW. Etiketterna ligger för långt under rattarna även här (samma mall som g).
(n) Instrumentets översta rad (på/av, INSTR-väljare, +/-, Preset, FX, NONE): göres högre med luft över och under knapparna (padding/marginal i höjd), så att raden inte ser trång ut.
(o) Avdelare under huvudflikraden (PATTERN | KICK | INSTRUMENT | ANALYZER | AI CHAT | LIVE CODE | brödsmula) (Tobias 28 okt kväll): en tydlig linje med lite luft under raden, så att toppmenyn skiljs från innehållet. Samma stil som avdelarna i instrumentets flikrad.
(p) Edit harmonics för FM-bärvåg och modulator (Tobias 28 okt kväll): Arbetare 1 läser först FM-koden och kollar om bärvåg/modulator är enperiodsvågor. Sample får ingen övertonsredigering (inte en period).
(q) Övertonsredigering av hela instrumentets våg (Tobias-idé, osäkert): summan är inte en fast period (filter, envelopes, fler oscillatorer, FM, sample), så direkt redigering går inte. Möjlig väg: "BAKE TO OSC", rendera en period av summan (via delade renderhjälpen i (f)) till en ny våg i en oscillator, som sedan går att redigera i övertonsredigeraren. Plan först.
(q, beslut Tobias 28 okt kväll) BAKE TO OSC ersätter de andra källorna: en period av oscillatorsumman (före filter, envelopes, FX) läggs i OSC 1, övriga källor slås av, ett ångra-steg. Bekräfta-dialog först som säger vad knappen gör (vilka källor som slås av, att unison/detune/FM/rörelse blir en stillbild, att det går att ångra).
(r, Tobias 28 okt) Övertonsredigeraren behöver förklaringar, den är avancerad: (1) tooltips på allt man ändrar (FRAME/All frames, penselval DRAW/LINE/SMOOTH/RANDOM, antal övertoner, PRESET, de tre rutorna WAVEFORM/dB/PHASE, staplarna: mittlinje 0 dB, uppåt +12, längst ner borttagen, grå = före); (2) en HELP-knapp i fönstret som öppnar en kort förklaring (vad en wavetable och en frame är, hur man tar bort grundtonen, vad fasen gör). Bara UI-text, inga nya parametrar. Tobias testade redigeraren i Lemon: den fungerar.
(s, Tobias 28 okt) FX-ratten på varje rad i PATTERN (StepSequencer.cpp:31-33, 159) styr bara den gamla länken (en kanal + belopp, fxAmount). Med send-matrisen (ett belopp per FX-kanal) stämmer den inte. Förslag: ta bort ratten och visa samma sammandrag som i instrumentets flik ("FX1 50% FX3 30%" / "None") som öppnar send-popupen (FxSendControl, Components.cpp:495-740). Arbetare 1 läser först hur motorn räknar när både gamla länken och matrisen är på, och om gamla länken kan migreras. Rubriken "FX" blir "FX SEND". Tooltipen "pick the channel in the top right corner" tas bort.
(s, beslut Tobias 28 okt, ersätter förslaget ovan) (1) FX-ratten tas bort från raderna i PATTERN och VOL-ratten blir större i stället. Ingen ersättningsknapp på raden. (2) Sammandraget "FX1 50%, FX3 50% ..." får aldrig plats i FX-knappen i instrumentets flik. Knappen visar bara hur många kanaler som är på: "3 x FX" (och "FX" eller "None" när inga är på). Vilka kanaler och belopp syns i popupen (en rad per kanal, FX1-FX8, kryssruta + reglage + %), eventuellt som tooltip på knappen. Rubriken över ratten på raden försvinner med FX-kolumnen. Arbetare 1 kontrollerar att inget annat läser fxAmount-ratten på raden innan den tas bort (gamla länken, MCP-verktyg, hjälptexter).
(l, tillägg Tobias 28 okt) De små diagrammen i TIMING-fliken idag är mycket tydliga: man ser vid uppspelning exakt hur långt varje källa hinner spelas. Behåll det. Det stora diagrammet för den valda motorn ska ha samma stil och samma uppspelningsmarkör (vit linje = senaste noten, streckad = släppt), bara större, med POS DELAY/LENGTH/CURVE ritade i det. De små diagrammen för alla sju motorer får finnas kvar som en rad (miniatyrer, klick väljer motor) så överblicken inte går förlorad. Rattarna för den valda motorn läggs under det stora diagrammet och är större.
(l, tillägg 2, Tobias 28 okt) Två lägen i TIMING, användaren väljer: OVERVIEW (dagens sju rutor med eget litet diagram, men med större och fasta rattar) och FOCUS (stort diagram för vald motor + stora rattar under, miniatyrrad för att byta motor). Växlare högst upp i fliken. Valet är UI-läge (ingen ljudparameter, inga nya ID:n/hash), minns senast valda. Båda lägena delar diagramkomponenten och rattmallen från (g), så kontrollerna är lika stora och aldrig dolda. Bygg FOCUS först, behåll OVERVIEW som val.
(l, tillägg 3, Tobias 28 okt) FOCUS visar samma sammanlagda diagram som INSTR (f): en bana per motor + gemensam kontur. Vald motors bana är större och markerad och redigeras med drag (POS DELAY/LENGTH); klick på annan bana byter motor. Miniatyrraden i tillägget ovan utgår (banorna ersätter den). INSTR visar samma komponent skrivskyddat, klick hoppar till TIMING.
(t, Tobias 28 okt) JOG-popupen: av/på-knappen för test-BPM (strömknappen) ska ligga nära BPM-rutan (140) och ha en etikett, t.ex. "TEST BPM" ovanför eller bredvid, så man förstår vad som slås på. Etiketten förklarar också att rutan är BPM som används när DAW:en står still. Bara layout och text. Arbetare 1 läser JOG-koden först för att se exakt vad knappen styr, så etiketten blir rätt.
(u, Tobias 28 okt) Förhandsspelning (preview) ska vara exklusiv. När man trycker på en lokal preview (högtalarknappen i en flik, t.ex. den blå vid TIMING, eller en not/skiva) medan huvud-PREVIEW uppe i hörnet kör hela mönstret: huvudpreview stoppas och knappen släcks, och den tryckta previewn spelas så länge man håller ner. När den är klar/släppt går det inte tillbaka automatiskt till huvudpreview. Gäller alla förhandsspelningar mot varandra (huvud, tangent/not, skiva, JOG, flikens högtalarknapp): en i taget. Arbetare 1 listar först alla ställen som startar ljud utan DAW (en gemensam "preview-ägare" i processorn är troligen enklast) och testar att inget fastnar på (hängande ton).
(t, tillägg Tobias 28 okt) JOG-popupen får en KNOB för test-BPM bredvid BPM-rutan (samma parameter, rutan och ratten följer varandra; dubbelklick nollställer). Ordning: av/på med etikett "TEST BPM", ratt, ruta. Ratten byggs med den gemensamma rattmallen (g).
(v, Tobias 28 okt) MIDI IN: INSTRUMENT i pianorullen (INSTR 1) men tangentbordsspel startar ändå kicken. Trolig orsak, att bekräfta: InstrumentProcessor.cpp:652 släpper bara igenom MIDI IN medan värden står still (clock.isHostPlaying() ger return false), och rad 1174 (onlyRow) gäller likadant. Spelar Lemon sitt transport, går tangenttryckningen som en vanlig värdnot och startar hela mönstret inklusive kicken. Önskat: valet INSTRUMENT ska gälla även när värden spelar (bara vald rad låter, mönstret startar inte), och ALL ger dagens beteende. Arbetare 1 reproducerar först (Lemon stoppad respektive spelande) innan koden ändras, och kollar att värdens egna toner till plugin-instansen fortfarande fungerar när pianorullen är stängd.
(v, tillägg) Tobias: även de andra raderna (synthar) spelar, alltså som om ALL vore valt. Koden för valet är i sig riktig när värden står still (PianoRoll.cpp:245 sätter midiInLayer, InstrumentProcessor.cpp:630-673 spelar bara vald rad). Kvarstående misstänkt: värden spelar, då medvetet return false (rad 653) och onlyRow = -1 (rad 1174). Beslut behövs: ska INSTRUMENT gälla även när värden spelar? (Tobias vill det.)
(v, tillägg 2) Tobias: Lemon spelade inte, allt skedde i pluginet, och ändå spelar alla rader. Då är hostPlaying-teorin osäker. Två misstänkta, båda att mäta (inte gissa): (1) Lemon rapporterar position->getIsPlaying() = true även när den står still (TransportClock.cpp:19), då hoppar InstrumentProcessor.cpp:653 över MIDI IN; (2) midiInLayer blir -1 trots öppen rulle, t.ex. isShowing() falskt i Lemons fönster (PianoRoll.cpp:245). Arbetare 1: logga tillfälligt hostPlaying och midiInLayer vid note-on, be Tobias spela, ta bort loggen.
(v, avslutad 28 okt) Tobias: inget fel. Datortangenterna fångas av Lemon, inte av pluginet, och Lemon spelar hela instrumentet. Ingen kodändring och ingen logg. Om Tobias senare provar med ett riktigt MIDI-keyboard (Lemon stoppad) ska bara vald rad låta; hittas något annat då öppnas punkten igen.
(w, Tobias 28 okt) Pianorullens lanes (PianoRoll.cpp:1959 tips, :2005 showLaneMenu, :2028 addLane; värden i note.lock[param], :1990). Önskemål:
 1. Välj lane direkt (VOL, FILTER CUTOFF, PAN, FX) i stället för en liten knapp som skapar en ny; alla fyra synliga som flikar/val.
 2. Meny OTHER med ALLA parametrar som går att ändra per ton, inte bara de fyra (kräver fler lås per not; ändrar mönsterdatats format, så projektets laddning/sparning och gamla projekt måste klaras).
 3. Ta bort en ändring på en enskild ton (tillbaka till ratten, "inget lås" finns redan som prick, rad 1992) och ta bort hela lanen, även med innehåll (nu finns bara "Remove this lane", rad 2011).
 4. Rita med BEZIER-kurva i lanen, utöver värde per ton.
 5. Kopiera hela lanen som automation till DAW med de utsatta värdena.
 6. FX-lane: välj vilken FX-kanal (FX 1, FX 2 ...) lanen styr, inte bara "Effect send". Per-not-värde eller kurva ersätter den generella sändningen för den noten/tiden (fungerar som automation); frågan om ersätta eller multiplicera ska Tobias svara på.
Obs: nya parametrar och modulationskällor sist (ENGINEERING.md). Stor punkt; Arbetare 1 delar upp: (a) val + ta bort, (b) FX-kanalval, (c) bezier, (d) OTHER + dataformat, (e) kopiera till DAW.
(w, tillägg) Tobias: knappen "+ LANE" (längst ner till vänster i pianorullen, liten) måste bli betydligt större, i punkt 1 (val av lane).
(w, rättelse) Tobias: "+ LANE"-knappen försvinner när lanen väljs direkt (punkt 1), så ingen större knapp behövs. Tillägget ovan gäller inte.
(w, beslut) Tobias: lane-värde ERSÄTTER den generella FX-sändningen (inte multiplicera). Därför måste det vara enkelt att ta bort hela lanen och enskilda utsatta värden (punkt 3 prioriteras: ett tydligt val, t.ex. dubbelklick/Delete på ett värde och en synlig knapp för hela lanen).
(x) Tobias: SWING, HUMAN och DELAY (Modules.cpp:2464-2467, 2838) är för små; större rattar, helst en per rad. Hör till den gemensamma ratt/etikett-mallen (g), Arbetare 1.
(y) Tobias: kickens två på/av-knappar nere till vänster (synth/sample) tas bort; de ersätts av en MIX-knapp (synth-sample, 0-100 %) mellan flikarna SYNTH och SAMPLE. Hjälptexten (MIX/MODE) uppdateras. Dessutom: ENVELOPE-visningen ska alltid synas överst i kick-synthfliken (nu är ytan överst tom). Inga nya parametrar om befintligt mix-värde räcker; annars sist (ENGINEERING.md). Arbetare 1.
(y, placering) Tobias: MIX-knappen sitter i menyraden i kickfliken, längst till höger, till vänster om FX, med etiketten "Synth/Sample-mix".
(y, utrymme) Tobias: när på/av-knapparna och raden nere till vänster (KEY, SAMPLE-text, hjälptext) försvinner/flyttas frigörs ytan; flytta ned och förstora rattarna/panelerna i kick-synthfliken så att hela höjden används (hör ihop med mallen g).
(z) Tobias: samma luft och avdelare som (n) och (o) i kickens menyrader: raden SYNTH/ENVELOPE/SAMPLE/PRESET/FX och underraden PITCH/AMP/BEZIER ska bli högre med marginal i y-led, och avdelare mellan raderna och innehållet. Gäller alla flikrader i hela pluginet, via den gemensamma mallen (g).
(z, utvidgning) Tobias: gäller ALLA menyer i hela pluginet, särskilt översta undermenyraden i varje flik (kick, instrument, övriga), inte bara kick. Tydligare delning mellan menyer, och tydlig skillnad huvudmeny / undermeny / underunderrad (olika höjd, bakgrund, avdelare, aktivt läge). Dessutom en snygg hover-effekt på alla menyobjekt (flikar, knappar i rader, popupmenyer). Allt via en gemensam komponent/mall.
(å) Tobias: mer glow-effekter i designen: svag glöd runt kanter på paneler/kort så att man ser vad som hör ihop, och på avdelare (separators). Del av den gemensamma mallen (g/z): en färg- och glödstil som alla paneler använder. Håll det diskret och billigt att rita (ingen tung blur per frame; förrendera vid behov). Arbetare 1.
(ä) Tobias: PREVIEWS-delen (beat-räknare, nu med röd border) får glasdisplay-design: se docs/work/skisser/previews-glas.png (skiss) mot nuläget previews-nu.png. Knapparna (preview-knappar, tonväljare C5, klocka, retur) ska se ut som glasknapparna intill glasdisplayen (BPM/takt). Master-volymrattan (OUT) ska bli stor. Del av mallen (g/å). Arbetare 1.
(x, förtydligande) Tobias: SWING, HUMAN och DELAY får en egen rad vardera i vänsterrutan (en ratt per rad, etikett vid sidan, värde under/intill), inte tre i rad. Flytta ned/anpassa rutan (GRID, PATTERN PRESET, FILL, pianoknappen) så att höjden räcker.
(prioritet 28 okt) Tobias: bygg å (glöd), ä (glas-PREVIEWS) och z (menyer/hierarki/hover) först, före övriga rattstorlekar.
(mellanrum) Tobias: mer margin och padding överallt (paneler, rader, rattgrupper, knappar). Sätts som gemensamma konstanter i mallen (g/z) så att det går att justera på ett ställe.
(å, inställning) Tobias: glödstyrkan blir en användarinställning i SETTINGS: Av / Svag / Stark. Sparas som plugin-inställning (inte i projektets parametrar/preset, ingen ändring av parameter-ID/hash). Standard: Svag. Ändring ritar om hela GUI:t direkt.

(0.4-snittet, beslut Tobias 28 okt) 0.4 handlar om design och diagram. Med: allt ovan utom (p), (q) BAKE TO OSC och (w) pianorullens lanes. FX-punkt 2 (ta bort FX-kanal i send-popupen, f3b1df0) är med. FX-punkt 4 (lane-värde ersätter matris-send) och (w) skjuts till 0.5; Arbetare 2:s ocommittade ändringar för punkt 4 ligger kvar i KickBase-VST-w2 (se PROGRESS-w2.md).

(Tobias 29 okt, till 0.5) Filterdiagrammet ska visa allpass-filtret: en tunn extra kurva för fas eller gruppfördröjning, så att AP FREQ och AP AMT syns som en puckel vid vald frekvens. Idag syns inget, eftersom diagrammet bara visar amplitud och allpass bara ändrar fas.

(0.5, ny punkt, Tobias 28 okt) Riktig grafik: bakgrundstexturer (och senare ikoner/rattar) som bilder från Gemini i stället för enbart kodritat. Läggs in via BinaryData. Första steg: texturer bakom paneler/moduler; accent- och glödfärger ritas fortfarande i koden så att lagerfärgerna fungerar. SVG för ikoner, PNG i 2x för texturer. Licens: bilderna får användas fritt (AGPL). Underlag/brief till Gemini: docs/work/gemini-grafik-brief.md. Inget av detta ingår i 0.4.

(0.5, ny punkt, Tobias 29 okt) GATE FX: en gate som effekt i FX-kanalerna, med en egen rad i mönstret ("GATE FX #1"). Raden har inget ritat mönster: gaten är på/av efter vad som markeras i pattern-fliken, styrs som dagens gate (djup, smooth). Före/efter reverb avgörs av placeringen i kanalens kedja, så GATE PRE/POST FX stryks. Dagens instrument-gate lämnas som den är. Att utreda först: om radregistret (RowRegistry) tål en rad som hör till en effekt. Obs: gaten klipper hela kanalens retur (även andra instrument som skickar dit); skriv det i hjälptexten.
(0.5) Riktig FM-visning i OSC-diagrammet: rita vald modulators våg (OSC 2/3) i stället för torr våg (0.4 visar bara text och varning). Kontrollera Resources/AGENT_GUIDE.md för gamla standardvärden (cutoff 300 Hz, SUB 30 %).

## Tillägg 29 okt kväll (Tobias, efter test)
0.4:
- MAX-rutan (TIMING, "MAX 10 s - +") ska se ut som övriga rutor: label ("MAX LENGTH"/"LENGTHS") och border; högre max (t.ex. 30 s). Flytta ner t.ex. FILTER ENVELOPE hellre än att placera MAX högst upp.
- PRESET-knappen på INSTRUMENT-huvudfliken är orimligt bred (som kickens: smal) och menyn ska öppnas centrerad mot knappen, inte längst till vänster.
- HELP-knappen i env-diagrammet (INSTR) ligger ivägen: flytta till hörnet/bredvid, gärna större.
- SUB START i MIX-fliken ändrar knappt ljudet: undersök (sinus med fasförskjutning är nästan oförändrad; ev. OK).
- WARP ENV: utred vad den gör, om den gör något, och hur den kan visualiseras.
0.5: (HOLD struken 29 okt: sustain styrs av notlängden, NOTE OFF) ATTACK på pitch, SUSTAIN på filter env; sammansatt våg med zoom på INSTR.

### MAX LENGTH per sak (Tobias 29 okt kväll, 0.4, del av G1)
- Dropdown i MAX LENGTH-rutan väljer vilken grupp man ändrar: ENV 1 (standard), PITCH, FILTER ENV, FM. Varje grupp får eget steg-param (nya parametrar sist, gamla projekt = nuvarande maxTime-värde).
- Idag styr ett gemensamt steg sex knobbar (`Params.h:199-224`).
- 30 s kräver ändrat APVTS-intervall (idag 10x legacyMax) och skiftar normaliserade värden: Arbetare 1 skriver kort plan och rapporterar före bygge.

### EDIT HARMONICS-knapp (Tobias 29 okt kväll, 0.4, task G6)
- Knappen "Edit harmonics..." ska alltid synas i OSC-flikarna, i det lediga hörnet nere (under PHASE/UNISON/RAND), högre knapp med EDIT på rad 1 och HARMONICS på rad 2.

### MAX LENGTH visas som gånger (Tobias 29 okt kväll, ändrar G1)
- Visa som multiplikator "x1" ... "x10" (max x10 räcker, 30 s utgår = ingen intervallrisk), plus resulterande längd bredvid/under med "=" (t.ex. "x2 = A 100 ms · D 4 s · R 0.6 s").
- Dropdown per grupp kvarstår (ENV 1 standard).

### AUTO FIX till 0.5 (Tobias 29 okt kväll)
- Mätdelen (`AutoFix.cpp`) klar; UI, APPLY och koppling från analyskörning görs i 0.5 tillsammans med GATE FX. Arbetare 2:s oincheckade ändringar (FX punkt 4 m.m., worktree KickBase-VST-w2) följer med 0.5.

### SIDECHAIN in i MASTER (Tobias 29 okt kväll, 0.5 om inte annat sägs)
- SIDECHAIN blir en under-flik i MASTER, mellan MIXER och MAXIMIZER (MASTER har redan under-flikar). Huvudfliken SIDECHAIN tas bort i INSTR och FX (`InstrumentEditor.cpp:67`, `FxEditor.cpp:30`). Kolla hjälpavsnittet (`HelpDialog.cpp:524`), sparad flikstate och färg (Theme::mix → master).

### SIDECHAIN i MASTER flyttas till 0.4 (Tobias 29 okt kväll, task G7)
- Gäller punkten ovan, nu 0.4. Brödsmulan (sökvägen i displayen) ska stämma: MASTER > SIDECHAIN (jfr bdda107). Kontrollera i både INSTR och FX.


- (Tobias 29 okt, till 0.5) ENV-fliken ([EnvPage.cpp:102-241](Source/Ui/EnvPage.cpp)) ska vara samma envelopes som i INSTR-fliken och se likadana ut: samma knappar/parametrar, och MAX LENGTH (dropdown + xN + = A/D/R) för alla fyra (ENV 1, FILTER, ENV 2, ENV 3), inte bara en visnings-DURATION-knapp (EnvPage.cpp:121-134, bara vyn). Klargör med Tobias om INSTR-fliken och ENV-fliken ska dela kod/komponent.

- (Tobias 29 okt, ALLT i 0.5) GLOBALA ENV: ENV-fliken blir 4 globala envelopes (ENV 1-4) som rader som LFO-fliken, inte kopplade till instrument. MAX LENGTH (xN, max x10) följer med envelopen.
  - Högerklick på envelope-grafen i INSTR-fliken: kopiera till INSTR 2/3/4.., kopiera till GLOBAL ENV plats 1-4; GET FROM GLOBAL 1-4; GET TO PITCH / FILTER ENV / AMP ENV.
  - Dra en global ENV till mål: pekaren grön om släpp går, röd annars; rattar/grupper som tar emot ENV får grön border under drag. Släpp på INSTR-graf frågar vilken (AMP/PITCH/FILTER). Dra även till knobbgrupper (Arbetare 1 listar vilka grupper som kan ta emot ENV).
  - LFO/makro på ENV:ets egna rattar (automation), sist.
  - Öppna frågor till Tobias: kopia (föreslaget) eller live-länk; gamla per-instrument ENV 2/3 konverteras till globala vid laddning, gamla params kvar (nya sist, ENGINEERING.md); varje ton startar sin egen kopia av den globala ENV.
  - KRAV: MAX LENGTH-inställningen (källa + xN) är en del av envelopen och följer med vid ALLA kopieringar/hämtningar/drag (INSTR<->GLOBAL, mellan instrument, GET TO AMP/PITCH/FILTER). Testa: kopiera ENV med x10, målet ska visa x10 och samma A/D/R.
  - KRAV: alla rattar i varje global ENV ska ha "Create automation clip" (högerklick, finns redan i Components.h:12) och "Link to controller", precis som övriga rattar. Gäller även MAX LENGTH-rattarna och multiplikatorn.
- (Tobias 29 okt, till 0.5) PITCH och FILTER ENVELOPE får full ADSR + AMOUNT, så att globala ENV passar överallt. Idag: filter har attack/decay/amount (envAttack, envDecay, envAmount), pitch bara amount/decay (pitchEnvAmount, pitchEnvDecay, Params.h:127). Saknas: pitch attack/sustain/release, filter sustain/release (ersätter punkterna ATTACK på pitch och SUSTAIN på filter ovan). Nya params sist (docs/ENGINEERING.md); defaults så att gamla presets låter likadant (sustain 0, release som dagens beteende). Varje ratt får Create automation (se krav ovan).

### 0.5: XY-platta per filter (Tobias 29 okt)
- FILTER 1 och FILTER 2 får var sin XY-platta (som Fruity Philter). X och Y kan kopplas till valfri parameter via rullista; standard X = CUTOFF, Y = RESO.
- Plattans X/Y är vanliga automatiserbara parametrar (nya parametrar sist, docs/ENGINEERING.md): "Create automation clip", "Link to controller", inspelning av automation och LFO/makro på X och Y (t.ex. LFO 1 på X, LFO 2 på Y) ska funka som på alla andra rattar.
- Förslag: plattan är ett andra handtag på de två valda rattarna (rattarna rör sig med), inte en separat modulering ovanpå.
- Fyra kvadranter (olika saker under/bakom axlarna): senare, bara om det behövs; X/Y-tilldelning täcker det mesta.
- Öppen fråga: ska plattan ligga bredvid rattarna eller ersätta filterdiagrammet när den visas?
- Placering (Tobias): en knapp i övre högra hörnet på varje filterdel (FILTER 1 / FILTER 2) som öppnar XY-plattan. Svarar på frågan ovan: plattan öppnas via knappen, inte permanent bredvid rattarna.
- Beslut (Tobias): inga fyra kvadranter; plattan är andra handtag på rattarna (rattarna rör sig med).

### 0.5: MODIFIERS per instrument i INSTR-fliken (Tobias 29 okt)
- Nytt avsnitt (arbetsnamn MODIFIERS) i INSTR-fliken med små effekter på hela instrumentet. Först ut: VIBRATO (RATE, DEPTH, DELAY/fade-in, SYNC).
- Kandidater i samma kategori: TREMOLO, CHORUS, AUTO-PAN. Tobias väljer vilka som kommer med.
- Varje modifier har ON och egna rattar med automation/LFO/makro som alla andra rattar. Nya parametrar sist (docs/ENGINEERING.md); default AV så gamla presets låter likadant.
- Varning för bas: chorus/panorering kan förstöra monokompatibilitet och fas mellan kick och bas. Förslag: lågpass-skydd (wet filtreras bort under ca 150 Hz) och kollas i mix-rapporten.

### 0.5: START-fasvågorna i tidsdiagrammet (Tobias 29 okt)
- De små sinusvågorna (TimelineChart.h:175-200) behålls: de visar källans startfas (START + PHASE/SUB/FM-fas, vänd av POLARITY) med en fuchsiaprick vid startpunkten.
- Gör dem begripliga: etikett "START PHASE" med gradvärdet bredvid vågen, och tooltip som förklarar vågen och pricken.

### 0.5: TIME/GATE-effekt à la Gross Beat + 10 FX-slots (Tobias 29 okt)
- Ersätter "GATE FX som egen rad": EN effekt (arbetsnamn TIME GATE) som öppnas i en egen ruta/fönster. Stor redigerbar kurva (volym över tid, punkter med linjer), TRIG SYNC / POS SYNC, ATT / REL / TNS, preset-rutnät (Basic, Gate, Trance Gate, Sidechain, Tremolo m.fl.). Alla rattar är vanliga parametrar (automation/LFO/makro).
- Preset-byte med automation: en parameter "PATTERN" (val 1..N) som kan automatiseras; vid byte laddas mönstret på nästa taktgräns (inget klick). Mönstren sparas som data i preset/projekt, användarens egna med.
- Utöka FX-kanalens effektkedja till 10 slots (idag: kolla antal i koden). Nya slots sist (docs/ENGINEERING.md), gamla presets oförändrade.
- Ärva: Arbetare 2:s FX-punkt 4 och AUTO FIX hör ihop med FX-kedjan.
- KORRIGERING (Tobias 29 okt, efter sökning om Gross Beat): effekten har TVÅ delar, inte bara volym.
  - VOLUME: volymkurva över tid (gate, trance gate, tremolo, sidechain-känsla), 0-100 %, RELEASE 0-1000 ms, TENSION -100..100 %.
  - TIME: ljudet spelas från en rullande buffert (2 takter). Kurvans lutning styr uppspelningen: lutning = normal fart, vågrät = paus/stutter/repeat, brantare = snabbare, negativ = baklänges, långsam lutning = tape stop/slow-down. Ger stutter, reverse, scratch, tape stop.
  - Egna mönster-platser (36 time + 36 volume) som kan länkas ihop (Time N + Volume N) och väljas via MIDI-not eller automation. Klockratt för manuell scratch. Click reduction, smoothing och DC-borttagning.
  - Mål: Kickbase ska kunna göra samma sak, i egen ruta, med vår kurvvy; rullande buffert i FX-kedjan.
- Gross Beat, kontroller (skärmbild 29 okt): TIME- och VOLUME-knapp (mängd), scratch-klocka, HOLD, preset-rutnät för TIME (Basic, Complex, Chaos) och VOLUME (gates, trance gate, saw gate, fades, sidechain, tremolo m.fl.), ATT / REL / TNS, zoom, rutnätsval vid kurvan (X, 1/16 .. 1/3), kurvverktyg, TRIG SYNC och POS SYNC (t.ex. 4 beats), HOLD av/på. Alternativmeny: Click reduction (Off / Low drums / High melodic), Smoothing attack compensation, Remove DC offset, HQ resampling, View scratching clock.
- TIME-delen är VALFRI: av som standard, bufferten allokeras och körs bara när den slås på. Bygg VOLUME först, TIME efteråt. Mät CPU med TIME på innan den släpps.

### Gross Beat: TIME-kurvan läses så här (rättelse, 29 okt)
- Y-axeln är uppspelningsposition relativt nu (0 = nu, -1 = en beat bakåt i bufferten), X är 0..4 beats. Lutning k ger hastighet 1+k: platt = normal fart, lutning -1 (den diagonala hjälplinjen) = fryst, brantare = baklänges, svagare = tape stop. Lodrätt hopp bakåt = stutter. Det tidigare "platt = paus" stämde inte.
- Tobias TIME-presets (Basic 1-3, Complex 1 och 3, Chaos 5, Chaos 8) är tolkade från skärmdumparna: `docs/work/grossbeat/time-presets.json`, bilder i `docs/work/grossbeat/bilder/`. Chaos 8 är ofullständig. GATE-presets kommer från Tobias.

### Gross Beat: TRIG SYNC och POS SYNC (29 okt)
Båda rullistorna har samma val: Off, 1/4 beat, 1/2 beat, 1 beat, 2 beats, 4 beats (standard 4 beats, = hela mönstret). TRIG SYNC = när ett nytt mönsterlopp får starta efter en trigger (noten/MIDI); POS SYNC = hur ofta mönstrets position låses till DAW:ns takt. Off = ingen låsning. GATE-presets: `docs/work/grossbeat/gate-presets.json`.

### Gross Beat: TNS = spänning på kurvsegmenten (29 okt)
TNS-ratten är bipolär (tooltip "Volume envelope tension: -17%") och böjer alla kurvsegment i VOLUME-kurvan samtidigt, utan att flytta punkterna. Negativ = konvex (långsam start, snabb slut, som Sidechain/Fade Out In), positiv = konkav, 0 = rak linje. Så 'curve' i presetsen är en flagga, och själva böjen kommer från TNS (preset kan spara ett TNS-värde, t.ex. Fade Out In med TNS -17 %). ATT/REL jämnar ut kanterna på gaten (attack/release i ms).

### Gross Beat: markera flera punkter (29 okt, Tobias)
I kurvredigeraren ska man kunna markera flera punkter: klick-och-dra-ruta (select) och markera alla (Ctrl+A). Markerade punkter flyttas och ändras tillsammans (dra en = alla följer; Shift för att lägga till/ta bort en punkt i markeringen; Delete tar bort markerade). Gäller både VOLUME- och TIME-kurvan, och bör gå att återanvända i ENV-redigeraren.

### Gross Beat: Bezier-kurvor (29 okt, Tobias fråga)
Gross Beats orangea handtag är ett handtag per segment (böjer linjen, som tension), inte riktig Bezier. Förslag: 0.5 gör som Gross Beat, ett handtag per segment plus TNS som global böj. Riktig Bezier (två handtag per punkt, S-kurvor, mjuka övergångar) som tillval per punkt ("smooth") i samma redigerare, lagras som kontrollpunkter i presetfilen. Beslut av Tobias: bara ett handtag nu, eller full Bezier direkt?

### Gross Beat: kurvredigerarens beteende, beslut (29 okt, Tobias)
Beslut: full Bezier byggs som ett eget Bezier-läge. Standardläget är ett handtag per segment (räcker i de flesta fall). Bilder: `docs/work/grossbeat/bilder/editor-kurva-handtag.png`, `editor-saw-bygge.png`.
- **Ny punkt:** högerklick i kurvan lägger till en punkt.
- **Mellanpunkt/handtag:** mellan varje par punkter finns ett handtag (liten ring, halvvägs på linjen) som dras för att böja segmentet; ringen följer med när punkterna flyttas.
- **Lodräta hopp:** två punkter på samma x (dra en punkt rakt upp/ner över en annan) ger hopp; så byggs sågtänder och gater för hand. Måste gå smidigt, det är hela syftet (Tobias bygger saw-presets själv).
- **Flervals** (select/markera alla, se ovan) och TNS gäller i båda lägena.

### Gross Beat: snap till takten (29 okt, Tobias)
Kurvredigeraren ska ha snap: punkter och handtag låser sig till valfri del av takten när man drar (av, 1/4, 1/8, 1/16, 1/3, 1/6 m.fl., som rutnätsknapparna vid Gross Beats kurva). Snap gäller x (tid) och bör finnas för y (volym) också, både i handtagsläget och Bezier-läget, och med flervals (hela markeringen flyttas i steg). Alt/Ctrl hållen tillfälligt stänger av snap. Alla Gross Beat-presets i `gate-presets.json` ligger på ett rutnät (1/8, 1/16 eller 1/4), så de kan byggas exakt med snap.
- Y-axeln i bilderna är märkt 0 till -8; nivåerna i Trance Gt 4 och Saw Gate 2 ligger på -1/-3/-5 (0.875/0.625/0.375). Kolla skalan (dB eller linjär) när redigeraren byggs.

## Envelope per lager (29 okt, Tobias)

- Varje oscillator (OSC 1-3, Sample, FM) får en egen envelope, inspirerad av Toxic Biohazard (egen envelope per oscillator).
- Knobbarna i OSC-fliken är redan fulla. Förslag: växlare WAVE | ENV i displayen till vänster (där wavetable visas nu), per oscillator. WAVE är standard.
- ENV-vyn återanvänder kurvredigeraren från Gross Beat (punkter, mittpunkt, snap, flerval).
- Senare, vid behov: en ENVELOPES-vy som visar alla lagers envelopes bredvid varandra.
- Öppet: kolla hur det förhåller sig till ENV 2/3 och amp-envelopen på INSTR-fliken innan det byggs.

## 10 globala ENV (29 okt, Tobias)

- 10 globala envelopes som modulationskällor, samma upplägg som LFO 1-10 (dras till knobbar).
- Startas av varje ton, eget tillstånd per stämma (full polyfoni). Mät CPU före release.
- Lagrets egen volymenvelope (WAVE | ENV i OSC-fliken) är något annat än de 10 globala, men båda använder samma kurvredigerare.
- Gamla ENV 2/3 konverteras till de första globala vid laddning.
- Nya modulationskällor läggs sist (docs/ENGINEERING.md).

### ENV-redigeraren: tillägg från Sytrus (29 okt, Tobias)

- Sustain- och decay-markörer på kurvan (var kurvan hålls medan tangenten är nere, och var den släpper).
- ATT / DEC / SUS / REL-knobbar under kurvan för snabb envelope utan att rita.
- TEMPO-läge: envelopen följer takten i stället för sekunder.
- Vi kopierar inte Sytrus flikrutnät (parameter × källa). Dra-och-släpp av källor på knobbar behålls.

## Från Massive och Sytrus (29 okt, Tobias)

- Mod-markör under varje kopplad knob: liten markör som visar källa och mängd.
- Vel och KTR per envelope. Tooltip måste förklara dem: Vel = hur mycket anslagsstyrkan ändrar envelopens nivå, KTR = hur mycket tonhöjden ändrar den (höga toner kortare, låga längre).
- 10 macros (idag 4, se MATRIX-fliken) i en egen flik, precis som LFO och ENV. Högerklick på varje kopplingsbar knob: CONNECT WITH MACRO 1-10. Dra-ut-sättet finns kvar. Macros visas också som routes i MATRIX.
- MOD OSC (ringmod, fas, position, filter-FM från egen oscillator): eventuell egen flik bland motorflikarna. Arbetare 1 kollar först överlappet med befintliga FM-fliken.
- Inte med: Performer/Step-källor (Gross Beat täcker), Global-flikens Randomize och Copy/Paste av sektion (kanske senare).
- Preset-browser med taggar: senare, med bättre preset-system.

## Randomize och Copy/Paste av sektioner (29 okt, Tobias: ta med)

- Sektion = en flik/del av samma sort: OSC 1-3, SAMPLE, FM, FILTER 1-2, FX-platser m.fl. Copy/Paste mellan lika delar (OSC 1 till OSC 2, FILTER 1 till FILTER 2) och mellan presets. Mellan olika sorter är Paste grå.
- Modulationer (LFO, ENV, macros) på sektionen följer med och kopplas till den nya platsen (bekräftat av Tobias 29 okt).
- Randomize per sektion och för hela ljudet, med procentruta (30 % = liten ändring, 100 % = allt). Lås per sektion så att t.ex. kick och SUB skyddas. Ångra (undo) måste fungera.
- Nya parametrar och källor sist (docs/ENGINEERING.md).

## Från Massive XL (Tobias 29 okt: "allt låter bra")

Med i 0.5:
1. **Lås per sektion vid Randomize** (XL har hänglås över varje sektion) och en Divergence-liknande styrka (%-rutan vi redan har).
2. **Mer i ENV-redigeraren**, men utan röra: Delay, Hold, Shape per segment, sustain-nivå som streckad linje, läget Gate/OneShot. Kravet från Tobias: varje reglage ska ha en tydlig tooltip. Hold (tid kvar på toppen efter attack, räknas i tid) är något annat än Note Off (när tangenten släpps startar REL). Förklara skillnaden i tooltipen. Visa avancerade reglage först när de behövs om det blir för fullt.
3. **LFO:** formväljare som cirkelmeny med slumpruta, Bi/Uni, Delay och Fall/Rise (fade-in).
4. **Historik:** hoppa tillbaka till valfri punkt i Undo-listan.
5. **Sub-presets per modul på fler ställen**: FM, filter, OSC, SAMPLE m.m., inte bara effekter. Samma format som Copy/Paste av sektioner.
6. **Tracker-kurvor** (tonhöjd styr något) som utveckling av KTR.
7. **Fasmodulation mellan oscillatorer**: Arbetare 1 jämför mot FM-fliken innan en ny MOD OSC-flik byggs.

Lämnas: Generate (sampel till preset), Morpher/Animator, öppen routing, Mutator som byter filtertyper, polyfonisk aftertouch.

### Sub-presets och sampel (Tobias 29 okt)
- Ett sub-preset av SAMPLE (och Copy/Paste av den sektionen) måste ta med själva ljudet, inte en filväg. Multisampling och wavetables sparas redan inbäddade (`MultiSample.cpp:413`, `PluginBase.cpp:445`); Arbetare 1 verifierar att SAMPLE-fliken gör likadant.
- Storlek: ett sub-preset med sampel kan bli flera MB. Undersök 16 bitar/mono där det går, och varna om presetet blir stort.
- Licens för medföljande sampel spelar roll först när presets delas.

## Från Serum 2 (Tobias 29 okt: "lägg in det")

Med i 0.5:
1. **LFO-lägen Free / Retrig / Envelope.** Envelope kör LFO:n en gång per ton, som en ENV (bra för ett svep i en kick).
2. **LFO-kontroller:** Rise, Delay, Smooth och Phase, plus Trip/Dot-synk. Rise och Delay är samma sak som fade-in från Massive XL, bygg dem en gång.
3. **VELO och NOTE som egna källor med redigerbar kurva** (kurvritare). Ersätter Vel/KTR och Tracker-kurvorna ovan, bygg en gång.
4. **Routing per källa och filter:** Filter / Main / Direct / None, med blendratt mellan FILTER 1 och 2. Direct går förbi filter och effekter (t.ex. SUB). None gör en oscillator till ren modulationskälla (jämför MOD OSC). Mönstret finns redan som Max-växlarna per källa i `MasterPage.cpp:101`.
5. **SAMPLE-fliken:** loop-lägen (FWD, FWD/REV) och LS/LE-markörer.
6. **Tooltip med modulationskällor:** håller man musen över en ratt står det vilka källor som styr den ("LFO 3, ENV 2"). Kompletterar markören under rattarna.
7. **Init per modul** (nollställ en sektion till standardvärden) och Init för hela ljudet. Hör ihop med Copy/Paste och Randomize.

Redan bestämt och Serum gör likadant: ENV med ATK/HOLD/DEC/SUS/REL, BPM/MS och zoom; macros med egna namn och antal mål; räknare för antal kopplingar på varje källflik; av/på stänger av en hel modul.

Valfritt, inte med utan nytt besked: Chaos-LFO (Lorenz), två warp-platser per oscillator (Arbetare 1 jämför mot OSC/FM först), Voicing med Mono/Legato/Porta/Curve, LFO Host (följer DAW-positionen).

Lämnas: Granular, Spectral, clip-sequencer, arpeggiator.

## Wavetables från Serum-paket (Tobias 29 okt: "de ska in i 0.5")

Underlag: bvker.com "100 Free Serum Wavetables" (PDF). Licensen står inte på sidan, läs https://bvker.com/user-agreement/ innan något av paketet bäddas in i delade preset.

Med i 0.5:
1. **Import av wavetable** från ljudfil (Serum-format, 2048 sampel per ram). Importlägen: konstant ramstorlek, FFT 256–2048 och pitch-följande. Ett kicksampel kan bli en wavetable. Arbetare 1 kollar först om dagens import klarar Serum-filer.
2. **Process-verktyg:** Remove DC Offset och Normalize each (varje våg för sig). Gäller även sampel. Likspänning i en kick-våg ger klick och äter headroom.
3. **Rita och morpha egna vågor:** ritverktyg, rutnät, formelfält, Morph (Crossfade / Spectral) mellan vågor. Stort bygge, tas sist i 0.5.
4. **Baka (resample):** rendera en oscillator med warp/FM till en ny wavetable. Hör ihop med de två warp-platserna ovan.

Ordning: 1 och 2 först, 3 och 4 när resten av 0.5 är klart.

- Tobias 29 okt: letar själv upp wavetable-.wav-filer (Serum-format). Arbetare 1 testar importen mot filer han lämnar (t.ex. i en mapp `docs/work/wavetables-test/`, ej incheckade om licensen är oklar). Serum-presets (.fxp) stöds inte, bara .wav.

- Wavetable-import, nuläge och tillägg (PL läste koden 29 okt, `Source/Core/Wavetable.cpp:693-753`, `fromAudio`): 2048-samples-cykler stöds redan, men hårdkodat. Filen delas i 2048-cykler, varje cykel halveras till 1024, **max 16 frames** väljs jämnt över filen (resten slängs), mono, DC-borttagning och normalisering sker alltid. Kortare filer än 2×2048 blir en enda cykel.
  - **Auto-detektering (Tobias 29 okt: importen ska läsa av vad filen verkar vara):** (1) läs Serums `clm `-chunk i WAV-huvudet (innehåller frame-storleken, oftast 2048) – Arbetare 1 verifierar mot riktiga filer; (2) utan chunk: om längden går upp i 256/512/1024/2048/4096, välj den storlek som ger mjukast skarv mellan frames; (3) annars en enda cykel. Importen visar vad den hittade ("2048 × 64 frames, Serum") och låter Tobias ändra.
  - Att utreda: fler än 16 frames (Serum har ofta 256), och om halveringen till 1024 kostar för mycket i höga övertoner.
