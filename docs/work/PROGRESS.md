# Läge (uppdateras efter varje delsteg; kort)

Plan: [../plan-v2.md](../plan-v2.md). Opus = projektledare, Sonnet = bygger. Sessionsnamn: kolla `ListAgents`.
Läs [LESSONS.md](LESSONS.md) innan du bygger eller testar. Lägg till en rad där när ett fel upptäcks.

## Nu
- 20 okt: fas R på content-r (Sonnet). R9 steg 3 klart (väntar på commit-hash/Opus-godkännande): trimRow från takt 0, DeferredRelease med SpinLock, LengthMenu med SafePointer + dialog kopplad till komponenten, låst rad tar inga toner efter slutet (Pattern::acceptsTick; pianorullen visar ingen extra takt, stegvyn och klistra in ignorerar), högerklick på radetiketten öppnar LengthMenu. lengthtest, persisttest, KickbaseTests gröna. Ingen skärmbild (inget visuellt nytt utom att den låsta rullen slutar vid slutet).
  Nästa: R9 steg 4 (MCP get_pattern bars/locked, set_pattern length "auto"|n, repeat_to, add_note/set_pattern ger fel "row is locked to N bars" i låst rad, AGENT_GUIDE/AnalysisAbout, preset-checklistrad). Sedan fasslut (VST3+HostTest, hel svit, 42 bilder, docs/content-r-report.md, Tobias testlista). Ej verifierat: LengthMenu-dialogen för förkortning är aldrig klickad på riktigt.

## Klart
- Fas 0, A, D1, W, E2, D2/D3 (godkända, pushade; tippen content-d2 c0ef101).

## Öppet / väntar
- Frågor till Tobias: plan-v2 avsnitt 4.

## Logg (nyast sist, en rad per delsteg: datum, steg, commit, test)
- 20 okt R1-R3 f9be216: pianorulle (kryss-y, scrollbarens klick, vänsterkant, EDIT IN LANE/hjälptext dolda, Alt=sudd, SELECT ALL, + BAR), ENV-header, håll-provlyssning, LISTEN vid PITCH/AMP. rendersig-trion identiska.
- 20 okt R4-R7 (utan SNAP): analyzer-lista, export per instrument + without effects, LFO-fas kontinuerlig (Modulation.cpp phaseOffset). Tester: ModulationTests, rendersig-trion, blocksizetest OK; bilder ENV/KICK/ANALYZER tittade på. EFFECTS CLEAR finns inte i koden. SNAP väntar på Opus OK av spec.
