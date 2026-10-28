# RING, Echo, Flanger: DSP-kärnan (Arbetare 2, 2026-10-28)

Uppdrag: plan-punkt 8, ingen UI. Arbetare 1 gör UI-kopplingen efteråt.

## Läget i koden (kontrollerat först)
| Del | Finns | Var | Brist |
|---|---|---|---|
| RING (B) som WARP-läge | **ja**, med `ringSource` (OSC 2 / OSC 3 / OSC 2 x OSC 3) | `InstrumentSynth.h:14,30`, `InstrumentSynth.cpp:79,129,409-437`, `Params.cpp:438,776` | ingen test |
| RING-modulator som FX | **ja** | `Effects.cpp:220` (FREQ, DEPTH, MIX) | ingen test |
| Echo (Fruity Delay 2) | **ja**: TIME sync, OFFSET, IN PAN, IN VOL, FB MODE, FEEDBACK, CUT, DRY VOL, SYNC, MIX | `Effects.cpp:182,918-980` | tid i ms saknas (plan-v2.md:297 "sync/ms"); alla 10 rattar (`knobsPerSlot`, `Effects.h:25`) är upptagna; ingen test |
| Flanger | **ja**: RATE, DEPTH, DELAY, FEEDBACK, MIX | `Effects.cpp:261,1800-1830` | bara sinus och fast stereofas (kvarts varv); ingen test |

Slutsats: kärnan finns. Det som saknas är tester och några få tillägg där det finns plats.

## Vad jag bygger
1. **Tester (nytt, `Tests/FxTests.cpp`)** på påhittade signaler genom `Effects`-kedjan: Echo (ekots tid vid impuls, FB MODE Invert vänder tecken, Ping-pong växlar sida, DRY VOL/MIX), Flanger (MIX 0 = bit-identisk, kamfilter-hack vid DELAY, feedback stabil), RING-FX (sinus x sinus ger summa/differens-frekvens), RING warp (ringSource 0/1/2 ger olika, ändliga signaler). Inga ljudfiler, deterministiska.
2. **Flanger, två nya rattar sist** (plats P6, P7 av 10, standard = dagens ljud, så gamla presets och projekt låter likadant):
   - `SHAPE`: Sine (standard) / Triangle. Triangle ger jämnare svep.
   - `SPREAD`: höger kanals LFO-förskjutning, 0..1 av ett varv, standard 0,25 (= dagens kvarts varv). 0 = mono-svep.
3. **Echo**: ingen ny ratt (alla 10 används). Tid i ms kräver att `knobsPerSlot` höjs, vilket rör `Params.cpp`/`PluginBase.h` och alla presets: görs inte nu. Förslag till PL: be Tobias avgöra om ms behövs (TIME-sync räcker för tempo-låst psytrance).
4. **RING (warp och FX)**: inga ändringar, bara test. Etikett/tooltip "OSC1 x OSC2" är UI (Arbetare 1).

## Filer jag rör
- `Source/Core/Effects.cpp` (Flanger-tabell + `FlangerFx`) – nya rattar sist i tabellen.
- `Tests/FxTests.cpp` (ny), `Tests/TestUtils.h`, `Tests/TestMain.cpp`, `CMakeLists.txt` (testmål).
- Docs: `docs/work/PROGRESS-w2.md`, denna fil.
- **Rör inte** `Params.cpp`, `PluginBase.h`, `Effects.h`, UI. Inget konflikt med Arbetare 1.

## Villkor
Nya val sist, standard = dagens beteende (byte-identisk render med standardvärden, testas). Engelska rattnamn. `build.bat Debug all` grönt före commit; ingen push.
