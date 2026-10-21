# Fas A: rapport (gren `content-a`, från `content-0`)

Skriven av Sonnet 18 okt 2026 (granskad av Opus steg för steg). Planen: [content-plan.md](content-plan.md), reglerna: [ENGINEERING.md](ENGINEERING.md).
Fas A ger ett bibliotek med fria ljud, kickens två vyer SYNTH | SAMPLE och ett verktyg för att hämta CC0-ljud från Freesound.
**Inget ljud har ändrats:** `rendersig`, `rendersigreal` och `rendersigwide` är identiska med baslinjerna efter varje steg. Ingen ny parameter har lagts till.

## 1. Vad som byggts (sex steg, sex commits + en rättning)

| Steg | Commit | Innehåll |
|---|---|---|
| 1 | 7b54e9a | `Library::Manifest` (läsa, kontrollera, skriva), `Settings::libraryFolder()`, `tool librarycheck`, `tool librarytest` |
| 2 | 7a809bf | `LibraryImport`: 24-bit mono WAV, 44,1 kHz, `tool importlibrary` |
| 3 | d119118 | `LibraryIndex` (delad, bakgrundstråd, filter) och antialiasfiltret före nedsampling |
| 3b | ae65944 | Trådfixar efter Opus granskning: `Thread::wait/notify`, avbrytbar skanning, `shutDown()` |
| 4 | e0b73b5 | Kickens SYNTH \| SAMPLE, biblioteksvyn, LIBRARY-raden i SETTINGS, `installAll.bat` |
| 5 | db251bb | `tool freesound`, AI-verktygen `list_library` och `load_library_kick`, `Item::url`, PERSONAL-raden |

Diff mot `content-0`: 29 filer, +3 593 / −24 (med den här rapporten). Nya filer: `Source/Core/Library{Manifest,Import,Index}.{h,cpp}`, `Source/Ui/KickLibraryPanel.{h,cpp}`,
`Tools/LibraryTests.cpp`, `Tools/Freesound.cpp`, `installAll.bat`.

## 2. Besluten a–i (Opus OK till Sonnets spec) och hur de uppfyllts

- **a) `LibraryIndex`:** `juce::SharedResourcePointer`, tråden stoppas med sista instansen. Ingen skanning i någon timer: den görs vid start, när KICK > SAMPLE eller SETTINGS visas
  (`refreshIfChanged`, som bara tittar på ändringstider för mappar och manifest) och vid RESCAN. Listan är en virtualiserad `juce::ListBox`. `TemporaryUserFolders` pekar också `libraryFolder` till tempmappen.
- **b) Importen:** 24-bit mono WAV, `juce::WindowedSincInterpolator`, tystnad trimmas bara före attacken och efter slutet (−60 dB av toppen, 24 samplingars pre-roll), fade bara i slutet (10 ms), −1 dBFS sample peak mätt efter fade.
  Kickar högst 2 s, övrigt högst 30 s.
- **c) Ingen UNDO** för biblioteksval.
- **d) Ingen `install-library.ps1` eller INSTALL-knapp** (kommer i fas C). LIBRARY är en fjärde rad i FOLDERS (CHANGE / RESET / OPEN), bara i instrumentet. `placeHint` behövde inte tas bort.
- **e) AI-verktyg:** `list_library` och `load_library_kick` (bara Kicks; PERSONAL listas med märket `personal`), dokumenterade i [AGENT_GUIDE.md](AGENT_GUIDE.md).
- **f) Flikarna:** SYNTH \| SAMPLE i modulens rubrikrad, till vänster om Preset (`ModulePanel::setHeaderTabs`). I SAMPLE visar displayen samplingens vågform (statisk). PITCH/AMP hör bara till SYNTH.
  Fliken är UI-tillstånd: `PluginBase::kickTab` (atomisk int, sparas inte; testat att tillståndet är oförändrat).
- **g) MODE → REPLACE** med en gest (`beginChangeGesture`, `setValueNotifyingHost`, `endChangeGesture`) och bara om MODE stod på CLICK. Testat åt båda håll med en parameterlyssnare.
- **h) Freesound:** via `juce::WebInputStream`, token krypterad med DPAPI (`Settings::protect/unprotect` delas nu med AI-nyckeln), `freesoundtest` mot en mock-server (`freesoundBase`), inget hemligt skrivs ut.
- **i) Klasserna** `LibraryManifest`, `LibraryIndex`, `LibraryImport` i Core; verktygen delar kärnkoden med testerna.

## 3. Avvikelser från planen och specen

1. **Licenslistan** är exakt `CC0-1.0`, `MIT`, `BSD-2-Clause`, `BSD-3-Clause`, `PERSONAL` (inga jokertecken). Apache-2.0 togs bort ur Sonnets lista (inte i planens vitlista).
2. **Freesound-nycklarna** heter `FREESOUND_CLIENT_ID`, `FREESOUND_CLIENT_SECRET_APIKEY` och `FREESOUND_OATH_REDIRECT` (Tobias namn) och `KICKBASE_FREESOUND_ID/_SECRET/_REDIRECT` (planens). Miljövariabeln går före en gitignorerad `.env-local` i arbetsmappen. `redirect_uri` ur `FREESOUND_OATH_REDIRECT` används i både authorize och tokenbytet.
3. **Freesound bara i verktyget:** inget Freesound i pluginet och ingen nyckel i SETTINGS (Tobias beslut). Står i ENGINEERING och AGENT_GUIDE.
4. **`Item::url`:** en ny valfri post-egenskap (sidan för ett enskilt ljud, t.ex. `https://freesound.org/s/<id>/`); gamla manifest läses som förut. `ImportOptions::decorateItem` fyller i posterna.
5. **Antialiasfilter** (Opus önskemål efter granskning): JUCE:s interpolator filtrerar inte vid nedsampling, så ett linjärfas Kaiser-FIR (passband 20 kHz, stopp 22,05 kHz, 95 dB, jämn ordning) körs före interpolatorn när källan är över 44,1 kHz.
   44,1 kHz-filer kopieras bit för bit. Egen design i stället för `dsp::FilterDesign` för att styra ordningen (gruppfördröjningen order/2 måste vara ett heltal).
6. **Windows reserverade namn** (CON, PRN, AUX, NUL, COM1-9, LPT1-9, också med filändelse) avvisas i manifestets sökvägar (Opus önskemål).
7. **Egen designdetalj:** ett klick på en rad i biblioteket laddar kicken direkt (som LOAD). Ingen förhandslyssning före laddning.

## 4. Mätningar och bevis

- **Ljudet:** `rendersig*` identiska med `baseline-kod/init/wide-kontrolltakt.txt` (jämförda efter radslut, baslinjefilerna har CRLF). `blocksizetest` och `audiotest` gröna.
- **Tester:** `librarytest` 149 kontroller, `freesoundtest` 29 kontroller, `KickbaseTests` och hela verktygssviten gröna, `KickbaseHostTest` ok på båda VST3.
- **Omsampling:** en impuls vid 48/96/22,05/88,2 kHz hamnar vid sampling 1102 / 1102 / 1102 / 1102 (väntat 1102,5 / 1102,5 / 1102 / 1102,5): inom en halv sampling, latensen kompenserad.
  Antialias: 23 kHz vid 48 kHz −99,5 dB, 30 kHz vid 96 kHz −132 dB, 40 kHz vid 96 kHz −119 dB, 25 kHz vid 88,2 kHz −123 dB (kravet var −60). 1 kHz ändras −0,087 dB (kravet ≤ 0,1; samma värde vid alla frekvenser, troligen mätfönstret).
- **Import:** peak −1,00 dBFS, tystnad borta (attacken vid sampling 58 efter filtret), sista samplet < 1e-3, samma ljud ger byte för byte samma fil.
- **Index:** 2 000 paket skannas på 1,1 s; en skanning avbruten mitt i stoppas på 3 ms utan våld (`stopThread` ger true) och publicerar inget.
- **Gränssnitt:** 42 skärmbilder jämförda med före UI-steget (`Tools/pngdiff.py`): KICK-fliken och SETTINGS skiljer som väntat, i övrigt se punkt 5.
- **Freesound på riktigt:** en sökning med Tobias nyckel (`tool freesound search psytrance kick`) gav 187 CC0-träffar, och en riktig hämtning av två originalfiler (OAuth2) gick bra.

## 5. Kända saker

- **En pixel:** (514,177), en nivå mörkare (29,31,45 → 28,30,44) i alla INSTRUMENT-bilder, kanten av SEMI-rattens halo. Deterministisk. Troligen en cachad rattgradients avrundning som beror på vilken ratt som byggs först. Accepterad av Opus.
- **Två bilder kan skilja mellan körningar:** `inst_n10_tab23` (ENV, en liten röd klippt text vid sidfoten) och AI-fliken (`inst_upper3_tab30/38`, ca 175 px). De skilde redan mellan 54bc067 och före UI-steget. Står i ENGINEERING.
- **mp3 stöds inte** i bygget (`JUCE_USE_MP3AUDIOFORMAT` är av): sådana filer rapporteras som "not a readable audio file". Står i hjälptexten för `importlibrary`.
- **OAuth2 är provat på riktigt av Tobias** (18 okt): `tool freesound auth` och `code <kod>` gav en token, och `tool freesound fetch 128623 128622 Kicks` hämtade två CC0-kickar (ASBS, Pure Psytrance Kick 018 och 019) till `Library\Kicks\Freesound CC0` (2 ljud, 0 fel, `librarycheck`: ok, CC0-1.0).
  Resten av flödet (förnyelse av token, 429) är bara testat mot mock-servern.
- **Redirect-adressen** är `https://tkdev.se/kickbase/oath2` (sajten svarar 200 med HSTS, och `http://` svarar 308 till `https://`). OAuth2 förutsätter TLS för den. Tobias bytte till https i `.env-local` och i Freesound-appen samtidigt: `redirect_uri` måste vara exakt densamma på båda ställena och i både authorize och tokenbytet. Koden innehåller inte adressen (den läses ur `FREESOUND_OATH_REDIRECT`).
- **`ARTCRAFT_API`** ligger fortfarande i git-historiken (commit cb45841 på origin) och ska roteras innan repot blir publikt (regel 10). **Roterad av Tobias 18 okt 2026** (den gamla är borttagen); historiken ska ändå skannas före publicering.
- **Biblioteket är tomt** tills Tobias lägger paket i `Dokument\Kickbase\Library` (eller hämtar med `tool freesound fetch`). Inget ljudinnehåll ligger i repot.
- **Direkt faltning** i antialiasfiltret: en 5 minuters 96 kHz-fil tar några sekunder att importera. Acceptabelt för ett offlineverktyg.

## 6. Release-bygget

Bygge: `build.bat Release all` (alla mål, 372 steg) gick igenom utan fel, och `KickbaseTests` (körs av build.bat) gick igenom. På Release-bygget (`build\Release`):

- `rendersig`, `rendersigreal` och `rendersigwide` identiska med baslinjerna;
- `KickbaseHostTest` ok på både Kickbase.vst3 och Kickbase FX.vst3 (laddar, spelar, öppnar och stänger editorn, laddar ur);
- `librarytest` (149 kontroller) och `freesoundtest` (29 kontroller) gröna;
- `perftest`: 10 instrument 0,078 s CPU per sekund ljud (RelWithDebInfo: 0,094).

Hela sviten kördes på RelWithDebInfo (se punkt 4); på Release kördes bara det som ovan. Inga nya skärmbilder togs på Release-bygget (samma kod, samma bilder).
