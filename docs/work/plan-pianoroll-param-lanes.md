# Plan: parameterrader (lanes) i pianorullen, med ändringsbar höjd

Arbetare 2, 28 okt. Bara en plan, ingen kod. Bygger på `fx-matrix`; rör inte `main`.

## Mål
Under pianorullens ton-rutnät: en eller flera rader ("lanes") där man ritar värden per ton eller per steg, som velocity-remsan idag (`PianoRoll.cpp:8` `velocityHeight = 40`, `PianoRoll.cpp:845` `velocityArea`). Man ska kunna välja vilken parameter en rad visar, lägga till/ta bort rader och dra i skiljelinjen för att ändra höjd.

## Läget idag
- En fast remsa längst ner = velocity (`PianoRoll.h:88,247`, `Drag::velocity`, `noteForVelocity/velocityX/setLiveVelocity` `PianoRoll.h:174-179`). Y-läget i `resized()` (`PianoRoll.cpp:843-846`) räknar med en konstant höjd.
- StepSequencerns "lane" (`StepSequencer.h:21` `Lane { velocity, gate, pitch, slide }`) är en annan, separat sak: ett enda lane per rad.
- Per ton lagras `velocity`, `length`, `pitch`, `slide`, ackord, extra toner i `Pattern::Note` (`Pattern.h:122-136`) som atomics, och ändringar går genom `Pattern`-API:t som UndoManager lyssnar på.

## Två sätt att lagra värdena (beslut behövs)
| | A. Värde per ton (parameter-lås, som FL) | B. Automationskurva per rad |
|---|---|---|
| Lagring | nytt fält i `Pattern::Note`: liten array `lock[maxLanes]` (float, NaN/flagga = "ej satt") | ny kurva per rad + parameter, tid i ticks |
| Ljud | tillämpas när tonen triggas (och ev. glid under tonen för slide) | måste samplas varje block av `StepScheduler` |
| Ljudtrådsrisk | låg: samma väg som velocity | högre: ny schemaläggning, interpolering, ramp |
| Känns som | "den här tonen har mer cutoff" | "cutoff svepas över takterna" |

**Rekommendation: A först** (billigt, passar stegsekvenseraren, samma mönster som velocity). B kan läggas till senare som en egen rad-sort utan att ändra A.

## Vilka parametrar (förslag, första omgången)
Bara sådana som redan finns som parametrar och som är meningsfulla per ton: filter-cutoff, filter-resonans, pan, FX-send-belopp (nya matrisen: välj FXn), drive. Velocity och längd är redan inbyggda. Listan hålls i ett register (ID + namn + intervall) så att en ny parameter bara är en rad; nya parametrar sist (`docs/ENGINEERING.md`).
Ljudparametrar som styrs av lås får en ny modulationskälla-liknande väg i `InstrumentProcessor`: lås-värdet adderas/ersätter parametern från tonens start tills nästa ton (eller tonens slut). Detta bör avgöras med specialisten/projektledaren eftersom det är ljudtrådskod.

## UI-design
1. **Radhuvud** (vänsterkolumnen under tangenterna, 46 px bred): rullista med parameter + liten "x" som tar bort raden. `+` längst ner lägger till en rad (tak 4).
2. **Höjd**: skiljelinjen mellan rutnät och lanes är en drag-list (`Drag::splitter`, markör `UpDownResize`). Varje rad har minimihöjd 28 px; gemensam total höjd sparas, standard = dagens 40 px så att utseendet är oförändrat med bara velocity-raden. Rutnätet får resten. Höjderna sparas i UI-state (inte i presetet), som zoom/scroll idag.
3. **Rita**: samma gester som velocity: klicka/dra på en tons stapel sätter värdet, Alt+klick återställer ("ej satt"), box-markerade toner kan dras tillsammans; musen över visar värdet i %/Hz i hint-raden (`hintHeight`).
4. **Undo**: varje drag = en transaktion i UndoManager (som velocity idag).
5. **Hjälptexter** för raden, rullistan, splittern och "+"-knappen (helpcheck).

## Datamodell och filer som ändras
- `Pattern.h/.cpp`: `Note::lock[]`, accessors `setLock/clearLock`, kopiera med i `copyNote`/klippbord (`PianoRoll.cpp:671,772`), persistens i R9b-formatet bakom en versionsflagga (gamla filer har inga lås).
- `PianoRoll.h/.cpp`: `std::vector<LaneState> lanes` (parameter, höjd), `laneArea[]` ersätter `velocityArea`, `Drag::lane`, `Drag::splitter`; velocity blir lane 0 (alltid första raden, kan inte tas bort).
- `StepScheduler`/`InstrumentProcessor`: hämtar lås vid trigger. Lås på sluten-kopplad rad med flera röster (polyfoni): varje stapelton har eget lås.
- `Help.cpp`, MCP-verktyg (`ToolLayer.cpp`) för att sätta lås, AI-namnlistan.

## Faser
1. **Ytan utan ljud** (bara UI): flera lanes + splitter, velocity som lane 0, höjd sparas. Ingen ny ljudtrådskod. Testa: layout vid olika höjder, att velocity beter sig som förut.
2. **Lås i datamodellen**: `Note::lock`, persistens, undo, klippbord. Test: spara/ladda, undo, kopiera/klistra.
3. **Ljud**: tillämpa lås vid trigger för cutoff/pan/FX-send. Rendersig-trion + blocksize + audiotest (baslinjerna har inga lås, ska vara oförändrade). Ny rendertest: en ton med cutoff-lås = samma ljud som samma cutoff satt som parameter.
4. (Senare) Automationskurvor (B) och fler parametrar.

## Risker
- `PianoRoll.cpp` är 2 549 rader och har många hårdkodade `velocityArea`-referenser; fas 1 ska göras i ett svep med en sökning efter `velocityArea/velocityHeight`.
- Egen pianorulle per skiva (`docs/work/own-piano-roll-spec.md`) ändrar samma fil; samordna ordningen med den som äger den specen.
- Ljudtrådskod ändras först i fas 3; fas 1-2 är säkra för rendersig.

## Frågor till Tobias/projektledaren
1. Är A (värde per ton) rätt för första versionen, eller ska det vara kurvor (B)?
2. Vilka parametrar ska med först?
3. Ska höjden sparas med projektet eller bara gälla sessionen?
