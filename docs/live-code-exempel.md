# Live Code: exempel att prova

Skrivet av projektledaren 21 okt. **Ej provkört av mig** (inga ljudtester av exemplen, bara läst koden `Source/Core/LiveCode.cpp` och `Resources/strudel/glue.js`). Om ett exempel ger en felrad, skriv upp raden och exemplet så rättar vi det.

## Så fungerar mappningen

- Koden är Strudel (JavaScript, mini-notation). Skriv den på LIVE CODE-sidan i instrumentet (`Source/Ui/LiveCodePage.cpp`). Sidan läser 4 cykler, 1 cykel = 1 takt (384 tick).
- Block: börja en rad med `$:` (eller `namn:`). Ett block tar slut vid tom rad eller nästa etikett, och alla block spelas samtidigt. Utan etiketter gäller bara sista blocket.
- `s("bd")` eller `s("kick")` = kickraden (16 steg per takt, ruta = round(position × 16)).
- `s("i1")` … `s("i10")` = instrument 1–10. Utan `s(...)` hamnar noterna på instrument 1.
- `note(...)`: notnamn (`c2`, `e#3`, `bb1`) eller heltal. **C2 = tonhöjd 0**, ett heltal är halvtoner över C2. Notnamn utan oktav = oktav 3. Tonomfång −48…48.
- `gain` blir velocity (0,05–1, standard 0,8). Notlängd = händelsens längd, så `c2@3 c2` ger en lång och en kort not.
- Samma tonhöjd två gånger på samma tick hoppas över. Ackord `[c3,e3,g3]` blir staplade toner.
- Modulation från koden: `.lfo("parameter-id", hz, djup, form)` med form `sine`, `tri`, `saw`, `isaw`, `square` eller `rand`, och `.env("parameter-id", attackMs, decayMs, djup)`. Djup −1…1. Max 8 olika modulatorer samtidigt, bara parametrar som går att modulera (felrad annars). Id:n som är testade: `bass1Cutoff`, `bass1Resonance`. Fler id får du från `list_parameters`.
- Känd begränsning: Strudels egna `.lfo`/`.env` (objektform) fungerar inte, bara vår variant ovan.

## 1. Fyra på golvet + rullande bas (utgångspunkten)

```
$: s("bd*4")
$: note("[~ c2 c2 c2]*4").s("i1")
```

Kick på varje fjärdedel, basen tre 16-delar mellan varje slag.

## 2. Basen byter ton var fjärde takt

```
$: s("bd*4")
$: note("<[~ c2 c2 c2]*4 [~ c2 c2 c2]*4 [~ eb2 eb2 eb2]*4 [~ g1 g1 g1]*4>").s("i1")
```

`<...>` byter element en gång per cykel. Fyra takter, tredje och fjärde med annan ton.

## 3. Halvtoner som tal och lite variation

```
$: s("bd*4")
$: note("[~ 0 0 0]*2 [~ <0 3> 0 <0 5>]*2").s("i1")
```

Tal är halvtoner över C2. `<0 3>` växlar varje takt.

## 4. Euklidisk kick och bas

```
$: s("bd(5,16)")
$: note("[~ c2 c2 c2]*4").s("i1").gain(0.9)
```

Fem kickslag fördelade över 16 steg. Prova `bd(3,8)`, `bd(7,16)` och `bd(9,16)`.

## 5. Offbeat-bas med lång ton

```
$: s("bd*4")
$: note("~ c2@3 ~ c2@3 ~ c2@3 ~ c2@3").s("i1")
```

`@3` gör noten tre gånger så lång. Försök med `c2@2 c2` för kortare.

## 6. Melodi på instrument 2 ovanpå basen

```
$: s("bd*4")
$: note("[~ c2 c2 c2]*4").s("i1")
$: note("c4 [e4 g4] bb3 [g4 e4]").s("i2").gain(0.6)
```

Kräver att instrument 2 är aktivt och har ett ljud. Annars hörs ingenting från det.

## 7. Ackord (staplade toner)

```
$: s("bd*4")
$: note("<[c3,eb3,g3] [ab2,c3,eb3] [bb2,d3,f3] [g2,bb2,d3]>").s("i2").gain(0.5)
```

Ett ackord per takt. Max antal toner per stapel styrs av `Pattern::maxStack`.

## 8. Reglage som kan dras (slider)

```
$: s("bd*4")
$: note("[~ c2 c2 c2]*4").s("i1").gain(slider(0.8, 0.2, 1))
```

`slider(startvärde, min, max)` ger ett reglage. Numreringen följer ordningen i koden, så lägg till nya `slider` sist. Under LIVE hörs ett draget värde direkt. Annars kräver det ny körning.

## 9. LFO på basens filter

```
$: s("bd*4")
$: note("[~ c2 c2 c2]*4").lfo("bass1Cutoff", 0.5, 0.6, "tri").s("i1")
```

Filtret svänger med 0,5 Hz, djup 0,6, triangelform. Prova `"sine"`, `"saw"`, `"square"`, `"rand"` och hz mellan 0,1 och 4. Ordningen `note(...).lfo(...).s(...)` är samma som testerna använder.

## 10. Envelope på resonansen

```
$: s("bd*4")
$: note("[~ c2 c2 c2]*4").env("bass1Resonance", 5, 200, 0.5).s("i1")
```

Snabb attack (5 ms), decay 200 ms, djup 0,5, för en "zap" på varje not.

## 11. Allt tillsammans (en liten låt)

```
kick: s("bd*4")

bas: note("<[~ c2 c2 c2]*4 [~ c2 c2 c2]*4 [~ eb2 eb2 eb2]*4 [~ g1 g1 g1]*4>")
  .lfo("bass1Cutoff", 0.25, 0.5, "sine")
  .env("bass1Resonance", 5, 150, 0.3)
  .s("i1")

lead: note("<[c4 ~ e4 ~] [g4 ~ e4 ~]>").s("i2").gain(slider(0.5, 0.1, 0.9))
```

Etiketter (`kick:`, `bas:`, `lead:`) fungerar som `$:`. Blocken separeras med tomma rader.

## 12. Matriskoll: syns kodens modulering i matrisen?

Varje `.lfo`/`.env` blir en intern källa CODE 1–8 och en rutt i modulationsmatrisen (max 8 olika källor, max 32 kodrutter av 64 totalt). CODE-källor sparas inte i projekt eller presets och finns inte i de handritade modulationsmenyerna, så de ska bara synas medan koden körs.

### 12a. Två LFO:er + en envelope (tre rutter)

```
$: s("bd*4")
$: note("[~ c2 c2 c2]*4")
  .lfo("bass1Cutoff", 0.5, 0.6, "tri")
  .lfo("bass1Resonance", 0.25, 0.4, "sine")
  .env("bass1Cutoff", 5, 150, 0.3)
  .s("i1")
```

Kontrollera:
1. Cutoff- och resonansratten rör sig av sig själva (ringen runt ratten visar modulationen). Hörs filtret svepa?
2. Titta i modulationsmatrisen / ratternas modulationsring: finns tre CODE-rutter (två på cutoff, en på resonans)? Jag vet **inte** om matrisvyn i gränssnittet listar CODE-källor (koden i `PluginBase::sweepCodeRoutes` och `ToolLayer::applyCodeMods` hanterar dem, men jag har inte sett vyn). Skriv upp vad du ser.
3. Via AI-chatten: be den köra `get_state` och lista modulationsrutterna, det visar CODE 1–3 säkert.
4. Ändra `0.6` till `0.9` och kör om: rutten ska uppdateras, inte dubbleras (nyckeln är lager + parameter + sort, första träffen gäller).
5. Radera allt i editorn (eller stoppa) och kör: gamla CODE-rutter ska tonas ut av `CodeSweeper` (timer 100 ms) och försvinna.

### 12b. Fyll alla 8 källor (gränsfallet)

```
$: note("[~ c2 c2 c2]*4")
  .lfo("bass1Cutoff", 0.1, 0.5, "sine")
  .lfo("bass1Cutoff", 0.2, 0.5, "tri")
  .lfo("bass1Cutoff", 0.3, 0.5, "saw")
  .lfo("bass1Cutoff", 0.4, 0.5, "isaw")
  .lfo("bass1Cutoff", 0.5, 0.5, "square")
  .lfo("bass1Cutoff", 0.6, 0.5, "rand")
  .lfo("bass1Resonance", 0.7, 0.3, "sine")
  .lfo("bass1Resonance", 0.8, 0.3, "tri")
  .s("i1")
```

Förväntat: går igenom. Lägg till en nionde `.lfo("bass1Resonance", 0.9, 0.3, "saw")` och du ska få felraden "All 8 CODE modulators are in use". OBS: raderna med samma parameter, sort och lager kan räknas som samma rutt (första gäller), så om du bara ser en rutt på cutoff är det den regeln, inte ett fel. Skriv upp vad som händer.

### 12c. Avsiktliga fel (ska ge felrader, inte krasch)

```
$: note("[~ c2 c2 c2]*4").lfo("finnsInte", 1, 0.5).s("i1")
```
Okänt parameter-id → felrad.

```
$: note("[~ c2 c2 c2]*4").lfo("instrumentCount", 1, 0.5).s("i1")
```
Går inte att modulera → avvisas.

```
$: note("[~ c2 c2 c2]*4").lfo("bass1Cutoff", 1, 0.5, "trekant").s("i1")
```
Okänd form → felrad.

```
$: note("[~ c2 c2 c2]*4").lfo("bass1Cutoff", 1000, 5).s("i1")
```
Orimliga värden: hz begränsas till 200 och djup till 1, ingen felrad (enligt `ToolLayer.cpp:974,999-1002`).

Noterna ska skrivas även när modulationen avvisas. Är något av detta inte som beskrivet är det en bugg som vi vill veta om.

### 12d. Fler parametrar att pröva (ej verifierade)

`bass1EnvAmount`, `bass1Drive`, `bass1Sustain`, `bass1Release` m.fl. kan vara modulerbara. Byt id i 12a och se om du får en felrad. Listan över alla id:n får du med `list_parameters` (be AI-chatten köra den). Jag kontrollerar dem i nästa CI-körning (den som innehåller testerna för LC E).

## 13. Strudel-effektnamn -> Kickbase (mappning godkänd av Tobias 21 okt, dokumenterad 22 okt)

Strudels egna effektmetoder (`.lpf`, `.room` ...) kräver webbljud (superdough) och gör **ingenting** i vår motor. Använd i stället motsvarigheten nedan. Tabellen bygger på läst kod (`Params.cpp:284-345`, `Effects.cpp:65`, `glue.js` F1/F3); ej provlyssnad.

Viktigt: parametrar och effektplatser sätts **en gång per körning** (de är inte per not som i Strudel). Per-not-variation får du med `.lfo`/`.env` (avsnitt 9, 10, 12). Skriv `.set`-raderna direkt före mönstret.

| Strudel | Kickbase (instrument 1 som exempel) | Enhet / anmärkning |
|---|---|---|
| `.lpf(hz)` / `.cutoff(hz)` | `instr1.Cutoff.set(hz)` | Hz, 20-20000. Filtertyp: `instr1.FilterType.set("LP 24")` |
| `.hpf(hz)` | `instr1.FilterType.set("High")` + `instr1.Cutoff.set(hz)` | Filtret är ett enda per instrument (Filter 2 kan läggas till i FILTER-fliken) |
| `.lpq(x)` / `.resonance(x)` | `instr1.Resonance.set(x)` | 0-1 |
| `.attack(s)` / `.release(s)` | `instr1.Attack.set(ms)` / `instr1.Release.set(ms)` | Millisekunder (Strudel använder sekunder: ×1000). Attack 0-50, Release 1-300 |
| `.decay(s)` / `.sustain(x)` | `instr1.Decay.set(ms)` / `instr1.Sustain.set(x)` | Decay 5-2000 ms, Sustain 0-1 |
| `.distort(x)` / `.shape(x)` | `instr1.Drive.set(x)` (+ `instr1.DriveMode.set("Hard")`) | Drive 0-1; för kick `kick.Drive.set(x)` |
| `.crush(bits)` | `instr1.DriveMode.set("Crush")` eller effekt `setEffect("bitcrusher")`; kick: `kick.Crush.set(x)` | Kickens Crush 0-1 |
| `.room(x)` + `.size(x)` | `fx1.slot1.setEffect("reverb")`, sedan `fx1.slot1.mix.set(x)` | Instrumentet måste skickas till FX-kanal 1 (SEND/INSERT i FX-fliken). Kontrollera rattnamnen med felraden |
| `.delay(x)` + `.delaytime(s)` + `.delayfeedback(x)` | `fx1.slot2.setEffect("echo")`, sedan `fx1.slot2.mix.set(x)` | Rattnamn i felraden om de skiljer sig |
| `.pan(x)` | `instr1.APan.set(x)` (OSC A; B/C: `BPan`, `CPan`) | -1..1. Strudel: 0..1 (0,5 = mitten), räkna om: `x*2-1` |
| `.gain(x)` | `.gain(x)` (fungerar redan: velocity) | 0,05-1 |
| `s("bd")` | `s("bd")` eller `s("kick")` (fungerar redan) | Kickraden |
| `.slow/.fast/.every/.off ...` | fungerar (Strudel core) | Mönsterfunktioner, inte ljudeffekter |

Typnamn för `setEffect` (gemener, bara bokstäver/siffror): off, filter, distortion, eq, chorus, phaser, echo, reverb, compressor, trancegate, comb, ringmod, bitcrusher, autofilter, maximizer, sidechain, disperser, widener, flanger, transient, dynamiceq, octaver, bandducker.

Exempel:

```
instr1.Cutoff.set(900)
instr1.Resonance.set(0.5)
instr1.Release.set(80)
fx1.slot1.setEffect("reverb")
$: note("[~ c2 c2 c2]*4").s("i1").lfo("bass1Cutoff", 0.5, 0.6, "tri")
```

Byggt 22 okt (grenen an-master): `.lpf`/`.cutoff`, `.lpq`/`.resonance`, `.attack`/`.decay`/`.sustain`/`.release` (sekunder blir ms), `.distort`/`.shape` (Drive) och `.pan` (0..1) på ett instrumentmönster blir parameterskrivningar, ett värde per körning (första notens värde). Gäller bara instrument, inte kicken. Ett uttryckligt `.set(...)` på samma parameter vinner.

## Om det inte fungerar

- **Inget ljud:** kontrollera att LIVE är på, att instrumentet du adresserar (`i1`, `i2`) finns och att Play är igång.
- **Felrad om parametern:** `bass1Cutoff` och `bass1Resonance` är testade. Andra id:n kan vara omodulerbara. Fråga efter `list_parameters` och skriv upp vilka som avvisas.
- **Slicern:** slicern styrs av MIDI-ton (ton 2–9 = skiva 1–8), inte av Live Code.
- Strudel-funktioner som kräver webbljud (`.room`, `.delay`, `.lpf` och liknande) ger inget i vår motor. Bara toner, längd, gain och våra `.lfo`/`.env` används.
