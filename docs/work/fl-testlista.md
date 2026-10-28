# FL-testlista, första releasen

Format: **Gör så** → *förväntat resultat*. Bocka av `[ ]` när det stämmer. Klicka i pluginfönstret först så att det har tangentbordsfokus (annars kan FL ta Ctrl+Z, Ctrl+S m.fl. före Kickbase).

## Meny och projekt
- [ ] Meny > New Project med osparade ändringar → *frågar om du vill spara först, startar sedan ett tomt projekt.*
- [ ] Ctrl+S → *projektet sparas, en grön bekräftelse visas, projektnamnet står i rubriken.*
- [ ] Meny > Open och Meny > Save as → *öppnar respektive sparar en .kbproj; senaste tre projekten finns i listan.*
- [ ] Meny > Export → *exporten ligger i menyn (inte som egen knapp).*
- [ ] Slå på Meny > Autosave every 3 min, ändra något, vänta över 3 min, välj Open autosave → *en autosparad version finns att öppna.*
- [ ] Öppna ett gammalt projekt/preset → *låter som förut, JOG SPEED står på 1x.*

## Alla flikar (allmänt)
- [ ] Klicka i en rullgardinsmeny, använd piltangenterna, tryck Enter → *fokus syns, pilarna byter val, Enter väljer.*
- [ ] I en flervalsmeny, tryck Enter → *menyn öppnas med ett markerat val.*
- [ ] Ctrl+Z efter vridning av en ratt, ritat steg, valt mönstermall, delpreset och inklistrat spår → *varje gest ångras som ett enda steg.*
- [ ] Ctrl+Shift+Z eller Ctrl+Y → *gör om steget.*
- [ ] Meny > History… → *listan visar stegen med namn; att klicka på ett steg hoppar dit.*
- [ ] Ladda ett huvudpreset eller projekt → *historiken töms.*
- [ ] Flytta ratt, flik, reglage och diagram med musen → *ser ut som designskissen: mörka rattar med skugga, flikar som piller med glöd, platt underflikremsa med liten pil under vald flik, mörka diagram.*

## Förhandslyssning (PREVIEW)
- [ ] Gå mellan flikarna och tryck förhandsknappen längst till höger → *motorflik: bara motorn; INSTRUMENT/FILTER/MIX: hela instrumentet utan mönster; PATTERN, ANALYZER, AI CHAT, LIVE CODE: hela mönstret; KICK: bara kicken.*
- [ ] Starta PREVIEW medan FL spelar noter → *PREVIEW går i Kickbases egen loop och störs inte av FL:s toner.*
- [ ] Stäng PREVIEW → *FL tar över igen; första noten dubbleras eller tappas inte.*
- [ ] Ett-takts-loop i FL, starta PREVIEW → *loopen i FL styr inte förhandslyssningen.*
- [ ] Spela valfri ton på MIDI-tangentbordet under PREVIEW → *påverkar inte förhandslyssningen.*

## Transport, JOG och LOOP (verktygsraden)
- [ ] Dubbelklicka på tempot, skriv 150 under PREVIEW → *tempot ändras. När FL spelar är tempot och takten låsta med tipset "styrs av DAW".*
- [ ] Rulla musens hjul över takträknaren under PREVIEW → *uppspelningen hoppar till vald takt när hjulet vilat en stund.*
- [ ] LOOP finns nu i JOG-panelen (inte i toppbaren): öppna panelen, LOOP på med takt 1–2, ändra till 2–3 under gång, stäng av → *loopar strikt i intervallet; DAW:ens uppspelning måste stoppas för hand. Stäng och öppna panelen: LOOP-knappen och takterna visar rätt läge.*
- [ ] BRAKE med kurvan VINYL, TAPE, HARD och SLOW i tur och ordning (rullistan under BRAKE) → *VINYL: skivspelarstopp; TAPE: lång, tonhöjden glider ner; HARD: kort och rak; SLOW: långsam broms och långsam start. Alla vilar en stund och går tillbaka till inställd JOG SPEED. Håll hjulet under bromsen: bromsen avbryts.*
- [ ] Tryck LP-knappen bredvid restart → *JOG-panelen öppnas. Hjulet ritas som en vinylskiva, nålen vandrar inåt med mönstret och hoppar ut vid loopen, handen visas över skivan.*
- [ ] Läge STOP: starta PREVIEW, håll hjulet → *musiken stannar; vrid framåt: den spelar i vridhastighet; släpp: normalt tempo.*
- [ ] Läge KEEP GOING → *musiken fortsätter; framåt = snabbare, bakåt = långsammare (aldrig baklänges).*
- [ ] Läge REC, sätt JOG SPEED-automationen i Write/Touch/Latch, spela in och scratcha → *kurvan ritas i FL; vid uppspelning följer musiken scratchen; när du släpper går kurvan tillbaka till 1x.*
- [ ] Läge STOP, scratcha → *ingen kurva skrivs.*
- [ ] Dra JOG SPEED till 2 → *dubbelt tempo, LP-ikonen lyser.*
- [ ] Dra en LFO till JOG SPEED, sedan REMOVE AUTOMATION → *statusraden visar LFO:n; borttagning sätter 1x.*
- [ ] Högerklick på JOG SPEED, skriv en automationskurva i FL → *hastigheten följer kurvan.*
- [ ] Starta FL:s uppspelning med DAW SYNC och rör hjulet → *FL:s tempo och position ändras inte.*
- [ ] Stäng panelen medan hjulet hålls → *tempot återgår till normalt.*

## INSTRUMENT och OSC
- [ ] Spela C5 från FL med pianorullen öppen → *mönstret startar och spelar (inte bara en live-ton).*
- [ ] Meny för OSC-förinställningar: radera mappen Dokument\Kickbase\Presets\Oscillators, öppna menyn → *14 startpresets i 6 kategorier (Sub, Bass, Reese, Acid, Growl, Layer); ladda en i varje OSC.*
- [ ] Sätt OSC 1 WARP till RING (B) → *formknapparna byts mot "RING: OSC 1 x OSC 2", "x OSC 3", "x OSC 2 x OSC 3"; AMOUNT blandar torrt (0) till full ring (1). Källan måste stå PÅ, annars blir full ring tyst.*
- [ ] Dra WIDTH upp från 1 under spelande ton → *inga klick.*

## SAMPLE och slicer
- [ ] SHOW SLICES → *varje skiva är en färgad ruta med "Slice N" och ton; ett klick spelar bara den skivan.*
- [ ] Hjulet i vågformen, Skift+hjul → *zoom mot musen respektive scroll.*
- [ ] MANUAL + SLICES: dra en gräns, högerklicka, dubbelklicka → *flyttar, tar bort respektive lägger till en gräns.*
- [ ] CREATE (i Grid/Auto) → *beräknade gränser blir manuella markörer och visas som SLICES.*
- [ ] Skapa fler än 16 skivor → *texten säger att bara de första 16 får rader.*
- [ ] Öppna PATTERN med skivor → *en rad per skiva ("Slice 1" …); rita i raden "Slice 3" → låter som skiva 3 spelad direkt.*
- [ ] Spara och ladda projektet med skivrader → *raderna och noterna finns kvar; Ctrl+Z ångrar tillägg/borttagning av rader.*

## KICK
- [ ] Förhandsknappen på KICK-fliken → *spelar bara kicken.*
- [ ] Peka på noder i pitch-kurvan → *tonnamn visas vid varje punkt (C4 = 60).*
- [ ] Fråga AI-chatten om kickens pitch-noder → *AI:n kan ange ton och Hz per nod.*
- [ ] Ladda de nya kickpresetsen → *låter som förväntat, ingen klippning.*
- [ ] Ladda kickpresetsen Clean Full-On, Clean Tight, Clean Deep, Clean Round, Clean Punchy, Clean Long, Clean Snappy, Clean Warm, Clean Sub, Clean Modern → *rena psytrance-kickar, svans kring 40–45 Hz, ingen klippning (Clean Warm är hetast, toppen nära 0 dB).*
- [ ] Ladda Psy Clean C, C Tight, D, D Long, F Soft, F Sub, A Punch, G (jämför med TK_Kick Psy140 Clean G) → *rena sinus-kickar med brant svep; tonen är Fixed, svans C 33, D 37, F 45–50, G 50, A 56 Hz. Låter de lika bra som originalet? Vilken ton/karaktär funkar mot basen? (A Punch har lite klick/drive.)*
- [ ] Ladda basinstrumenten Rolling Tight, Rolling Deep, Rolling Bright, Rolling Fat, Rolling Soft, Rolling Snap (ihop med en kick) → *rullande bas som sitter under kicken utan att mulla eller klippa.*
- [ ] Ladda huvudpresetsen newest 1–10 (ett i taget, spela E4 i låtens tempo) → *tio olika stilar (full-on, dark forest, progressive, hi-tech, Zenonesque, hård, acid goa, deep reese, minimal, 3 lager); kick + bas sitter ihop, ingen klippning, FX-kanal A hörs svagt som send. Bara mätta, inte avlyssnade: lyssna och notera vilka som ska finjusteras.*

## PATTERN och pianorulle
- [ ] Spela mönstret → *spelande toner lyser i pianorullen.*
- [ ] Kick i 2 takter och instrument 1 i 1 takt, Loop 1–2 → *loopen håller sig inom intervallet; kort spår loopar internt.*
- [ ] Mönster på 3 takter med kick i varje takt → *takt 3 har alla slag; längdindikatorn stämmer med verklig längd.*
- [ ] Glide-ton i samma takt som ett ackord → *alla samtidiga toner glider lika långt.*
- [ ] Glide-ton med en stapelton på samma tick → *stapeltonen hörs.*
- [ ] Högerdrag på tomt rutnät → *ritar ett glidestreck, kan sluta på en ton eller i fri höjd, och går också före en ton.*
- [ ] Högerklick utan drag → *vanliga menyn visas.*
- [ ] Spara och ladda, exportera MIDI → *streck bevaras vid sparning; MIDI-exporten approximerar glid.*
- [ ] Andra raders toner → *syns svagt (spöktoner) i radens färg och går inte att klicka.*
- [ ] POP OUT i pianorullens nedre rad → *rullen öppnas i eget fönster som går att ändra storlek på; stegen syns kvar i huvudfönstret.*
- [ ] DOCK eller stäng fönstret → *rullen kommer tillbaka.*
- [ ] Knappen LISTEN PIANO ROLL → *dyker upp direkt när rullen visas, även med låg animationstakt.*

## MIX och kombinerad våg
- [ ] Kombinerad våg: mushjul, mittknappsdrag, rullningslist → *zoomar runt musen (0,1–32x), panorerar, tidsaxeln visas i ms.*
- [ ] WIDE/NARROW → *vågen får hela bredden respektive går tillbaka.*
- [ ] Dubbelklick i vågen → *återställer zoom och position.*
- [ ] MIX-fliken som helhet → *alla kontroller svarar och ljudet följer.*

## FX och SIDECHAIN
- [ ] Ladda ett gammalt projekt med Maximizer → *Classic, inte Smooth, och MODE-ratten står rätt.*
- [ ] Maximizer MODE, KEEP/COMPARE, shimmer, Low Multiplier, AP/START, HALFTIME → *hörtest: varje läge låter olika och rätt, utan klick.*
- [ ] FX-slot med SIDECHAIN CURVE → *dämpar enligt kickens envelope/kurvan från SIDECHAIN-fliken.*
- [ ] FX-slot med SIDECHAIN 2-BAND → *LOW och MID-HI dämpas var för sig med eget djup och egen release.*
- [ ] FX-slot med PHASE ROTATION, FREQ 60 Hz, ROTATION 180 deg på en bas under kicken → *basens grundton byter fas mot kicken (summan blir starkare eller svagare beroende på vridning), nivån i övrigt oförändrad.*
- [ ] Echo, Flanger, BAND DUCKER och övriga FX2-typer → *ljud i alla slots, inga klick eller tystnad.*

## LIVE CODE
- [ ] Skriv `s("` → *förslag: bd/kick och i1…i10.*
- [ ] Skriv `note("` eller `n("` → *förslag på tonnamn och exempel (ackord, `*4`, `~`, `<>`, `@3`).*
- [ ] Skriv `$:` på tom rad → *förslag och en mall.*
- [ ] Skriv `lfo("` och `env("` → *parameternamn föreslås.*
- [ ] `s("i1").chord("major","c2",0,["1/4","4/4","2/4"])` → *C2 kort, E2 en hel cykel, G2 en halv.*
- [ ] Tryck FORMAT → *en kedja per rad, `s("…")` först, `//`-rader orörda; Ctrl+Z ångrar.*
- [ ] Editorn → *mörkt tema med färgkodning.*
- [ ] Kör Live Code, sedan Ctrl+Z → *körningen ångras som ett steg.*

## AI CHAT
- [ ] Ange en OpenAI-nyckel (OPENAI-knappen, SAVE), be om "gör kicken kortare" → *AI:n svarar och ändrar kicken; nyckeln visas inte i loggar.*
- [ ] Starta Ollama med en modell som stöder verktyg (t.ex. qwen3), välj OLLAMA, SAVE → *modelllistan hämtas; samma uppmaning fungerar utan nyckel.*
- [ ] ANTHROPIC och GEMINI → *fungerar som förut efter bytet av leverantör.*
- [ ] Be AI:n läsa tillståndet → *kickens pitch-noder med ton och Hz finns med.*

## Övertonsredigerare, USER-kategori, projektfiler
- [ ] Högerklicka på en oscillators vågform > Edit harmonics → *ett stort fönster öppnas med vågformen överst och staplar för styrka och fas under.*
- [ ] Dra i staplarna, snabbt och långsamt → *vågformen följer musen direkt, inga staplar hoppas över, ljudet ändras efter en kort stund.*
- [ ] Dra stapel 1 längst ner → *grundtonen försvinner. Dubbelklick på en stapel nollställer den, dubbelklick i vågformen nollställer allt.*
- [ ] Zoom 16 / 32 / 64 och Ctrl+hjul → *staplarna blir bredare respektive smalare.*
- [ ] Penslarna DRAW, LINE, SMOOTH, RANDOM → *LINE ritar en rak linje, SMOOTH jämnar ut, RANDOM ger nya värden vid varje drag.*
- [ ] Presets SAW, SQUARE, TRIANGLE, ODD, EVEN, BASS WARM → *spektrumet och vågformen ändras och låter som namnet.*
- [ ] Stäng fönstret, spara projektet, öppna det igen → *den ändrade vågen är kvar.*
- [ ] Spara en egen preset utan att välja kategori → *den hamnar i USER i presetlistan.*
- [ ] Meny > Export MIDI efter att ha spelat in ett mönster → *filen går att öppna i DAW:en med rätt toner och längder.*
- [ ] Ladda ny instans, välj bas-preset Full-On Roll (PSY ROLLING BASS) → *bara rad 1 ljuder; mellanregistret rullar mot PSY CLEAN-kicken utan att kicken försvinner.*
- [ ] Jämför Wide Mid, Dekel Groove och Forest 1/32 i DAW → *Wide Mid ljusast, Dekel och Forest mörkare; säg vilken som är för tunn.*
- [ ] Ladda huvudpreset Roll Full-On, Roll Wide, Roll Dekel, Roll Forest (PSY ROLLING) i DAW → *kick och bas rullar ihop, basen ducker för kicken, bara ett lager ljuder; säg vilken som är för tunn eller för mörk.*
- [ ] Öppna din GLIDE BASE (Full Instrument) i DAW → *glide-tonerna är nu längre (60 ticks) och ska höras som ett tydligt glid; originalet finns som GLIDE BASE.mofp.bak.*
- [ ] Ladda Glide Sub, Glide Acid, Glide Dive (GLIDE BASSES) i DAW → *basen glider mellan tonerna utan klick, ducker för kicken; säg om glidet är för kort eller för långt.*
- [ ] Ladda Roll Full-On och Glide Sub i DAW, slå FUNDAMENTAL DUCK av och på → *på: kickens slag tydligare, basens mellanregister oförändrat; säg om basen hackar eller tunnas ut (se docs/school/fundamental-duck.md).*
- [ ] Ladda Roll Wide, Roll Dekel, Roll Forest, Glide Acid, Glide Dive → *grundtonen sänks i kickens takt, inget klick; jämför med duck av.*
- [ ] Ladda Duck Demo, slå FUNDAMENTAL DUCK av och på → *av: kick och bas sväver/tar ut varandra i sub-området; på: kicken står fram, basen dippar bara i kickens slag; säg om skillnaden hörs.*
- [ ] Ladda New Init 1–5 i DAW → *varje är en egen startpunkt av fabriks-Init: 1 med duck, 2 halftime, 3 bara sub, 4 rullande, 5 acid-duo; säg vilka som är användbara att bygga vidare på.*

- [ ] (Lemon) Mid Growl/Pluck/Wide, Hi Zap/Acid/Lead, Synth Pad/Pluck/Stab: kategorierna MID BASS, HI BASS, SYNTH visar rätt, låter som namnet, ingen klippning.

- [ ] (Lemon) Genrepresets, 7 genrer × (1 kick + 2 basar): kategorierna MINIMAL, GOA, DARK PSY, DANCE, TRANCE, TECHNO, EMPIRIKAL visas; kick+bas ihop låter som genren; Minimal/Dance/Techno-kickarnas svans (48–52 Hz) över psy-målet, lyssna om den krockar med basen.
- [ ] (DAW) UNISON-kategorin: Unison Reese, Wide och Dark visas; mono under ca 150 Hz kan behövas (lowSide ca −25 dB, se docs/school/reese.md); lyssna om de slår fint mot kicken.
- [ ] (Lemon) Bas-sample, AMP/PITCH under vågformen: av → *som förut*; kurvan korta AMP → *samplingen tystnar efter kurvans längd*; PITCH +12 → *glider ner till egna tonen*.
- [ ] (Lemon) Bas-sample REVERSE: på → *spelas baklänges i varje ton, med LOOP loopar den*; av → *som förut*.
- [ ] (Lemon) Kick CLICK 2/3: nivå 0 → *som förut*; med fördröjning → *extra klick efter anslaget*; BP/LP/HP → *olika klang*. SAMPLE-vyn: START → *klipper början*, REVERSE → *baklänges*, FADE → *tonar ut slutet*.
- [ ] (DAW) UI för sample-editor, REVERSE och klick-lager (Arbetare 2): bas-samplingen har AMP/PITCH/REVERSE under vågformen; kicken har CLICK 2/3 och i SAMPLE-vyn START/REVERSE/FADE; reglagen hörs som i föregående rader och inget överlappar i fönstret.
- [ ] (DAW) FX-matris, bara parametrar än så länge (Arbetare 2, gren fx-matrix): automatisera "Instr 1 FX Send 1" och "Kick FX Send 3" – ljudet går till respektive FX-kanal utan klick; "FX 1 Mute" tystar kanalen mjukt (sends ligger kvar, svansen fortsätter när den slås av); gamla presets låter som förut.
- [ ] (DAW) FX-matris-UI (Arbetare 2, gren fx-matrix): FX-väljaren på kick/instrument visar en sammanfattning ("FX1 50%, FX3 30%" eller "None"); klick öppnar en popup med en rad per kanal (på/av + reglage i %); av = 0 och reglaget låst, värdet minns bara medan fönstret är öppet; "+ add FX channel" lägger till en kanal överallt (ny send 0); gamla INS/SEND-länken är en vanlig rad; FX-sidan har FX1..N som rullista och kan ta bort sista kanalen (frågar om den har effekter/sends); M-knapp per FX-kanal i mixern tystar mjukt; gamla presets låter som förut.
- [ ] (DAW) Parameterlås per ton, ljud (Arbetare 2, gren fx-matrix): sätt lås via MCP `set_pattern` (cutoff/pan/fxsend per ton): en ton med cutoff-lås låter som samma cutoff på rattten, pan-lås flyttar tonen mjukt utan klick, FX-send-lås skickar tonen till instrumentets FX-kanal; toner utan lås ärver inte det förra låset; mönster utan lås låter som förut.
- [ ] (Lemon) Pianorulle, lane-menyn → *VEL, CUT, PAN, FX*. Dra i CUT/PAN/FX → *stapeln växer, tonbeskrivningen visar t.ex. "CUTOFF 62 %", hörs*. Alt+klick → *låset tas bort*. Ctrl+Z → *ett drag = ett steg*. Kopiera/klistra in och flytta en ton → *låset följer med*. Ny ton → *börjar utan lås*. Stapla toner → *ledtråd att låset gäller huvudtonen*.
- [ ] (Lemon) Skivrulle: öppna PATTERN på en skivrad → *pianorullen visar en tangent per skiva; rita en ton → låter som den skivan*; fler än 16 skivor → *inget tak i rullen*.
- [ ] (Lemon) Echo-FX: TIME MS finns inte längre → *10 rattar per slot; gamla projekt med Echo laddas och låter som förut*.
- [0.4 p1, Lemon] Öppna ett gammalt projekt med FX-länk i SEND-läge: raden syns som vanlig FX-rad i popupen (ingen dubbelrad), ljudet är detsamma. INS-länk visas som "INS FXn".
- [0.4 p3, Lemon] PATTERN-raderna har bara en (större) VOL-ratt, ingen FX-ratt; låga rader ser rätt ut. FX-knappen på fliken visar None / FX / "2 x FX"; hovra visar kanalerna.

- [ ] 0.4 punkt 2 (Lemon): Öppna FX-popupen på en instrumentrad, tryck '- remove FXn' på sista kanalen. Med effekter/sends ska en bekräftelse visas. Ctrl+Z ska återställa effekter, EQ, kompressor, nivå och sends.
