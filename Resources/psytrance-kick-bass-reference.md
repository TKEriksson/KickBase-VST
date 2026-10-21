# Psytrance kick & bas – referens för presetdesign i KickBase

Kunskapsbas för agenten när den skapar presets och mönster. Sammanställd från guider, forumtrådar och artistmaterial (se källor längst ner) plus etablerad praxis.

**Så här ska agenten använda filen**
- Värden markerade **[källa]** kommer från källorna. Värden markerade **[startvärde]** är rimliga utgångspunkter från allmän praxis, inte uppmätta från någon specifik låt. Verifiera alltid med renderloopen (fas 3) och Tobbes öron.
- Ljudet avgörs av helheten kick + bas + mönster + sidechain. Designa dem tillsammans, aldrig var för sig.
- Inget här är en mall att kopiera rakt av. Gorovich gör varje ljud från grunden för varje låt. Använd recepten som startpunkter och variera.

---

## 1. Kicken

### Anatomi
1. **Click/transient** (några ms): definierar attacken. Kan vara pitch-svepets början, ett kort brusburst eller ett lager med kort sampel/hi-hat. [källa]
2. **Pitch-svep**: sinus som faller snabbt från hög frekvens till grundtonen. [källa]
3. **Body**: området ~100–200 Hz. Övertoner (2:a/3:e partial) ger mer kropp. [källa]
4. **Tail/sub**: den utklingande grundtonen. Kickens tonart bestäms av den längsta, mest utdragna tonhöjden. [källa]

### Regler
- **Längd högst en åttondel** så att basen får plats. Vid 144 BPM motsvarar en åttondel ungefär 208 ms. [källa]
- **Klassisk vs modern**: ett långsammare pitch-svep ger en mer utpräglad attack som i tidig psytrance. Moderna kickar sveper oftast snabbare. [källa]
- **"Fishtail"-envelope**: sänk volymen kort direkt efter transienten och låt den sedan komma tillbaka. Det tar bort det "träiga" området runt 200–600 Hz. [källa]
- **Fasreset vid varje slag** så att alla kickar blir identiska. [källa]
- **Lätt mättnad** och försiktig gain staging. Varken click eller tail ska krossas. [källa]
- **Sista envelope-punkten måste nå noll** för att undvika klick mellan slagen. [källa]
- **Kortare kick → längre basnoter är möjliga**, och tvärtom. [källa]
- **Ingen universell kick/bas-stämning:** källorna föreslår både samma grundton/oktav och en kicksvans några halvtoner ovanför basen. Börja i samma tonart, jämför några alternativ på samma fras och behåll det som ger tydlig attack och stabil low-end. Ändra inte tonhöjd automatiskt utifrån en enda regel. [källa]

### Startvärden [startvärde]
| Parameter | Utgångspunkt | Kommentar |
|---|---|---|
| Grundton | F1–G1 (~44–49 Hz) | G1 ≈ 49 Hz används i en källa [källa]; F/G kallas klassisk psy-frekvens i en annan [källa] |
| Svepets startfrekvens | 1–4 kHz | Högre = mer click |
| Svepets tid till grundton | 30–80 ms | Kortare = modern och tight, längre = klassisk och "boingig" |
| Fishtail-dip | −3 till −9 dB, ~10–30 ms efter transienten | |
| Total längd | 150–210 ms vid 140–145 BPM | ≤ 1/8 |
| Mättnad | lätt, 5–20 % | |

---

## 2. Basen

### Grundmönstret: K B B B
- Kick på varje fjärdedel, sedan tre sextondelsnoter bas, var och en en sextondel lång. [källa]
- **Rullkänslan**: sänk volymen på första basnoten efter kicken så att örat inte uppfattar den som taktens början. Hur mycket är ett konstnärligt val. [källa]
- **Ingen överlappning**: varje not ska tystna innan nästa börjar, helst kortare än en sextondel. [källa]
- Över ~140 BPM blir sextondelsbasar svåra att få tydliga. Det kräver extremt korta, distinkta envelopes. [källa]

### Ljudet
- **Oscillator**: saw (övertonsrik), eventuellt blandad med fyrkant. Retrig vid varje not. [källa]
- **Amp-envelope**: omedelbar attack, ingen decay, full sustain, mycket kort release. [källa]
- **Filter**: lågpass med låg cutoff som envelopen öppnar snabbt och stänger inom några millisekunder, vilket ger plucket. Använd decay, inte release. [källa]
- **Klassisk vs modern**: klassiska basar har mindre konsekvent oscillatorfas, mjukare attack och långsammare filterenvelope, vilket låter mer organiskt. Moderna basar har konstant fas och därmed identisk attack på varje not. [källa]
- **Mono i botten**: bredd läggs bara parallellt och högpassat, så att stereoinnehållet hålls borta från grundtonen. [källa]
- **FM-bas** är ett alternativ till saw + filter, med kort filter- och amp-decay och mono. [källa]
- **Liv över tid**: små variationer per steg och långsamma LFO:er på parametrar. [källa]

### Startvärden [startvärde]
| Parameter | Utgångspunkt |
|---|---|
| Filter cutoff (bas) | 150–400 Hz |
| Filter env amount | +2 till +4 oktaver |
| Filter env decay | 30–90 ms (kortare vid högre BPM) |
| Resonans | 10–30 % (högre för acid) |
| Gate | 40–80 % av en sextondel |
| Första B efter kick | 60–85 % velocity |
| Drive | lätt till måttlig, före filtret för rundare, efter för aggressivare |

### Rytmvarianter (inte bara rullande)
| Mönster | Beskrivning | Typisk stil |
|---|---|---|
| **K B B B** | klassisk rullande | full-on, twilight [källa] |
| **Offbeat** | en åttondelsnot mellan kickarna, som klingar en åttondel | progressive psytrance [källa] |
| **Triol/galopp** | tre noter på en fjärdedel i triol | psytrance generellt [källa] |
| **Swing** | sextondelar med shuffle | groove-orienterad psy [källa] |
| **K BB** eller **K B _ B** | luckor i rullet för groove | hypnotiskt och minimalt [startvärde] |
| **1/32-rull** | dubbelt så tätt, mycket korta noter | dark, forest, hi-tech [startvärde] |
| **Acid-linje** | 303-liknande med slide, accent och tonhöjdsrörelse | 90-talspsy och goa-influerat [startvärde] |

---

## 3. Samspelet kick + bas

- **Kritisk punkt**: där kickens tail möter basens första sextondel. [källa]
- **Fasutsläckning åtgärdas** genom att invertera fasen på ett element, ta bort grundtonen på första basnoten (låg-cut eller i vågformen), justera tempot så att basen träffar en annan punkt i tailen, eller fasjustera runt grundtonen. Även ett lågcut vid ~18 Hz kan förskjuta fasen tillräckligt. [källa]
- **Ett dominant sub-element**: oftast bär kicken suben och basen fyller låga mellanregistret. [källa]
- **Skjut basens start några ms** för att separera attackerna. [källa]
- **Ducking**: basen måste duckas under kicken, via sidechain eller via att noterna helt enkelt inte överlappar. [källa]

### Tre kick-tail-alternativ: välj med A/B-test

Källorna ger tre rimliga mål; inget kan väljas säkert från en frekvensregel ensam.

1. **Samma grundton/oktav:** använd `kickKeyFollow=Follow` och ställ `kickTune` så att uppmätt tail matchar basroten. Prova först när en stadig tonreferens och tydlig harmonisk koppling är viktigast.
2. **Några halvtoner ovanför:** behåll Follow och prova `kickTune` cirka +2 och +3 semitoner mot samma basmönster. Behåll bara om transienten blir tydligare och low-end fortfarande låter stabil; källan föreslår även ca 45–50 Hz, men det är inte ett facit.
3. **Oberoende sweet spot:** sätt `kickKeyFollow=Fixed` och prova ett valt tailområde oberoende av inkommande MIDI. Det kan passa en kick som ska vara konstant genom melodiska basnoter; kontrollera att den inte skapar tonarts- eller lågändekrockar i varje relevant not.

**Samma MCP-test för alla tre:** spara `get_state`, använd samma tempo, MIDI-not, mönster, bars och seedade render, ändra bara `kickKeyFollow`/`kickTune`, kör `render_preview` hel/kick/bass och `analyse_mix`, jämför tail-Hz, spektrum, peak/RMS och limiterreduktion. `undo` eller återställ state mellan kandidaterna. Be användaren lyssna på nivåmatchade varianter. Verktygen kan mäta tonhöjd/överlapp men kan inte avgöra vilken fasvariant låter bäst eller ens bevisa fasutsläckning.

### I KickBase
- **Sidechain MODE = Kick envelope** följer kickens form automatiskt. Kurvan i UI:t ska visa att basen är nästan tyst under kickens kropp och är tillbaka till full nivå vid första B.
- **Note end = Cut** säkerställer att noter inte överlappar.
- **VEL-raden**: sänk första B efter varje kick för rullkänslan.
- **GATE-raden**: styr notlängden per steg. Variera för groove.
- **PITCH/SLIDE**: acid- och melodiska linjer.
- **KEY / inkommande MIDI** ger en gemensam tonartsreferens. Tail och basgrundton kan stämmas lika, men prova även små intervallskillnader; se avsnitt 3 eftersom källorna inte är ense om ett universellt mål.

---

## 4. Subgenrer

| Stil | BPM | Kick/bas-karaktär |
|---|---|---|
| Psytrance generellt | 125–150 [källa] | ihållande, pumpande bas genom hela låten [källa] |
| Full-on | 140–148 [källa] | energisk, melodisk och krispig bas [källa]; tight KBBB |
| Twilight/night full-on | ~142–148 [startvärde] | djärvare, lägre basnoter [källa] |
| Progressive psy | 132–140 [startvärde] | minimalistisk, med rötter i minimal techno [källa]; offbeat-bas eller mjukare roll |
| Dark psy | 150+ [källa] | tunga basar och mörka ljud [källa]; ofta 1/32 och mer distortion [startvärde] |
| Forest | 145–160 [startvärde] | organisk, knastrig, mycket rörelse i filtret [startvärde] |
| Hi-tech | 170–200+ [startvärde] | extremt korta noter och 1/32 [startvärde] |
| Zenonesque / psy-techno | lägre tempo [källa] | djup och mörk, techno-element [källa] |

---

## 5. Stilprofil: Dekel, Gorovich och Empirikal

### Fakta
- **Empirikal** var en israelisk duo som Nimrod Gorovich och Dekel Kamiosky bildade 2013. De blandade new wave med progressiv och uplifting psytrance, med influenser från Simon Posford, LOUD, Infected Mushroom och Psysex, samt tribal- och naturelement. Exempel: *Psy Spy* (143 BPM, B-moll) och *Changa* (142 BPM) på TIP Records 2015. [källa]
- **Gorovich**: började trumma som nioåring och har bakgrund som filmkompositör. Inspirerad av 90-talstrance som X-Dream, Hallucinogen, Synchro, Midimiliz, Organic Noise och Prometheus. Skapar varje kick, trumma och ljud från grunden för just det ögonblick det används i, genom att spela in sina maskiner. Debutalbumet *Vortex* kom 2019 på Iboga Records. [källa]
- **Dekel**: blandar världsrytmer med analoga och modulära syntar till en "elektro-hypnotisk" känsla. Influerad av Eitan Reiter, Simon Posford, Union Jack och Cosma. Debuterade på HOMmega Records, senare Stereo Society. [källa]
- **Data** (mixgraph): Dekels median-BPM är 133 och Gorovichs 138. Båda skriver mest i dur. Grooven är hög och energin måttlig jämfört med psytrance i stort. Dekel har ovanligt låg ljushet för genren. [källa]

### Tolkning till ljuddesign [startvärde]
Nedanstående är tolkningar av fakta ovan, inte uppmätta värden.
- **Groove före energi**: hypnotiskt snarare än aggressivt. Använd velocity- och gate-variationer, luckor i rullet och swing i stället för maximal drive.
- **Mörkare topp**: lägre filter-cutoff och mindre övre mellanregister än typisk full-on. Rundare, fetare basar.
- **Perkussiv bas**: trummisbakgrunden syns i basar som beter sig som slagverk, med korta och dynamiska noter och accenter på oväntade steg.
- **90-talsarv**: acid-resonans, slides, lite ofasad och organisk bas (fri fas), modulerade filter som långsamt rör sig över många takter.
- **Tempo 130–140**, utrymme för långa utvecklingar.
- **Unika ljud**: lägg till liten slumpmässig variation per preset så att två presets i samma kategori aldrig låter identiska.

---

## 6. Receptkort [startvärde]

Alla värden är utgångspunkter som ska verifieras via render.

**A. Klassisk full-on roll (145 BPM)**
Kick: snabbt svep, ~180 ms, tydlig click. Bas: saw, KBBB, cutoff ~250 Hz, env decay ~45 ms, gate 60 %, första B 75 % velocity, konstant fas. Sidechain: kick envelope.

**B. Modern progressive offbeat (136 BPM)**
Kick: rund, ~200 ms, lite click. Bas: offbeat-åttondelar, mjukare filter (decay ~120 ms), lätt drive, mono. Sidechain: kick envelope, djup.

**C. Galopp/triol (140 BPM)**
Bas: triolmönster, varannan not med lägre velocity, kort gate (~45 %). Kick tight, ~170 ms.

**D. Twilight/night (146 BPM)**
Bas: lägre oktav, mer drive, resonans ~25 %, KBBB med gatevariation. Kick: snabbt svep, hårdare mättnad.

**E. Hypnotisk groove i Dekel/Empirikal-anda (134 BPM)**
Bas: KBBB med luckor (K B _ B, K B B _), swing ~10–15 %, låg cutoff (~180 Hz), måttlig resonans, långsam LFO på cutoff över 8–16 takter. Kick: rund, varm, lite click. Dur-tonart.

**F. 90-tals-acid i Gorovich-anda (138 BPM)**
Bas 1: rullande grund. Bas 2: acid-linje med slide och accent, resonans 50–70 %, envelope mod hög. Fri oscillatorfas för organisk känsla. Kick: klassiskt långsammare svep.

**G. Dark/forest nervös (152 BPM)**
Bas: 1/32-rull, mycket korta noter (gate ~35 %), snabb env decay (~25 ms), drive före filter, LFO på cutoff. Kick: kort (~140 ms), hård.

---

## 7. Verifieringschecklista för renderloopen
- Kontrollera att kicksvans och bas är tonalt rimliga; jämför samma ton/oktav med ±2–3 halvtoner som test, inte som facit.
- Ingen överlappning mellan basnoter (amp-envelopen når noll före nästa not).
- Bas under ~120 Hz är mono.
- Första B efter kicken är tydligt lägre i nivå än övriga (vid rullstil).
- Kontrollera fas/timing separat: jämför kick solo, bas solo och summan i mono. Spektrumrapporten bevisar inte fasutsläckning.
- Kicken ≤ 1/8 vid aktuell BPM. Inga klick vid slutet av kicken.
- Spektral ljushet matchar stilen (lägre för E/F, högre för A/D).

---

## 8. Sammanvägd kunskap från researchguiderna

Transkriptionerna i [research/TEST026-10-19](../research/TEST026-10-19/) och referens-PDF:erna ger användbara arbetsmetoder, men inte ett enda standardljud. Tids- och frekvensvärden är startpunkter; materialet motsäger sig självt, och lyssning i den aktuella mixen väger tyngre.

### Det som återkommer

- **Bygg kick och bas som ett par.** Börja med en kick vars transient och svans fungerar i rytmen, lägg sedan basen i luckorna och jämför dem både separat och tillsammans. Kolla flera slag: en bra enskild transient räcker inte om low-end varierar eller överbelastar mastern.
- **Forma basnoten i tid.** En saw/öppen oscillator med filter-envelope kan ge övertoner i anslaget och en stabilare kropp efteråt. Anpassa attack, decay, sustain, release och notlängd efter BPM och nästa kick/not; kort decay är pluckigare, längre håll ger mer kropp men ökar risken för överlapp.
- **Fas och timing är ett lyssnings-/mätproblem, inte ett diagrammål.** Stabil oscillatorfas kan göra varje anslag mer konsekvent. Ändra en sak i taget (fas, starttid eller tonhöjd), nivåmatcha A/B och lyssna i mono. En vågformsbild som ser jämnare ut garanterar inte mer punch.
- **Rensa före EQ.** Muta lager och bypassa effekter för att hitta vad som faktiskt orsakar grumlighet. Gör sedan små cuts/boosts som löser en hörbar eller mätbar konflikt; undvik att EQ:a varje spår reflexmässigt.
- **Låt reverb, delay och bredd ligga utanför subben.** Filtrera effektreturer och håll kickens/subbens kärna centrerad; prova bredd på högre innehåll och kontrollera mono. Pre-delay (ca 20–50 ms i en källa) kan låta transienten komma fram före reverb, men är ingen obligatorisk inställning.
- **Skilj peak från loudness.** Tung buss-/masterkompression eller limiting kan platta till kickens transient och göra low-end mindre stabil. Nivåmatcha före/efter, kontrollera sample peak/RMS och limiterreduktion och sänk källor innan du försöker lösa allt med mer masterdrive.
- **Kompressionsdetektorn är inte ljudvägen.** Ett high-pass i en extern kompressors sidechain kan hindra subben från att styra kompressionen utan att filtrera själva ljudet. Kickbase Master exponerar inte den inställningen; föreslå den bara som DAW-/externkompressor-lösning.
- **Stil styr prioriteringar, inte fysiken.** Full-on brukar gynnas av tydlig/tight rullning; progressive kan lämna mer utrymme och använda offbeat; acid/squelch får karaktär av rörlig cutoff och resonans; forest/dark kan tåla mer rörelse och grit. Välj rytm, spektrum och mängd distorsion efter målet, inte etiketten ensam.
- **Pan är inte alltid stereopanorering.** En balanspan kan bara ändra vänster/höger-nivå medan en stereopan flyttar hela stereobilden. Jämför i mono och var försiktig med stereobredd i låga frekvenser.

### Kompakt felsökningskarta

| Symptom | Kontrollera först | Liten, testbar åtgärd |
|---|---|---|
| Kicken försvinner eller low-end varierar | Kick och bas separat/summa; timing, fas, notöverlapp, DUCK och limiterreduktion | Nivåmatcha A/B; justera en fas-/timingvariabel eller ducking i taget. Retuna inte utan att jämföra. |
| Basanslaget varierar mellan noter | Oscillatorns fasstart/random, envelope, filterrörelse, noter som överlappar och FX | Bypassa processning stegvis; prova fast fas och justera amp/filter-envelope. |
| Grötigt trots mycket EQ | För många lager, långa FX-returer, överlappande noter | Muta/bypassa först; korta not/FX-tail eller sänk en källa före smala EQ-ingrepp. |
| Kick/bas klipper eller mastern pumpar | Sample peak, RMS, limiterreduktion, summa över flera slag | Sänk den dominerande källan eller mixernivåerna; minska överlapp/duckproblem före mer limiting. |
| Squelch/acid är statisk eller vass | Cutoff-envelope/LFO, resonans, drive och grundtonens renhet | Lägg till måttlig filterrörelse/resonans; jämför drive före/efter filter och skydda subben. |
| Stereo tappar kropp i mono | Pan-typ, stereobredd och lågfrekvent sidoinnehåll | Behåll kick/sub centrerad; begränsa bredden längst ner och kontrollera i mono. |

### Så används detta med KickBase

1. Hämta `get_guide` för både `agent` och `psytrance`, läs nuläget med `get_state`, och utgå från ett befintligt preset om det passar.
2. För ett objektivt mixproblem: `analyse_mix`, invänta `get_mix_report` och läs först findings. Begär bara relevanta källor/barer/harmoniker/dynamics/synth/frame-data efteråt. Jämför `render_preview` för helheten och `part="kick"` / `part="bass"` för isolerade källor.
3. Välj en minimal ändring med parametrar som faktiskt finns: kick `kickTune`/`kickKeyFollow`/envelopes; lager `bassNTune`, `bassNOctave`, `bassNDuck`, `bassNEq...`; mönster `set_pattern`/`set_gate`; duckkurva `set_sidechain_curve`; FX `set_effect`. Läs intervall och hjälptext via `list_parameters` / `list_effects`.
4. Kör samma analys/render igen. Skriv i rapporten vad som observerades, möjliga orsaker, säkerhet, exakt ändring och mätresultat. Använd `add_report_note`; märk inte ett förslag som tillämpat.
5. Säg när bara mätvärden har granskats och be användaren lyssna innan en smakbedömning eller sparat preset godkänns.

**Begränsningar att alltid redovisa:** KickBase-analysen mäter mono och har ca 171 ms tidsfönster; den kan därför inte bevisa stereo-/fasproblem eller beskriva mycket korta transienter exakt. Instrumentkällorna mäts före FX-sändning (FX-tails syns i Master), och externa DAW-spår/automation ingår inte. Föreslå inte en automatisk fasfix, aliasingdiagnos eller stereo-fix utan data som mäter det.

### Källspår och nya idéer

- **Transkript (21 filer):**

| Fil | Komprimerad takeaway |
|---|---|
| `7 Rules of Psytrance - Psytrance Tutorial Ableton.txt` | Bygg arrangement och kontrast runt samspelet kick/bas; optimera inte bara enskilda ljud. |
| `Every Genre's Kick Drum Explained In Detail.txt` | Kickens ton, transient och svans ska följa genrens roll; en kickmodell passar inte allt. |
| `Every song with HEAVY LOW END should use this compression trick.txt` | HPF i kompressorns detector-sidechain kan hindra subben från att styra kompression utan att filtrera ljudvägen. |
| `Everything YOU Need to Know about Psytrance Squelches!.txt` | Squelch formas av rörlig resonant filtrering; envelope och modulation är centrala. |
| `Everything you NEED to know about Phase Allignment for Kick & Bass!.txt` | Bedöm fasjustering med kontrollerade jämförelser och lyssning, inte bara vågformsbild. |
| `How to NAIL your Kick & Bass Everytime!.txt` | Välj kick först och forma basens envelope/frekvensutrymme efter den; kontrollera ihop. |
| `How to make Interesting Yet Understandable Psytrance Tracks!.txt` | Variation och lager ska utveckla energi/kontrast utan att göra helheten överlastad. |
| `How to make Psytrance basslines 10 different types.txt` | Välj rytmisk/klanglig basvariant efter groove och stil; lås inte AI:n till KBBB. |
| `Instant Clarity + Loudness In Your Mix & Master (this is how).txt` | Tydlighet/loudness börjar med balans och gain staging; limiting löser inte tidigare problem. |
| `Is your Psytrance Bassline clean Here is how to fix that!.txt` | Jämför transienter/vågformer för att undersöka ojämn bas, men bekräfta med hörsel. |
| `Over EQ’ing Your Mix The Perspective Shift You’re Missing.txt` | Vid grötighet kontrollera lager, arrangemang och nivå innan fler EQ-ingrepp. |
| `Producing a Track From Scratch in 99 MINUTES  Melodic Techno & House.txt` | Generellt arbetsflödesexempel från annan genre; inte en källa till specifika psytrance-parametervärden. |
| `Properly Tweaking Psytrance Kicks with Scorb and Divination.txt` | Pitchkurva, transient/klick och svans är separata kickdesignfrågor; lyssna i tempo. |
| `Psytrance Kick & Bass - Frequency, Phase and The Sweet Spot.txt` | Föreslår ca 45–50 Hz men avvisar universell stämning/faslinjering; värdena är en hypotes att prova. |
| `Strong Prog Basses like Omiki, Blastoyz & Interactive Noise!!!.txt` | Progressive-bas behöver inte följa strikt rullande standard; låt referens och groove styra. |
| `The LIKELY Reason Why Your Kick & Bass STILL Don’t Compare To The Pros.txt` | Kontrollera kick/bas-fas och ducking; maximera inte vågformsamplitud utan att lyssna. |
| `Unfck Your Master (Limiters Are KILLING Your Low End).txt` | Om limitering skadar low-end, felsök källnivåer/transienter innan hårdare masterprocessing. |
| `What is causing this Kick & Bass to Sound Bad.txt` | Ta bort processning ett led i taget; överprocessning kan förvärra transient/peak-problem. |
| `You’ve Been Doing It Wrong…HOW TO PAN (the correct way).txt` | Skilj balanspan från stereopan; kontrollera stereoljud i mono. |
| `guidesonline.txt.txt` | Källkarta för kick/bas, syntes, squelch, stereo, arrangemang och effekter; inte en sammanhängande metod. |
| `tips.txt` | Samlar riktvärden för low-end, envelopes, mono och FX; flera värden motsäger andra guider. |

- **PDF-sammanställningar:**

| Fil | Sidor | Takeaway |
|---|---:|---|
| `Psytrance Mixing & Effects Reference Compendium.pdf` | 3 | Föreslår filtrerade FX-returer, ca 20–50 ms pre-delay och mono kick/sub; riktlinjer, inte regler. |
| `Psytrance Sound Design Reference Guide.pdf` | 1–3 | Kick/bas-frekvensdelning och nudge; saw + lågpass/envelope som basstart; FM/squelch/lead-idéer. |

- **Markdown/specs (7 filer):**

| Fil | Typ | Takeaway |
|---|---|---|
| `BEZIER.md` | Produkt/UI | Bézierkurvor för pitch/transient och analyzer-vyer är idéer, inte befintliga kontroller. |
| `CODEMUSIC.md` | Produkt/UI | Föreslår Live Code som textstyrning av modulering, sekvenser och FX (AI/MCP-vision). |
| `KickAndBaseSchool.md` | Produktion | Praktiska tumregler om kick/bas, mono, envelopes och FX; värdena är startpunkter. |
| `MERIDÉER.md` | Produkt/UI | Idéer om low-end shaper, FM-squelch/lead och psytrance-FX; inte godkända funktionskrav. |
| `layout_optimization_osc_presets_spec.md` | Produkt/UI | Fristående OSC-presets och flikomläggning skulle kunna låta AI återanvända oscillatorljud separat. |
| `pattern_and_ai_ui_improvements_spec.md` | Produkt/UI | Skiljer MCP-server från AI-chatt och föreslår kollisionsvarningar; severity-gränserna är inte validerade standarder. |
| `sample_slicer_independent_layer_spec.md` | Produkt/UI | Föreslår sample blend/separat rad, slicer-pianoroll och filter-bypass per källa. |

- `chatthistory.md` är arbets-/chattlogg, inte slutlig specifikation; dess filterarbete/testresultat är blandat med ostrukturerad logg. Dra inte därifrån en fristående ljudregel.
- **Visuella mockuper** visar PULSES-liknande pitch/amp-kurvor över kickvågform, separata Pitch/Amp/Click-vyer, kick-/basvågformsöverlagring, Bézierkurvor och LFO-rate/snap/drag-to-target (`image-13.png`–`image-30.png`, `psylab.bmp`, `image-34.png`–`image-35.png`, `quadraticubic psylab.png`, `bezier_smooth_med2_slow.gif`, `image-1.png`, `image-2.png`, `image.png`). De ger AI:n visuellt vokabulär för *vilken kurva/modulation* som avses; de bevisar inte att mockupens kontroller finns i KickBase.
- **Produktmockuper** visar tänkta AI SERVER (MCP) och AI CHATT-lägen, OSC-presets, SAMPLE/FILTER-flikar, slicer-pianoroll och modulationsring (`PATTERN-FLIK FÖRBÄTTRINGAR SAMT ÄNDRAD AI KNAPP.png`, `FÖRSLAG_FLIKAR_OCH_INFÖRA_OSC_PRESETS.jpg`, `MINDMAP 2.png`, `WARP ENV VISUELLT I WARP AMOUNT.png`). Behandla som idéer, inte aktuell funktionalitet.
- [Skärmbilden från TEST2026-10-20](../research/TEST2026-10-20/plugintips_with_instant_phase_shift.bmp) visar en kommentar om en extern additiv synth där partialers pitch, nivå och fas styrs separat i realtid. Det är en möjlig framtida differentierande idé; KickBase har inte denna separata partialkontroll i nuläget.
- Länkade sidor som gick att läsa gav kompletterande sammanhang från Myloops, Daniel Sokolovskiy, CineTrance, SoundBridge, Melodigging, Psytrance Blueprint, FX23, Attack Magazine, MusicRadar och Waves. YouTube-länkarna svarade men videoinnehållet gick inte att läsa; några andra länkar gav 403/404. Se därför transkriptionerna och sidornas faktiska innehåll som olika evidensnivåer, inte dubbel verifiering.

---

## 9. Mät först, gissa aldrig

**Det nuvarande MCP-underlaget kan belägga:**
- `analyse_mix` / `get_mix_report`: samtidiga frekvensöverlapp, maskering av kick, kick-tail som ligger utanför valt område, tonhöjdsskillnader, energi under 30 Hz, limiterreduktion och nivåskillnader mot en referens. Resultaten är heuristiska, per källa/takt och efter duckning/EQ.
- `render_preview`: sample peak, RMS, DC, spektral centroid, bandenergi, stereomått, kicklängd/tail, checks och spektrogram för den renderade instansen. Använd `part="kick"` / `part="bass"` för att isolera källorna.
- `get_mix_report` kan även ge noter, modulation, effekter och mönstertidslinje så att mätfynd kopplas till parametrar och slag.

**Detta är inte bevis för:** fasutsläckning eller bästa fasläge; stereokorrelation per frekvensband; aliasing; hur det låter för en människa; eller vad externa DAW-spår/automation gör. FFT-fönstret är cirka 171 ms, instrument mäts före sina FX-sändningar och energin i ett frekvensband är inte sample peak/clipping. Säg uttryckligen när orsak eller mätning saknas.

---

## Källor
- [Psytrance produzieren Workshop #1 – bonedo.de](https://www.bonedo.de/artikel/psytrance-produzieren-workshop-1-kick-bass-percussion/)
- [Psytrance kick bass parameters – Elektronauts](https://www.elektronauts.com/t/psytrance-kick-bass-parameters/253875)
- [Psytrance bassline + kick – Elektronauts](https://www.elektronauts.com/t/psytrance-bassline-kick/214592)
- [ShapeMaster PsyTrance Kick patch – VCV Rack Community](https://community.vcvrack.com/t/shapemaster-psytrance-kick-patch/14414)
- [Ultimate guide: Sonic Academy Kick 2 – Audiotent](https://www.audiotent.com/blogs/production-tips/production-tipsultimate-guide-sonic-academy-kick-2)
- [Sonic Academy Kick 3 review – MusicTech](https://musictech.com/reviews/plug-ins/sonic-academy-kick-3-review/)
- [Making psytrance bass on Digitone 2 – r/Elektron](https://pol1.lr.ggtyler.dev/r/Elektron/comments/1i9q518/making_psytrance_bass_on_digitone_2)
- [How to make psytrance sounds – Soundcy](https://soundcy.com/article/how-to-make-psytrance-sounds)
- [Timing, tuning and transient tactics for kick and bass – Future Music](https://www.pressreader.com/australia/future-music-9629/20170921/282471414025334)
- [Psychedelic trance – Wikipedia](https://en.wikipedia.org/wiki/Full_On)
- [List of trance genres – Wikipedia](https://en.wikipedia.org/wiki/List_of_trance_genres)
- [Into the Vortex with Gorovich – Trancentral](https://trancentral.tv/2019/07/into-the-vortext-with-gorovich-interview/)
- [Gorovich – Vortex (Iboga Records)](https://iboga-beatspace.bandcamp.com/album/vortex)
- [Gorovich – mixgraph](https://www.mixgraph.io/artists/gorovich)
- [Get to know DEKEL – IDM Magazine](https://idmmag.com/featured/dekel-vortex-phoenix-festival-of-fire/)
- [DEKEL – Shamanic Groove](https://shamanic-groove.com/artists/dekel/)
- [Dekel – mixgraph](https://www.mixgraph.io/artists/dekel)
- [Empirikal – Psy Spy EP (TIP Records)](https://tiprecords.com/promo/Psy_Spy_Ep/)

### Tillkommande researchlänkar (kontrollerade vid sammanställningen)
- [Myloops – kick och bas tillsammans](https://www.myloops.net/how-to-make-psytrance-kick-and-bass-work-together)
- [Myloops – arrangera en psytrance-låt](https://www.myloops.net/arranging-a-psytrance-track-from-start-to-finish)
- [Daniel Sokolovskiy – psytrance bassline synthesis](https://dsokolovskiy.com/blog/all/psytrance-bassline-synthesis/)
- [CineTrance – psytrance kick process](https://cinetrance-records.com/blogs/cinetrance-blog/psytrance-kick-process)
- [SoundBridge – kick/bass phase](https://www.soundbridge.io/fr/bind-kick-bass-pha-979)
- [Melodigging – progressive psytrance](https://www.melodigging.com/genre/progressive-psytrance)
- [Attack Magazine – acid synth lines](https://www.attackmagazine.com/technique/making-acid-synth-lines-psytrance/)
- [Psytrance Blueprint – tutorials](https://psytrance-blueprint.com/tutorials/)
- [FX23 – Psylab Pro](https://fx23.net/psylab-pro/)

## Saknade mätningar

Varje framtida slutsats ska visa mätdata eller uttryckligen säga **SAKNAS**. Förslag till nästa analysator-/renderloopsteg:

| Saknad mätning | Verktygsidé |
|---|---|
| **SAKNAS →** fas/korrelation mellan kick och bas vid första basslaget | Rendera källorna sample-synkat; visa fasvinkel och korrelation i subband, vid varje kick och efter duckning. Jämför före/efter-fix utan att automatiskt välja fas. |
| **SAKNAS →** stereobredd och mono-kompatibilitet per frekvensband | Beräkna M/S-nivåer och L/R-korrelation per band och takt; varna endast vid tydligt mätbart sub- eller mono-problem. |
| **SAKNAS →** transienter kortare än nuvarande FFT-fönster | Lägg till högupplöst envelope/peak-tidslinje och klick-/ploppdetektion med millisekundzoom; behåll FFT-vyn för tonbalans. |
| **SAKNAS →** aliasing från warp, FM, drive och distortion | Offline-test med frekvenssvep/rena partialer och jämförelse mellan samplingsfrekvens/översampling; rapportera endast vikta spegelkomponenter som faktiskt mätts. |
| **SAKNAS →** FX-returer per kanal och per ursprungskälla | Mät pre/post-send-signaler separat så att echo/reverb/sidechain-FX kan kopplas till den källa som orsakar svansen eller maskeringen. |
| **SAKNAS →** externa DAW-spår och automation i samma analys | Erbjud host-bounce/multitrack-import med tidslinje så att pluginrapportens interna analys kan jämföras med hela arrangemanget. |
| **SAKNAS →** integrerad loudness och true peak | Lägg till LUFS-I/short-term, true-peak och crest per källa/master; skilj dem från RMS och bandenergi. |
