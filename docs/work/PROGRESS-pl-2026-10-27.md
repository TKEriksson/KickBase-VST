# PROGRESS projektledare (kickbase-vst-56), 2026-10-27

## Gjort
- Skrev `docs/masterplan.md` (T1-T15 från Tobias testlista). Specialisten (b5) godkände den med ändringar; alla inarbetade.
- Delade ut: Arbetare 1 = uppspelningspaketet A1-A3, sedan B1; Arbetare 2 = A5, sedan B2/B4.
- Tobias beslut: (1) värdens noter startar alltid mönstret när värden spelar, MIDI IN bara när värden står still (A1); (2) huvudpreset tömmer undo-historiken, del-preset är en transaktion (fas C); (3) Copilot får pusha/mergea bara när Tobias uttryckligen säger det; (4) `installRel.*` hålls utanför repot; (5) gamla worktrees får tas bort; (6) OK på omtolkningen av LOOP/BPM i D3; (7) jog-wheel/scratch (D3b) väntar tills transporten är stabil.
- Tog bort worktrees `-f2, -lc, -lce, -lcui, -mx, -rel` (grenarna finns kvar). Ocommittat och ospårat (utom .log/.obj) säkrades först i `%LOCALAPPDATA%\Temp\claude\worktree-backup-2026-10-27\`.
- Specialisten godkände A1/A2-orsakerna (PROGRESS-w1.md) med justeringar; vidarebefordrade till Arbetare 1.
- Bad specialisten granska allt Copilot gjort; fynd ska in i masterplanen.

## Öppet
- Tobias: `installRel` till `.gitignore`?
- Väntar på: Arbetare 1 (A2-test + fixar), Arbetare 2 (A5), specialistens Copilot-granskning.
- Påminnelse: alla agenter dokumenterar löpande i sin egen `PROGRESS-*.md`.
- 2026-10-27 Täckningskontroll mot testtexten: lade till D3b-detaljer, D4-detaljer, D7 (presets). Alla T1-T15 finns nu med.
- Specialistens Copilot-granskning mottagen: inga allvarliga fel. Lade in Fas H (H1-H5) i masterplanen; H1 (Maximizer MODE-migrering) verifieras tidigt. Perch-buggen = A5 bekräftad. Loggade min trunkering av masterplanen i LESSONS.md.
- Tobias: separata VST:er (VS) struket ur plan-v2 tills allt är klart; han lägger in det själv när det är dags. installRel.* lagt i .gitignore (ej committat).
- Masterplanen: lade in Fas I (resten av plan-v2), uppdaterade Städning, Beslut, Frågor. Inga öppna frågor.
- Arbetare 1 rapporterar: A1 och A2 fixade i worktree, testsvit grön (Debug), ej provat i FL. Fortsätter med T13 (total != rows), sedan A3-diff till specialisten.
- Tobias: releaser bara när stora sektioner klara; första = hela testlistan T1-T15 (fas A-E, H). Fas G uppdaterad.
- Tobias: PL får merga till main efter byggd release + hans test + rättelser; PL påminner och frågar först.
- Lade in avsnittet Arbetssätt i masterplanen.
- Arbetare 2: A5 klar (14c37c2 på w2): appendWorkLesson flyttar uppdaterad post sist. Test i chattest. Fortsätter B4, B2. Obs: chattest slutar annars med 'FEL: chatten misslyckades' (nätverk/nyckel, oundersökt).
- Arbetare 1: T13 kärnan korrekt (kick takt 0/1/2: contentBars=3, lengthBars=3). 'AUTO 2 takter' misstänks vara UI utan patternChanged()/refreshLengths(); behöver FL-test. A3 klar i play-pack (Debug, ocommittat), skickad till specialisten. Fortsätter A4, B1.
- Tobias: alla FL-tester görs när releasen är byggd, inte löpande. Arbetarna väntar inte på FL.
- Specialisten godkände A1-A3. Tobias beslut: PREVIEW har företräde över värdens noter. Skickat till Arbetare 1.
- Arbetare 1: 7d9c736 i play-pack: A1-A3 + PREVIEW-företräde (InstrumentProcessor.cpp ~612, utan enhetstest, processorn saknar testsele) + A4 (InstrumentVoices.cpp:177, alla röster glider; test ackord). Debug grönt. Kvar i A4: stackade extra-glidetoner kan glida förra tonen. FL-testlista i PROGRESS-w1.md. Fortsätter B1.
- Tobias: säg alltid åt arbetarna att fortsätta efter varje rapport.
- Tobias: efter varje release skriver PL en publik PDF (innehåll + FL-test/godkännande). Inlagt i Fas G.
- Tobias: specialisten stängd; arbetarna jobbar långa pass och rapporterar bara när klara, blockerade eller på fråga.
- Tobias: arbetarna ställer alla frågor via PL (inte direkt till honom).
- Tobias: vid fråga/blockerare ska arbetarna gå vidare med nästa punkt medan de väntar.
- Arbetare 1: B1, B3, B5, H2, D6, D3a, A4-resten klara i play-pack (ej FL-testade); global LOOP-knapp beställd. D1: Tobias valde streck mellan toner (högerdrag, Alt+drag fallback). Arbetare 2: B4, B2, H1, H3, H4 klara (5ec5379), D4 steg 1 (be1d320); H5: content-s2 bygger, fix-slicer-test hoppas över (gammal bas, JucePlugin_Name), E2 ej blockerad; .chord tolkas efter Tobias text.
- Skrev docs/work/undo-spec.md (fas C) som specialist; Arbetare 1 informerad, implementerar C.
- Tobias: historikdialog (Meny → History..., Undo/Redo/Restore) i stället för Undo/Redo-rader; inlagt i undo-spec.md.
- Tobias: skicka screenshots till honom när designen (E1) är klar. Arbetare 2 tar fram dem med snapshots.ps1; PL skickar med SendUserFile.
- Tobias (sessionen 93 % använd, reset 12:40): Arbetare 1 pausad så att designen (Arbetare 2, E1) går först. Väcks efter reset eller när Tobias säger till.
- Arbetare 1 väckt efter att bilderna skickats. E1 underkänd av Tobias (bara flikraderna ändrade, knobbar/färger/diagram som förut); Arbetare 2 fick full designlista (knobbar, färger, stem, rubrikrad, kontroller, diagram) och ska jämföra mot skissen innan nya bilder.
- Arbetare 2: own-piano-roll-spec.md godkänd, tak 16 skivrader OK med UI-varning; rendersig-trio + blocksizetest + audiotest krävs. RING (B): tydlig UI-text + val OSC2/OSC3/båda inlagt i masterplan (rad 118).
- 27 okt eftermiddag/kväll: RING (B) bara på OSC 1 (OSC 2/3 avstås). Arbetare 1 (play-pack): FX 6f485d6, D3b JOG/LP + REC-läge + LP-design/handikon/nål 394b5f7, audiotest-fix (clear-argument, InstrumentProcessor.cpp:862) → 0 allokeringar, OSC-startpresets 45a1b0e; jobbar nu med AI-leverantörer (Anthropic/OpenAI/Ollama, inga nycklar i repot, mock-sele). Arbetare 2 (w2): E2 steg 2 slice-rader f613eaa, H5 klar 47d191b (content-d1 bara rapport, content-ph/fix-matrix redan i w2, slicertester slängda; ta E2:s StateTests vid merge), E1 pågår: metallrattar + stam på underflikar (inga nya bilder, Tobias tittar i Release).
- Beslut Tobias: JOG-mål/flera JOGS efter första releasen (masterplan Fas I, 031e66d).
- Kvar före Release: AI-leverantörer (Arbetare 1), E1-rest (Arbetare 2), merge-ordning (E2 först). Därefter Release-build, samlad FL-testlista (PROGRESS-w1/w2), publik PDF i docs/releases/, påminn Tobias och fråga före merge till main. Inget pushat. Tobias paus vid session-limit ca 30 min.

## 28 okt (projektledaren)
- En enda release (rel-0.4 inlagd i rel-0.3); arbetsfördelning i `plan-arbetsfordelning-rel03.md`, köordning för Arbetare 1 uppdaterad (färgväljare/TOP DECAY, autocomplete, SOUNDSTAGE, MD-hjälpdialog).
- Branch `pl-presets` (lokal, ej pushad): 2b5986b LOOP flyttad in i JogPanel (PLAY-menyn borttagen), 1193cc9 BRAKE-kurvor VINYL/TAPE/HARD/SLOW (broms A; valet sparas inte med projektet, `PluginBase.h` brakeCurve). Debug all grön, alla tester gick igenom.
- Docs på main (lokalt): testlistan har DAW-rader för LOOP i JOG-panelen och BRAKE-kurvorna; masterplanen har 28 okt-besluten och SAMPLE → SYNTH som senare idé.
- Arbetare 1: SOUNDSTAGE klar (e47666c, 16 mix*Stage-parametrar sist), fortsätter TOP DECAY, sedan MD-hjälpdialog. Arbetare 2: fick "fortsätt" 28 okt 08:10.
- Egna misstag loggade i `LESSONS.md` (28 okt). Tobias 28 okt: logga alltid misstag och uppdatera progressfilen.
- Kvar för PL: rel-0.3-build, testlistans PDF i `docs/releases/` med hörtestsektion, Tobias testar i DAW, merge först efter ja.

- 28 okt: Arbetare 1 klar med gemensam hjälpdialog (HelpDialog, flikar pianorulle/LIVE CODE/SOUNDSTAGE), lokal commit på play-pack. TOP DECAY oklart, Tobias tillfrågad, ligger sist i kön.
- 28 okt: TOP DECAY definierat av Tobias = NOISE-decay på kicken (kickNoiseDecay, Params.cpp:194, tak 400 ms) utökas till kickens längd. Arbetare 1 fick uppgiften.
- 28 okt: Tobias: allt i appen ska vara pa engelska. Arbetare 1 far oversatta SOUNDSTAGE-hjalpen och soka efter svenska UI-strangar.
- 28 okt: Arbetare 2 far genrerna (max 3/kategori) och sedan Fas J (docs/school finns som utredning).
- 28 okt: Arbetare 2 klar med genrepresets (8d03b40: 7 kickar, 14 basar, nya kategorier) och Fas J (28e3657: docs/school/genrer.md, preset-builder-skill). Ej lyssnat. Fick order: genreutredning med kallor.
- 28 okt: Arbetare 2 genreutredning klar (09aff89, 7 genrefiler i docs/school/; Empirikal, Minimal-BPM, Dance, Goa-kick markerade obekrafta). Sista order: kallsok + full preset-audit, sedan vanta pa Tobias lyssning.
- 28 okt: Tobias vill ha START (ms) per motor i MIX-fliken; langd osakert, forslag skevt 0-4000 ms. In i planen som punkt 8b for Arbetare 1.
- 28 okt: Tobias: START per motor ska aven kunna synkas till takt (borja efter viss takt).
- 28 okt: Tobias: aven langd och kurva pa envelope per motor (tillagg till 8b).
- 28 okt: Arbetare 2 klar (af40b4e, slutaudit utan klippning, DC<=0,004; smaavvikelser peak -0,3 dB och svansar noterade i PROGRESS-w2). Standby tills Tobias lyssnat i Lemon.
- 28 okt: Tobias: START/langd/kurva per motor (5b) ska vara med i rel-0.3. Flyttad fore overtonsredigeraren.
- 28 okt: Arbetare 2 satt i arbete: AUTO FIX-matdel (karna, utan UI), plan forst. Tobias lyssnar efter att releasen ar byggd/slappt; flodet (test fore merge) galler tills han sager annat.
- 28 okt: Tobias: "du kan också hjälpa till". Testlista-utkast rel-0.3 skriven i docs/work/testlista-rel03-utkast.md (PDF gors vid bygget).
- 28 okt: Arbetare 2 klar med AUTO FIX-matkarna (AutoFix.h/.cpp) och FX-karna (69f1ab0: Flanger SHAPE/SPREAD, FxTests). Echo ms-tid parkerad, fraga Tobias om knobsPerSlot. Arbetare 1 tar UI.
- 28 okt: Tobias beslut: Echo racker med taktsteg, ingen ms-ratt, knobsPerSlot oforandrat.

## 28 okt: tkdev.se uppdaterad (projektledaren)
- Tobias bad om nya skärmbilder och ny text "överallt där plugget finns". Byggde `build.bat Debug all` i pl-worktreen (w2 mergad, grönt) och tog 9 bilder med `KickbaseTool snapshot` (flikkoder: PATTERN 1000, KICK 10, FILTER via KB_SOURCETAB=5/20, MIX KB_SOURCETAB=7/20, LFO 22 mod, MATRIX 24, SOUNDSTAGE 26, ANALYZER 30 + KB_SPECTRUM=1, AI 40 + KB_CHATDEMO=1).
- tkdev (lokal commit, ej pushad): `kickbase/page.tsx` (9 bilder, ny text, alt utan "FL Studio"), nya jpg i `public/projekt/kickbase/`, migration `20261028000000_kickbase_projekt_uppdatering.sql` (uppdaterar body, cover och images). Migrationen är inte körd mot databasen.
- Andra ställen: bara `oath2`-sidorna, robots och sitemap nämner Kickbase (inga funktionstexter). Statusen är fortfarande "inte släppt".
- Ej gjort: push/deploy/migration (kräver Tobias). tkdev har andra ocommittade ändringar (cv, startsidan) som inte rörts.
