# Plan: fria ljud, fler källtyper, 303, polyfoni och sampler

Planen är skriven av Opus (granskaren) efter Sonnets förberedelse och Tobias svar (18 okt 2026). Sonnet bygger, fas för fas. Sonnets
källforskning (licenser, paket, länkar) står kvar oförändrad i [bilagan](#bilaga-källor-och-licenser-sonnet-18-okt). Varje fas ska kunna
släppas för sig, och **med standardvärden ska ljudet vara bit för bit som i dag**.

**Läs först:** [docs/ENGINEERING.md](ENGINEERING.md) (regler, verktyg och fällor; den gäller för allt nedan),
[docs/audit/audit-report.md](audit/audit-report.md) (vad granskningen ändrade) och den här filen i sin helhet.

## 0. Utgångsläge

- **Gren:** bygg från `dev` *efter* att `audit` slagits ihop (en ren fast-forward: `dev` ingår helt i `audit`). Tobias döper sedan mappen till
  Kickbase (fråga 8 i `docs/audit/questions-for-tobias.md`, svar A): vänta med att börja tills det är gjort, så att sökvägarna inte byts mitt
  i en fas. En gren per fas: `content-a`, `content-d1`, `content-p`, `content-b1`, `content-c`, `content-f`, `content-e`.
- **Baslinjer:** `docs/audit/baseline-kod-kontrolltakt.txt`, `baseline-init-kontrolltakt.txt`, `baseline-wide-kontrolltakt.txt` (efter
  kontrolltakten). De ska vara identiska efter varje fas med standardvärden.
- **Tobias beslut:**
  - Kickbase blir öppen källkod under **AGPL-3.0** ("Öppen källkod", "1. Ja").
  - GitHub-repot är **privat tills allt är klart** ("Inte än, öppnar det när vi är klara"); han öppnar det själv.
  - Freesound hämtas via **API-nyckel** ("Api nyckel låter väl smidigt"), som han skapar själv.
  - Ordningen på instrumenten är fri ("Spelar ingen roll", "vi gör det som blir bäst").
- **Tobias önskemål:**
  - fria presets och samplingar för kick och bas, och fler oscillatorer/vågformer;
  - en 303 ("Open303, ja allt låter asbra", också AKWF);
  - piano, bells, music box, trance plucks och trance-synthar med såg/fyrkant/sinus;
  - att kunna kombinera det man hittar till egna presets;
  - kicken i två flikar ("Du vet bäst hur vi kan implementera");
  - typ per oscillatorplats ("3 eller kanske max 5"), också ren sampling;
  - ett bibliotek med kickar, synthar och basar, loopar och långa synthljud, och bra verktyg för att dela upp ("splicea") en loop;
  - färdiga basloopar, och basgångar i MIDI (ingen CC0-samling finns: vi skriver egna mönster);
  - "freeware synth som instrumenttyp med deras presets" (svar i beslutstabellen nedan).

  Antal platser och hur typen väljs är överlåtet åt oss.

## 1. Beslut (svar på Sonnets frågor)

| Fråga | Beslut | Varför |
|---|---|---|
| Ordning | **A → D1 → P → B1 → C → F → E** | A ger nytta direkt och rör inte motorn. D1 är litet och isolerat. P (polyfoni, ny) krävs för piano/bells/pads innan samplingskällor blir användbara. B1 bygger källtypen som C och F använder. |
| B1 eller B2 | **B1: tre platser, typ per plats.** Inte fem. | Fem platser rör `oscsPerLayer`, `static_assert`, egna tabellers lagring och ~45 nya ID:n per instrument, kostar CPU och UI. Tre platser + typval + polyfoni räcker för allt Tobias nämnt. Kan göras senare om tre visar sig för lite. |
| SUB och BRUS som typer? | **Nej**, de får vara som de är. | De är billiga, alltid användbara och har egna rattar; att göra dem till platser ändrar ID:n och presets i onödan. |
| Hur typen väljs | En liten rullista **TYPE** i varje OSC-flik (WAVETABLE / SAMPLE / MULTI), till vänster om TABLE. OSC-fliken visar bara den valda typens kontroller. | Ingen ny plats på sidan; samma mönster som TABLE/WARP i dag. |
| Biblioteket | **Enskilda samplingar** (kick, one-shot, en ton, kort loop ≤ 30 s) **bäddas in** som i dag (base64 i projekt/preset). **Multisamplingar** (fas C) **refereras** med biblioteks-id + innehållshash; ett val "Embed in project" per instrument bäddar in dem (stort). | Inbäddning fungerar redan, är robust och flyttbar; bara multisamplingar är för stora (MB per instrument). |
| 303 | **D1:** Open303:s `TeeBeeFilter` som en ny **filtermodell** (egen parameter), inte en femte filtertyp. Ingen hel 303-röst (D2). | En femte val i `filterType` flyttar DAW-automation (se 2.3). 303-ljudet = såg/fyrkant (finns) + TB-filtret + accent (velocity) + glide (finns) + envelope (finns). |
| Sampler | **Egen** liten zonspelare + ett **offline**-verktyg som läser SFZ (delmängd) och skriver vårt format. Inte sfizz. | sfizz är arkiverat och har en stor beroendekedja; vi behöver bara zoner, rotton, nyckel-/velocity-område, slinga och release. |
| Maxlängd | Enskild sampling: **30 s** mono i minnet (1,3 MB vid 44,1 kHz float). Längre: inte nu (ingen streaming). Multisampling: bara de zoner som används laddas. | Räcker för loopar och långa synthljud på några takter. |
| GPL-synthar (Surge, Vital, Odin …) som instrumenttyp | **Inte nu.** Med AGPL-3.0 är koden juridiskt möjlig, men deras presetbanker har ingen klar licens (Vitals gratispresets är restriktiva, bara Vitalium är CC0). | Hela värdet ("deras presets") saknas alltså, och integrationen är enorm. Sampler + CC0-SFZ-bibliotek (tusentals instrument) och en Vitalium-importör ger mer. Kan tas upp igen när faserna är klara. |
| Licensfilerna | **Fas 0** nedan, i den första content-grenen (inte i `audit`, som bara innehåller granskningen). | Litet och fristående, och behövs innan något tredjepartsinnehåll läggs in (Open303 i D1). |

## 2. Regler för alla faser (utöver ENGINEERING.md)

1. **Parameter-ID:n.** Nya instrumentparametrar heter `bassN…` (som alla instrumentparametrar), annars hamnar de utanför
   instrument-presets (`Presets::belongsTo`), moduleringen (`isModulatable`) och `parameterOwner`. Kickens heter `kick…`. Visningsnamnen
   heter "Instr N …".
2. **Nya fält läggs sist.** `InstrumentField`: nya fält läggs före `count`, inte inne i oscillatorblocken: `oscFieldCount = 15` och
   `ParamID::oscField` räknar med att OSC 1–3 ligger i följd ([Params.h:84-104](../Source/Core/Params.h#L84-L104)). Ett nytt fält per
   oscillator blir därför tre nya fält sist (`aType`, `bType`, `cType`) med en egen hjälpfunktion.
3. **Utöka aldrig en befintlig valparameter** (`filterType`, `kickSampleMode`, effekttyperna …) med nya val. DAW:en sparar automation som
   normaliserat värde (0–1), och ett nytt val flyttar alla befintliga val (index / (antal − 1)). Lägg en **ny** parameter i stället
   (t.ex. `bassNFilterModel`).
4. **Standardvärdet ger dagens ljud,** och ett gammalt projekt/preset utan den nya parametern laddas med standardvärdet. Bevisa med
   `rendersig*` och ett sparat tillstånd från före fasen (`KickbaseHostTest <vst3> state save` med gamla bygget, `state load` med nya).
5. **Kontrolltakten:** allt nytt i ljudtråden ska klara `blocksizetest` (identiskt vid alla blockstorlekar). Det som räknas "per fönster"
   görs bara när `controlPhase == 0` (ENGINEERING.md avsnitt 3).
6. **Ljudtråden:** ingen allokering, inga lås, ingen filläsning (`audiotest`). Laddning av samplingar och multisamplingar sker på en
   bakgrundstråd; den färdiga datan lämnas till ljudtråden med en atomisk pekare och den gamla frigörs via `DeferredRelease`.
7. **Varje ny parameter:** hjälptext ([Help.cpp](../Source/Ui/Help.cpp), `tool helpcheck` = 0), unikt visningsnamn, med i
   `list_parameters`, [AGENT_GUIDE.md](AGENT_GUIDE.md) och [AnalysisAbout.h](../Source/Core/AnalysisAbout.h).
8. **Nya animerade vyer** (vågformsvy, biblioteksvy med provlyssning) ärver `VisualTimer`, och läser aldrig inställningsfilen i `paint`.
9. **Licenser:** inbakat eller i en delad installation bara CC0, eller MIT/BSD med notis. Varje fil i biblioteket har en post i sitt
   manifest (källa, URL, upphovsperson, licens, datum, `redistributable`). Inget ljudinnehåll i repot. Open303:s `License.txt` i
   `THIRD-PARTY.md` och i programmets "om"-text.
10. **Repot blir publikt.** Inga nycklar, tokens, lösenord, personliga sökvägar med känsligt innehåll eller PERSONAL-ljud får någonsin
    checkas in, inte heller i en commit som sedan tas bort (historiken publiceras). Freesound-nyckeln och Anthropic-nyckeln läses ur
    miljövariabler eller användarens inställningar (`Dokument\Kickbase\settings.xml`, krypterad som AI-nyckeln). Före publiceringen: en
    genomsökning av hela historiken (t.ex. `git log -p | grep` efter `sk-ant`, `secret`, `token`, `api_key`, långa base64-strängar, `.wav`,
    `.ogg`) och en rapport till Tobias.
11. **Varje fas avslutas med:** hela sviten + `rendersig*` mot baslinjerna + `KickbaseHostTest` på båda VST3 + Release-bygge + skärmbilder
    (1/3/10 instrument och FX) + en kort rapport (vad, hur verifierat, mätningar). Opus granskar innan grenen slås ihop.

## 3. Faserna

### Fas 0: licens och tredjepartsnotiser (gren `content-0`, liten)

- `LICENSE` med AGPL-3.0 (originaltexten från gnu.org, oförändrad), en rad i README ("Kickbase är fri programvara under AGPL-3.0"), och
  `THIRD-PARTY.md`: JUCE (AGPLv3-vägen), Open303 (MIT, notisen ordagrant, från fas D1), AKWF (CC0), och en rad per bibliotekskälla som
  installeras automatiskt.
- En rad i SETTINGS eller en "About"-text i pluginet som pekar på licensen och notiserna (AGPL kräver att användaren kan få källkoden).
- Inget annat i grenen. Tobias läser LICENSE-raden innan den slås ihop.

### Fas A: bibliotek + kickens två flikar

**Bibliotek (infrastruktur, ingen ljudändring):**
- Mapp `Dokument\Kickbase\Library\<Kategori>\<Paket>\` med `manifest.json` per paket:
  `{ "package", "source", "url", "license", "redistributable", "date", "items": [{ "file", "name", "tags", "rootNote", "bpm", "key", "author" }] }`.
  Kategorier: `Kicks`, `Bass`, `Synth`, `Pads`, `BassLoops`, `SynthLoops`, `Multi` (fas C).
- `LibraryIndex` (Source/Core): läser manifesten på en bakgrundstråd, aldrig på ljudtråden. En post utan giltig licens i vitlistan
  (`CC0-1.0`, `MIT`, `BSD-*`, `PERSONAL`) visas inte. PERSONAL-poster märks i gränssnittet och får aldrig hamna i en installerare.
- `tool importlibrary <mapp> <kategori> <licens> [källa-url]`: konverterar (mono, 44,1 kHz, float, trimmad tystnad, normaliserad till
  −1 dBFS), skriver manifestet, vägrar utan licens. `tool librarycheck`: validerar alla manifest (tester).
- `install-library.ps1` + en knapp i SETTINGS (som `WavetablePack`) för de paket som kan hämtas automatiskt (CC0 på GitHub, t.ex. ett
  VCSL-urval).
- `tool freesound` (verktyget, inte pluginet):
  - `search <fråga>` listar bara ljud med `license:"Creative Commons 0"`, med id, namn, taggar, längd och användare.
  - `fetch <id …> <kategori>` hämtar originalfilen och kör samma konvertering som `importlibrary`. Manifestet får källa
    (`https://freesound.org/s/<id>/`), användare, licens och datum.
  - Nyckel och klient-id läses ur miljövariabler (`KICKBASE_FREESOUND_ID`, `KICKBASE_FREESOUND_SECRET`), aldrig ur repot.
  - Originalfiler kräver OAuth2. Ett engångsgodkännande i webbläsaren ger en kod som klistras in. Access-token (24 h) och refresh-token
    sparas krypterade i användarens inställningar (som AI-nyckeln, DPAPI), aldrig i repot. Previews (förlustbehäftade) används bara för
    att lyssna.
  - Respektera API:ets gränser (per minut och per dygn, `/apiv2/usage/`). Vänta och försök igen vid 429.
- Kategorier att fylla först: Kicks (~187 CC0 psytrance-kickar), BassLoops (~145), Pads/långa ljud (~45). Kontrollera licensen per ljud
  (fältet `license` i svaret), inte per sökning.

**Kickens flikar:**
- KICK-sidan får två **vyflikar** SYNTH | SAMPLE (som PITCH/AMP i dag). Flikarna byter bara vy, de ändrar ingen parameter: att byta flik
  ska aldrig ändra ljudet.
- SYNTH = dagens sida. SAMPLE = biblioteket (kategori Kicks; sök, taggar, LISTEN) + det som rör samplingen: MODE
  (`kickSampleMode`: CLICK / LAYER / REPLACE), TUNE, OFFSET, LEVEL, LOAD/X. "Lager" (LAYER) finns alltså redan som val (fråga 9.4).
- Att välja en kick i biblioteket laddar den som i dag (`loadClickSample`, inbäddad), och sätter MODE till REPLACE om den stod på CLICK.
  Det är en tydlig handling av användaren, och den går att ångra.
- Ny parameter behövs inte. Kick-presets fungerar som förut.

**Tester:** `librarycheck` (vitlista, saknad licens avvisas); import av en testfil ger rätt manifest; att ladda en kick ur biblioteket och
spara/ladda projektet ger samma ljud (`persisttest`); gamla kick-presets oförändrade; skärmbilder av båda flikarna.

### Fas D1: TB-303-filtret

- Kopiera Open303:s filter-filer (Sonnet har listat dem: `rosic_TeeBeeFilter`, `rosic_OnePoleFilter`, `rosic_RealFunctions`,
  `GlobalFunctions`, `rosic_NumberManipulations`, `GlobalDefinitions`) till `Source/ThirdParty/open303/` med `License.txt`. Ändra inget i
  dem utom det som krävs för att kompilera utan varningar (läggs i en egen `#pragma warning`-ram om det behövs).
- Ny parameter `bassNFilterModel` (val: `Kickbase` = dagens SVF, standard; `TB-303`). Med TB-303 används TeeBeeFilter (läget `TB_303`)
  med cutoff, resonance, filter-envelope och keytrack från de befintliga rattarna; `filterType` ignoreras och visas nedtonad.
- Accent = tonens velocity (finns), slide = glide-toner i mönstret (finns). En accent-ratt (`bassNAccent`, 0 = av) kan läggas till om
  velocity inte räcker. Pröva först utan.
- Filtret körs per sample. **Mät** först hur TeeBeeFilter beter sig vid 44,1/48/96 kHz utan Open303:s översampling; behövs översampling,
  gör den i filtret (2x, med ett halvbandsfilter) och mät CPU.
- **Tester:** standard = identiskt ljud; `blocksizetest` med TB-303 påslaget; en referensrendering av TeeBeeFilter ensam (samma insignal och
  inställningar) mot vår väg, med tolerans; NaN-skydd (`audiotest`, `reset` i `resetDspState`); CPU med 10 instrument.

### Fas P: polyfoni (ny fas, krävs för piano, bells, music box, pads och plucks)

Instrumentet är i dag **monofont**: en röst per instrument ([InstrumentSynth.h:83](../Source/Core/InstrumentSynth.h#L83)); varje ny ton
klipper den förra. Det är rätt för basar och leads, men piano, klockor och plucks med lång utklingning låter fel.
- Ny parameter `bassNVoices` (1–8, **standard 1** = exakt dagens beteende, bit för bit). Med fler röster blir `InstrumentSynth` en röst
  av flera i ett instrument; röstval: ledig röst, annars den äldsta (stjäl med en kort release mot klick). Glide gäller bara med 1 röst.
- Pianorullens toner får överlappa (ackord) när röster > 1. Kontrollera hur `Pattern` och pianorullen hanterar överlappande toner i
  dag (fråga till Sonnet: du byggde pianorullen).
- Modulationskällorna ENV 2/3, VELOCITY och RANDOM följer den **senaste** tonen, som i dag ("senaste tonens"): inga nya källnummer.
- CPU: 3 oscillatorer × unison × röster. Visa en varning i gränssnittet när röster × unison blir stort, och mät `perftest` med 8 röster.
- **Tester:** `rendersig*` identiska med 1 röst; ackord i pianorullen ger överlappande toner med 4 röster; röststöld utan klick; `blocksizetest`.

### Fas B1: källtyp per plats

- Nya parametrar `bassNOscAType`, `bassNOscBType`, `bassNOscCType` (fälten `aType`, `bType`, `cType` sist i `InstrumentField`):
  `Wavetable` (standard) / `Sample` / `Multi` (fas C; dold tills den finns).
- **Sample-typen:** en `SampleSlot` per oscillatorplats (3 per instrument, laddas på bakgrundstråd, `DeferredRelease`), med samma kontroller
  som dagens SAMPLE-flik (KEY, LOOP, START, nivå), plus oscillatorns egna SEMI/FINE/PAN/LEVEL. Unison och warp gäller bara Wavetable
  (rattarna nedtonade för Sample).
- **Dagens SAMPLE-flik och dess parametrar finns kvar oförändrade** (gamla projekt), men nya ljud använder hellre en plats med typ Sample.
  Ska den gamla fliken döljas när den är tom? Fråga Tobias när B1 finns att prova.
- Lagring: platsens sampling sparas som dagens (base64) under ett nytt barn `OscSample` med lager och plats; rensas när ett tillstånd utan den
  laddas (`persisttest`, som för egna wavetables).
- **Tester:** standardtyp = identiskt; byte till Sample och tillbaka; spara/ladda; instrument-preset bär samplingen; 0 allokeringar.

### Fas C: multisampling (Multi-typen)

- **Format:** en mapp per instrument i `Library\Multi\<namn>\` med `instrument.json` (zoner: fil, rotton, lokey/hikey, lovel/hivel, slinga,
  release) och samplingarna som WAV (mono eller stereo, 44,1/48 kHz). Innehållshash (t.ex. SHA-256) över de konverterade samplingarna och
  zonerna, i manifestet.
- `tool importlibrary` läser en delmängd av SFZ (`sample`, `lokey`, `hikey`, `pitch_keycenter`, `lovel`, `hivel`, `loop_mode`,
  `loop_start`, `loop_end`, `ampeg_release`) och skriver formatet. Börja med VCSL (klockspel, vibrafon, kalimba) och ett piano från
  sfzinstruments.
- **Spelare:** zonval per ton (nyckel + velocity), interpolation (minst 4-punkts Lagrange/Hermite), slinga, release; använder instrumentets
  filter, envelopes, drive, FX och mönster. Kräver polyfoni (fas P) för att låta rätt.
- **Lagring:** projektet/presetet sparar biblioteks-id + hash. Saknas posten: tyst, en varning i OSC-fliken och i `repairpresets`, ingen
  krasch. "Embed in project" (per instrument) bäddar in zonerna i projektet. Laddning asynkront; `getStateInformation` väntar aldrig på disk.
- **Minne:** bara de zoner som används laddas; en gräns per instrument (t.ex. 200 MB) med varning.
- **Tester:** zonval, slinga, saknad post, hash som inte stämmer (varning), embed-rundtur, 0 allokeringar, `blocksizetest`, CPU med 8 röster.

### Fas F: samplerverktyg för loopar och långa ljud

Byggs ovanpå Sample-typen (B1):
- Vågformsvy (`VisualTimer`) med start, slut och slinga. Punkterna hamnar vid nollgenomgång, med korta crossfades mot klick.
- Slice: på transienter (känslighet), på rutnät (1/4, 1/8, 1/16, N delar) eller manuellt. Delarna läggs kromatiskt från en vald ton, så att
  mönstret och pianorullen spelar dem.
- Tempo: loopens BPM (angiven eller gissad ur längden). Uppspelning (1) i originaltempo eller (2) omsamplad till projektets tempo (tonhöjden
  följer). Tidsstretch utan tonhöjdsändring: inte nu (Rubber Band är GPL/kommersiell, SoundTouch LGPL; ta upp med Tobias senare).
- Exportera delarna till biblioteket (PERSONAL om källan inte är CC0).
- **Tester:** slice-positioner deterministiska; uppspelning identisk mellan körningar och blockstorlekar; ångra.

### Fas E: innehåll

- Egna presets med motorn: acid 303 (D1), trance-plucks, supersaw-leads (såg/fyrkant/sinus), rolling/offbeat-basar, piano/bells/music box
  (C), pads och originalmönster (FILL). Som **användarpresets** i en egen mapp (inte fabrikspresets) tills Tobias godkänt dem.
- En importör för Vitalium-presets (Vital-JSON, CC0) som utgångspunkt; resultatet trimmas för hand.
- Varje preset kontrolleras med `repairpresets` och lyssnas på i en analys (`analyse_mix`).

## 4. Hur Sonnet och Opus arbetar ihop

- **Före varje fas** skickar Sonnet till Opus: vilka parametrar (ID, typ, intervall, standard), vilka filer och klasser, och hur ljudet bevisas
  oförändrat. Opus svarar med OK eller ändringar innan koden skrivs.
- **Efter varje fas:** grenen pushas (aldrig `main`, aldrig sammanslagen utan Tobias OK), och Opus granskar diffen, kör sviten och
  `blocksizetest` och tittar på skärmbilderna.
- **Frågor som rör ljudet eller beteendet** går till Tobias i `docs/audit/questions-for-tobias.md` (nya frågor läggs sist, numrerade
  vidare). Inget som hörs görs utan hans svar.
- **När något i ENGINEERING.md visar sig fel eller saknas:** rätta filen i samma gren.

## 5. Öppna frågor till Tobias (i questions-for-tobias.md som 9–11)

Besvarade och inarbetade ovan: licens (AGPL-3.0), publicering (sist), Freesound (API-nyckel), ordning (fri).

9. **Personliga ljud:** är det ok att biblioteket har en PERSONAL-nivå för ljud som bara får användas i dina egna låtar (t.ex. Looperman),
   som aldrig följer med när Kickbase delas och aldrig hamnar i repot?
10. **Kicken:** SAMPLE-fliken får MODE CLICK / LAYER / REPLACE (lager ovanpå synten finns redan i motorn). Ok?
11. **Polyfoni:** instrumenten är i dag monofona (en ton i taget). För piano, klockor och pads får varje instrument en VOICES-ratt (1–8,
    standard 1 = som i dag). Ok?


## Bilaga: källor och licenser (Sonnet, 18 okt)

Sonnets genomgång, oförändrad (avsnittsnumren är Sonnets). Inte juridisk rådgivning; kontrollera licensen per fil när något läggs in.

### 3. Vad som finns fritt (verifierat av mig 18 okt; inte juridisk rådgivning)

| Källa | Vad | Licens | Användning |
|---|---|---|---|
| [AKWF](https://www.adventurekid.se/akrt/waveforms/adventure-kid-waveforms/) / [AKWF-FREE](https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE) | 4000+ enkelcykelvågor | CC0 (sidan: "waived all copyright") | redan installerat; finns kvar |
| [NeoSoundFonts/Single-Cycle-Waveforms](https://github.com/NeoSoundFonts/Single-Cycle-Waveforms), [sc-wavetable-converted](https://github.com/NeoSoundFonts/sc-wavetable-converted), [BMuXBeats/Vitalium](https://github.com/BMuXBeats/Vitalium) | fler enkelcykel/wavetables | listade som CC0-1.0 på GitHub, **innehållet ej granskat** (konverterade från andra bibliotek: kolla ursprung) | möjliga tillägg efter kontroll |
| [Open303](https://github.com/RobinSchmidt/Open303) (Robin Schmidt) | TB-303-emulering (C++, `rosic_*`) | **MIT** (License.txt: "Copyright (c) 2009 Robin Schmidt", notisen ska följa med) | motorn får användas i ett stängt plugin |
| [JC303](https://github.com/midilab/jc303) | JUCE-port av Open303 | GPLv3 (motorn även MIT) | **kopiera inte** koden; använd Open303 direkt |
| [VCSL](https://github.com/sgossner/VCSL) | inspelade instrument (klockspel, vibrafon, marimba, xylofon, rörklockor, kalimba/mbira, harpa m.fl.), WAV + SFZ | CC0 ("essentially Public Domain") | bells/music box/plucks |
| [sfzinstruments](https://github.com/sfzinstruments) | `Osiris_Piano`, `karoryfer.scarypiano` (piano), `meatbass`/`pastabass`/`ergo` (bas), `string-cyborgs` m.fl. | CC0-1.0 (per repo) | piano, stråk, ev. basar; **samplingarna kan ligga på annan plats** (repot har bara mappningar), kolla per instrument |
| [Freesound, CC0-filter "psytrance kick"](https://freesound.org/search/?q=psytrance+kick&f=license%3A%22Creative+Commons+0%22) | 187 träffar (kick, bas, "kick and bass in C", "DARK kick and bass 160BPM") | CC0 per ljud (verifierat på [ett exempel](https://freesound.org/people/dannyu81/sounds/505459/)) | kickbibliotek; **nedladdning kräver Freesound-konto/API-nyckel**: Tobias hämtar eller ger en nyckel; spara URL + upphovsperson per fil |
| Gratispaket (Gumroad m.fl.), Sampling Plus (G-Town, Sonatina) | | royalty-free / CC Sampling+ | **inte** för inbakning/delning; bara Tobias egna låtar |
| Surge/Odin 2 fabriksljud, WaveEdit-bankerna | | koden GPL, ljudens licens ej funnen | bara inspiration |
| MIDI-basgångar | | ingen CC0-samling funnen; gratis är "royalty-free" | skriv **egna** originalmönster (FILL har redan flera) |

#### 3b. Fler källor (andra omgången, 18 okt): synthar, presets och synth-multisamplingar

| Källa | Vad | Licens | Användning |
|---|---|---|---|
| [Analog Archive Sample Pack](https://booth.pm/en/items/8874394) (ijimi) | 392 multisamplade patches från 17 analoga synthar (bl.a. Juno-6, Jupiter-4, JX-3P, JX-8P, SH-2, Moog Minitaur, MicroBrute, Korg Mono/Poly, Oberheim Xpander, Tetra, Elektron Analog Four, CS-30L), SFZ + AIFF, **gratis**, 30 000+ samplingar, ~32 GB | CC0 (sidan: "Creative Commons CC0 (Public Domain)", fritt för kommersiellt bruk) | **fas C**: såg/fyrkant/sinus-synthar, plucks, leads, pads som multisamplingar. För stor för att ta allt: ta ett litet urval och konvertera. Ingen TB-303/SH-101 i paketet. Hämtning via Google Drive (manuellt), gå inte via tredjepartsspeglar. |
| [Vitalium](https://github.com/BMuXBeats/Vitalium) (BMuXBeats) | **32 Vital-presets**: music box, "Bells in Heaven", HarpBell, Stellar Space Piano, Rusty Rhodes, Electric Handpan, FM Bass1, Plucked Echoed Synth Bass, RavePad, Sine2Saw, Pluggy-Yggulp, m.fl.; + LFO:er och wavetables | CC0-1.0 (repo-etikett och LICENSE-fil 7 kB); författaren "B. MuXBeats" står i varje preset, ingen README-text om villkor | **fas E, som förlaga:** `.vital` är JSON (flat `settings` med hundratals parametrar: `osc_N_*`, `filter_N_*`, `env_N_*`, `lfo_N_*`, `modulations`; wavetables/`sample` inbäddade). En importör kan mappa en delmängd (oscillator-tabell/pos, unison, filter, envelopes, LFO, modulationer) till våra parametrar; resultatet blir aldrig identiskt med Vital. Bra underlag för music box/bells/pluck. |
| [NeoSoundFonts/escape-from-digisynth](https://github.com/NeoSoundFonts/escape-from-digisynth) | CC0-presets för Surge/Oatmeal ("toy keyboard"-ljud) | CC0-1.0 | liten nytta (fel stil); bara om vi vill ha retro-ljud |
| [open-synth (Blackshield-Company)](https://github.com/Blackshield-Company/open-synth) | "5 600 presets", sample-ROMpler | kod Apache-2.0, samplingar CC0 (freepats/sfzinstruments), **presetens licens ej angiven**, ursprunget oklart, README pekar på ett annat konto | **använd inte** (osäker proveniens) |
| [FigBug Wavetable / SocaLabs](https://github.com/FigBug/Wavetable) | BSD-kod, 100 presets | "wavetables har egen licens", presetlicens ej angiven | **använd inte** utan kontroll |
| Surge XT, Helm, Odin 2 | patchar: Surge/Helm/Odin 2 | koden GPLv3; ingen separat licens funnen för patchar/ljudbank | bara inspiration; kopiera/konvertera inte (GPL-risk i ett stängt plugin) |
| Vital (gratisversionen) | fabrikspresets | egen licens som enligt ett forumcitat **inte** tillåter vidaredistribution | använd inte; Vitalium ovan är något annat (CC0) |
| [sfzinstruments: synthesizers](https://sfzinstruments.github.io/synthesizers/) | Wavestate Pads, Minifreak Pads (CC0), Caveman Cosmonaut, Cowsynth (CC0), Tofurkey (MIT) | enligt listan | pads/oddities, inte såg/fyrkant-basar |
| [FreePats](https://freepats.zenvoid.org/) | GM-banker | **blandat** (Salamander CC-BY, andra CC BY-SA, GM-bank GPL) | kontrollera per instrument; nu hellre sfzinstruments/VCSL (CC0) |
| Gratis Vital-/synthpaket (Psy World m.fl.) | psytrance-presets | licens oklar eller royalty-free | bara Tobias egna låtar |

**Slutsats för användning/implementering:** andra synthars presets går inte att läsa in rakt av (andra parametrar, andra motorer). Det som går är (1) multisamplingar som ljudkälla (Analog Archive, VCSL) och (2) en **konverterare från Vital-JSON** för CC0-presets (Vitalium) till våra parametrar som utgångspunkt, som vi sedan trimmar för hand. Allt annat bygger vi själva i vår motor.

#### 3c. Kan vi bygga in en annan synths *motor* som instrumenttyp och därmed använda dess presets?
Tekniskt ja, men det avgörs av **två licenser**: motorns kod (måste passa hur Kickbase distribueras) och presetens (måste uttryckligen tillåta det). Kickbase har i dag **ingen licensrad i repot** och använder JUCE 8.0.15 (JUCE har egen dubbellicens: kommersiell nivå eller AGPLv3; Tobias måste kolla vilken han har). Det avgör vägen:

| Motor | Kodlicens | Presets | Läge |
|---|---|---|---|
| Open303 | MIT | inga stora banker | **ja** (fas D) |
| TinySoundFont (SF2-spelare, en header) | MIT | SF2-banker finns (licens per bank) | möjlig; litet |
| sfizz (SFZ-spelare som bibliotek) | BSD-2 (arkiverat repo; beroenden Abseil Apache-2, dr_libs MIT m.fl.; libsndfile är LGPL: välj dr_libs) | alla CC0-SFZ-bibliotek (VCSL, sfzinstruments, Analog Archive) | **möjlig**; stor beroendekedja, alternativ: egen liten SFZ-läsare (se fas C) |
| FigBug Wavetable | BSD-3 (kräver JUCE-licens) | presetlicens ej angiven | nej tills presetlicens finns |
| Surge XT, Vital (kod), Helm, Odin 2, Dexed, ZynAddSubFX | **GPLv3** | oklar licens (Surge/Helm/Odin), restriktiv (Vitals gratispresets), CC0 bara Vitalium (32 st) | bara om Kickbase själv släpps som GPLv3; och då ändå ingen färdig presetbank med klar licens, och det vore en enorm integration |

**Slutsats:** den väg som ger mest användbart innehåll är att göra Kickbase till en **sampler/SFZ-spelare** (instrumenttyp "SAMPLER" = fas C) och lägga CC0-SFZ-bibliotek där; det är "freeware-synthar med färdiga presets" i praktiken (tusentals CC0-instrument). 303 via Open303 (fas D), och en Vital-JSON-importör för Vitalium (fas E). Att bädda in hela andra synthar är dyrt och licensmässigt fastlåst.

#### 3d. Biblioteket: två nivåer
`Dokument\Kickbase\Library` med manifest per post (`source`, `url`, `author`, `license`, `redistributable`, `bpm`, `key`, `tags`):
- **BUNDLE-nivå (redistributable = true):** CC0 (eller MIT med notis). Får ingå i en installation som delas, och i repots dokumentation som lista.
- **PERSONAL-nivå (redistributable = false):** "royalty-free för egna låtar" (t.ex. Looperman, gratis Gumroad-paket), Tobias egna filer. Visas med märke, tas **aldrig** med i installerare eller repo.
- Kategorier: Kicks, Bass (enskilda toner), Synth (enskilda toner), Pads/långa ljud, Bass-loopar, Synth-loopar, Wavetables, Multisamplingar.
- CC0-träffar på Freesound (filter "Creative Commons 0"): ~187 psykickar, **145 "psytrance bass loop"** (t.ex. "Psytrance bass in C 140bpm Minibrute2S", "HiTech Bassline with Hats loop", "Psy Trance Kick Bass Drum Patterns Loop 138bpm"), **45 långa synth/pads** (t.ex. "Badass Minimoog bass C4/C7", pads och drones). Nedladdning kräver Freesound-konto/API-nyckel. Looperman-loopar är "royalty-free enligt upphovspersonen", **inte CC0** ⇒ PERSONAL-nivån.

Regler för allt som tas med: bara CC0 (eller en licens som uttryckligen tillåter inbakning, då med notis); `LICENSES.md`/manifest med källa, URL, upphovsperson, licens och datum för varje fil; allt annat får bara ligga hos Tobias, aldrig i repot eller i en utdelad installation. Open303:s `License.txt` måste följa med i `THIRD-PARTY` och i programmets "om"-text.

