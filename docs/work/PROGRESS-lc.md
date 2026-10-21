# Läge LC-spike (arbetare 2, gren spike-lc)
- 20 okt: spike klar, rapport i [lc-spike-report.md](lc-spike-report.md). Fungerar; rekommenderar QuickJS + Strudel.

# LC-bygget (projektledaren, gren content-lc, worktree KickBase-VST-live)
Plan: (1) motor `Source/Core/LiveCode.h/.cpp` + bunt i BinaryData + `live_code_create` (klart i kod 20 okt, test kvar);
(2) tester i Tools/ (mappning, fel, tidsgräns); (3) Live-läge: bakgrundstråd som frågar queryArc 2-4 cykler fram, kö till schemaläggaren;
(4) UI-flik LIVE CODE med reglage + kodblock i chatten (Play/Stop/Create); (5) transpiler (`$:`, `.p()`, reglage-id).
Mappning: 1 cykel = 1 takt = 384 tick. s("bd"/"kick") = kickraden (steg = round(b*16)); s("i1".."i10") = instrument; note/n utan s = instrument 1.
Pitch: C2 = MIDI 36 = pitch 0 (ton utan oktav = oktav 3, som i Strudel). gain = velocity (standard 0.8). En not per tick tills polyfonin finns.
Strudel-banner: AGPL, visas i UI/THIRD-PARTY.md (kvar att göra).

## Arbetare 1: LC steg E, ljudtester + beslut 4.1-4.3 (branch content-lce, ovanpå 4f40a85)
- 4.1: oförändrat (gemener `.lfo/.env`, sträng-id). Objektnamn kommer senare som tillbakakompatibel utökning.
- 4.2: `Modulation::maxCodeRoutes = 32` (`Modulation.h`) och vakt i `ToolLayer::applyCodeMods` (före `modRoutes.add`): felraden "Live Code may own at most 32 of the 64 modulation routes". Obs: idag nås taket aldrig av koden, eftersom det finns högst 8 platser och en rutt per plats (`applyCodeMods` nycklar på lager|param|sort). Vakten är skydd om platserna eller rutterna per plats ökar. Testet fyller taket för hand.
- 4.3: `PluginBase::sweepCodeRoutes` anropas först i varje `applyCodeMods`, och en `CodeSweeper` (juce::Timer, 100 ms) startas av `startCodeSweep()` när en post stängs eller `clearCodeMods(false)` körs. Timern stannar när ingen CODE-rutt väntar på utfasning. Utan MessageManager (verktygstester) startas ingen timer.
- Tester: `ModulationTests.cpp` `testCodeSources` (3.1: LFO-period/form/spann, utfasning + idle, ENV attack/decay/omstart/andra instrument), `Tools/Tool.cpp` (3.2: `.lfo` svänger parametern, djup 0 lämnar den orörd; 4.3: sopning; 4.2: tak).
- Av: Arbetare 1, content-lce. Ej byggt av mig. Kör: bygget, ModulationTests/fulltest, verktygstestet (Tool.cpp), rendersig, rendersigreal, enginetest.
