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
- **Kickens tail ska ligga på basens grundton.** [källa]

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

### I KickBase
- **Sidechain MODE = Kick envelope** följer kickens form automatiskt. Kurvan i UI:t ska visa att basen är nästan tyst under kickens kropp och är tillbaka till full nivå vid första B.
- **Note end = Cut** säkerställer att noter inte överlappar.
- **VEL-raden**: sänk första B efter varje kick för rullkänslan.
- **GATE-raden**: styr notlängden per steg. Variera för groove.
- **PITCH/SLIDE**: acid- och melodiska linjer.
- **KEY / inkommande MIDI**: kickens tail och basens grundton ska följa samma ton.

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
- Kickens tail-frekvens = basens grundton (±1 halvton).
- Ingen överlappning mellan basnoter (amp-envelopen når noll före nästa not).
- Bas under ~120 Hz är mono.
- Första B efter kicken är tydligt lägre i nivå än övriga (vid rullstil).
- Ingen fasutsläckning vid kick/bas-övergången: jämför sub-nivån med och utan bas.
- Kicken ≤ 1/8 vid aktuell BPM. Inga klick vid slutet av kicken.
- Spektral ljushet matchar stilen (lägre för E/F, högre för A/D).

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
