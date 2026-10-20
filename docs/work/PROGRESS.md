# Läge (uppdateras efter varje delsteg; kort)

Plan: [../plan-v2.md](../plan-v2.md). Opus = projektledare, Sonnet = bygger. Sessionsnamn: kolla `ListAgents`.
Läs [LESSONS.md](LESSONS.md) innan du bygger eller testar. Lägg till en rad där när ett fel upptäcks.

## Nu
- 20 okt: fas R pågår på content-r (Sonnet). R9 steg 1 klart (a5b63de). **R9 steg 2 pågår (ej committat)**: längd följer innehållet (Pattern::refreshLengths, AUTO/valt/extra), PianoRoll (LengthMenu, + BAR, readNotes/applyNotes via forEachNote) och PatternModule (TOTAL LENGTH + checkLengthChanges borttagna) ändrade, Tool.cpp lengthTest/rollLengthTest omskrivna.
  Nästa: bygga (`build-mcp.bat KickbaseMcp KickbaseTests`), rätta resterande kompileringsfel (Tool.cpp scenarier ~280-333/939/1525, Scenarios.h:97, Profile.cpp:205, Presets.cpp ~394-410, ToolLayer ~468/497, MixAnalysis ~955-984, tester) med Params::applyLegacyLengthParams + fillBars; sedan rendersig/real/wide mot baslinjer, blocksizetest, audiotest, persisttest, lengthtest, presettest, instrumenttest, KickbaseTests; committa; rapport till Opus (diffstat, testutdata, svar: en skrivare/offlinekopior via state-XML, fasta 11x1024 pekartabeller, (a)-(d)).
  Sedan steg 3 (högerklick Lås längd på radkortet, ta bort gamla PatternModule-längdkod/LENGTH-visning) och steg 4 (MCP get_pattern/set_pattern length "auto"|n, repeat_to, AGENT_GUIDE). Kvar att kolla: bekräftelse vid förkortning finns i LengthMenu (verifiera), inga parametrar flyttade, ingen allokering i ljudtråden.

## Klart
- Fas 0, A, D1, W, E2, D2/D3 (godkända, pushade; tippen content-d2 c0ef101).

## Öppet / väntar
- Frågor till Tobias: plan-v2 avsnitt 4.

## Logg (nyast sist, en rad per delsteg: datum, steg, commit, test)
- 20 okt R1-R3 f9be216: pianorulle (kryss-y, scrollbarens klick, vänsterkant, EDIT IN LANE/hjälptext dolda, Alt=sudd, SELECT ALL, + BAR), ENV-header, håll-provlyssning, LISTEN vid PITCH/AMP. rendersig-trion identiska.
- 20 okt R4-R7 (utan SNAP): analyzer-lista, export per instrument + without effects, LFO-fas kontinuerlig (Modulation.cpp phaseOffset). Tester: ModulationTests, rendersig-trion, blocksizetest OK; bilder ENV/KICK/ANALYZER tittade på. EFFECTS CLEAR finns inte i koden. SNAP väntar på Opus OK av spec.
