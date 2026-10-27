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
