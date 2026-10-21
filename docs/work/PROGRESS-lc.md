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

## Arbetare 1: känd begränsning + punkt till F1-designen (21 okt)
- Känd begränsning, skuggning: Strudel core har egna `Pattern.prototype.lfo/env/bmod` (`Resources/strudel/strudel.all.js:6525-6535`, via `modulate()`, för WebAudio) och globala `lfo(x)`/`env(x)`. Vår `glue.js:79`/`glue.js:85` skriver över metoderna `.lfo/.env` (glue läggs sist av `build-bundle.sh`). Strudels egen `.lfo({...})` fungerar därför inte i värdobjektet; ingen regression, eftersom superdough inte används. `strudel.all.js` och `glue.js` är i synk (diff tom).
- Punkt till F1-designen (`lc-f-design.md`, ligger oincheckad i lcui, rörs inte av mig): byt namn på `.lfo/.env`, eller kasta ett tydligt fel om första argumentet är ett objekt (Strudels signatur) i stället för en sträng-id.
- Testplan för lucka 4-6 (lc-e-design §3, ej byggd, kräver ingen ny kod i kärnan), i `Tools/Tool.cpp` bredvid CODE-blocket:
  - Lucka 4 gränser: (a) åtta unika (lager, param, sort) fungerar, den nionde ger "All 8 CODE modulators are in use"; (b) djup 5 klipps till 1 (jfr `ToolLayer.cpp:974`), rate 1000 till 200, attack 99999 till 10000, decay 0 till 1 (`:999-1002`); (c) NaN/Infinity/sträng i `modNum` (glue.js) ger felrad från QuickJS, inte krasch (testas via `live_code_create`); (d) okänd form ger felrad.
  - Lucka 5 platsåteranvändning: stäng en post, skapa en annan före utfasningen (inga `update`-steg emellan) och kontrollera att den nya inte får samma plats (`ToolLayer.cpp:989`, `routed && !idle`); pumpa 3000 steg, `sweepCodeRoutes`, då går platsen att återanvända.
  - Lucka 6 stopp/laddning: `clearCodeMods(false)`, `setStateInformation`, fabrikspreset och `loadPresetXml` ska lämna `codeMods` tom och inga CODE-rutter efter svepning.
