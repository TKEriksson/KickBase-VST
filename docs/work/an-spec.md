# AN: Analyzer-sidan (spec, utkast 20 okt, Sonnet)

Källor: plan-v2 "AN" (rad 248-257), research-notes/gaps-to-specs.md A1-A3, A8 (prio 1). Gren `content-an` från `content-fx1`.
Nuläge: ANALYZER är en underflik i `tools` (InstrumentEditor.cpp:34, SpectrumPage med SPECTRUM + ANALYSIS + REPORT). Mätmotorn finns i
`Source/Core/MixAnalysis.*` (offline-render, FFT 8192, kick/instrument/master, fynd med allvarlighetsgrad).

## Steg
1. **Egen toppflik + underflikar.** Flytta ANALYZER till en toppflik (bredvid PATTERN/INSTRUMENT) med underflikar `[KICK+BASS] [COLLISIONS] [SPECTRUM] [REPORT]`. Befintliga SpectrumPage-vyer flyttas oförändrade till SPECTRUM/REPORT (ingen funktion försvinner). Inga nya parametrar. Baslinje: rendersig identisk (ren UI).
2. **Mätningar A1-A3 i MixAnalysis** (ren offline-matematik på renders vi redan gör, ingen ljudtrådsändring): per kickslag och 16-del, Pearson-korrelation + sum/diff-energi 30-150 Hz mellan kick och bas, summans topp mot största enkeltopp (dB); per slag topp/fas vid första nollgenomgång/slutvärde (klickflagga >-60 dB, topp-spridning >1 dB); L/R-korrelation och M/S per 4 band (<120, 120-500, 0.5-4k, >4k) per takt, varna vid korrelation <0.3 under 120 Hz. Nya fält i `Finding`/export (`lowband.correlation[16th]`, `lowband.sum_vs_max_db`), `analyse_mix` i ToolLayer får samma fält. Enhetstester på syntetiska signaler (motfas = -1, samma fas = +1).
3. **KICK+BASS-vyn (PsyScope-lik).** Kick och bas som lager och summa i samma bild: vågform (ca 1 slag, zoom/FREEZE) med fasfärg, korrelation per 16-del som stapelrad, spektrum 20-300 Hz för kick/bas/summa. Läser senaste analysen (knapp ANALYSE som idag) så inget körs i ljudtråden. Rent läsande ritkomponent i `Source/Ui/` (ny `KickBassView`), timer bara när fliken syns.
4. **COLLISIONS-vyn.** Lista över fynd med grön/gul/röd (hänsyn till nivåskillnad >12 dB, oktav/kvint, ducking enligt plan-v2), textlogg ("INSTR 1: sub-bass clash with KICK"), klick på rad markerar band/takter i spektrumet. Gradienten (färgfältet över frekvens x takt) avstängbar i OPTIMIZATIONS (ny inställning i Settings, inte APVTS-parameter). Rapportskrivaren får de nya mätningarna + lektionsvarningarna (kickens svans som fångar basen, RAND-fas på bas, för mycket 200 Hz).
5. **Slutsteg:** testsvit (KickbaseTests + nya mättester), Init-check, helpcheck för nya vyer, MCP/AGENT_GUIDE nämner nya fält, PROGRESS + rapport. A8 (latens per FX-typ, impulstest) tas som ett eget litet testfall här om FX2 inte hunnit före.

## Val / risker
- Ingen automatisk fasändring (bara fakta, som gaps-to-specs säger); fasverktyg hör till PH.
- Step 2 är kärnan (samma motor ger vyerna och AI-verktygen); steg 3-4 är ren UI ovanpå.
- Uppskattning: 3-4 sessioner. Rör inte CMakeLists-blocken för javascript/Resources/strudel (LC).
