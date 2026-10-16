# Kickbase – design

En VST3-synth för kick och rullande bas i psytrance. Kick och bas byggs i **samma plugin**
så att de kan låta perfekt ihop: exakt ducking, gemensam stämning och fasjustering ner på
sample-nivå, något som separata plugins inte kan göra.

Produktnamn: **Kickbase** (instrument) och **Kickbase FX** (effekt), tillverkare TKDEV.
Kodnamnet i repot och i verktygen är fortfarande MyOwnFruit.

Status (16 okt 2026, gren `natt-kick-och-bas`): allt nedan finns och är renderat offline och
testat med enhetstesterna; plugin:et har körts i FL Studio (utseende och laddning), men
pianorullen, mixern och de senaste effekterna är **inte provade i FL Studio än**. Pluginval
ska köras om innan en release.

## Grundmodell

**DAW-tonen bestämmer tonart och när det låter. Plugin:et bestämmer vad som händer inuti.**

- **Tonart:** tonen i DAW:ens pianoroll (t.ex. C5) ger tonarten. Oktaven ignoreras; varje
  motor har en egen oktav/transponering (kickens svans kring C1, basen en oktav upp).
- **Grind:** så länge tonen hålls spelar plugin:et sitt mönster.
- **Synk:** mönstret följer DAW:ens tempo och **taktstreck** (via `AudioPlayHead`), inte
  tonens start. En slarvigt placerad ton eller start mitt i en takt ger ändå kick på slaget.
- **Mönsterlängd:** 1–4 takter. Mönstret loopar inom tonen (8 takters ton + 1 takts mönster
  = 8 varv). Längre förlopp och arrangemang sköts i DAW:en.
- **Kort ton:** om tonen tar slut innan mönstret går en inställning (settings) att välja:
  *klipp direkt* / *spela klart takten* / *spela klart mönstret*.

## Kick

- **Stegsekvens** som FL Studios Channel Rack: 16 steg per takt, klicka ut slagen.
- **Syntetiserad kropp** (som Kick 3): en sinus vars pitch och volym följer **fria envelopes**
  man ritar i en vågformsvy (noder med spänning; pitch upp till 0,5 s, volym upp till 2 s), plus
  övertoner, klick och drive. KEY = Follow stämmer svansen efter tonen (A#0-A1), Fixed håller
  den på C1 + TUNE.
- **Klicklager:** syntetiserat klick, eller en inladdad sampling (WAV/AIFF/FLAC, dra och
  släpp) som spelas över de första millisekunderna.

## Bas

- **Tre baslager**, vart och ett med eget mönster, eget ljud, egen ducking och egen effektkanal.
- **Toner** ligger fritt i tiden (96 ticks per fjärdedel): ett lager kan ha 16-delar, ett annat
  8-delar och ett tredje 64-delar. Stegsekvensen (rutnät 1/4 ... 1/64 och trioler) och
  **pianorullen** (som FL Studios: dra, förläng, ±24 halvtoner från tonarten, slide-triangel,
  velocity-remsa) redigerar samma toner. Pianorullens snap är lagrets GRID.
- **Per ton:** start, längd, tonhöjd, velocity och en glide-flagga: en glide-ton (som en slide-ton i FL Studio) slår inte
  an utan glider tonen som klingar till sin tonhöjd under sin egen längd.
- **GATE:** per lager en egen av/på-rytm (eget rutnät, djup, mjukhet) som hackar ljudet, oberoende av tonerna.
- **Ljud:** två oscillatorer per lager. Wavetables (Basic, PWM, Sync, Harmonics, FM, Growl, ...)
  eller klassiska vågformer med ett klick (SIN/TRI/SAW/SQR), med warp (sync, bend, PWM, mirror,
  FM), unison (≤ 7), detune, blend, fas, slumpfas, **pan och stereospridning per oscillator** →
  sub (samma oktav eller en ner) och brus → filter (LP24/LP12/band/high, keytrack) med en egen kort
  filter-envelope per ton (det plockiga psy-ljudet) → drive. ADSR och glide. Lagren är stereo.
- **LFO:er:** tre, tempo-synkade. Dra en LFO på valfri ratt (Massive-stil); ringen utanför
  ratten visar och styr mängden. Högerklick på en ratt ger DAW:ens meny (automation m.m.).
- **Lärdom från referenslåtarna:** riktiga psy-basar är mörkare än man tror (övertonerna från
  den femte och uppåt ligger 20-30 dB under grundtonen), tonerna fyller ofta hela steget och
  formas av amp-envelopen, och kickens svans ligger ofta 2-3 halvtoner över basens grundton.

## Kick + bas tillsammans

- Ducking av basen under kicken, sample-exakt, utan extern sidechain. Varje lager har ett eget
  djup (DUCK); kurvan är kickens envelope eller en egen ritad kurva. SIDECHAIN-fliken visar
  varje lagers ducking i sin färg, och en ring i DUCK-ratten visar hur mycket som duckas just nu.
- **FX-kanaler A–E:** kick och lager skickar (kanal + mängd, uppe i hörnet på respektive flik) till
  en kanal med fem platser (13 effekttyper), en alltid-närvarande EQ (parametrisk, med spektrum
  av ljudet som kommer in) och kompressor (med vågformsvy), nivå och en egen duckning under kicken.
- **MASTER-fliken:** en mixer med gain, pan och dB-mätare för varje källa (kick, lager 1-3,
  FX A-E) samt master, och brytarna SC (sidechain på/av) och MAX (genom maximizern eller förbi
  den). Maximizern har drive, clip och en limiter med lookahead; SOUNDSTAGE visar spektrum,
  stereobild (vektorskop), bredd, mono-under-frekvens och en stereoförstärkare.
- **Rendering är deterministisk:** slumpen (brus, slumpfas) är seedad, så samma inställningar
  ger bit-identisk ljud offline (viktigt för analysverktygen och en framtida renderloop).
- Kickens svans och basen stäms mot samma tonart.
- Fasjustering där kick och bas möts: basens oscillator startar i samma fas varje slag.
- Separata utgångar (kick / bas) så de kan mixas på egna kanaler. Huvudutgången har alltid
  hela mixen, så att inget blir tyst om DAW:en aktiverar alla utgångar.

## Ljud från en annan synth (effektversion)

Basljudet ska kunna komma från en annan synth (t.ex. Serum) som hackas upp av basmönstret.

I FL Studio kan plugins i Channel Rack (generatorer) inte ta emot ljud; det kan bara plugins
i mixerns effektplatser. Därför byggs samma kod som **två plugins**:

- **MyOwnFruit** (instrument, Channel Rack): kick + syntetiserad bas, tonart från DAW-tonen.
- **MyOwnFruit FX** (effekt, mixerkanalen där den andra synthen hamnar): synthens ljud är
  effektens ingång. FX:en gate:ar det med basmönstret, duckar det under sin egen kick och
  lägger till kicken. Tonarten ställs med en parameter (ingen MIDI behövs); tempo och
  position kommer från DAW:en som vanligt. Tonhöjd och glide kommer från den andra synthen.

Båda versionerna delar sequencer, kickmotor och ducking. Allt som ska låta perfekt ihop
finns alltså fortfarande i ett och samma plugin.

När DAW:en står still släpper FX-versionen igenom ljudet orört och spelar ingen kick (annars
skulle ett stoppat projekt loopa en kick i all oändlighet). Instrumentet går däremot på egen
klocka när en ton spelas med transporten stoppad, så att det kan provspelas.

## Utseende och känsla

Inspiration: Polyverse Manipulator (som Tobias har och gillar). Inspiration, inte kopia –
egen identitet, inga lånade logotyper eller grafik.

- Mörk blå/violett bakgrund, stora runda rattar med lysande färgade bågar och värdet i mitten.
- Funktioner i tydliga moduler med färgad rubrik och på/av-knapp (som Manipulators nedre rad).
- **Kurvor man drar i direkt:** envelopes (ADSR, pitch-envelope för kicken, filter-envelope)
  visas som grafer med handtag. Lätt att ställa in är viktigare än många rattar.
- Stegsekvenser som går att klicka och dra i, med tydlig markering av var uppspelningen är.
- Byggs som en egen JUCE `LookAndFeel` plus egna komponenter för kurvor och steg.

## Presets

Fabrikspresets togs bort medvetet när synten byggdes om (bara Init/Default finns); nya byggs
med `fitbeat` när synten är klar. Egna presets sparas som filer. Delarna har var sin presetmeny.
Menyn finns i plugin:ets egen meny. De exponeras medvetet **inte**
som VST3-"programs": en värd kan sätta program-parametern när ett projekt laddas och skulle
då skriva över det sparade ljudet. Mönsterlängden behålls när ett preset laddas.

## Senare / idéer

- One-shot-sampling som basljud (stäms om per steg; glide begränsad).
- Variationer (t.ex. fill var åttonde takt).
- Egna användarpresets (spara/ladda fil).
- Kickens pitch-envelope med fler punkter; LFO/modulering på basens filter.

## Beslut

- Inladdade klicksamplingar **bäddas in** i projektet (max 2 s, mono), så att projektet
  fungerar även om filen flyttas.
- Gränssnittets texter är på engelska (som i andra plugins); koden kommenteras på svenska.

## Byggordning

Varje steg ska gå att höra och testa för sig.

1. ✅ **Tempo-synk + två versioner** (provat i FL Studio).
2. ✅ **Kickmotor.**
3. ✅ **Kickens stegsekvenser** i gränssnittet.
4. ✅ **Basmotor** med stegsekvenser, tonhöjd/slide per steg och filter-envelope per slag.
5. ✅ **Kick + bas:** ducking efter kickens envelope, fas, stämning.
6. ✅ **Utbyggnad:** trioler, samplat klick, Note End, separata utgångar, presets.
7. ✅ **Egna presets** per del (fil per preset under Dokument\MyOwnFruit\Presets), fler rutnät.
8. ✅ **Effekter** (7 st, PRE/POST gate, valbar ordning) i en EFFECTS-flik.
9. ✅ **SIDECHAIN** (ritbar duckkurva) och **MASTER** (drive, clip, lookahead-limiter).
10. ✅ **LFO:er** (2 st, tempo-synkade, två mål var).
11. ✅ **Bassynt v2** (två oscillatorer, FM, filtertyper), kickens glide, analysverktyg och
    presets anpassade mot referenslåtar (`fitbeat`), WAV-export.
12. ✅ **Kickbase:** tre baslager, wavetables, Kick 3-lik kick, effektkanaler A–E med EQ och
    kompressor, brushed-metal-tema, live-scopes, mixer, SOUNDSTAGE, pianorulle, stereo per oscillator,
    Echo efter FL:s Fruity Delay 2, seedad slump.
13. **Nästa:** WAV-import till wavetables (Serum-stil) och ett tydligare wavetable-gränssnitt;
    nya fabrikspresets med `fitbeat`; pluginval och Release-bygge; prova allt i FL Studio och
    finjustera efter öra. Därefter ljuddesign-skill, renderloop och AI-styrning
    (se `docs/sound-design-roadmap.md`).
