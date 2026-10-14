# MyOwnFruit – design

En VST3-synth för kick och rullande bas i psytrance. Kick och bas byggs i **samma plugin**
så att de kan låta perfekt ihop: exakt ducking, gemensam stämning och fasjustering ner på
sample-nivå, något som separata plugins inte kan göra.

Status: steg 1 provat i FL Studio. Steg 2–6 byggda natten 14–15 okt 2026 (gren
`natt-kick-och-bas`): automatiskt testade, renderade offline och validerade med pluginval,
men **inte provade i FL Studio än**.

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

- **Stegsekvenser** som FL Studios Channel Rack: 16 steg per takt, klicka ut slagen.
- **Syntetiserad kropp:** sinus med pitch-envelope (startfrekvens, kurva, tid) som landar
  på tonarten; amplitud-envelope; drive.
- **Klicklager:** syntetiserat klick, eller en inladdad sampling (WAV/AIFF/FLAC, dra och
  släpp) som spelas över de första millisekunderna.

## Bas

- **Stegsekvenser** med valbar upplösning per mönster: 1/16 (rullande, galopp), 1/8T
  (K-B-B per slag) eller 1/16T. Varje rutnät har sitt eget sparade mönster.
- **Per steg:** på/av, tonhöjd relativt tonarten (t.ex. +12), slide till nästa steg
  (303-stil), gate-längd, velocity.
- **Ljud:** oscillator → filter, där varje basslag har en egen kort filter-envelope (det
  plockiga psy-ljudet). Inställbar glide-tid.

## Kick + bas tillsammans

- Ducking av basen under kicken, sample-exakt, utan extern sidechain.
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

Fabrikspresets (ljud + mönster) finns i plugin:ets egen meny. De exponeras medvetet **inte**
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
7. **Nästa:** prova allt i FL Studio och finjustera ljudet efter öra.
