# Plan v2 (20 okt 2026): Tobias test 19 okt + resten av content-plan

Skriven av Opus (projektledare, session `kickbase-vst-af` 20 okt) efter genomläsning av docs/, `research/TEST026-10-19/` (chatthistory,
specarna, skisserna). Sonnet bygger, Opus godkänner. Ersätter ordningen i [content-plan.md](content-plan.md) avsnitt 1; reglerna där
(avsnitt 2) och i [ENGINEERING.md](ENGINEERING.md) gäller fortfarande. Löpande läge: [work/PROGRESS.md](work/PROGRESS.md).

## 0. Arbetssätt (spara tokens, missa inga fel)

**Roller.** Sonnet: läser kod, skriver kod, testar, skriver rapport. Opus: spec-OK, designbeslut, granskar diffen (bara kärnfilerna).
Sessionernas namn byts per chatt: kolla `ListAgents`. Tobias kollar då och då; frågor till honom samlas i avsnitt 4 och i PROGRESS.md.

**När Sonnet väntar på Opus:** (1) före varje fas: kort spec (parametrar ID/typ/standard, filer, hur ljudet bevisas oförändrat),
(2) designval som syns eller hörs, (3) när fasen är klar. **Inte** för rena buggfixar inom en godkänd fas: bygg, testa, commit, fortsätt,
och rapportera i klump (commit-hashar + 5–15 rader).

**Bygg och testa så lite som möjligt (Tobias 20 okt: "bara när det behövs och så lite som möjligt", minimalt med skärmbilder):**
- Samla flera rättningar innan du kompilerar; kompilera inte efter varje liten ändring. Ingen kompilering för docs-ändringar.
- Under arbetet: bara målet `KickbaseMcp` (verktyget + snapshot). VST3-målen och `KickbaseHostTest` bara vid fasens slut.
- rendersig-trion bara när ljudtrådskod ändrats, en gång per klump, inte per ändring.
- Skärmbilder bara när en layout ändras och någon måste se den: en bild per ändrad vy (1 instrument om layouten inte beror på antalet).
  Ingen bild för en ren logikrättning (scroll, hotkeys, tooltips).
- 42-bildssviten bara vid fasens slut och bara om fasen ändrat layout. Release-bygget bara när Tobias ska installera.
- Hellre ett riktat enhetstest/verktygstest än hela sviten. Hela sviten en gång per fas.

**Standardvärden och gamla presets (Tobias 20 okt: gamla presets är oviktiga, han gör nya i fas E):**
- Nya parametrar behöver **inte** ge dagens ljud: välj det standardvärde som låter och fungerar bäst, och skriv det i specen.
- rendersig är kvar som larm för **oavsiktliga** ljudändringar (refaktorering, buggar). Avsiktlig ändring: uppdatera baslinjen i samma
  commit och skriv varför i commit-texten. Ändrar en commit ljudet utan att säga det är det ett fel.
- Parameterordningen (lägg till sist, utöka inte valparametrar) gäller fortfarande tills Tobias säger annat (avsnitt 4 fråga 4).
- **Presetlistan:** varje fas lägger till en rad per nytt tillstånd i [work/preset-checklist.md](work/preset-checklist.md): vad det är,
  var det sparas (preset / projekt / global inställning / egen presettyp), och om InitPreset och MCP-verktygen behöver det. Används i fas E.

**Tvärgående regler (gäller varje fas som rör området; Tobias specar skrevs i olika omgångar, så de här binder ihop dem):**
- **Varje presetmeny** (OSC, effekt, effektkedja, mönster, huvudpreset) får RESET med bekräftelse. Huvudpresetlistan: Init + NEW EMPTY
  (ingen kick, ingen synth, allt tomt). Bygg RESET i samma fas som menyn (layout_optimization §RESET).
- **Tre olika av/på för filter, blanda aldrig ihop dem:** FILTER-flikens av/på (hela instrumentets Filter 1+2), FILTER per källa
  (OSC 1–3, SAMPLE, NOISE) och FILTER ENVELOPE av/på (bara envelopen). Samma namn i UI, kod och MCP-beskrivningar.
- **Mönsterlagringen** (R9) är grunden för ackord (P), underrader SAMPLE/SLICER (F2), kopiera/duplicera/mönsterpreset/MIDI-export (L4)
  och arrangemang (IO). Ändringar där ska passa alla fem.
- **AI och MCP:** nya parametrar syns i MCP automatiskt; nya specialverktyg i samma fas. Rapportskrivaren, AGENT_GUIDE, MCP-beskrivningarna
  och skillen gås igenom samlat i E0 (BEZIER.md sista stycket: "allt nytt, och fortfarande ALL info").
- **API-nycklar** (AI-fasen): krypterat lokalt, aldrig i repo, logg, rapport eller preset.

**Testpolicy (regel 11 gäller vid fasens slut, inte efter varje delsteg):**
- Delsteg: bygg bara berörda mål (`build-mcp.bat <mål>`, nedan). Kör bara testerna som rör ändringen.
- Rör ändringen ljudtråden: `rendersig`, `rendersigreal`, `rendersigwide` (+ `rendersigtb303` om filter) mot baslinjerna, och
  `blocksizetest`/`audiotest` om ny DSP med tillstånd. Rör den bara UI: en skärmbild av den ändrade vyn, titta själv. Ingen 42-bildssvit per delsteg.
- Fasens slut: hela sviten en gång, `KickbaseHostTest` state save (förra fasens bygge) / load (nya), 42 bilder + `pngdiff` om UI ändrats,
  Release-bygge **bara** när Tobias ska installera, kort rapport `docs/content-<fas>-report.md`.
- Ny DSP: testa vid flera inställningar (impuls/kurva), inte bara standard (Ladder-felet i D2).

**Kända dyra misstag (gör inte om):**
- Heredoc med backslash förstör C++/Python: skriv skript med Write, ändra med Edit; filer kan vara CRLF (`file`).
- Tredjepartshuvuden med globala makron bara i en `.cpp` (pimpl), aldrig i en header (Open303 ändrade ett instruments ljud).
- JUCE 8.0.15: läs källan i `build/RelWithDebInfo/_deps/juce-src/modules` innan du bygger runt ett beteende. `Font::getStringWidthFloat`
  finns inte: `juce::GlyphArrangement::getStringWidthInt`.
- Bygg inte allt i onödan; Release (LTO) är långsam: bara vid fasens slut. Inga upprepade skärmbilder.
- Utöka aldrig en befintlig valparameter (DAW-automation flyttas). Undantag beslutas av Opus och står i rapporten.
- Installation: `installAll.bat RelWithDebInfo RelWithDebInfo` (FL Studio och Claude Desktop stängda). Tobias kör den själv.

**Byggskript** (lägg i din scratchpad, anropa `cmd //c "<fullsökväg>\build-mcp.bat KickbaseMcp Kickbase_VST3"`):
```
@echo off
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSPATH=%%i
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cmake --build C:\Users\hunga\Documents\GitHub\KickBase-VST\build\RelWithDebInfo --target %* || exit /b 1
echo TARGET-BUILD-DONE
```
Verktyget: `build/RelWithDebInfo/KickbaseMcp_artefacts/RelWithDebInfo/KickbaseMcp.exe tool <kommando>`. Jämför rendersig* efter `tr -d '\r'`.

**Grenar:** kedjade som förut. `content-r` från `content-d2`, sedan `content-p` från `content-r` osv. Aldrig merge till dev/main utan Tobias.
**Kontext:** efter varje fas: uppdatera PROGRESS.md, föreslå ny chatt/compact.

## 1. Ordningen och varför

| # | Fas | Gren | Innehåll (kort) | Varför här |
|---|---|---|---|---|
| 1 | **R** buggar + småfix | `content-r` | pianorulle, ENV-flik, provlyssning, analyzer-text, export per instrument, LFO-fas | Tobias hittade dem i riktig användning; billiga; inget ljud ändras med standard |
| 1b | **M** 10 LFO | `content-m` | upp till 10 LFO (3 som standard), lägg till/ta bort, fällbara kort med scroll | Tobias 20 okt; liten, Modulation färsk efter SNAP; S och LC bygger på det |
| 2 | **P** polyfoni + ackord | `content-p` | godkänd spec (avsnitt 3) | redan godkänd; krävs för ackord, piano, sampler-lager |
| 3 | **L** layout | `content-l` | INSTR-underflik, ENV-headers, mönsterflikens globala rad, AI-knappar, instrumentmeny | ny yta behövs för allt som följer (S, F2, K) |
| 4 | **S** källor | `content-s` | FILTER-av/på per källa, brustyper, WARP ENV-källa, OSC-display, OSC-presets | små, fristående, Tobias vill ha dem |
| 5 | **KB** kunskapsbas | (docs i `content-s`) | KickAndBaseSchool + transkripten → AI-guide och skill | billig för Sonnet; behövs innan Analyzer och fas-verktyg designas |
| 6 | **K** kickverktyg | `content-k` | 16-delsmarkörer, markörens tid/frekvens/ton, zoom (LISTEN gjord i R) | psytrance-kärnan (lektionen) |
| 7 | **AN** Analyzer-sida | `content-an` | egen toppflik: PsyScope-lik kick+bas-vy, faskorrelation, kollisionsgradient, rapportskrivaren | bygger på K och KB |
| 8 | **PH** fasverktyg | `content-ph` | fasrotation av grundtonen per instrument (som PHA-979), övertons-/fasredigerare för wavetables | använder AN för att se resultatet |
| 9 | **F2** sampler | `content-f` | SAMPLE-lager BLEND/INDEPENDENT/OFF, SPLICER med egen rad, slices, tempo | ersätter gamla B1 och F; kräver P och L |
| 10 | **FX** effekter | `content-fx` | Echo som Fruity Delay 2, Flanger, riktig Sidechain/Maximizer i platser, Master FX, effekt-presets, RESET/NEW EMPTY | fristående; efter de stora |
| 11 | **AI** chatt | `content-ai` | flera leverantörer (Anthropic, Gemini, OpenAI, Ollama/LM Studio), modellista | fristående |
| 12 | **C** multisampling | `content-c` | som content-plan, men som läge i SAMPLE-lagret | efter F2 |
| 13 | **BZ** Bézier-kick | `content-bz` | eget kurvläge för kickens pitch/amp med kubiska handtag | stort; efter K |
| 14 | **E** innehåll | `content-e` | **först** genomgång av presetbygget (E0), sedan presets som använder allt nytt | efter alla funktioner (Tobias) |
| 15 | **LC** Live Code | `content-lc` | kodflik + kodblock i chatten | vision; bygger på MCP-verktygslagret |
| 16 | **IO** import/export/projekt | `content-io` | IMPORT/EXPORT-center, ZIP, projektfil, autospara, arrangemang | Tobias: sist, efter presets |
| 17 | **VS** separata VST:er | — | Kick-VST, Instrument-VST, effekter som egna VST3 | Tobias: sist |

Gamla **B1** (typ per oscillatorplats) utgår: SAMPLE-lagret med INDEPENDENT/SPLICER (F2) täcker behovet. Tas upp igen om något saknas.

## 2. Faserna i detalj

### R: buggar och småfix (`content-r` från `content-d2`)

Ljudet med standardvärden ska vara bit-identiskt (rendersig*). Ordning:
1. **Pianorullen** (pattern_and_ai_ui_improvements_spec.md §4, §12):
   - Krysset träffar en rad för högt (y-offset): hitta orsaken (hotspot eller koordinatberäkning), skriv den i commit-meddelandet.
   - **Scrollbaren** överst: klick i den får aldrig sätta steg i raden under (kickens). Marginal under den.
   - Vänsterkanten på det valda instrumentets kort klipps.
   - `EDIT IN LANE` döljs (inte bara nedtonas) när raden har pianorulle. Hjälprutan "This layer is edited in the piano roll…" döljs när
     raden har pianorulle; raderna packas tätare.
   - Alt = sudd, Ctrl+A och en SELECT ALL-knapp; tooltips listar tangenterna.
   - Förläng raden: klick i takten efter slutet förlänger (pianorulle och steg), plus en "+"-knapp. Krymp bara takter som lagts till
     automatiskt och fortfarande är tomma. (Längdgränsen och 2-potenserna ersätts i steg 9.)
2. **ENV-fliken:** SYNC krockar med DRAG TO A KNOB: en header-rad ovanför grafen. Knappen heter **DRAG**, pekaren blir en hand.
   (ON/OFF och SYNC för alla fyra envelopes kommer i fas L.)
3. **Provlyssning:** klick-och-håll på grafer (wavetable, SAMPLE-vågformen) spelar så länge knappen hålls, loopar om ljudet tar slut.
   Högtalarpekaren bara när man inte är nära ett handtag/linje. Kicken som den är, men LISTEN flyttas till PITCH/AMP-knapparna med
   högtalarikon; samma knapp på synthens envelope-graf. Kontrollera också att en lång sampling spelar hela tonens längd i mönstret (och
   loopar med LOOP); står "spelas bara ~1 s" även där är det ett fel.
4. **ANALYZER/HARMONICS:** texterna (instrument, Hz, ton) i en fast lista ovanför spektrumet; i spektrumet bara färgade streck för
   grundton och övertoner. Inga hoppande etiketter.
5. **EFFECTS CLEAR:** bekräftelsedialog.
6. **Export:** välj Kick / Alla instrument / Instrument N / Allt, och en ruta "Without effects". (Preset-export, ZIP och projekt: fas IO.)
7. **LFO (bugg):** ett byte av rate (Hz eller Sync, mus eller automation) nollställer fasen. Fasen ska fortsätta där den är.
   Bit-identiskt när raten inte ändras. Test: automation av rate mitt i en cykel ger ingen diskontinuitet i LFO-värdet; `blocksizetest`.
   - **SNAP i Hz-läget** (Opus beslut på Tobias fråga): Sync-läget **behålls** (gamla projekt och exakta värden för AI/automation), och
     Hz-läget får `SNAP` (av/på, standard av) + `SNAP RANGE` (±%, standard 5 %). Inom zonen låses raten mot närmaste notvärde i takt med
     tempot; displayen visar närmaste notvärde ("≈ 1/8 T") och om den är låst. Nya parametrar sist, globala LFO-parametrar (inte `bassN`):
     läs dem i LFO-modulen varje bit (ENGINEERING 3). Att lämna zonen ger ingen fasnollställning.
   - Nollställning av fasen sker bara genom LFO:ns befintliga retrigger-läge (per ton/takt), aldrig genom rate-byte. Finns inget
     retrigger-läge: säg till, ingen ny parameter utan OK.
8. **Undersök och rapportera (bygg inget än):**
   - **WARP ENV:** Tobias hör ingen skillnad. Är den låst till filter-envelopen? Vad gör den exakt (kod, fil:rad)? Mät med `rendersig`-lik
     rendering med och utan. Förslaget (källväljare Direct/Filter Env/ENV 2/ENV 3 + egen decay 2–150 ms) byggs i fas S.
   - ~~**SAMPLE "mullrig/dålig kvalitet":**~~ Struken (Tobias 20 okt: troligen filtret som var på). Kolla bara att filtret är av som standard.
   - **TOTAL LENGTH** (mönsterfliken): vad gör den i dag? Om den är verkningslös med radlängder: förslag att dölja den (parametern blir kvar).

9. **Radlängd automatisk och utan gräns, TOTAL LENGTH bort (Tobias 20 okt, pattern_and_ai-specen rad 284–295).** Spec till Opus först.
   - Radens längd **följer innehållet**: sista takten med en not/ett steg, i hela takter. Ingen gräns i UI; intern spärr (t.ex. 1024
     takter) bara mot trasiga filer/AI. Man kan alltid scrolla minst 1 takt efter slutet.
   - Förläng: klick i takten efter slutet (steg och pianorulle) eller "+"-knappen lägger till **en** tom takt.
   - Krymp: när man tar bort noter krymps varje rad (steg och pianorulle) automatiskt till sista takten med innehåll.
   - **LENGTH per rad: AUTO (standard) eller ett valt antal takter 1–1024** (Tobias 20 okt). AUTO = följer innehållet. Valt antal = låst
     längd, t.ex. 3 takter kick + 1 takt paus. Högerklick "Lock length" = byt till valt antal med nuvarande längd. Kortare än innehållet:
     bekräftelse, noterna efter slutet tas bort (minnet frigörs). TOTAL LENGTH och radvalet "Total" tas bort. Mönstret = längsta raden,
     kortare rader loopar.
   - Längden flyttas från valparametrar till mönsterdata i state (inte automatiserbar). De gamla längdparametrarna ligger kvar oanvända
     så att parameterordningen inte ändras. Gamla presets behöver inte läsas rätt (avsnitt 0).
   - **Minne och CPU:** inget förallokerat för max. Bufferten växer i bitar (t.ex. 4 takter) i meddelandetråden och byts in; när raden
     krymper frigörs överskottet (DeferredRelease). Ljudtråden allokerar aldrig (`audiotest`) och läser bara rader som finns. Rapportera
     minnet före/efter (dagens förallokering för 64 takter × alla instrument mot ett typiskt mönster).
   - MCP: mönsterverktygen och deras beskrivningar följer med; rad i preset-checklist.md.
   - **Framtida krav på lagringen (specen ska visa att de ryms utan ny omskrivning):** ackord per not (P: `c1`–`c3`), underrader per
     instrument med egen längd och M/S (F2: `INSTR X - SAMPLE`, `INSTR X - SLICER`, allokeras bara när de används), kopiera/klistra/
     duplicera spår och mönsterpreset (L4), MIDI-export, flera arrangemang (IO).

Slut: rendersig* identiska, `blocksizetest`, `audiotest`, skärmbilder av mönsterfliken, ENV-fliken, analyzern; rapport.

### M: 10 LFO (`content-m` från `content-r`), spec till Opus först
- `Modulation::numLfos` 3 → 10. LFO 4–10 får samma parametrar som 1–3 (inkl. Snap/SnapRange); ID:n hashas (VST3), ordningen påverkar inte
  automation. Modulationskällorna LFO 4–10 läggs **sist** i källistan; ModRoutes sparas via ID.
- **Lägg till / ta bort:** aktiva LFO sparas i state (inte automatiserbart). Standard: LFO 1–3 aktiva (= i dag, rendersig identiskt).
  "+ ADD LFO" (upp till 10); ta bort med × eller högerklick, bekräftelse om LFO:n har kopplingar; kopplingarna tas bort, värdena återställs.
  En inaktiv LFO kostar 0 CPU (hoppas över i `Modulator::update`) och syns inte i MATRIX, DRAG-listor eller MCP-listor.
- **LFO-fliken:** lodrät scroll med ett kort per LFO; varje kort fälls ihop till en rad (namn, minikurva, rate, DRAG, pil). Fällt läge
  sparas i UI-tillståndet. Snabbval "visa bara den valda".
- MCP: `set_lfo`/kurvverktygen tar 1–10; att sätta en inaktiv LFO aktiverar den. AGENT_GUIDE och preset-checklist.md uppdateras.
- Kolla Kickbase FX: har den egna LFO:er? Ska vara opåverkad (params-fx.txt och FX-rendering identiska) om inget annat bestäms.
- Test: rendersig* identiska, `blocksizetest`, `audiotest`, perftest med 10 aktiva LFO, `persisttest` (lägg till/ta bort/spara/ladda).

### P: polyfoni och ackord (`content-p` från `content-r`)

Opus godkände Sonnets spec 18 okt med svaren nedan (Sonnets egen spec gick förlorad med sessionen; content-plan.md "Fas P" + detta gäller):
- Parameter `bassNVoices` 1–8, **standard 1 = bit-identiskt**. Ackord: varje `Note` får upp till 3 extra intervall (atomiska, 0 = inget),
  samma tick/längd/velocity; lagras i R9:s nya mönsterlagring (inte längre fast [rad][tick]); XML-attribut `c1`–`c3` bara om ≠ 0.
- (a) VOICES = 1: bara grundtonen spelas; ackordmarkören nedtonad med tooltip "chord needs VOICES 2 or more". Ingen CHORD-meny på kicken.
- (b) Röstval: 1) ledig röst, 2) äldsta i release, 3) äldsta som hålls. Steal med 3 ms-ramp. Mät klicket (största sampelhopp och energi
  över 5 kHz runt steal) mot en mjuk referens.
- (c) Allt förberett i prepareToPlay. Mät och skriv i rapporten: minne per instans (private bytes före/efter, 48 och 192 kHz) och
  prepareToPlay-tid för 80 röster; Tobias kör två instanser (×2). Över ~100 MB vid 192 kHz: ta upp det (billig sänkning: lägsta
  kamfrekvens 40 Hz halverar Comb-minnet).
- (d) VOICES där GLIDE sitter; annars LAYER OUT. (Fas L flyttar om grupperna, placera så att flytten blir enkel.)
- Krav: (1) en ledig röst som aktiveras mitt i en 32-bit får fönstrets aktuella parametrar, tempo och modellval; utjämnade värden sätts
  direkt till målet. Test: samma ton efter att rösten varit ledig med andra inställningar låter som en ny röst. (2) VOICES sänks medan toner
  klingar: rösterna över gränsen tonas ut (3 ms), klipps inte. (3) Röstvalet deterministiskt i sampelordning (`StepEvent::sequence`);
  `blocksizetest`-läge med stealing och ackord. (4) Alla som läser/skriver `Pattern::Note` hanterar ackord: grepa och lista i rapporten
  (XML, copy/paste/undo, transponering, FILL/slump, AI-verktygen läser och skriver ackord, MIDI-export som riktiga noter, analysexporten).
  (5) Högerklick i pianorullen: kontrollera att det inte redan gör något; krock: CHORD i befintlig meny eller Alt-klick (Alt är nu sudd
  från fas R: använd menyn). (6) Kickbase FX opåverkat (params-fx.txt och FX-rendering identiska). (7) Seeds per röst deterministiska
  (seed + 1000·k); swing/humanize lika för hela ackordet. (8) perftest: standard 1 röst (= före P, lediga röster kostar 0), typiskt
  (4 röster, ackord på 3 instrument), värsta fall; varning i UI när röster × unison blir stort.
- Glide bara för senast spelade rösten; ackordets extra toner glider inte. ENV 2/3, VELOCITY, RANDOM följer senaste tonen.
- Ordning med baslinjekontroll efter varje steg: 1) InstrumentVoices med VOICES = 1, bit-identiskt; 2) röster, stealing, parametern;
  3) ackord i Pattern och XML; 4) pianorull-UI; 5) tester och rapport. **Skicka diffstat + testutdata till Opus efter steg 2 och 4.**
- Release-bygge i slutet av P (Tobias installerar).

### L: layout (`content-l`)

Underlag: `FÖRSLAG_FLIKAR_OCH_INFÖRA_OSC_PRESETS.jpg`, `PATTERN-FLIK FÖRBÄTTRINGAR SAMT ÄNDRAD AI KNAPP.png`,
`layout_optimization_osc_presets_spec.md`, `pattern_and_ai_ui_improvements_spec.md` §2, §3, §9, §12. **Skicka en skärmbild (1 instrument)
per vy till Opus innan finputs**; Tobias justerar efteråt.
1. INSTRUMENT-flikar: `[INSTR] [OSC 1] [OSC 2] [OSC 3] [SAMPLE] [FILTER]`. INSTR = envelope-grafen + PITCH, SUB, NOISE, FILTER ENVELOPE,
   VOLUME ENVELOPE, WARP ENV, DRIVE, LAYER OUT (+ VOICES/GLIDE). OSC-flikarna visar bara oscillatorn (+ plats för OSC-preset, fas S).
   Modulationsraden (ENV 2, ENV 3, VELOCITY, RANDOM) tas bort ur instrumentvyn; VELOCITY/RANDOM-DRAG flyttas till ENV-fliken.
2. ENV-fliken: alla fyra envelopes får samma header: ON/OFF, SYNC (ms/notvärde), DURATION (visningsomfång, bara vy, sparas i UI-tillståndet,
   inte som parameter). Nya parametrar sist, standard = i dag: `bassNEnv1On`/`bassNFEnvOn`/`bassNEnv2On`/`bassNEnv3On` (på) och Sync för
   volym- och filter-envelopen (som fas E2). FILTER ENVELOPE ON/OFF rör bara envelopen, inte filtret (filtrets av/på = FILTER-fliken).
   DRAG i headern på **alla fyra** (layout_optimization "Rensning"): ENV 1 VOLUME och FILTER ENVELOPE blir nya modulationskällor
   (läggs sist i källistan; spec till Opus). VELOCITY och RANDOM får DRAG i ENV-fliken.
3. Mönsterfliken: NOTE END, SWING, HUMANIZE i toppraden; vänsterpanelen bara för valt spår.
   AI-knappen vid loggan = **AI SERVER (MCP)** med på/av; toppfliken AI = **AI CHAT**.
4. Instrumentmeny: högerklick på spårets kort och på raden i instrumentväljaren (eller "…"-ikon): Rename, Color, Duplicate, Move up/down,
   Delete, Copy/Paste track, Export MIDI, Save pattern preset. Namn och färg sparas i projektet (inte parametrar).
5. "Bredare fönster i pianorullen": utred om editorn kan ändra storlek per flik; annars: lämna.

### S: källor (`content-s`)
Standardvärden enligt avsnitt 0 (bästa ljud, inte nödvändigtvis dagens).
0. FILTER-flikens av/på: hela instrumentets Filter 1+2 förbi (pattern_and_ai §9 sista stycket).
1. FILTER av/på per källa: OSC 1–3, SAMPLE, NOISE (bool; standard på, men **av för SAMPLE** (Tobias 20 okt)). Av = förbi Filter 1 och 2
   (inte förbi amp-envelopen). Matrisen i sample_slicer §5 gäller.
2. Brustyp: White (standard = dagens brus), Pink, Brown, Vinyl (seedad).
3. WARP ENV: källa (Filter Env = dagens, standard; Direct med egen decay 2–150 ms; ENV 2; ENV 3) efter R:s utredning. Visning: lila
   ring/indikator på WARP-ratten när den moduleras (återanvänd befintliga modulationsringar), lampa vid AMOUNT.
   R8-fynd: WARP ENV gör inget när OSC 1:s WARP-läge är Off (standard): varna "WARP ENV needs a WARP mode on OSC 1" (eller välj ett läge).
4. OSC-displayen: WAVE / SCOPE / SPECTRUM (VisualTimer, bara när synlig), click-to-audition i alla lägen.
5. OSC-presets: `.oscpreset` i `Presets/Oscillators/<kategori>/`, meny uppe till höger i OSC-fliken med `<` `>`, Save, RESET.
   Rör aldrig andra oscillatorer, SAMPLE, filter eller effekter. `persisttest`/`presettest`-fall. Innehåll enligt layout_optimization
   §5.1 (våg/tabell + position, warp-läge + amount, pitch/fine/octave, detune, unison, fas, oscillatorns egen envelope/PITCH-data).

### KB: kunskapsbas för AI (docs, Sonnet ensam, ingen kod)
Läs `research/TEST026-10-19/KickAndBaseSchool.md` och alla `transcripts/*.txt`. Skriv `docs/kick-bass-school.md` (engelska, för AI:n):
regler med tal (kicken slut vid 2:a 16-delen, 35–43 Hz i svansen, kick vinner där kick och bas möts, ingen slumpfas på basen, fasjustering
efter filter, post-ringing, 200 Hz-kontroll via pitch→amp ...), mappat till **våra** parametrar och verktyg. Länka den från AGENT_GUIDE,
servera den som MCP-resurs, och gör en Claude Code-skill i `.claude/skills/kickbase-sound-design/SKILL.md` som pekar på den och på
`psytrance-kick-bass-reference.md`. Tobias fyller på med fler lektioner senare: samma fil, nya avsnitt.

### K: kickverktyg (`content-k`)
Ur lektionen: markörer för 1/16 och 1/8 i kickens AMP/PITCH-grafer räknade ur BPM; markörens tid, frekvens och tonnamn uppe till höger;
tonnamn vid pitch-punkterna; zoom i tid (LENGTH) och frekvens (PITCH). Tre klickar (pitch, längd, start, LP/HP) och sub-kontroll som i
image-27: utred mot vad KickEngine redan har, förslag till Opus innan bygge.
Ur KickAndBaseSchool.md (lektion 1, tempo/pitch/amp), också i K:
- **Längd mot tempo:** markera i AMP-grafen var kickens svans bör vara slut (2:a 16-delen vid aktuell BPM, t.ex. ~206 ms vid 145 BPM), och
  en diskret varning när svansen går längre.
- **Svansens slutfrekvens:** visa den (Hz + ton) och markera 35–43 Hz-zonen i PITCH-grafen.
- **"Amp-envelopen som omvänd EQ":** klicka en frekvens i PITCH-grafen (t.ex. 200 Hz) → samma tidpunkt markeras i AMP-grafen
  (tid ↔ frekvens-koppling), så att man ser hur starkt just det frekvensområdet spelas.
- Kickens kompression och "render" (image-27): utred tillsammans med de tre klickarna.

### AN: Analyzer-sida (`content-an`)
Ny toppflik ANALYZER (helsida, flera underflikar). Nedre ANALYZER-fliken och rapportskrivaren flyttar hit. Innehåll: kick + bas i lager
och summa (PsyScope-likt) med FREEZE, faskorrelation per 16-del, spektrum; kollisionsfliken med allvarlighetsgrad (grön/gul/röd, hänsyn
till nivåskillnad > 12 dB, oktav/kvint, ducking) och textlogg ("INSTR 1: sub-bass clash with KICK"), gradienten avstängbar i OPTIMIZATIONS.
Rapportskrivaren uppdateras med allt nytt (P, filter, ackord ...). Inspiration: bilderna `psylab*`, PsyScope (fx23.net/free-vsts).
Ur lektionerna (KickAndBaseSchool.md), också i AN (som mätningar i vyn, i rapporten och som AI-verktyg):
- Var i takten energin är störst: "kicken ska vinna där kick och bas möts" (basens topp får inte vara högre än kickens tap); varna.
- Kickens svans som fångar basen (post-ringing in i nästa kick) och bas som tar energi ur kicken.
- Fasvarning: slumpfas (RAND) på ett basinstrument ger olika start varje ton: varna (lektion 2: "big no-go").
- Spektrumkontroll av t.ex. för mycket 200 Hz i kick+bas.

### PH: fasverktyg (`content-ph`)
Fasrotation av instrumentets grundton (som Voxengo PHA-979, efter filter och effekter, länkad L/R), styrbar för AI. Övertons-/fasredigerare
för wavetables (högerklick på tabellen, som Serum): amplitud och fas per överton (t.ex. sänka grundtonen).
- **AI-verktyg:** hitta kickens transient och var kick och bas överlappar, läs och sätt grundtonens fas, föreslå lägre velocity på toner
  där summan blir för stark, och (avancerat) ta bort basens grundton just där den överlappar kicken.
- **LFOTool-verktyg** (Tobias vill ha ett): först en kontroll av vad SIDECHAIN:s ritbara kurva redan klarar (volymform per instrument,
  synkad, klippa basens post-ringing före nästa kick). Saknas något (t.ex. flera kurvor, en kurva per instrument oberoende av kicken,
  andra mål än volym): bygg det som ett eget "SHAPER"-läge, spec till Opus först.

### F2: sampler (`content-f`)
`sample_slicer_independent_layer_spec.md` + `MINDMAP 2.png`. SAMPLE-lagret: LAYER MODE BLEND (= i dag) / INDEPENDENT (egen kromatisk
pianorulle `INSTR X - SAMPLE`) / OFF; SPLICER på/av (egen rad `INSTR X - SLICER`, slices per Grid/Auto/Manual, kromatiskt från vald ton),
M/S per underrad, tempo (originaltempo eller omsamplat; time-stretch som eget steg efter F2, Tobias 20 okt). Kräver underrader i `Pattern` (byggs på R9):
spec till Opus först. Ur specen också: SENSITIVITY för Auto-Detect, MAP TO (tonomfång), REVERSE SLICE, pianorullen visar "Slice 1, 2 …"
i stället för tonnamn, underrader allokeras inte när de är av, filtermatrisen §5. Hjälptexten i mönsterfliken gäller även underraderna.

### E0: genomgång av presetbygget (först i fas E, Opus + Sonnet, sedan Tobias)
Innan nya presets byggs: gå igenom med Tobias hur en preset byggs så att allt blir rätt och allt kommer med där det ska.
1. Sonnet: utgå från [work/preset-checklist.md](work/preset-checklist.md) och koden; lista allt tillstånd (parametrar, LFO-kurvor,
   mod-routes, mönster/pianorulle, samplingar, wavetables, effektkedjor, namn/färger, ackord, SNAP osv.) och var det sparas i dag.
2. Test: spara och ladda en preset med **allt** påslaget (verktyget + `KickbaseHostTest`); allt som inte kommer tillbaka är ett fel.
3. Opus + Tobias bestämmer: vad hör till preset, projekt, global inställning eller egen presettyp (OSC-, effekt-, mönsterpresets);
   hur InitPreset ska se ut; hur MCP/AI bygger en preset (så att AI:n inte missar delar).
4. Rättningar, sedan skrivs en kort guide `docs/presets-guide.md` (för Tobias och AI-skillen). Först därefter presetarbetet.
5. Samma genomgång för **rapportskrivaren, AGENT_GUIDE, MCP-beskrivningarna och skillen**: allt nytt med (BEZIER.md sista stycket).
   Mål som i Psylab: ett klick återkallar hela resultatet (kick, bas, mönster, sidechain, effekter).

### FX: effekter (`content-fx`), ur layout_optimization
- ECHO som Fruity Delay 2: input (pan, vol), feedback (Normal/Invert/Ping-pong, mängd, CUT-filter), time (sync/ms) + OFS, dry vol.
- Ny FLANGER (rate, depth, feedback, delay time, mix).
- De förenklade Sidechain/Maximizer-effekterna ersätts av de riktiga algoritmerna (samma som master/SIDECHAIN-fliken) i platserna 1–5.
- MASTER-fliken: 5 platser på hela summan, alla effekter inklusive riktiga Sidechain, **utom** Maximizer (finns redan på master).
  Genvägsknapp "MASTER FX" på FX-sidan där FX 12 skulle sitta, avvikande färg. Ingen CLEAR-knapp (RESET/EMPTY i presetmenyerna).
- Två presetnivåer: FX Chain Preset (hela kedjan, finns) och Effect Preset per effektlåda, filtrerad på effekttyp. RESET i båda.

### LC: Live Code (`content-lc`), Opus research 20 okt (förslag, spec till Opus först)
- **Språk = Strudel** (JS + mini-notation, codeberg.org/uzu/strudel, AGPL-3.0 = samma licens som Kickbase, alltså tillåtet; behåll
  copyright i källan och visa den i UI + THIRD-PARTY.md). AI-modeller kan Strudel bra, och koden kan klistras in på strudel.cc.
- **Motor:** JUCE 8:s `juce::JavascriptEngine` är QuickJS (choc). Bunta `@strudel/core` + `@strudel/mini` (fast version, egen bundle, inget
  nätverk) som BinaryData. Körs **aldrig** i ljudtråden: bakgrundstråden frågar mönstret (`queryArc`) några cykler framåt och skickar
  händelser/automation via en låsfri kö; ny kod byts in vid nästa cykel. Sandlåda: ingen fil/nätverk, minnesgräns, tidsgräns (avbrott).
- **Mappning:** `s`/`note`/`n` → kick och instrument (t.ex. `.inst(2)`), Strudel-kontroller (`cutoff`, `resonance`, `gain` …) + egna
  (`warp`, `snap` …) → våra parametrar; `slider(v,min,max)` → riktiga reglage i LIVE CODE-fliken, tvåvägs (ändrar texten).
  Två lägen: **Live** (spelar via schemaläggaren) och **Create** (skriver in i mönstret/pianorullen (R9-lagringen), LFO, effekter).
- **Chatt:** egen AI CHAT ritar kodblock med Play/Stop/Create. **Claude Desktop:** MCP-verktyg `live_code_run/stop/create` + en
  **MCP App** (`ui://kickbase/live-code`, officiellt MCP-tillägg sedan jan 2026, fungerar med lokala stdio-servrar som vår) som visar
  kodblocket med Play/Stop/Create och reglage direkt i chatten och anropar verktygen. Utan MCP Apps-stöd: vanligt kodblock + verktyget.
- **Först ett spike-test:** kör Strudels mönstermotor i QuickJS utan webbläsare (beroenden, t.ex. `@kabelsalat/web`), mät tid per fråga.
  Faller det: egen mini-notation i C++ (samma syntax, delmängd).

### AI, C, BZ, IO, VS
Enligt Tobias specar (layout_optimization §Effects, §RESET, §MASTER; pattern_and_ai §6, §10, §13; BEZIER.md; CODEMUSIC.md). Spec till Opus
när fasen står på tur. Redan beslutat: nya effekttyper (Flanger, Sidechain, Maximizer i platser) läggs **sist** i typlistan som redovisat
engångsundantag från regel 3 (projekt sparar index och laddar rätt; bara DAW-automation av typen kan förskjutas); kontrollera med state load.

## 3. Status
Se [work/PROGRESS.md](work/PROGRESS.md).

## 4. Frågor till Tobias (standardvalet gäller tills han svarar)
1. Grenkedjan content-0 → a → d1 → w → e2 → d2 är godkänd men inte sammanslagen. Får vi slå ihop den i `dev` (fast-forward)? *Standard: nej, vi kedjar vidare.*
2. ~~Radlängd max 64?~~ Besvarad 20 okt: ingen gräns, TOTAL LENGTH bort (R steg 9).
3. Tre klickar och sub-kontroll i kicken (image-27): vill du ha dem, eller räcker dagens klick? *Standard: utreds i fas K.*
4. Gamla DAW-projekt med automation av Kickbase: finns det några du vill behålla? Om inte kan vi före första publika versionen fritt
   utöka valparametrar (t.ex. nya effekttyper, radlängder som inte är 2-potenser) i stället för att lägga dem sist. *Standard: regeln gäller.*
5. ~~SAMPLE-lagrets FILTER-knapp~~ Besvarad 20 okt: OFF för SAMPLE, ON för OSC och NOISE.
6. ~~Time-stretch~~ Besvarad 20 okt: F2 utan time-stretch (omsampling till tempot); time-stretch som eget steg efter F2.
7. ~~LFO Sync-läget~~ Besvarad 20 okt: Sync behålls.
