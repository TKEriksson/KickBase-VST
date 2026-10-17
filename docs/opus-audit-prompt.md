# Uppdrag: full granskning av Kickbase (för Opus)

Du granskar **Kickbase** (VST3-instrument: kick + upp till 10 instrument, mönster synkat till DAW:en, JUCE 8.0.15) och **Kickbase FX**. Repo: `C:\Users\hunga\Documents\GitHub\MyOwnFruit`, gren `dev` (main ligger på eb92729 och är inte sammanslagen sedan dess). Tobias (svensk) äger projektet, men Claude skriver all kod. Svara Tobias på svenska.

Tobias vill att **allt kollas noga**. Det här är ingen snabb genomgång. Det är ett jobb som får ta lång tid, och hellre fem timmar för mycket än en missad bugg.

## 0. Läs först (i den här ordningen)

1. `CLAUDE.md` (projektet) och `~/.claude/CLAUDE.md` (Tobias arbetssätt: läs bibliotekens dokumentation innan du bygger kring ett fel, förklara _orsaker_ och peka ut de få viktigaste raderna `fil:rad`, korta påminnelser, inte i varje svar).
2. Minnet: `C:\Users\hunga\.claude\projects\c--Users-hunga-Documents-GitHub-MyOwnFruit\memory\MEMORY.md` och `project-status-handoff.md` (vad som är byggt, i vilken ordning, kända fällor).
3. `docs/DESIGN.md`, `docs/sound-design-roadmap.md`, `docs/AGENT_GUIDE.md` (den guide en AI får via MCP-bryggan: den ska stämma med verkligheten), `docs/preset-links-plan.md` (historik över presetarbetet).
4. `git log --oneline -40` på `dev`.

Målet med projektet: ett psytrance-instrument där kick och bas (och fler instrument) ska bli rätt tillsammans, och där en AI kan hjälpa till via verktygen. Nuvarande ljud är fortfarande ett utkast på många ställen.

## 1. Regler

- **Ändra aldrig ljudet av misstag, och sänk aldrig kvaliteten hörbart.** Ta två baslinjer innan du ändrar något: `KickbaseMcp tool rendersig > baseline-kod.txt` (kör mot Init från koden, det gamla reproducerbara startläget) och `KickbaseMcp tool rendersigreal > baseline-init.txt` (kör mot det riktiga, inbakade Init). Jämför efter varje steg. Blir en rad annorlunda är det en ljudändring: mät den (nolltest, toppskillnad i dB) och skriv ner den. En ändring i *koden* (optimering, städning) får inte ändra ljudet alls, utan ska vara bit-identisk eller bevisat ohörbar. Kan den sänka kvaliteten hörbart: gör den **inte**, utan lägg den som en fråga till Tobias i frågedokumentet (se nästa punkt).
- **Du får ändra ljud (sound design) om du tycker att det blir bättre, men aldrig på originalen.** Rör inte Tobias presets, fabrikspresets eller `Resources/InitPreset.mofp`. Spara det du ändrar som en kopia med namnet `<originalnamn> (edited by Opus)` (som användarpreset, så att Tobias kan jämföra med originalet), och skriv i rapporten vad som ändrades och varför. Kom ihåg att en ljudändring också kan göra det sämre: motivera den med mätning och lyssna/jämför (`audit`, `rendersig`, analysverktygen), och ta kvalitetsfrågor via frågedokumentet.
- **Tobias hinner inte svara medan du jobbar. Du arbetar självständigt.** Alla frågor till honom skriver du i `docs/audit/questions-for-tobias.md` (en post per fråga: sammanhang, alternativ, din rekommendation, vad som händer om han inte svarar, status). Fråga inte i chatten och vänta inte: gå vidare med allt du kan göra säkert. När första körningen är klar går ni en iteration tillsammans och tar frågorna.
- **Inget får gå otestat.** Varje rättning, optimering och förbättring ska testas efteråt: ett test eller en mätning som visar att den fungerar och inte skapar nya fel, plus hela testsviten och `rendersig`. Saknas testet: skriv det först. Skriv i rapporten *vilket* test som bevisade varje ändring.
- **Parameter-id:n (`bass1Cutoff`, `mixKickGain`, `fxA…`) och preset-mappnamn får inte döpas om.** De står i Tobias projekt och presets. Behövs det: skriv en migrering och testa den med gamla filer.
- Jobba på en egen gren från `dev` (t.ex. `audit`). Små commits med tydliga meddelanden, avsluta med `Co-Authored-By: Claude Opus <noreply@anthropic.com>` (eller den rad systemet ger). Pusha grenen, men **slå inte ihop något och rör inte `main`** utan att fråga.
- Stora omstruktureringar och allt som ändrar hur något _låter eller ser ut_ på ett sätt Tobias märker: lägg det som en fråga med en tydlig rekommendation i `docs/audit/questions-for-tobias.md` och gör det inte förrän han svarat. (Undantag: namnbytena i 4.7 och 6.3(f) är beställda, men görs sist och med särskild försiktighet.) Små, säkra rättningar (buggar, felaktiga tooltips, döda variabler) gör du direkt, med test.
- Tobias vill inte skriva C++ själv i det här projektet. Ge honom inga kodläxor, men håll honom insatt: berätta vad du hittade, varför det var fel, och var i koden det sitter.
- Skicka aldrig något utåt (git push till andra remotes, publicera, mejla). Radera ingenting du inte tittat på.

## 2. Bygge och test

- Bygge: `build.bat RelWithDebInfo`, eller riktat: vcvars64 följt av `cmake --build build\RelWithDebInfo --target <mål>`. Mål: `MyOwnFruit_VST3`, `MyOwnFruitFX_VST3`, `MyOwnFruit_Standalone`, `MyOwnFruitTests`, `KickbaseMcp`, `KickbaseHostTest`. Releasebygget ligger i `build\Release`. Tobias installerar själv med `createVst.bat` (admin, FL stängd).
- Tester som **alla** ska gå igenom efter varje större steg:
  `MyOwnFruitTests.exe`, och `KickbaseMcp tool` + `eqtest`, `rolltest`, `lengthtest`, `instrumenttest`, `presettest`, `auditiontest`, `settingstest`, `chattest`, `listentest`, `perftest`, samt `KickbaseMcp --selftest-link`, `KickbaseHostTest <vst3>` på **båda** VST3-filerna, och `rendersig`.
- **Viktigt om Init i testerna:** instrumentets Init är numera en inbakad fil (`Resources/InitPreset.mofp`). Efter bytet föll `eqtest`, `rolltest`, `presettest` och `--selftest-link` eftersom de antog det gamla Init (t.ex. `kickTune` 0). Jag la därför till `PluginBase::preferCodeInit`: `tool`-kommandona och `--selftest-link` kör mot Init från koden (reproducerbart startläge) utom `snapshot`, `render`, `export`, `repairpresets` och `rendersigreal`, som använder det riktiga Init. **Jag hann inte köra om hela sviten efter den ändringen** (Tobias avbröt körningen): kör allt först av allt, och rätta det som eventuellt brister. Granska också om testerna borde köras mot båda Init-varianterna.
- Verktyget: `KickbaseMcp tool params instrument|fx` (hela parameterregistret), `tool audit`, `tool repairpresets [apply]` (läser om och sparar om alla användarpresets, och verifierar att ljudet är identiskt), `tool snapshot instrument|fx <png> <flikar>` (skärmbild av gränssnittet utan DAW; flikcifffror: lägre siffra 0 EFFECTS, 1 SIDECHAIN, 2 LFO, 3 ENV, 4 MATRIX, 5 MASTER, 6 SOUNDSTAGE, 7 ANALYZER, 8 SETTINGS, tiotalet = övre flik, 20 = instrumentsidan; miljövariabler som `KB_INSTRUMENTS=10`, `KB_SPECTRUM`, `KB_HARMONICS`). Använd skärmbilder för att _se_ varje sida, i flera storlekar och med 1, 3 och 10 instrument.

## 3. Fällor jag (Claude Sonnet) har gått i, undvik dem

- Python-heredoc förstör `\n` i C++-strängar. Skriv patch-skript med Write-verktyget, och använd `chr(92)` för bakstreck.
- `tasklist | grep cl.exe` träffar även excel.exe. Leta efter `^ninja.exe`.
- När du kapar text med `s.index(slutmarkör)` måste sökningen börja _efter_ startmarkören, annars dubbleras innehåll.
- `Pattern::Note` går inte att kopiera (använd referens).
- JUCE `AlertWindow::showAsync`: med N knappar returnerar knapp X (0-räknat) `(X+1)%N`, så första knappen ger 1 och sista ger 0.
- PowerShell 5.1 saknar `&&`; Git Bash saknar `cat` på stora filer utan att hänga: läs med Read-verktyget.
- Filer i repot har blandade radslut (CRLF/LF). Bevara det som finns.
- Läs JUCE:s egen dokumentation och källa för den installerade versionen _innan_ du bygger en lösning runt ett bibliotekets beteende (Tobias krav).

## 4. Vad som ska granskas (kryssa av, skriv resultat i rapporten)

### 4.1 Varje parameter, varje ratt, varje knapp

Gå igenom `tool params instrument` och `tool params fx` rad för rad. För **varje** parameter:

- Finns den i gränssnittet (eller är den medvetet dold/AI-only)? Gör den något hörbart? Hittar du döda parametrar, dubbletter eller parametrar som inte längre används: lista dem.
- Rätt namn, enhet, intervall, standardvärde, skevhet (skew), avrundning, visning ("+3.0 dB", "Off" ...)? Stämmer standardvärdet med ljudet i Init och med presets?
- Tooltip/beskrivning: stämmer den med vad ratten faktiskt gör nu? Språk och ton lika överallt (engelska, kort, med exempel).
- Går den att automatisera i en DAW (namn, gruppering, `ParameterID`-version)? Modulerbar (MATRIX/LFO) där det är meningsfullt?
  Gör samma sak för **alla** knappar, menyer, flikar och högerklick: vad händer, finns det ett tillstånd som aldrig går att nå, och finns det något som ser klickbart ut men inte är det.

Sidor att täcka: PATTERN (rutnät, pianorulle, GATE/LENGTH, FILL, M/S, SWING/HUMAN/DELAY, kortet för vald rad, Pattern preset), KICK (inklusive sample-OFFSET, envelopes, EQ), INSTRUMENT (OSC 1–3, SAMPLE, SUB, NOISE, FILTER med ENV/ATTACK, WARP, DRIVE, LAYER OUT, MODULATION, diagrammen med NOTE OFF), EFFECTS (11 kanaler × 5 platser, EQ, kompressor, nivå/duck, FX Preset), SIDECHAIN, LFO, ENV, MATRIX, MASTER (MIXER och MAXIMIZER), SOUNDSTAGE, ANALYZER, SETTINGS, AI-fliken, EXPORT, förhandslyssning/pianot, topprad (BPM, takt, FREE RUN, preset).

### 4.2 Ljudtråd och korrekthet

- Inga allokeringar, lås, filläsning eller blockerande anrop på ljudtråden. Atomics på rätt ställen. Denormals och NaN/Inf-skydd i alla filter, feedback och effekter.
- Byte av samplerate och blockstorlek, mycket små och mycket stora block, offline-rendering (`renderOffline`, export, `createOfflineCopy`), 12 utgångsbussar, tomma/ogiltiga buffertar.
- **Trådsäkerhet vid laddning:** preset-laddning, projektladdning och `setInstrumentCount` körs på meddelandetråden medan ljudtråden spelar. Kontrollera `PluginBase::loadPresetXml`, `loadInstrumentPreset`, `loadPatternPreset`, `Pattern::fromValueTree/rowFromValueTree`, sidechain-kurvan, LFO-kurvor, `modRoutes` och `ensureTablesInUse` (blockerar): kan ljudtråden läsa något halvskrivet?
- Groove: `InstrumentProcessor::renderSegment` med `pending`/`due`, `delaySamplesFor` (swing + humanize + delay, bara senare). Gränsfall: notändar över blockgräns, loopvarv, transportstopp, tempoändring.
- Mönster: ticks (96/fjärdedel), lager 0–9 + `kickLayer`=10, pianorulle, slide, längd, loop per rad, olika längder och rutnät per rad.
- Mätare/analys (`MixAnalysis`): kick-svansens mätning i tidsdomänen, sample peak/rms per källa, "där tiden finns"-pekaren i exporten, `fxChannelUsageMask`.

### 4.3 Prestanda

- CPU per röst och totalt med 10 instrument, unison, wavetables, warp, FX på alla kanaler, spektrum/analysator igång. Kör `perftest`, och mät sedan själv med profiler eller tidtagning. Hitta de tre dyraste delarna och föreslå/gör åtgärder som **inte ändrar ljudet** (jämför `rendersig`).
- **Optimeringsprincip (Tobias):** leta efter optimeringar i koden, men gör ingen som skapar risk för krasch (trådsäkerhet, gränsfall, minnesfel) eller som i onödan försämrar ljudkvaliteten. En optimering ska vara värd sin risk: mät först hur mycket den vinner (CPU-procent, millisekunder), skriv vinsten i rapporten, och gör den bara om vinsten är tydlig och risken liten. Osäker eller liten vinst med märkbar risk: lägg den som fråga i frågedokumentet i stället. Varje optimering ska ha ett test (rendersig identisk, `perftest` före/efter, gränsfallstester).
- Gränssnittet: timers (Hz), `repaint` av hela sidor, ritning av diagram/scopes/spektrum, minnesanvändning, hur snabbt ett fönster öppnas och stängs.
- Konstruktorn laddar nu Init (en XML-fil på ~150 kB med ~2050 parametrar) via `loadEmbeddedInit`: mät uppstartstiden i en DAW-lik värd (`KickbaseHostTest`) och kolla att det inte blir märkbart.

### 4.4 Sparande och bakåtkompatibilitet

- `getStateInformation/setStateInformation`: rundtur, gamla projekt (mönster version 6, kicken som lager 3 förr), saknade parametrar, för många/få instrument, sparade namn (`presetBass<n>`, `presetRow<n>`, `presetFx<n>`).
- Preset-systemet (`Presets.h/.cpp`, `PluginBase::save/loadUserPreset`): `belongsTo` per del, kanoniska id:n som flyttas till målraden/-kanalen, äldre filformat (utan `scope`) som göms eller laddas, `resetSectionToDefaults`, bassamples/click-sample i filerna.
- **Nytt, granska extra noga:** länkade presets (`saveInstrumentPreset`, `savePatternPreset`, `loadInstrumentPreset`, `loadPatternPreset`, `readPresetLink`, `findUserPreset`, `firstFreeFxChannel`; dialogerna i `PresetButton`). Länken är ett _namn_. Kantfall: presetet byter namn eller raderas, två presets med samma namn, ogiltiga filnamn (`createLegalFileName`), alla 11 FX-kanaler upptagna (menyn för att ersätta en), avbryta i mitten, kick-raden, FX-versionen, laddning via AI (`ToolLayer::loadPreset` laddar alltid länkad effekt på första lediga kanal och ett mönster utan sitt länkade instrument: är det rätt?), instrument som redan skickar till en kanal.
- FX Preset = en kanal (`scope="channel"`), Maximizer = `Section::master` (bara på/av, drive, clip, ceiling, release), Soundstage = `Section::soundstage` (width, mono-below, enhance), Sidechain (`sc*` + DUCK-djup per instrument). **Inga routes** (`mix*Max`/`mix*Duck`, vem som går genom maximizern eller ducks) i dessa presets, bara i Full. Mixern har medvetet inget eget preset. Kolla att `belongsTo`, fabrikslistorna och knapparnas läge stämmer, och att `master*Eq*` inte hamnar utanför.
- Fabriks-Init för instrumentet är numera filen `Resources/InitPreset.mofp` (Tobias "SOUNDS KIND OF GOOD 5", reparerad), inbakad via `KickbaseGuides`. FX-versionen använder fortfarande listorna i koden. Kontrollera att detta är robust: saknad resurs, parse-fel, och att `Presets::factoryName/Count` fortfarande säger "Init".

### 4.5 MCP-bryggan och AI-lagret

- `ToolLayer.cpp`: varje verktyg gör det beskrivningen säger. `docs/AGENT_GUIDE.md` och `AnalysisAbout.h` ska stämma i detalj med parameternamn, intervall och beteende (särskilt nytt: swing/humanize/delay per instrument, kick OFFSET, filter ATTACK, pitch-kurva, preset-sektionerna, M/S).
- Delad analysator mellan ANALYZER-fliken och AI-verktygen; exportens JSON (`meta.where_is_time_and_snapshots`, moduleringar, limiter, taktposition per ram).
- Bryggans säkerhet och fel: ingen port/konfiguration som läcker, tydliga felmeddelanden, timeouts. (Tobias måste starta om Claude Desktop helt efter `install-mcp.ps1 Release`.)

### 4.6 Design och användbarhet

- Samma färger, avstånd, typsnitt, ramar och skiftläge överallt. Kontrast och läsbarhet i mörkt tema. Etiketter som inte får plats (klipps de snyggt?), tomma lägen ("none saved yet", inga toner), tooltips på **allt**.
- Fönsterstorlekar och skalning (DPI), 1/3/10 instrument, scroll i mixer/sidechain/mönster, flerval-menyer (FX-kanaler, instrument), dolda oanvända FX-kanaler, kortet för vald rad (ram + padding), spektrumets mjukhet.
- Flöden: spara/ladda preset (alla sorter), byta instrument, lägga till/ta bort instrument (bekräftelse), ångra, projekt som öppnas på nytt.
- Ge **konkreta** förbättringsförslag med skärmbild före/efter där det går. Fråga Tobias om smaksaker; fixa uppenbara fel själv.

### 4.7 Kodkvalitet och städning

- **Namn:** "bass" lever kvar internt från när instrumenten hette bas (`Section::bass`, `BassField`, `BassSynth`, `bassPresetNames`, XML-taggen `BassSample`, mappen `Bass`, id:n `bassN…`, "Gate" som namn på bassektionen i FX-versionen, `MyOwnFruit` kontra `Kickbase` i mål/mappar). Föreslå och (efter OK) genomför ett namnbyte av **C++-namn** till `instrument`, med id:n/mappar orörda eller migrerade (se regler). Gör det i en egen commit, mekaniskt, med `rendersig` identisk efteråt.
- Död kod, dubbletter, ofullständiga kommentarer, kommentarer som ljuger, magiska tal, för långa funktioner, filer som blivit för stora (`PluginBase.cpp`, `Modules.cpp`, `Tool.cpp`). Dela upp där det ökar läsbarheten, men inte bara för sakens skull.
- Kompilatorvarningar (aktivera fler och rätta), `static_assert` för det som får inte ändras (id:n, antal), enhetstester för det som saknar test.
- Docs: `README.md`, `docs/*`, hjälptexterna (`Help.cpp`), AGENT_GUIDE. Sådant som inte längre stämmer ska rättas eller tas bort.
- **`MyOwnFruit` → `Kickbase`:** projektet heter inte MyOwnFruit längre. Själva huvudmappen och allt som innehåller namnet (CMake-mål som `MyOwnFruit_VST3`/`MyOwnFruitFX_VST3`/`MyOwnFruitTests`, `MyOwnFruitTool.exe`, `build.bat`/`createVst.bat`/`install-*.ps1`, `MyOwnFruit.vst3` bredvid `Kickbase.vst3` i byggmapparna, kommentarer, docs) ska bytas till Kickbase om det går. Försiktighet, i den här ordningen:
  1. **Plugin-identiteten får inte ändras:** `PLUGIN_CODE`, `PLUGIN_MANUFACTURER_CODE`, bundle-id och allt som bestämmer VST3-klass-id i JUCE. Annars hittar DAW:en inte längre Tobias projekt. Läs JUCE:s dokumentation för 8.0.15 om vad som bestämmer id:t, och bevisa med `KickbaseHostTest` + ett sparat projekttillstånd att det laddas som förut. Produktnamnen "Kickbase" och "Kickbase FX" är redan rätt.
  2. Byt namn på CMake-mål, skript, docs och kommentarer i en egen mekanisk commit; bygg om från ren build-mapp; alla tester + `rendersig` identiska.
  3. **Sist, och bara på Tobias OK:** själva mappen på disken (`C:\Users\hunga\Documents\GitHub\MyOwnFruit`) och GitHub-repot. Det går inte att göra mitt i din egen session, det ändrar sökvägen som Claude Codes minne är knutet till (`...\.claude\projects\c--Users-hunga-Documents-GitHub-MyOwnFruit`), gör `build\`-mapparnas CMake-cache ogiltig, och att döpa om GitHub-repot är utåtriktat. Skriv i stället en steg-för-steg-instruktion i frågedokumentet (stäng allt, döp om mappen, flytta/kopiera minnesmappen, bygg om, kör `install-mcp.ps1`, uppdatera remote) så att Tobias kan göra det.

## 5. Vad jag vet har ändrats nyligen och är extra värt att granska

Allt detta ligger på `dev` och är byggt av Claude Sonnet under två dagar, med tester men utan lång användning:

- Delad analysator, kick-svans (`kickTailMeasuredHz/DesignHz`), sample peak/rms per källa.
- NOTE OFF-markör i envelope-diagram (`EnvelopeView::NoteOff`, `typicalNoteSeconds`) och filter-envelopens ATTACK (`envAttack`, standard 1 ms = oförändrat ljud).
- Mixern: dolda oanvända FX-kanaler, remsor med fast bredd, horisontell scroll, fast master; flervalsmenyer (`SourceToggles`); `DuckRow` med vertikal scroll.
- Pattern: per rad M/S (`soloMask`, sparas inte), SWING/HUMAN/DELAY per instrument och globalt (`patSwing`, `patHumanize`, `delay` bara senare), kort med ram, Pattern preset per rad, FILL, pianorulle.
- Pitch-envelopens kurva, kickens `kickSampleOffset`, spektrumets utjämning (`SpectrumPage::smoothColumns`, 40 dB/s).
- Presets: FX Preset per kanal, Maximizer/Soundstage/Sidechain som egna delar utan routes, länkade presets, inbakad Init, `repairpresets`.

Kända avgränsningar jag medvetet lämnat:

- AI:ns `load_preset` frågar inte (första lediga FX-kanal, ingen dialog) och `save_preset` kan inte skapa länkar.
- Länkar är namn: byter man namn på ett preset bryts länken (en varning visas vid laddning).
- Ingen "rename"-funktion för presets.
- `ensureTablesInUse` blockerar (byggs vid laddning, inte på ljudtråden).

## 6. Arbetssätt och leverans

1. **Inventera först:** kör alla tester, ta baslinje med `rendersig`, ta skärmbilder av varje sida (1/3/10 instrument), exportera `tool params` till `docs/audit/params-*.txt`.
2. **Skriv rapporten löpande** i `docs/audit/audit-report.md`: en tabell per område med `fil:rad`, vad som är fel/oklart, hur allvarligt (krasch / fel ljud / förvirrande / kosmetiskt), och status (rättad i commit X / fråga nr N i `questions-for-tobias.md` / föreslagen).
3. Rätta i den här ordningen: (a) krascher, trådsäkerhet och fel ljud, (b) data/sparande/bakåtkompatibilitet, (c) fel och missvisande text (tooltips, guide), (d) prestanda utan ljudändring, (e) design och städning, (f) namnbytet av `bass` → `instrument` samt från `MyOwnFruit` → `KickBase` - sist och separat.
4. Efter varje steg: alla tester + `rendersig` jämförd mot baslinjen + `KickbaseHostTest` på båda VST3-filerna.
5. Avsluta första körningen med: en kort svensk sammanfattning till Tobias (vad som rättades, vad som kräver hans beslut, med länk till `questions-for-tobias.md`, vad som är kvar; sedan tar ni en andra iteration på frågorna), det **viktigaste att han själv läser** (högst fem punkter med `fil:rad`), och att han ska köra `createVst.bat` för att installera Release. Bygg Release innan du säger att det är klart.
6. Uppdatera handoff-minnet (`project-status-handoff.md`) och `docs/AGENT_GUIDE.md` med det som ändrats. Föreslå en ny chatt för nästa stora uppgift.

Tobias vill att du hellre skriver en fråga för mycket i frågedokumentet om sådant som påverkar hur instrumentet låter eller känns, än gissar. Inga förbättringar eller optimeringar eller fixar får gå otestade så inga nya fel orsakas, samt att det inte får sänka kvalité hörbart iallafall. Om det sänker kvalité, ta upp det med Tobias som en fråga om vi ska göra det. Allt annat: gör det, mät det, skriv ner det.

Tobias kommer inte ha tid att svara på alla frågor under tiden utan du kommer jobba självständigt, så spara alla frågor i dokumentet och gå vidare med det du kan gör ni en iteration sen när första körningen är klar.
