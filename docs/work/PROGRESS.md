# Läge (uppdateras efter varje delsteg; kort)

Plan: [../plan-v2.md](../plan-v2.md). Opus = projektledare, Sonnet = bygger. Sessionsnamn: kolla `ListAgents`.
Läs [LESSONS.md](LESSONS.md) innan du bygger eller testar. Lägg till en rad där när ett fel upptäcks.

## Nu
- 20 okt: fas R på content-r (Sonnet). R9 steg 4 klart (MCP: get_state per rad bars/length_mode/content_bars/extra_bars, pattern.bars = längsta raden; set_pattern length auto|N, trim, repeat_to, fel "row is locked to N bars"; MixAnalysis pattern_timeline läser radens bars; AGENT_GUIDE, AnalysisAbout, preset-checklist). lengthtest (med MCP-fall) och analysetest gröna. Det finns inget add_note-verktyg i MCP, bara set_pattern.
  Nästa: FASSLUT: bygg VST3 + KickbaseHostTest en gång, HostTest state spara (gammal build)/ladda (ny), hel svit en gång, 42 bilder + pngdiff, docs/content-r-report.md, Tobias testlista (≤10 punkter, inkl. klick på bekräftelsedialogen vid förkortning och högerklick på radetiketten), hash/resultat till Opus. Ingen push/Release förrän Opus säger. Öppet: SNAP väntar på Opus-OK, Help.cpp har kvar hjälptexter för patternBars/rowBars (parametrarna visas inte längre).

## Klart
- Fas 0, A, D1, W, E2, D2/D3 (godkända, pushade; tippen content-d2 c0ef101).

## Öppet / väntar
- Frågor till Tobias: plan-v2 avsnitt 4.

## Logg (nyast sist, en rad per delsteg: datum, steg, commit, test)
- 20 okt R1-R3 f9be216: pianorulle (kryss-y, scrollbarens klick, vänsterkant, EDIT IN LANE/hjälptext dolda, Alt=sudd, SELECT ALL, + BAR), ENV-header, håll-provlyssning, LISTEN vid PITCH/AMP. rendersig-trion identiska.
- 20 okt R4-R7 (utan SNAP): analyzer-lista, export per instrument + without effects, LFO-fas kontinuerlig (Modulation.cpp phaseOffset). Tester: ModulationTests, rendersig-trion, blocksizetest OK; bilder ENV/KICK/ANALYZER tittade på. EFFECTS CLEAR finns inte i koden. SNAP väntar på Opus OK av spec.
