# Kickbase

VST3-plugins för kick och rullande bas i psytrance, byggda med [JUCE](https://juce.com) 8.

- **Kickbase** (instrument): kick + upp till 10 instrument (tre från början) som spelar ett mönster så länge en ton hålls i
  DAW:en. Tonen bestämmer tonarten, DAW:ens taktstreck var stegen hamnar.
- **Kickbase FX** (effekt): läggs på mixerkanalen där en annan synth (t.ex. Serum) hamnar.
  Synthens ljud gate:as av mönstret och duckas under en egen kick.

Designen finns i [docs/DESIGN.md](docs/DESIGN.md), planerna för ljuddesign-skill och AI-styrning i
[docs/sound-design-roadmap.md](docs/sound-design-roadmap.md).

## Bygga

Kräver Visual Studio Build Tools med C++-verktygen (CMake och Ninja följer med).

```
build.bat                    # Release (med link-time optimization, långsam att länka)
build.bat RelWithDebInfo     # snabbare att bygga under utveckling
build.bat RelWithDebInfo all # bygger också utvecklingsverktygen (KickbaseMcp / KickbaseTool) och KickbaseHostTest
```

I PowerShell skriver du `.\build.bat RelWithDebInfo` (med `.\` framför); i vanliga kommandotolken räcker `build.bat RelWithDebInfo`.

JUCE laddas ner automatiskt första gången. `build.bat` kör testerna efter bygget. Utan `all` byggs bara det som behövs för att
prova: instrumentet, FX-pluginet och testerna.

**Snabbare byggen:** JUCE:s tunga headers är förkompilerade (`Source/PchPlugin.h`, `Source/PchCore.h`), felsökningsinformationen
ligger i objektfilerna (/Z7) så att kompileringarna inte väntar på en gemensam PDB, och verktyget och MCP-bryggan är ett och samma
program (`KickbaseMcp.exe`, som också kopieras till `KickbaseTool.exe`) så att all kod bara kompileras en gång. Om `sccache` (eller
`ccache`) finns installerat används det automatiskt, vilket gör ren ombyggnad och byte av gren mycket snabbare
(`winget install Mozilla.sccache`). Det hjälper också att utesluta mappen `build` och repot ur Windows Defender.

## Installera

Stäng FL Studio och kör `createVst.bat` som administratör (högerklick > Kör som administratör), eller i en PowerShell som administratör, i projektmappen:

```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1          # Release (standard)
powershell -ExecutionPolicy Bypass -File .\install.ps1 RelWithDebInfo
```

Skriptet kopierar båda pluginsen till `C:\Program Files\Common Files\VST3\`, tar bort gamla
kopior och städar bort en felplacerad `VST3\Contents`-mapp om en tidigare kopiering hamnat fel.
Scanna sedan om plugins i DAW:en. Instrumentet finns också som fristående app:
`build\Release\Kickbase_artefacts\Release\Standalone\Kickbase.exe`.

## Använda

**Instrumentet:** lägg en lång ton i pianorullen (t.ex. F# över 8 takter) och tryck play.
Kick och bas spelar mönstret i PATTERN-modulen så länge tonen hålls. NOTE END i MIX
bestämmer vad som händer när tonen tar slut mitt i ett mönster.

**FX-versionen:** lägg den i en effektplats på synthens mixerkanal och välj tonart (KEY).
När DAW:en står still släpps ljudet igenom orört och ingen kick spelas.

**Flikarna:**
- **AI** (överst, bredvid INSTRUMENT): chatta med din egen AI direkt i synten. Den ändrar ljudet med samma verktyg som MCP-bryggan (parametrar, mönster, effekter, rendering och mätning) och du ser varje anrop i samtalet; UNDO ångrar dess senaste ändring. Det kräver din egen API-nyckel från Anthropic (console.anthropic.com): första gången frågar sidan efter den, och KEY-knappen byter eller tar bort den. Nyckeln sparas krypterad på datorn (Windows DPAPI) och skickas bara till api.anthropic.com; användningen debiteras ditt Anthropic-konto (tokenräknaren visar åtgången). Välj modell högst upp. Utan nyckel gör fliken ingenting.
- **PATTERN, KICK, INSTRUMENT** (överst): mönstret, kicken och instrumenten. INSTRUMENT-fliken visar ett instrument i taget: välj det i listan i flikraden, lägg till ett med **+** och ta bort det sista med **-** (det rensas helt ur minnet, med en bekräftelse om det har toner, ändrade inställningar, sampling eller kopplingar). Alla instrument kan döpas (menyn i väljaren, eller dubbelklicka på namnet i mönstret). Mönstret har två lägen,
  **PATTERN** (tonerna: stegen är en enkel pianorulle, **PIANO ROLL** den avancerade varianten, och ett lager som
  redigerats där visas som en miniatyr) och **GATE** (en egen av/på-rytm per lager med eget rutnät som hackar ljudet). Uppe i hörnet på varje flik väljer du effektkanal och mängd (FX).
- **EFFECTS** (nederst): elva kanaler, FX1-FX11, var och en med flikarna FX (fem platser, 15 effekttyper), EQ
  (dra noderna, live-spektrum) och COMP (kompressor med vågformsvy). Dra platserna för att byta ordning.
- **SIDECHAIN:** ducka basen efter kickens envelope eller en egen kurva; DUCK per lager, och vilka
  källor som påverkas.
- **LFO:** tre LFO:er. Hastigheten är tempo-synkad (1/32 till 16 takter) eller egen i Hz, och formen kan vara en egen kurva (**Custom**): dubbelklicka på en vanlig form för att rita om den, dra punkterna, dra rombernas mitt för att böja, dubbelklicka för en ny punkt, högerklicka för att ta bort en punkt eller välja en mall (Skift = hela åttondelar). Dra en LFO på en ratt för att modulera den.
- **ENV:** det valda instrumentets envelopes som diagram du kan dra i: VOLUME (ENV 1), FILTER, ENV 2 och ENV 3. ENV 2 och ENV 3 har ett handtag som dras till en ratt i fönstret ovanför.
- **MATRIX:** alla modulationskopplingar i en lista (högst 64): källa (LFO, MACRO 1-4, ett instruments ENV 2, ENV 3, VELOCITY eller RANDOM), hur mycket och vilken ratt (menyer med grupper). Överst till vänster fyra makrorattar med handtag att dra till rattar.
- **MASTER:** **MIXER** (gain, pan, dB-mätare, SC och MAX per källa) och **MAXIMIZER** (drive, clip,
  limiter med tak).
- **SOUNDSTAGE:** spektrum, stereobild, bredd, mono under en frekvens och stereoförstärkare.
- **ANALYZER:** jämför kickens och varje instruments spektrum samtidigt (välj vilka som visas), med övertonsserier (HARMONICS) och röda krockar där flera källor spelar på samma frekvens (COLLISIONS). Analysen är av som standard (ANALYSER) och kostar ingen processorkraft när den är av eller fliken inte syns.
  - **ANALYSE** spelar mönstret ett helt varv offline (pluginet fortsätter fungera, med förlopp) och skriver en rapport: krockar mellan kick och instrument och mellan instrument, ett instrument som täcker kicken, kickens svans för låg/hög, instrument som är ostämda mot varandra eller kicken (oktaver och unison räknas som rent), rumlande djupbas under 30 Hz och en limiter som jobbar för hårt. Varje fynd har allvarlighet, källor, frekvensområde och takter; klicka på ett fynd för att se det i diagrammet. SENS styr känsligheten (låg/normal/hög) och COPY kopierar rapporten som text. AI:n får samma rapport via verktyget `analyse_mix`.
  - Fliken har två vyer: **SPECTRUM** (live) och **ANALYSIS + REPORT** (ANALYSE, rapporten, REFERENCE och EXPORT). Ett klick på ett fynd visar det i spektrumet.
  - **EXPORT** kopierar rapporten som text eller sparar allt som mättes som en JSON-fil (för en AI eller för hand): fynd, översikt per källa, bandnivåer, övertonsserier och 1/12-oktavspektrum för varje källa och takt, kickens slag med duckning, nivån i varje band i varje ram genom hela loopen, och alla synthens inställningar (oscillatorer, wavetables, filter, effekter, modulering, mönster, toner). I listan kryssar du i vad som ska med.
  - Filen kan innehålla allt: presets, alla inställningar ordnade (oscillatorer A/B/C med wavetable, filter, envelopes, EQ, sändningar, effektkanaler med platser, master, mixer, modulering), mönstret omräknat till takt och taktslag, spektrogrammet (nivå i varje band i varje ram) med nivå, topp och ljusstyrka per ram, nivå/crest/ljusstyrka/energi i sex områden per källa och takt, och en förklaring för en AI som inte känner till Kickbase. I EXPORT kryssar du i vad som ska med (och "bara de källor som visas"); en AI väljer själv med `include`, `sources`, `bars`, `min_hz`/`max_hz` och `frame_step`.
  - En AI kan skriva **förslag och det den fixat** i rapportrutan (verktyget `add_report_note`), både via bryggan och chatten i synten.
  - **REFERENCE** läser in en låt vars medelspektrum visas bakom mastern (nivåanpassat) och som rapporten jämför balansen mot.
  - **EQ** visar och redigerar EQ:n för vald källa i diagrammet (fyra band och högpass/lågpass, dra punkterna). Kicken, varje instrument och mastern har egen EQ.
  - **SHOW** är en flervalslista över vilka källor som visas.
- **SIDECHAIN som effekt:** typen *Sidechain* i en FX-kanal sänker kanalen efter kickens eller ett annat instruments nivå (SOURCE, THRESHOLD, DEPTH, ATTACK, RELEASE). Skicka instrumentet som ska duckas 100 % till kanalen.
- **Dra och håll:** drar du en LFO, ett makro eller en envelope och håller muspekaren över en flik en kort stund byter fönstret till den fliken, så att du kan släppa källan på en ratt där (t.ex. en effekt).
- **SETTINGS:** inställningar för alla fönster och projekt (sparas i `Dokument\Kickbase\settings.xml`): **WINDOW SIZE** (50-160 %, för små skärmar), **AI LINK** (på/av och en egen **åtkomstnyckel**: är den satt svarar Kickbase bara en AI-brygga med samma nyckel, `KICKBASE_KEY` eller `install-mcp.ps1 -Key <nyckel>`), **FOLDERS** (egna mappar för presets, exporter och wavetables) och **WAVETABLE PACK** (installerar de 4000+ AKWF-vågformerna med en knapp: en nedladdning på cirka 20 MB från GitHub, bara när du trycker, och bara det).

**Presets:** varje del (hela pluginet, kicken, varje instrument, varje mönsterrad, en effektkanal, sidechain, LFO, maximizer och
soundstage) har en egen presetmeny med egna presets, "Save preset..." och "Open preset folder". Egna presets sparas som filer under
`Dokument\Kickbase\Presets\`. Ett instrument- eller kickpreset kan länka en effektkanal, och ett mönsterpreset ett instrument (ett namn,
ingen kopia). Fabriksljud finns för kicken och instrumenten; fler byggs med `fitbeat` (se nedan) när synten är klar.

**EXPORT** (instrumentet): exporterar kicken, basen, hela mixen eller alla tre som WAV-loopar på
4, 8 eller 16 takter, i DAW:ens senaste tempo och med den senast spelade tonen. Filerna föreslås
under `Dokument\Kickbase\Exports\`.

**Gränssnittet:**
- Dra i handtagen i kurvorna för att forma envelopes (shift = finjustering, dubbelklick = återställ).
- Klicka/dra i stegen för att slå på/av. Välj VEL/GATE/PITCH/SLIDE för tonernas lane:
  dra i staplarna, skrolla för tonhöjd, högerklicka för att återställa en ton. Rutnätet (GRID) väljer
  upplösning, 1/4 till 1/64 och trioler.
- **Pianorullen** (PIANO ROLL): klicka i tomt för en ny ton, dra för att flytta, dra kanten för längd,
  högerklicka för att ta bort, **GLIDE**-knappen ritar glide-toner (som slide-toner i FL: de glider tonen som klingar
  till sin tonhöjd under sin egen längd, och tonen blir kvar). Skroll = tonhöjd (±96 halvtoner från tonarten),
  Ctrl + skroll = zoom, Shift + skroll = tid. Remsan längst ner är velocity.
- **PRESET** i pianorullen sparar lagrets pianorulle som en fil (`Dokument\Kickbase\Presets\Piano Roll`) och laddar sparade i valfritt lager.
  **MIDI** exporterar pianorullen som en MIDI-fil eller importerar en MIDI-fil till lagret (glide-toner exporteras som vanliga toner).
- **Kicken är ett notlager** som baslagren: egen KICK-knapp bland lagren (rutnät, FILL-mallar, velocity, tonhöjd i lanet och pianorullen). En kicknots tonhöjd flyttar kickens svans, så man kan spela kick-melodier. Gamla projekt med kickens 16 steg öppnas som toner.
- **Provlyssning och spelmarkör:** klicka på en tom plats i kickens ruta för att höra kicken (även när DAW:en står still), och klicka på samplingens ruta på basens SAMPLE-flik för att höra lagret. Rutorna glöder när musen är över dem. När kicken eller en sampling spelas (från DAW:en, mönstret eller ett klick) lyser ett streck upp var i ljudet den är, vågformen fylls i bakom strecket och rutan blixtrar till.
- **Maximizer som effekt**: i effektkanalerna finns nu en Maximizer (GAIN, CLIP, CEILING, RELEASE, MIX; ingen fördröjning). Skicka kicken till en kanal med en Maximizer (FX SEND 100 %) för att göra bara kicken högljudd och jämn, vid sidan av mastern.
- **Sampling på basen och kicken**: släpp en WAV på ett baslagers sida (eller LOAD) så blir den en tredje ljudkälla bredvid oscillatorerna (nivå, start, root, loop, KEY låter den följa noterna). På kicken kan en sampling vara klick, ett lager eller hela kicken (MODE: Click, Layer, Replace).
- **Fler kick-lager och former**: vågform på kroppen, sub, FM, brusig topp, fler drive-lägen (Fold), bitkrossning, lågt skär och velocity som styr pitch-svepet.
- **Längd och loop per rad**: rutan under radens rattar (kick, instrument 1-10) ger raden en egen längd (1-64 takter; Total = hela mönstret), och loop-knappen bredvid säger om en kortare rad ska loopa tills den längsta raden har spelat klart (visas som dämpade kopior) eller spela en gång. TOTAL LENGTH är hela mönstret och är alltid minst lika lång som den längsta raden. En rad som är en pianorulle ställer sin längd i pianorullen (LENGTH i verktygsraden). Förkortas en rad (eller totalen) tas tonerna som inte får plats bort, med en bekräftelse först om något går förlorat (bara upprepningar tas bort utan fråga), och förlängs den frågar plugin om det den spelade ska kopieras in i de nya takterna. Remsan överst i stegvyn visar radernas längder jämfört med hela mönstret och var man är; klicka eller dra i den för att byta sida. Mönstret kan vara upp till 64 takter; stegvyn visas fyra takter i taget (< >), och **FOLLOW** låter stegvyn och pianorullen bläddra med när låten spelas.
- **Pianorullens verktyg:** PENNA (rita toner), MARKERA (dra en ruta i tomt, som på skrivbordet; Skift + dra gör detsamma med vilket verktyg som helst, och MARKERA-knappen trycks då in) och SUDD (klicka eller dra över toner). Markerade toner kopieras med Ctrl + C (eller COPY, som bara syns när något är markerat) och klistras in med Ctrl + V (eller PASTE): de inklistrade tonerna visas med annan färg och röd kant, först där de kopierade slutar. Klicka eller dra för att välja var, och bekräfta med OK / Enter eller avbryt med CANCEL / Esc. Ctrl + X klipper ut. Muspekaren blir en penna, ett sudd eller ett kryss.
- **Lyssna överallt:** i PREVIEWS-rutan överst spelar högtalaren hela synthen på den valda tonen (standard C5; tonen gäller alla provlyssningar). Håll ner musen för att höra (det loopar), släpp för att sluta; ett nytt tryck börjar om från början och ett dubbelklick låter det spela tills man klickar igen. Varje rad (kick, instrument 1-10) och varje pianorulle har en egen högtalare, och ett klick på en oscillators wavetable eller på en sampling spelar bara den. Pilen vid takträknaren börjar om mönstret vid 1.1. Muspekaren blir en högtalare över sådant som går att lyssna på.
- **Instrument (upp till 10):** varje instrument har eget mönster, egna oscillatorer, filter, envelopes, FX-send och sidechain-djup. SIDECHAIN visar en rad per instrument, mixern en kanal per instrument, och instrumentens egna utgångar (Instr 1-10) finns som extra utgångar. Gamla projekt öppnas som de är (kicken flyttas till sitt nya lager).
- **ENV 2, ENV 3, VELOCITY, RANDOM:** på INSTRUMENT-sidan öppnar du dragspelet **MODULATION**: två extra envelopes (attack, decay, sustain, release) som inte spelar ljudet utan rör andra rattar, plus tonens velocity och ett slumptal per ton som källor. Varje källa har ett handtag (DRAG) som dras till en ratt, eller sätts i MATRIX. Rattens ring visar källans färg och hur mycket den rör.
- **Fler rattar för variation:** sub-vågform (sinus/triangel/fyrkant/såg), NOISE TONE (mörkt till vitt brus), PITCH ENV (tonhöjdssvep i början av varje ton, för "thump" och laser), drive-lägen (Soft, Hard, Fold, Asym, Crush) och nya warp-lägen (Flip, Quantize, AM).
- **Wavetables:** 120 inbyggda tabeller i kategorier (Basic, Analog, Filter, FM, Digital, Random, Vocal, Organ, Psy, Evolve, Grit) som byggs när de behövs. TABLE-knappen öppnar en meny med kategorierna och ditt **bibliotek**: allt i `Dokument\Kickbase\Wavetables` (WAV-filer, undermappar blir menyer) kan väljas, och "[ Whole folder as one wavetable ]" gör en hel mapp enkelcykel-filer till en wavetable (högst 16 jämnt utvalda). `install-wavetables.ps1` hämtar de 4000+ AKWF-vågformerna (CC0) till biblioteket.
- **Fönstret** går att dra större och mindre i hörnet (hela gränssnittet skalas; storleken sparas i projektet). Den borstade metallytan är borta, och loggan glöder: en våg från vänster för varje kick och en från höger för varje basstöt, i färger som sakta glider.
- **Mönstret:** ett instruments rad väljs med rundknappen vid namnet; dess lane (EDIT IN LANE: Velocity, Note length, Pitch) ligger direkt under raden med en ram runt båda. Är det fler rader än det får plats visas en rullningslist, och hörnet längst ner till höger zoomar radernas höjd (dra upp/ner, dubbelklick = automatiskt; låga rader blir kompakta). **ALL LENGTHS** (visas när Note length är vald) skalar längden på alla instrumentets toner. Pianoknappen är grön med ett plus när raden inte har någon pianorulle än, och blå när den har en.
- Raderna i PATTERN har små rattar för volym och FX-send, som FL Studios Channel Rack.
- Dra in en WAV på kickmodulen (eller LOAD) för att använda en sampling som klick.
- Instrumentet har separata kick- och basutgångar utöver huvudutgången (som alltid har hela mixen).

## AI via MCP (Claude Desktop)

`KickbaseMcp.exe` är en MCP-server som låter en AI-app (t.ex. Claude Desktop) skapa ljud och presets i Kickbase:
läsa och sätta parametrar, effekter, kickens envelopes, mönster och sidechain, rendera och mäta (med spektrogram),
spara presets och ångra. Verktygen och guiden som AI:n läser finns i `Source/Core/ToolLayer.cpp` och `docs/AGENT_GUIDE.md`.

1. Bygg (`build.bat RelWithDebInfo`) och kör `powershell -ExecutionPolicy Bypass -File .\install-mcp.ps1`.
   Skriptet kopierar bryggan till `%LOCALAPPDATA%\Kickbase` och lägger in den i Claude Desktops inställningar (med backup).
2. Starta om Claude Desktop helt. Be Claude använda Kickbase-verktygen (den läser `get_guide` först).
3. Utan DAW jobbar AI:n mot en egen instans ("headless"). Vill du att den ska styra pluginet i FL: klicka på **AI** i
   pluginets topprad, och be AI:n köra `list_instances` och `use_instance`. Länken är lokal (named pipe) och av som standard.

**Stabil länk.** Varje Kickbase lyssnar på fyra pipes, så flera AI-fönster kan vara anslutna samtidigt. Tappar länken kopplar bryggan upp
sig igen själv (och skickar om anropet om det inte kom iväg), kollar med en ping efter en paus, och lägger tillbaka valet av instans om
AI-appen startar om bryggan. Långa anrop (rendering) blockerar inte bryggans svar på ping/listor. Händelser från båda sidor loggas i
`Dokument\Kickbase\Bridge\bridge.log`. Under SETTINGS > AI LINK ser du hur många som är anslutna. Efter en uppdatering: avsluta Claude
Desktop helt (den håller den gamla `KickbaseMcp.exe` låst), kör `install-mcp.ps1` igen och starta Claude Desktop. AI-chatten i pluginet
provar om vid nätverksfel, 429 och 5xx (2, 4 och 8 sekunder).

Förhandslyssningar och spektrogram hamnar i `Dokument\Kickbase\Previews`, presets i `Dokument\Kickbase\Presets`.
`KickbaseMcp --selftest-link` testar länken, och `KickbaseHostTest <plugin.vst3>` laddar pluginet som en värd
(spelar, öppnar och stänger editorn, laddar ur) för att hitta krascher vid avstängning.

## Utvecklingsverktyg

`KickbaseTool` (byggs med `build.bat RelWithDebInfo all`; samma program som `KickbaseMcp`, som också går att köra som `KickbaseMcp tool <kommando>`) testar utan DAW:

```
KickbaseTool render      instrument|fx <ut.wav> [bpm] [takter] [ton] [preset, t.ex. 16+k3+b5]
KickbaseTool export      <ut.wav> full|kick|bass <takter> [bpm] [ton] [preset]
KickbaseTool snapshot    instrument|fx <ut.png> [flik]
KickbaseTool analyze     <ljudfil> [start] [slut]
KickbaseTool fitbeat     <ljudfil> <start> <slut> rolling|galloping|triplet|offbeat
KickbaseTool comparebeat <ljudfil> <start> <slut> <mönster> <rendering.wav>
```

- `render` skriver en WAV och en PNG med vågformen och skriver ut toppvärde per sextondel. Slumpen
  är seedad: samma inställningar ger bit-identisk ljud. `p:id=värde` i presetfältet sätter parametrar.
- `export` gör samma sak som EXPORT-knappen.
- `snapshot` sparar en skärmbild av gränssnittet. Flik-numret är ensiffrigt för nedre panelen, tiotal
  för övre panelen, hundratal för EFFECTS-vyn (0 EQ, 1 COMP, 2 FX) och tusental 1 för pianorullen.
- `analyze` mäter en låt (eller en rendering): tempo, kickens svep, svans och envelope, basgången
  sextondel för sextondel och basens övertoner.
- `fitbeat` söker kick-, bas- och mixinställningar som låter som ett parti av en låt (analys
  genom syntes): ett medelslag per position i takten jämförs med vår kick + bas i tre band,
  kickens svep och basens övertoner. Skriver ut värdena i pluginets parametrar. Tar ~15 minuter.
- `comparebeat` mäter en rendering av ett preset på samma sätt och visar hur nära den kommer.

Referenslåtar läggs i `research/` (ignoreras av git och får inte checkas in).

## Struktur

```
Source/Core/        klocka, mönster, kick- och basmotorer, parametrar, presets (inget UI)
Source/Ui/          utseende, rattar, kurvredigerare, stegsekvenserare, moduler
Source/Instrument/  instrumentets processor och editor
Source/Fx/          FX-versionens processor och editor
Tests/              tester för kärnan, körs av build.bat
Tools/              KickbaseTool
```
