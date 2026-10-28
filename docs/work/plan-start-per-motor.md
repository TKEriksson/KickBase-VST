# 5b: START, SYNC, längd och kurva per motor (plan, Arbetare 1)

Status: plan, ingen kod. Granskas av projektledaren innan bygget.

## Vad som finns idag
- **En enda volymenvelope per lager** (`ampLevel`, ATTACK/DECAY/SUSTAIN/RELEASE, InstrumentSynth.cpp:352-377) som alla källor delar. Filter-envelope och pitch-envelope likaså (en per lager).
- **Det finns ingen tidsförskjutning per källa.** Det som heter START idag är något annat:
  - INSTR/PHASE-gruppens `START` = `phaseDeg`, startfas i grader för OSC 1-3 och sub (InstrumentSynth.cpp:170, Modules.cpp:1279).
  - MIX-flikens `START` per SUB/FM = `subPhaseDeg`/`fmPhaseDeg` (InstrMixPanel.cpp:71), också fas.
  - SAMPLE-flikens `START` = `sampleStart`, 0..1 inne i samplingen (InstrumentSynth.cpp:196, Modules.cpp:1632).
  - `nudgeMs` (-2..+2 ms) flyttar hela lagret (InstrumentProcessor.cpp:796), inte en motor.
- Det finns `noteTime` per röst (sätts till 0 i `noteOn`, InstrumentSynth.cpp:182) och används redan av filter-envelopen. Glide (`glideTo`) rör inte `noteTime`; polyfoni = en `InstrumentSynth` per röst, så allt per-röst-tillstånd följer med.

## Förslag (minsta lösning, återanvänder det som finns)
Ingen ny envelope-motor. Varje motor får en **portfaktor** `gate_e(t)` som multipliceras på motorns nivå, där `t = noteTime`:

```
gate_e(t) = 0                                  för t < start
          = shape(lengthMs, curve, t - start)  för t >= start
shape = 1 om length = 0 (AV = som idag), annars (1 - u)^k, u = (t-start)/length, k från CURVE
```
Volymenvelopen (ATTACK..RELEASE) ligger kvar ovanpå och styr hela lagret. Standard: START 0, LENGTH 0 (= av), CURVE 0 → bit för bit samma ljud som idag (faktor exakt 1.0f, hoppas över med `if`).

Där faktorn läggs in (InstrumentSynth.cpp, samma ställen som nivåerna):
- OSC 1-3: `levelA/levelB/levelC` (rad 406-408). OBS RING/FM använder B och C som modulatorer (rad 410-411, 437): faktorn läggs på *hörbar nivå*, inte på modulatorn, annars försvinner RING när OSC B tystnar. Beslut att granska.
- SAMPLE: `params.sampleLevel` (rad 551-580). Under START-tiden ska `samplePosition` **stå still** (annars missas starten). Egen räknare.
- NOISE: `params.noise` (rad 484/516). FM: `params.fmLevel` (rad 533-544). SUB: `params.sub` (rad 483), valfri; sub och FM ingår i MIX så de får samma tre knappar.

## Parametrar (nya, sist, standard = dagens ljud)
Per lager och motor (OSC 1, OSC 2, OSC 3, SAMPLE, NOISE, SUB, FM = 7 motorer):
| Parameter | Intervall | Standard |
|---|---|---|
| `<motor>StartMs` | 0..4000 ms, skevad (mer upplösning vid korta tider) | 0 |
| `<motor>StartSync` (val) | OFF, 1/16, 1/8, 1/4, 1/2, 1, 2, 4 takter | OFF |
| `<motor>LenMs` | 0..4000 ms (0 = hela tonen) | 0 |
| `<motor>Curve` | -1..1 (0 = linjär) | 0 |

= 28 parametrar per lager, läggs sist i `InstrumentField` (Params.h) och i Params.cpp:s slinga. ID:n, preset-index och hashar ändras inte; `rendersig` oförändrad eftersom standard = faktor 1. Hjälptext krävs för varje parameter (helpcheck), samt `Help::forParameter`.

**Justerbart max:** inte i v1. Max 4000 ms räcker till 1/4-not vid 60 BPM (1000 ms) och 4 takter vid 240 BPM (4000 ms). Längre via SYNC. Ett justerbart max skulle ändra vad sparade värden betyder om vi ändrar det i efterhand; vi låser därför intervallet.

## SYNC (tillägg 1)
- `StartSync` ≠ OFF → `StartMs` ignoreras (knoppen visar takter) och startfördröjningen räknas om vid `noteOn`: `ms = takter * 4 * 60000 / bpm`.
- Tempot hämtas från hostens bpm som processorn redan har (`transportStatus.bpm`, PluginBase) och skickas med i röstparametrarna per block, som `nudgeMs`.
- **Tempoändring mitt i en ton:** fördröjningen låses vid `noteOn` (en ton som redan startat ändras inte). Nästa ton använder nya tempot. Dokumenteras i hjälpen.
- **Transport startar mitt i en takt:** takterna räknas från *tonens* start, inte från takt-rutnätet, så det spelar ingen roll var transporten startar. (Att kvantisera mot taktgränsen skulle ge olika fördröjning per ton; avvisat.)
- Utan host (egen klocka): pluginens egen bpm används på samma sätt.

## Samspel
- **Envelope:** ATTACK/DECAY är oförändrade och startar vid tonstart. Är START längre än volymenvelopens släpp hörs motorn aldrig; gul varning på knoppen ("START is longer than the note").
- **Gate/notlängd (LENGTH i LAYER OUT):** tonen släpps vid notens slut; en motor med START efter notens slut blir tyst. Samma varning.
- **Glide:** `glideTo` ändrar bara frekvensen och rör inte `noteTime`, så motorernas START startar inte om vid glide/legato. Rätt beteende.
- **Polyfoni/röststöld:** `noteTime` är per röst och nollas i `noteOn`; en stulen röst startar om rent. Ingen delad state.
- **Oscillatorfas:** oscillatorerna kör fritt under START-tiden; fasen vid motorns start är därför "var den råkar vara", inte `phaseDeg`. Förslag: nollställ/`retrigger` motorns fas när START passeras om START > 0. Granska: ger tight anslag men kräver en liten flagga per motor.
- **FILTER/MIX:** faktorn läggs före filtren (samma plats som nivån), så filterkopplingen per källa är oförändrad.

## UI (Arbetare 1, Source/Ui)
- Egen rad i **MIX-fliken** under varje motors remsa: START (knopp + SYNC-val), LENGTH, CURVE. MIX har redan en remsa per motor (InstrMixPanel.cpp:11-17); raden blir tre små ratter. Förslag att MIX-fliken får en "ENGINE TIMING"-vy som växlas med den breda vågformen, för att inte trycka ihop remsorna.
- Mini-graf per motor som visar `gate_e(t)` ovanpå volymenvelopen (återanvänder EnvelopeView:s `addCurve`).
- HELP-texten för MIX utökas (jag redigerar redan Help.cpp/HelpDialog.cpp).
- Namnkrock: "START" används redan för fas. Nya knappar heter **DELAY** i UI:t (och `StartMs` i koden → hellre `<motor>DelayMs`); Tobias kallade det START, så beslutet "DELAY eller START?" ligger hos projektledaren/Tobias.

## Filer och krockrisk med Arbetare 2
- Jag rör: InstrumentSynth.h/.cpp (faktorn + räknare), InstrumentProcessor.cpp (bpm till röstparametrar), Params.h/.cpp (sist), Help.cpp, Source/Ui/InstrMixPanel.*.
- **Arbetare 2 rör samma filer** (reverse per note, click-lager, sample-editor core AMP/PITCH → InstrumentSynth.cpp:551-587 sample-grenen). Samplingsgrenen är den känsliga: hennes AMP/PITCH och min `samplePosition`-stillastående måste samordnas. Förslag: hon bygger klart sample-editor-core först, jag lägger sampleportens faktor sist och rebasar på hennes gren.
- Behöver bekräftas av Arbetare 2: om hon lägger en per-sample amp-kurva, så multipliceras min faktor ovanpå (ingen konflikt, bara samma rad).

## Öppna frågor till projektledaren
1. START eller DELAY i UI:t (kollision med fas-START)?
2. Faktorn på hörbar nivå (förslag) eller på motorns signal även som modulator (RING/FM)?
3. Nollställ oscillatorfasen vid motorstart (förslag: ja)?
4. Ta med SUB och FM i första versionen (förslag: ja, de finns i MIX)?
5. Max 4000 ms låst (förslag) eller justerbart senare?

## Tester (när bygget är godkänt)
- paramtest/helpcheck för de 28 nya parametrarna; rendersig oförändrad; ny test: START 100 ms → de första 100 ms är tysta för just den motorn, SYNC 1/4 vid 120 BPM = 500 ms, LENGTH+CURVE avtar monotont.
- blocksizetest före release.
