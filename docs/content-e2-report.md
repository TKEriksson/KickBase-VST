# Fas E2: ENV 2/3 i takter och slag (gren `content-e2`, från `content-w`)

Skriven av Sonnet 18 okt 2026 (spec godkänd av Opus). Tobias önskade att ENV-tiden ska kunna anges i takter och slag, som LFO:erna.
**Med standardvärden (läget ms) är ljudet bit för bit som förut.**

## 1. Vad som byggts

- **Parametrar** (sist i `InstrumentField`, med `ParamID::envModeField(env)` och `ParamID::envSyncField(env, which)`): `bassNEnv2Mode`, `bassNEnv2AttackSync`, `bassNEnv2DecaySync`, `bassNEnv2ReleaseSync` och samma för ENV 3.
  Mode är ett val {ms, Sync}, standard ms. Sync-tiderna är val ur en egen lista (LFO:ernas `rateNames()` passade inte): 0, 1/64, 1/32, 1/16 T, 1/16, 1/16 D, 1/8 T, 1/8, 1/8 D, 1/4 T, 1/4, 1/4 D, 1/2, 1 bar, 2 bars, 4 bars
  (`Modulation::syncTimeNames`, `syncTimeInQuarters`). Standardvärden ungefär som ms-standard vid 120 BPM (0, 1/8 punkterad, 1/8). Sustain är en nivå och berörs inte. 80 nya rader i `docs/audit/params-instrument.txt`.
- **Motorn:** `ModEnvParams::withTempo(bpm)` ger envelopen med tiderna i ms (oförändrad om Sync är av: bit-identiskt), annars fjärdedelar × 60000 / BPM, aldrig under ms-rattarnas minimum (0,1 / 5 / 5 ms).
  `InstrumentSynth::setTempo(bpm)` anropas varje 32-sampelsbit från `processChunk`, så ett tempobyte mitt i ett steg ger en kontinuerlig kurva (steget per sampling ändras, nivån hoppar inte).
- **Gränssnittet:** SYNC-knapp i ENV 2- och ENV 3-rutorna på ENV-fliken. Handtagens x-led styr ms-ratten, eller i Sync notvärdet (`EnvelopeView::Handle::xParamNow`). Diagrammet visar tiderna i ms räknade från aktuellt tempo. Hjälptexter och AGENT_GUIDE.

## 2. Tester

- **Enhet:** notvärdeslistan; 1/4 vid 120 BPM ger samma attack som 500 ms (±1 sampel); 1/4 vid 60 BPM samma som 1000 ms (±1); tempobyte mitt i attacken: ingen hopp i nivån, och attacken fortsätter med nya tempot.
- **`blocksizetest` läge 4** (tungt läge med ENV 2/3 i Sync): alla blockstorlekar identiska med block 32. **`persisttest` 4c:** läge och notvärden sparas och laddas, ENV 2 kan vara kvar i ms när ENV 3 är Sync, gamla projekt = ms.
- `rendersig`, `rendersigreal`, `rendersigwide`, `rendersigtb303` identiska med baslinjerna; `helpcheck` 0 utan förklaring; `KickbaseTests` gröna.
- **Gränssnitt:** 42 bilder mot `content-w`: 39 identiska; de tre som skiljer är ENV-fliken (bara SYNC-knappen, x 531–782, y 538–554).

## 3. Två saker som kom med i samma gren (Tobias önskemål)

- **WARP-mängdsratten** sitter nu bredvid WARP-menyn i OSC-panelen (rubriken AMOUNT ovanför ratten, på samma höjd som WARP-rubriken), inte bland de övriga rattarna (`KnobComponent::setLabelAbove`). OSC-vyn blev 16 px lägre.
- **Fabrikspreset "Basic Morph"** (full-preset nummer 2 i listan): 1 instrument, tabellen Basic, Morph av, LFO 1 (såg, en takt) på POS ±0,5, rullande sextondelar. Tobias skriver att han troligen tar bort alla full-presets utom Init senare.
  **Baslinjerna:** `baseline-kod-kontrolltakt.txt` och `baseline-init-kontrolltakt.txt` fick exakt en ny rad var, och `git diff` mot `content-w` visar bara +rader:
  `full 1 Basic Morph                 f7d9d390f481771d peak 0.6526 rms 0.2540`. Alla befintliga rader är byte-identiska. `rendersigwide` och `rendersigtb303` har inga fabrikspresets och är oförändrade. `presettest` och `repairpresets` (0 fel) gröna.

## 4. Kända saker

- ENV-sync gäller bara ENV 2 och ENV 3 (modulationskällorna); volym- och filterenvelopen (ENV 1 och FILTER) är kvar i ms.
- Tempot läses från värden (eller den egna klockan) en gång per 32-sampelsbit; tempot i ENV-diagrammet följer `transportStatus.bpm` (uppdateras 4 gånger i sekunden).
