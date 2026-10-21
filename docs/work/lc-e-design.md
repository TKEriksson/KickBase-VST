# LC steg E: modulation från kod, `.lfo()` / `.env()` (DESIGN + GRANSKNING, ingen kod från Arbetare 1)

Skriven av Arbetare 1 (content-g), 21 okt. Underlag: commit 4f40a85 på `content-lcui` (läst, inget ändrat där) och `docs/work/lc-f-design.md`.

## 0. Viktigaste först: E finns redan som kod på content-lcui

4f40a85 "Live Code: add code-driven modulation sources" (20 okt kväll, ej granskad, ej i main) implementerar redan hela E. Ingen ny kod ska skrivas för E; det som behövs är granskning, beslut om syntaxen och fler tester. Det som finns:

| Del | Var (content-lcui) | Kort |
|---|---|---|
| Orden | `Resources/strudel/glue.js` (+26, samma i `strudel.all.js`) | `.lfo(param, hz, djup, form?)` och `.env(param, attackMs, decayMs, djup?)` lägger en `_mod`-lista på varje hap |
| Plocka ut | `LiveCode::collectMods` (`LiveCode.cpp`) | en post per (lager, param, sort), första gäller; `s("bd")` = kick = lager -1, `iN` = instrument N |
| Källor | `Modulation.h`: `firstCodeSource = 74`, `numCodeSources = 8`, `numSources = 82` | CODE 1-8 sist i listan; `isCodeSource` |
| Ljudtråd | `Modulator::update`, `CodeSlot` (atomics), `CodeRuntime` | LFO i Hz (fem former + rand), ENV = AD som startar på instrumentets nya ton (`getNoteCount`), 20 ms utfasning, `idle` när tyst |
| Koppling | `ToolLayer::applyCodeMods` | en plats + en rutt per post; saknade poster stängs; ogiltigt id/ej modulerbart/ENV på kick = `data.problems` |
| Städning | `PluginBase::clearCodeMods`, `sweepCodeRoutes`; anrop i `setStateInformation`, fabrikspreset, `loadPresetXml`, LIVE-stopp, `LoadWatch` | |
| Spara | `ModRoutes::toValueTree` hoppar över CODE-källor, `fromValueTree` ignorerar dem | gamla projekt blir byte för byte desamma |
| Tester | `Tools/Tool.cpp` (lfo, env, okänt id, ej modulerbart, env på kick), `StateTests.cpp` (inga CODE-rutter sparas/laddas), `ModulationTests.cpp` (källnummer 74-81) | |
| AI/MCP | beskrivningen av `live_code_create` utökad, testet kräver `.lfo(`, `.env(`, `data.problems` | |

**Överlapp med order:** att skriva ett eget E vore dubbelarbete. Förslag: projektledaren granskar 4f40a85 mot punkterna nedan (och bestämmer vem som committar lcui-grenen), i stället för att jag bygger något parallellt.

## 1. Krav från ordern, kontroll mot 4f40a85

- **Modulationskällor sist:** ja. CODE 1-8 = källa 74-81 efter AMP/FILT ENV (54-73). Inga befintliga källnummer flyttas (de sparas som tal i `modRoutes`, `PluginBase.cpp` static_assert uppdaterad).
- **Inga parametrar mitt i listan:** E lägger ingen host-parameter alls (inget i `ParamID`/APVTS), så host-ordning och automationsid:n är orörda. Bra.
- **Rendersig 0 diff:** förväntat, men inte bevisat. Skäl: CODE-rutter sparas aldrig, `kind == 0` hoppar över platsen i `update()` (`rt = {}`, `continue`), och utan Live Code finns inga CODE-rutter. Det som ändras för alla: `numSources` 74 → 82 (större `sourceValues`/`sourceDisplay`, en 8-varvs loop i `update()`). Måste bekräftas av CI: `rendersig` och `rendersigreal` med 0 diff, annars stopp.

## 2. Granskningspunkter (min bedömning, att avgöra av projektledaren/Tobias)

**A. Syntax och namn (beslut krävs).** Planen säger `.LFO()/.ENV()`; koden har gemener `.lfo/.env` och sträng-id (`"bass1Cutoff"`). F1 (lc-f-design.md:10) beslutade objektträd med CamelCase (`LFO1.Sync(false)`, `instr1.Cutoff(..)`). Förslag:
1. Behåll gemener för mönsterord (Strudels stil, `.lpf`, `.fast`) och skriv i planen att `.lfo/.env` är det slutliga namnet; `LFO1`..`LFO10` i F är de handritade LFO:erna och hör inte ihop med CODE-källorna. Risk för förväxling: dokumentera i guiden.
2. När F1 finns: låt `.lfo/.env` även ta objektnamn (`instr1.Cutoff`) i stället för sträng, så att samma namn gäller överallt. Det är en tillbakakompatibel utökning, ingen ändring av E. Sträng-id behålls.
3. Test: orden får inte skugga något som redan finns i Strudels export (samma regel som F, lc-f-design.md:35). Kolla särskilt `lfo` och `env`, som kan finnas i en nyare Strudel.

**B. Semantik som är lätt att missförstå (ska stå i guiden och verktygsbeskrivningen):**
- Poster nycklas på (lager, param, sort). Står `.lfo` på flera haps med olika djup/hastighet gäller den första (`collectMods`); ett mönster som ändrar djupet över tid ger alltså inte det.
- LFO är fritt gående i Hz, inte synkad till tempo och inte fasstartad per ton. Tempo-synk och retrigger är ett senare tillägg (inte i E).
- ENV startar på varje ny ton i hela instrumentet (`noteCounts`), inte bara på den hap som bär `.env`. Toner som redan klingat när LIVE slås på startar inte ENV (`rt.wasOn`).
- ENV är AD utan sustain, stöds inte på kicken (felrad), `.lfo` på kicken går.
- Djup klipps till -1..1, Hz till 0.01-200, attack 0-10 000 ms, decay 1-20 000 ms (`applyCodeMods`). Det ska stå i beskrivningen.

**C. Delade resurser.**
- Rutterna är 64 till antalet (`maxRoutes`) och delas med handritade. Åtta CODE-källor kan som mest ta 8 × (antal parametrar) rutter; en kod kan alltså äta upp alla lediga rutter. Förslag: tak på rutter som koden får använda (t.ex. 32) och felrad när det tar slut (felet finns redan men först när alla 64 är slut). Beslut krävs.
- Åtta platser: nio unika (lager, param, sort) ger felraden "All 8 CODE modulators are in use". Räcker för v1; höj bara om en riktig låt behöver mer (en höjning är en ny källa sist, ingen omnumrering).

**D. Trådar.** Meddelandetråden skriver rutter och `CodeSlot`-atomics, ljudtråden läser (samma mönster som handritade rutter; `ModRoutes::remove` sätter `source = -1` först, `Modulation.cpp:183`). `CodeSlot::kind` sätts sist så att platsen börjar verka med färdiga fält. Ingen allokering i ljudtråden. Ska verifieras av granskare, inte antas: att `sourceDisplay` och `noteCounts` bara skrivs från ljudtråden.

**E. Livscykel.**
- Stopp (LIVE av) → `clearCodeMods(false)`: platserna tonar ut, rutterna tas bort av `sweepCodeRoutes` via `LoadWatch`. Det kräver att Live Code-sidan finns kvar; om fliken/fönstret stängs medan platsen tonar ut ligger rutten kvar tills nästa tick. Förslag: låt `PluginBase` själv svepa (t.ex. vid nästa `live_code_create`/stopp) så att ingen UI behövs. Går **inte** att nå via MCP: `live_code_create` utan `.lfo` stänger poster (`applyCodeMods` steg 1), men det finns ingen stoppväg för modulation utan att köra om koden. Granskare avgör om det duger.
- Laddning/preset/Init: `clearCodeMods()` först. Init och laddning startar aldrig LIVE och skapar inga CODE-rutter (testat för spara/ladda).
- Presets sparar inga CODE-rutter, men en preset som skrivs över rutterna rensar dem (rätt).

**F. UI.** `MatrixPage` visar CODE-rutter men källmenyn erbjuder dem inte (rätt: de väljs bara från koden). `Theme::sourceColour` ger dem LFO-färg med 0.75 mättnad. Att kontrollera visuellt: att en CODE-rutt i matrisen inte går att redigera/ta bort för hand på ett sätt som lämnar `codeMods` pekande på en borttagen rutt (då hoppar `applyCodeMods` över att skapa om den? Den hittar `find` = -1 och lägger en ny, vilket är OK, men ska testas).

**G. Generad fil.** `strudel.all.js` har samma 26 rader som `glue.js`. Om `strudel.all.js` är en byggd sammanslagning ska den genereras, inte handredigeras; kontrollera att det finns ett byggsteg eller en regel som håller dem lika, annars glider de isär.

## 3. Vad som saknas i testerna (testplan, att lägga i lcui-grenen av den som bygger)

Finns: rutt skapas (lfo/env), okänt id, ej modulerbart, env på kick, inga CODE-rutter sparas/laddas, källnamn/färg, beskrivning.

Saknas, i prioritetsordning:
1. **Ljud (kärnan):** `Modulator::update` med en LFO-plats: `sourceValue(74)` svänger med rätt period och form (sine/tri/saw/isaw/square/rand), `kind = 0` ger 0 efter ~20 ms utfasning och `idle` blir sant. ENV: noll före första ton, stiger över attack, faller över decay, startar om på ny ton (`setInstrumentNoteCount`), startar inte på toner som redan klingat.
2. **Påverkan på ljudet:** rendera 1 s med `.lfo("bass1Cutoff",...)` aktiv och utan; filterparametern (eller ljudet) skiljer sig med rätt riktning och djup; med djup 0 är det identiskt med utan.
3. **Regression:** `rendersig` och `rendersigreal` 0 diff; Init-check; gamla projektfiler (utan CODE) ger samma state byte för byte (finns delvis, `persistTest`).
4. **Gränser:** nio unika poster → felrad, åtta fungerar; alla 64 rutter slut; `depth` utanför -1..1 klipps; negativt/NaN/Infinity-argument ger felrad från `modNum`; okänd form ger felrad i JS-felvägen, inte krasch.
5. **Plats-återanvändning:** stäng en post, öppna en annan före utfasningen är klar → ny post tar inte den platsen (`routed && !idle`); efter `sweepCodeRoutes` går den att återanvända.
6. **Stopp/laddning:** LIVE-stopp, `setStateInformation`, fabrikspreset, `loadPresetXml` lämnar `codeMods` tom och inga CODE-rutter.
7. **Orden:** nytt ord får inte skugga Strudel (se 2A); `.lfo` utan `.s("iN")` hamnar på instrument 1; `s("bd").lfo` ger lager -1.
8. **MCP/AI:** exempelguiden uppdaterad med ett fungerande `.lfo`- och `.env`-exempel (ett livetest-fall per nytt ord, som lc-f-design.md:1 kräver).

## 4. Beslut som behövs (kort)

1. Behåll gemener och sträng-id (förslag: ja) och lägg objektnamn till i F1?
2. Tak för kodens rutter (förslag: 32 av 64)?
3. Ska `PluginBase` svepa själv så att stängd flik inte lämnar rutter kvar (förslag: ja, litet)?
4. Vem granskar/mergar 4f40a85 (projektledaren), och bygger vi ljudtesterna i punkt 3.1-3.2 före merge?

## 5. Ordning

E är kod-klar men oprövad → (1) granskning + punkt 3.1-3.3 körda i CI, (2) beslut 4.1-4.3 → (3) F1 (objektträd) bygger vidare. Ingen ny källa eller parameter behövs för E.
