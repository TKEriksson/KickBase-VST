# Kickbase: översikt över synthen och Live Code

Status 21 okt 2026. Skriven av projektledaren utifrån `docs/DESIGN.md`, `docs/AGENT_GUIDE.md` och källkoden (`Source/Core/LiveCode.cpp`, `Resources/strudel/glue.js`). Release-bygget (gren `release-int`, commit 56eac62) har klarat alla 19 CI-steg men är **inte provat i en DAW av Tobias ännu**. Det som är markerat *(ej verifierat)* har jag bara läst i koden eller i andra dokument.

Detta dokument är tänkt att kunna ges till en annan AI som bakgrund. Bifoga helst också `docs/live-code-exempel.md` (färdiga exempel) och `docs/AGENT_GUIDE.md` (fullständig parameterlista och verktyg, på engelska). Det sistnämnda är den detaljerade sanningen om parametrarna; vid konflikt gäller det.

## 1. Vad det är

**Kickbase** är en VST3-synth för psytrance-kick och rullande bas (JUCE 8, C++, licens AGPL). Kick och bas ligger i samma plugin så att ducking, stämning och fas kan göras exakt. Två plugins byggs från samma kod:

- **Kickbase** (instrument, Channel Rack i FL Studio): kick + upp till 10 instrument (baslager).
- **Kickbase FX** (effekt, mixerplats): tar ljud från en annan synth, gate:ar det med basmönstret, duckar det under sin egen kick och lägger till kicken. Har 3 LFO:er (instrumentet upp till 10).

Ägare och beslutsfattare: Tobias Eriksson. All C++ skrivs av AI; Tobias vill förstå och granska.

## 2. Grundmodell

- DAW-tonen (MIDI) bestämmer **tonart** och när det låter; oktaven styr även basens oktav (MIDI 52 = E4 ger bas-grundton E1 ≈ 41 Hz; 52–55 är vanliga psytrance-tonarter).
- Mönstret följer DAW:ens tempo och taktstreck, inte tonens start.
- Mönster = **toner fritt i tiden**: 96 tick per fjärdedel, 384 per takt. Varje rad (kick och varje instrument) är lika lång som sina toner (auto) eller låst till N takter (1–1024). Kortare rader loopar.
- Rendering är **deterministisk** (seedad slump): samma inställningar ger bit-identiskt ljud offline. Tester använder checksummor (`rendersig`) för att upptäcka oavsiktliga ljudändringar.

## 3. Delarna

**Kick.** 16-stegs sekvens eller fria toner i pianorullen (tonhöjd på kicken = melodi i svansen). Syntetiserad kropp: ritbara envelopes för pitch (upp till 0,5 s) och volym (upp till 2 s), vågform, sub, övertoner, klick, FM, brus, drive, clip (Soft/Hard/Tube/Fold), bit crush, lågt/högt skär. Kan ha sampling (Click/Layer/Replace) och bibliotek (`load_library_kick`). Svansen stäms efter tonarten (Follow) eller fast.

**Instrument 1–10 (bas).** Varje har: tre wavetable-oscillatorer (120 inbyggda tabeller + egen WAV; warp-lägen, unison ≤ 7, detune, fas, pan/stereo per oscillator) + sub + brus + en sampling, ett eller två filter (Kickbase, Ladder, Formant, Comb, TB-303; serie/parallell), amp-ADSR, filter-envelope per ton, drive, EQ, FX-send, sidechain-djup, GATE (egen av/på-rytm), swing/humanize/delay. **Glide-toner** fungerar som slide i FL Studio.

**Slicer/sampler** (nytt i releasen): en sampling kan delas i upp till 128 skivor (minst 5 ms); varje skiva har en egen rad "Slice N" i mönstret (E2, de första 16 skivorna); en not där spelar skivan. Den gamla modellen med `slicerMapTo` är borttagen. Orsaken till att den först var tyst var en riktig produktbugg (hållna toner nollställdes inte i `prepareToPlay`, fixat i `InstrumentProcessor.cpp:103-106`).

**Sidechain.** Basen duckas under kicken sample-exakt, per instrument (`bassNDuck`); kurvan är kickens envelope eller en egen ritad. Effekttypen *Sidechain* kan låta ett instrument ducka under ett annat.

**FX-kanaler FX1–FX11** (ID `fxA`–`fxK`): fem effektplatser (15 typer, bl.a. Maximizer, Echo efter FL Fruity Delay 2), alltid en parametrisk EQ och kompressor, nivå och duck. Källor skickar via `…FxChannel` och `…FxAmount`.

**Mixer + master.** Gain/pan/SC/MAX per källa, sedan master med drive, clip, lookahead-limiter, bredd, mono-under-frekvens. Mixern ligger före masterlimitern.

**Modulation.** Upp till 10 LFO:er per instrument (Sine/Tri/Saw/Square/Random/Custom-kurva, tempo-synk eller Hz), 4 makron, instrumentets envelopes (ENV 2/3, velocity, random, amp/filter-env) och en **matris** med upp till 64 rutter (källa → valfri ratt, mängd −1…1). Dra en LFO på en ratt i gränssnittet.

**Presets och projekt.** Fabrikspresets finns för Init, kick och instrument; egna sparas som filer *(sökvägen ej verifierad)*. Pianorulle som `.roll`, MIDI-import/export. **Projektfil `.kbproj`** (nytt, `Source/Core/ProjectFile.cpp`): PROJECT-knapp med meny Save/Open project, Export, och AI-server PÅ/AV med statuspunkt *(ej provat i DAW)*.

**Analys.** ANALYZER-fliken och verktyget `analyse_mix` hittar kollisioner mellan kick och instrument, felstämd kick, rumble och limiter-belastning, och kan spara en fullständig JSON-rapport för en annan AI.

## 4. AI-styrning (verktygslagret)

Plugin:et har en intern AI-chatt och en MCP-brygga. En extern AI styr det via verktyg (`Source/Tools/…`, beskrivna i `docs/AGENT_GUIDE.md`):

`get_guide`, `list_parameters` (id, intervall, hjälptext), `set_parameters`, `set_effect`, `set_pattern`, `set_kick_envelopes`, `set_modulation`, `set_sidechain_curve`, `load_preset`/`save_preset`, `render_preview` (ger mått och spektrogram, AI:n kan **inte höra**), `analyse_mix`/`get_mix_report`, `undo`, `export_midi`/`import_midi`, och för Live Code `live_code_create`, `live_code_run`, `live_code_stop`, `live_code_lock`.

AI:n ska alltid tala om att den bara kunnat mäta, inte lyssna. Åtkomstnyckel för bryggan sätts i SETTINGS > AI LINK (inga nycklar i repot).

## 5. Live Code

**Idén.** Skriv musik som Strudel-kod (JavaScript med mini-notation, https://strudel.cc) på LIVE CODE-sidan. Koden körs i QuickJS inuti pluginet (Strudel-bundeln `Resources/strudel/strudel.all.js` + vår `glue.js`) och **skrivs som toner i pianorullen**. Ljudet görs av Kickbase egen motor, inte av Strudels webbljud.

**Mappning.**
- 1 cykel = 1 takt = 384 tick; sidan läser 4 cykler (en omkörning efter 400 ms utan tangenttryck i LIVE).
- `s("bd")` / `s("kick")` → kickraden (16 steg/takt). `s("i1")…s("i10")` → instrument 1–10. Utan `s` → instrument 1.
- `note(...)`/`n(...)`: notnamn (`c2`, `e#3`, `bb1`; utan oktav = 3) eller heltal. **C2 = tonhöjd 0**, enhet halvtoner, begränsat till −48…48.
- `.gain()` = velocity (standard 0,8, 0,05–1). Notlängd = händelsens längd (`@3`, `*4`, `[ ]`).
- Samma tonhöjd på samma tick hoppas över. Ackord `[c3,e3,g3]` = staplade toner (max `Pattern::maxStack`).
- Block med `$:` eller `namn:` (slutar vid tom rad eller nästa etikett) spelas samtidigt. Utan etiketter gäller bara sista blocket.
- `slider(start, min, max)` ger drag-reglage (numrerade i anropsordning). `sliderLive(id, min, max)` läser ett värde.
- **Parameterträd** (`glue.js:119-160`, byggs ur parameter-id): `instr1.Cutoff.set(1300)` (= `bass1Cutoff`; `bass1.Cutoff` fungerar också), `kick.X.set(..)`, `.get()` för läsning. Gruppnamnet är id:ts första del (små bokstäver), ratten resten (`bass1Cutoff` → `instr1.Cutoff`). Tal, text eller true/false. `.set` skrivs efter körningen; `.get()` ger värdet från före körningen och kastar fel om samma ratt skrivits i samma körning. Grupper som krockar med Strudels namn (`s`, `n`, `note` …) hoppas över. `instr1.lock()` / `instr1.pianoroll.lock()` låser rad respektive toner.
- **Effekter** (`glue.js:162-202`): `fx1.slot2.setEffect("reverb")` byter typ på plats 2 i FX1 (namn utan versaler/mellanslag/understreck, t.ex. `trancegate`, `bitcrusher`; fel ger lista på giltiga). `fx1.slot2.mix.set(0.3)` sätter en ratt i effektens egna enheter (rattnamnen beror på vald effekt; fel ger lista). Oförändrad typ ger ingen skrivning, så handvridna ratter rörs bara när koden ändras.
- **Mätare** (`glue.js:204-215`): `meter.outputPeak`, `meter.gainReduction`, `meter.duck1..10`, `meter.solo`, bara `.get()`.
- Samtliga ovan är *(ej provade av mig i ljud; byggda och testade i CI via LC-testerna)*. Om `fx1.<ratt>.set` direkt på kanalen (nivå, duck) finns beror på parameter-id för `fxA…`, *ej verifierat*.

**Modulation från koden (vår egen tillägg).**
- `.lfo(paramId, hz, djup = 1, form)`; form `sine, tri, saw, isaw, square, rand`.
- `.env(paramId, attackMs, decayMs, djup = 1)`; AD, ej ADSR. Om kickens parametrar går att modulera härifrån är *ej verifierat*.
- Djup −1…1. Orimliga värden begränsas (hz ≤ 200, attack ≤ 10000 ms, decay ≥ 1 ms). Okänt id, ogiltig form eller icke-modulerbar parameter ger en felrad medan noterna ändå skrivs.
- Max 8 olika CODE-källor (CODE 1–8) och 32 kodrutter av matrisens 64. De sparas **inte** i projekt/presets, tonas ut av `CodeSweeper` (timer 100 ms) när koden tas bort, och finns inte i de handritade menyerna.
- Testade id: `bass1Cutoff`, `bass1Resonance`. Övriga *(ej verifierat; kontrolleras i nästa CI-körning)*.

**Kända begränsningar.**
- Strudels egna mönstereffekter och ljud (`.room`, `.delay`, `.lpf`, `.s("sawtooth")` osv.) gör inget ännu. Styr effekter i stället med `fxN.slotM…` och parametrar med `instrN.…set`. Att koppla `.room/.delay/.lpf` m.fl. till våra effekter och instrument är beslutat som nästa utbyggnad men inte byggt.
- Strudels egna `.lfo`/`.env` i objektform är överskuggade av våra (`glue.js:79,85`). Planerat: antingen byta namn eller ge ett tydligt fel.
- `.gain()` är velocity, inte mixerns gain (använd `.set` på mixerparametern).
- CREATE skriver noterna en gång; ett `slider`-drag efter CREATE ändrar inte redan skrivna toner (LIVE gör det).
- Planerat men inte byggt (LC F): flik med reglage, kodblock i chatten med Play/Stop/Create.

## 6. Hur du arbetar med en annan AI

1. Ge den: detta dokument, `docs/live-code-exempel.md`, och vid behov `docs/AGENT_GUIDE.md` och `docs/psytrance-kick-bass-reference.md`.
2. Be den skriva Live Code som följer mappningen ovan och be om **ett block per rad** (`$:`), noter i `c2`-skala och bara `.lfo/.env` från vår lista.
3. Klistra in koden på LIVE CODE-sidan. Får du en felrad: ge AI:n felraden ordagrant plus koden.
4. Be inte AI:n om effekter som `.room`/`.lpf`; be den i stället sätta parametrar med `.set` eller modulera med `.lfo`/`.env`.
5. Be om små steg: en kick + en bas först, sedan variation, sedan modulation.

Mall: *"Kickbase är en psytrance-synth. Läs bifogade dokument. Skriv Live Code i Strudel-syntax som följer dem, för en rullande bas (16-delar, tre toner mellan kicken) i tonarten E med långsam filtersvep. Använd bara `s("bd")`, `s("i1")`, `note`, `gain`, `.lfo`, `.env`."*

## 7. Nuläge och kända luckor

- Provat i CI (headless, offline): alla tester gröna i körning 18. **Inte provat av Tobias i FL Studio:** PROJECT-menyn, AI-server PÅ/AV, slicern, Live Code-exemplen.
- Ändrade testfixturer som bör granskas: `antest` (bas ~5 dB under kick, inga trösklar ändrade), slicertestet (0,8 s WAV, kravet 128 skivor kvar).
- Stack overflow i `KickbaseTests`: grundorsaken är inte funnen; nödlösning `/STACK:8388608` (1c3519c).
- Inte med i releasen: `sampleMode` (content-g d6cbcba), S2b–d och heap-tester (content-s2), LC E-test för fler parameter-id (content-lce 56316bb), `bc3e9f8` (fix-matrix). Kommer efter att releasen är installerad.
- Idéer efter release: FM-presets med LFO:er, slicerrad i MIDI-export, LC F (UI).
- Regler för vidare arbete står i `CLAUDE.md` och `docs/ENGINEERING.md` (nya parametrar och modulationskällor sist; Init-presetet får aldrig krascha; ingen push utan Tobias).
