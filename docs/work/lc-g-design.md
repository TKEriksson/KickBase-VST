# LC-G: flytande Live Code-fönster, KONSOL/flikar, notmarkering (design, 21 okt, ingen kod)

Status: kodläsning och förslag. Bygger inte förrän Opus/Tobias godkänt. Rör `LiveCodePage` (Arbetare 2:s område): samordna med dem.

## Nuläge (kodläsning)
- `LiveCodePage` (`Source/Ui/LiveCodePage.h:10`) är en vanlig `Component` som ägs av två editorer: `InstrumentEditor.h:50` och `FxEditor.h:41`. Det är alltså två instanser, en per plugin, och varje har en egen `CodeDocument` (`LiveCodePage.h:65`).
- Layouten är en kolumn i `resized()` (`LiveCodePage.cpp:106-139`): knappar, LÅS, flikarna KOD/FEL, editor eller `errorLog` (samma yta), statusrad, licensrad.
- Flikarna KOD/FEL är två `TextButton` + `showTab (bool errors)` (`LiveCodePage.cpp:266`). Felen skrivs till `errorLog` (`showError`, `:583`).
- Kodens text ligger i processorn (`get/setLiveCodeText`), så texten överlever att sidan byggs om. Läge och lås sparas i state (D 20 okt).
- Händelser: `LiveCode::query` ger `Hap{begin, end, value}` (`LiveCode.h:28-35`). Ingen källposition följer med än.

## G1: flytande fönster
- Mål: koden syns medan man arbetar i andra flikar.
- Förslag: flytta själva innehållet till en `LiveCodePanel` (editor, flikar, knappar) och låt `LiveCodePage` bara vara en värd. Panelen kan sedan sitta antingen i fliken eller i ett `juce::DocumentWindow` (inte modalt, `setUsingNativeTitleBar`, `setResizable`).
- Viktigt: ett enda `CodeDocument` per processor, inte per fönster, annars divergerar texten. Flytta därför `document` till en liten ägare (processorn eller en `LiveCodeModel` som delas) och låt panelen bara visa den. Då kan fliken och fönstret visa samma text samtidigt.
- Livslängd: fönstret måste stängas före editorn (VST-editorn kan förstöras av värden när som helst). `SafePointer` till fönstret i editorn, och stäng i editorns destruktor (LESSONS R1–R3: aldrig rå referens till processorn i asynkrona callbacks).
- Värdar som DAW:er: ett eget toppnivåfönster kan hamna bakom värden och tappa tangentbordsfokus. Alternativ utan eget fönster: en flik som kan "dockas" som överlägg över andra flikar (`addChildComponent` på editorn, dragbar). Jag rekommenderar överlägget först, eftersom det är billigare och undviker fokusproblem; eget fönster som val senare.
- Risk: två `LiveCodePage`-instanser (instrument och FX) delar inte state, så se till att knappen "flyt" bara gäller den egna.

## G2: KONSOL och flikar
- Dagens FEL-flik blir en KONSOL: samma `errorLog`, men med rader för både fel och info (kod körd, N händelser, `console.log` från koden, `.set` avslag).
- `console.log` är idag stubbad i glue (spiken: bara console, performance.now, setTimeout stubbas). Koppla `console.log` till en kö som GUI-tråden töms på i `timerCallback` (`LiveCodePage.cpp:294`). Ingen skrivning från ljudtråden.
- Flikar: KOD, KONSOL (med räknare på olästa fel), eventuellt fler kodflikar (en per "scen") senare. Ersätt de två `TextButton`/`showTab` med `juce::TabbedButtonBar` så nya flikar blir billiga.
- Tak: behåll bara de senaste N raderna (t.ex. 500) så texten inte växer obegränsat.

## G3: notmarkering
- Mål: markera i koden vilken del som spelar just nu (som strudel.cc).
- Kräver källposition per händelse. Strudel ger den via transpilern (acorn): `mini`-strängar och `.p()` får `loc` i `hap.context.locations`. Spiken noterade att acorn-transpilern ännu inte var buntad för `$:` men `$:` finns nu (A 59f5202), så positionerna finns troligen redan i hap-kontexten. Kontrollera `hap.context.locations` i glue innan något byggs.
- Flöde: `query` returnerar utöver `value` en lista `locs` (start, slut i texten). Bakgrundstråden köar `(tid, start, slut)`. GUI-tråden jämför mot uppspelningstiden i `timerCallback` och sätter markeringar i `CodeEditorComponent` (`setHighlightedRegion` för en, eller egen overlay som ritar rektanglar för flera).
- Risk: texten ändras medan koden körs, så positionerna blir fel. Lös genom att spara textversionen som positionerna hör till och släppa markeringar om `document` ändrats sedan dess (jfr `sliderText` i `LiveCodePage.h:70`).
- Tidpunkt: hap-tider är i cykler, uppspelningstiden i ticks; använd samma cykel-till-tick-mappning som schemaläggaren, annars hamnar markeringen fel.

## Ordning och kostnad
1. G2 först (minst risk, hjälper felsökning): konsol + `console.log`-kö.
2. G3 om `hap.context.locations` finns.
3. G1 sist (störst ombyggnad: `document` flyttas ut ur sidan).

## Öppet för Opus/Tobias
- Överlägg i editorn eller eget `DocumentWindow` för G1?
- Ska `console.log` räknas som fel (röd räknare) eller bara info?
- Arbetare 2 äger LiveCodePage: vem bygger G, och får `document` flyttas ut?
