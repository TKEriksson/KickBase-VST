# Fas D1: rapport (gren `content-d1`, från `content-a`)

Skriven av Sonnet 18 okt 2026 (granskad av Opus). Planen: [content-plan.md](content-plan.md), reglerna: [ENGINEERING.md](ENGINEERING.md).
Fas D1 ger instrumentet en andra filtermodell, **TB-303** (Open303:s `TeeBeeFilter`), vald per instrument med en ny parameter. Standard är dagens filter.
**Med standardvärden är ljudet bit för bit som förut:** `rendersig`, `rendersigreal` och `rendersigwide` är identiska med baslinjerna.

## 1. Vad som byggts

| Commit | Innehåll |
|---|---|
| 6b68806 | Filtermodellen: Open303-filen, `Tb303Filter`, parametern `bassNFilterModel`, MODEL i FILTER-gruppen, tester, hjälptext |
| 2e8f29d | `tool includecheck`: Open303:s headers får bara inkluderas av `Source/Core/Tb303Filter.cpp`, `Source/ThirdParty/open303/*` och `Tools/Tb303Tests.cpp` |
| 6f9a1fd | `open303/README.txt` listar exakt de fyra ändringarna mot uppströms 313bf0d (kontrollerat med diff av varje fil) |
| 0ddc04f, e955126, 5b1a62c | **Separat felrättning, inte en del av D1** (se punkt 5) |

- **Open303:** 11 filer ur commit `313bf0d` + `License.txt` + `README.txt` i `Source/ThirdParty/open303/`. Fyra ändringar mot uppströms (`rosic_NumberManipulations.h` rad 22 och 58, `rosic_RealFunctions.h` rad 124,
  `GlobalFunctions.h` rad 303 och 308), alla för att kompilera i x64 utan varningar och märkta "Kickbase:". MIT-notisen står ordagrant i [THIRD-PARTY.md](../THIRD-PARTY.md).
- **Parametern** `bassNFilterModel` (`InstrumentField::filterModel`, sist i fältlistan; val `Kickbase` = standard, `TB-303`). Tio nya rader i `docs/audit/params-instrument.txt`. Befintliga parametrars ordning och ID är oförändrade.
- **Filtret:** `Source/Core/Tb303Filter.{h,cpp}` (pimpl). `TeeBeeFilter` i läget `TB_303` körs på **2x** med JUCE:s halvbands-polyfas-IIR (`designIIRLowpassHalfBandPolyphaseAllpassMethod(0.1, -70)`),
  per sampling, så att cutoff och resonans kan moduleras per sampling som SVF-filtret. Cutoff, resonans, filter-envelope och keytrack kommer från de befintliga rattarna. `filterType` ignoreras och visas nedtonad. Accent = velocity, slide = glide (som förut).
  Modellbyte nollställer filtret (`reset`).
- **Gränssnittet:** MODEL-valet i FILTER-gruppen (rutan gjordes bredare efter att "Kickbase" klipptes: `ChoiceComponent::weight`), TYPE nedtonas via en `ParameterAttachment`, hjälptext i `Help.cpp`, [AGENT_GUIDE.md](AGENT_GUIDE.md).

## 2. Lärdom: tredjeparts-headers med globala makron

Första versionen ändrade `rendersigwide` (instrument 8). Orsaken var inte filtret utan Open303:s `GlobalDefinitions.h` och `GlobalFunctions.h`: de definierar makron (`PI`, `EPS`, `INF`, ...) och **globala** funktioner (`exp2`, `round`, `sign`, `log2`, ...).
Via `InstrumentSynth.h` läckte de in i alla filer som inkluderar den och bytte ut anrop där. Hittat genom att jämföra rå ljuddump per instrument mot ett `content-a`-bygge i en worktree.
Rättning: pimpl, Open303:s headers finns bara i `Tb303Filter.cpp` (JUCE-headers först). `tool includecheck` vaktar det, och den enda globala symbolen som Open303:s objektfiler definierar är den inline `double dB2amp(double)`. Regeln står i [ENGINEERING.md](ENGINEERING.md).

## 3. Mätningar och bevis

- **Ljudet med standard:** `rendersig`, `rendersigreal`, `rendersigwide` identiska med `docs/audit/baseline-kod|init|wide-kontrolltakt.txt`, `rendersigtb303` identisk med den nya `docs/audit/baseline-tb303.txt` (fyra rader: instrument 10 @48k/512, 3 @44,1k/128, 1 @96k/1024, 4 @48k/37).
  Kontrollerade på nytt efter varje commit, senast efter felrättningen i punkt 5.
- **Filtret ensamt** (`tool tb303measure`, `tool tb303unit`): passband 0,000 dB, alias −170 dB, ungefär 2,3 samplingars fördröjning vid 1 kHz (halvbandsfiltret). Jämförelse med en 4x-referens görs på **övertonernas storlek**, inte i tidsdomänen
  (en tidsjämförelse vilseleder: Open303 använder explicit Euler med en samplings fördröjning i återkopplingen).
- **Referenstest** (`tool tb303test`, fyra delar): vår väg mot `TeeBeeFilter` ensam med samma insignal och inställningar, modellbyte (NaN och topp vid resonans 100 %), denormaltest (tystnad i 10 s, sista sekunden inte dyrare), samt `reset`.
- **Blockstorlek:** `blocksizetest` läge 3 (tungt läge med TB-303): block 100, 256, 512, 1024 och 2048 mot 32 är bit för bit lika. Gröna också efter felrättningen i punkt 5.
- **Ljudtråden:** `audiotest` med TB-fall: 0 allokeringar och 0 frigöranden, inga icke-ändliga värden, återhämtning efter NaN. Gröna också efter punkt 5. Obs: `audiotest` går inte genom de nya `cancelCurrent` och `fadeOutQuickly`
  (de körs bara vid ett "ton av" strax efter ett steg). De innehåller bara aritmetik på redan allokerade fält, och `boundarytest` kör dem, men utan allokeringsmätning.
- **CPU** (`tool perftest`): TB-303-filtret kostar cirka 3x SVF-filtret per instrument. Hela processorn med 10 instrument ligger på cirka 1,1–1,25x av standardläget.
- **Sparat tillstånd** (`KickbaseHostTest`): ett tillstånd sparat med `content-a`-bygget laddas av det nya pluginet utan skillnad i någon parameter, och **Filter Model = Kickbase för alla 10 instrument** (kontrollerat explicit).
  Det nya pluginets eget tillstånd sparas och laddas tillbaka lika. Jämförelsen görs mot värdena från en ny instans som laddar tillståndet (MIDI-CC-parametrar och växlar snäpper, så en rå jämförelse ger falska skillnader).
- **Gränssnitt:** 42 skärmbilder jämförda med `content-a` (samma uiScale, `Tools/pngdiff.py`): 15 identiska, 27 INSTRUMENT-vyer skiljer bara i området x 194–885, y 271–344 (FILTER-raden, där MODEL tillkommit). Inget klipps.
- **Build:** `Release` med varningsnivå /W4 utan varningar. Se punkt 6 för Release-bygget.

## 4. Avvikelser från planen

1. **Pimpl i stället för en vanlig header** (punkt 2). Planen sa "kopiera filerna och ändra inget"; det gjordes, men inkluderingen måste isoleras.
2. **Fyra ändringar i Open303** (i stället för "inget utom det som krävs"): alla krävs för x64-kompilering utan varningar och finns förtecknade i `README.txt`.
3. **Ingen `bassNAccent`-ratt:** velocity räckte inte att pröva som sämre, så den lades inte till (planen: "pröva först utan").
4. **Halvbandsfiltret är ett IIR** (fasdistorsion nära Nyquist, ingen linjärfas): valt av Opus för CPU och minsta fördröjning.

## 5. Felrättning i samma gren: dubbel kick vid mönsterbyte (cherry-pick-bar till `dev`)

Tobias hittade i FL att två Kickbase-instanser efter varandra (Pattern 1 → Pattern 2, olika presets) ger en dubbel kick på första slaget. Det har **inget att göra med D1** och finns i `dev` och `content-a`.
Commits `0ddc04f`, `e955126` och `5b1a62c` kan plockas till `dev` var för sig utan resten av D1 (de rör `InstrumentProcessor`, `KickEngine`, `InstrumentSynth` och `Tools/PatternTests.cpp`).

- **Orsak:** FL skickar "ton av" när ett mönster tar slut, och med avrundning till hela samplingar kommer den några samplingar efter taktens första steg. Den gamla instansen hann spela steget (en kick) innan den tystnade, samtidigt som nästa instans började.
- **Rättning, del 1** (0ddc04f): slutar tonen mönstret (NOTE END = Cut, sista hållna tonen, eller All Notes Off) startas inga steg från 3 ms före "ton av" i samma bit (`renderSegment(..., stepLimit)`).
- **Del 2** (e955126): ett steg som redan startat i en tidigare bit eller ett tidigare block, inom 3 ms före "ton av", tonas ut på 3 ms (`KickEngine::cancelCurrent`, `InstrumentSynth::fadeOutQuickly`). Starttiden spåras i ppq över block (ingen allokering), och förra kickens svans får tillbaka sin nivå.
  Följer ett "ton på" direkt efter (nästa mönster i samma instans) avbryts ingenting. Först med 1,5 ms blev resten −35,4 dBFS vid vissa lägen mot kontrollrutnätet; med 3 ms klarar den −40 dBFS.
- **Del 3** (5b1a62c): starttiderna glöms vid `startPattern` och `stopPattern` så att ett gammalt värde aldrig matchar efter ett hopp; testet för "ton av" på ett blocks sista sampling och "ton på" på nästa blocks första.
- **`tool boundarytest`** mäter ljudet: bara kicken, alla 32 lägen mot kontrollrutnätet × "ton av" 0–12 samplingar efter takten, block 32/256/512, resten från 8 ms efter takten måste vara under −40 dBFS (uppmätt −40,5). Restart-fallen
  ("ton av" och "ton på" på samma sample i båda ordningarna, och över en blockgräns) ligger högst 0,01 dB från en ton som aldrig släpptes (gräns 1 dB). `rendersig*` är oförändrade.

## 6. Kända saker och kvar

- Marginalen i `boundarytest` är liten (−40,5 mot gränsen −40 dBFS); resten är en låg svans från den avbrutna kickens första millisekunder och hörs inte mot den föregående kickens svans.
- Tobias har provat bara RelWithDebInfo-bygget i FL. Release-bygget (`build.bat Release all`) kördes efter den här rapporten; resultatet står i commit-meddelandet för rapporten eller i PROGRESS-anteckningen.
- Fortfarande öppet från tidigare: Tobias måste rotera `ARTCRAFT_API` (ligger i gammal historia, commit cb45841) innan repot blir publikt.
