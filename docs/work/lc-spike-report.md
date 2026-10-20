# LC-spike: Strudel i JUCE:s QuickJS

**Resultat: fungerar.** Rekommendation: **QuickJS + Strudel** (reservplanen med egen mini-notation behövs inte).
Kod: `Spikes/strudel-quickjs/` (spike-lc). Mätt 20 okt på Windows 11, Release, JUCE 8.0.15, `@strudel/core` + `@strudel/mini` 1.2.6.

## Bunt
- `juce::JavascriptEngine` i 8.0.15 är QuickJS (ES2023, se `juce_javascript/javascript/juce_JavascriptEngine.h`). Har `maximumExecutionTime`, `stop()`, `registerNativeObject`.
- esbuild IIFE, `--platform=neutral --target=es2020`: **315 kB** (322 890 byte). `@kabelsalat/web` (beroende till core) ingår orörd och laddar utan fel; vi behöver inte stubba den.
- Stubbar som krävs (js/prelude.js): `console`, `performance.now`, `setTimeout`/`setInterval`. Utan `performance` kraschar laddningen.
- Strudels transpiler (acorn, `"..."` -> mini, `slider` med källposition) ingår **inte**; i stället anropar glue.js `setStringParser(mini)` så att strängar tolkas, och mönsterkoden körs med `new Function`. `$:`-block och `.p()`-syntax finns alltså inte utan transpiler (se Blockerar).
- Copyright-banner överst i bunten (`--legal-comments=inline` + egen rad). Bunten är i .gitignore, byggs med `js/build-bundle.sh`.

## Mätvärden (bakgrundstråd, en motor)
| | |
|---|---|
| Laddtid bunt | 81–210 ms (engångs) |
| Minne efter laddning | +5 MB (arbetsminne), +0,0–0,3 MB per mönster |

| Mönster | eval | queryArc(0,4) (haps) | 1000 x 1 cykel, median / max (JS + JSON) | JSON -> C++ median |
|---|---|---|---|---|
| `s("bd*4, ~ hh")` | 2,7 ms | 6,6 ms (21) | 1,9 / 28,5 ms | 0,06 ms |
| `note("c2 [e2 g2]*2").s("bass")` | 4,1 ms | 25 ms (25) | 5,0 / 37 ms | 0,07 ms |
| `n("0 3 5 <7 10>")` | 2,1 ms | 7,6 ms (21) | 1,5 / 3,8 ms | 0,02 ms |
| `s("bd*4").every(3,..).sometimes(..)` | 0,9 ms | 8,4 ms (24) | 1,8 / 29,6 ms | 0,03 ms |
| `...cutoff(sine.range(200,2000))` | 3,7 ms | 12,7 ms (19) | 2,7 / 13,3 ms | 0,03 ms |

Händelser blir C++-structar `Hap{begin,end,hasOnset,map<namn,var>}` via JSON (JSON.stringify i JS, `juce::JSON::parse`). Fält kommer rätt: `s`, `note`, `n`, `speed`, `cutoff`. Brus av enstaka 30 ms-toppar kan vara GC eller Windows schemaläggning; ej utrett. Det är ofarligt när frågan sker några cykler i förväg på bakgrundstråden. Första frågan (0–4) är långsammast (kall).

## Avbrott
- `maximumExecutionTime = 500 ms`: `while(true){}` avbröts efter **499 ms** med felet `interrupted`.
- `stop()` från annan tråd: avbröts efter 301 ms.
- Motorn **fungerar efteråt** (Strudel-tillstånd kvar, nya frågor ger rätt haps), både efter timeout, stop() och syntaxfel/okänd funktion (`not a function` kommer som felmeddelande).
- Tung Strudel-fråga (`.fast(1000000)`) avbröts också av tidsgränsen (511 ms).
- Obs: tidsgränsen måste sättas om till normalt värde efteråt (`maximumExecutionTime` är ett vanligt fält).

## slider() från C++
- `registerNativeObject("host", DynamicObject)` med `setMethod("slider"/"get")` fungerar. JS `slider(v,min,max)` anropar `host.slider(id,v,min,max)`; C++ registrerar reglaget (id, värde, min, max) och svarar med aktuellt värde.
- `slider` ger ett konstant värde per eval (en ändring kräver ny eval). `sliderLive(id,min,max)` (signal som läser `host.get(id)` vid varje fråga) ger direkta ändringar utan ny eval: vid 200 -> 200, vid 2000 -> 2000. Rekommenderat för riktig LC-flik.
- Id: utan transpiler numreras anropen i ordning. Tvåvägs (ändra kodtexten) kräver egen enkel textsökning efter `slider(` eller transpilern.

## Ljudtråd
Simulerad ljudtråd (256 samplingar/48 kHz) med JS på bakgrundstråd (1063 frågor + omladdning av mönster, 5 s): inget block blev sämre än utan JS (p99.9 16 ms utan, 31 ms med; max samma storleksordning; baslinjen är brusig av Windows sleep-upplösning, så detta är ett grovt test). Motorn rör aldrig ljudtråden i designen (egen tråd + kö). Ett riktigt test i pluginet återstår.

## Blockerar / öppet
- Inget blockerande. Öppet: Strudels transpiler (acorn, ~100 kB) behövs för `$:`, `.p()`, backtick-strängar och `slider` med källposition; kan buntas på samma sätt (ej testat).
- Ljudkontroller (`.cutoff` m.fl.) i Strudel är bara värden i haps; mappningen till Kickbases parametrar är vårt eget arbete.
- Frågetid 1–5 ms/cykel är gott om marginal om vi frågar 2–4 cykler framåt var ~100 ms.

## Rekommendation
**QuickJS + Strudel.** Nästa steg i LC-fasen: bunta transpilern, schemaläggare med lock-fri kö, mappning s/note/n -> kick/instrument, sliderLive.
