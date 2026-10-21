# Vår synth i korthet (för dig som sammanfattar guider)

**Kickbase** (TKDEV): VST3 för psytrance-kick + rullande bas i *samma* plugin, så att kick och bas kan fasjusteras och duckas sample-exakt. Fullt dok: [../DESIGN.md](../DESIGN.md), kunskap: [../psytrance-kick-bass-reference.md](../psytrance-kick-bass-reference.md), parametrar/AI-verktyg: [../AGENT_GUIDE.md](../AGENT_GUIDE.md), plan: [../plan-v2.md](../plan-v2.md). Läs `psytrance-kick-bass-reference.md` först och **upprepa inte** det som redan står där.

## Vad som finns (mappa reglerna mot detta)
- **Kick:** 16-stegs sekvens. Sinus-kropp där *pitch-envelope* (upp till 0,5 s) och *amp-envelope* (upp till 2 s) ritas som fria kurvor med noder. Plus övertoner, klick (syntetiskt eller sampling), drive. KEY = Follow (svansen stäms efter DAW-tonen) eller Fixed (C1 + TUNE). Biblioteket har kick-samplingar (SYNTH|SAMPLE-flikar).
- **Instrument 1–10 (bas, lead, plucks):** eget mönster, egen ljudkedja, egen FX-kanal. Toner fritt i tiden (96 ticks/fjärdedel, rutnät 1/4–1/64, trioler), pianorulle, per ton: start/längd/tonhöjd/velocity/glide. LENGTH-rad (på/av-rytm som hackar ljudet).
- **Ljudkedja per instrument:** 3 oscillatorer (wavetables: Basic, PWM, Sync, Harmonics, FM, Growl …; klassiska SIN/TRI/SAW/SQR; warp: sync/bend/PWM/mirror/FM; unison ≤7, detune, fas, slumpfas (RAND), pan/stereo per osc) + sampling + sub + brus → **filter** (LP24/LP12/band/high, Ladder, TB-303, Formant, Comb, filter 2 seriellt/parallellt, keytrack, egen kort filter-envelope per ton) → drive → ADSR/glide.
- **Modulering:** 3 LFO (tempo-synk), 10 på väg (fas M); dra LFO/ENV på valfri ratt, flera per ratt.
- **Kick+bas:** **ducking** per instrument (djup + egen ritad kurva, SIDECHAIN-fliken), kickens envelope eller egen kurva.
- **Effekter:** FX-kanaler 1–11, 5 platser/kanal, 15 effekttyper, EQ (parametrisk) + kompressor per kanal. **MASTER:** mixer (gain/pan/mätare), SC på/av, MAX = maximizer (drive, clip, lookahead-limiter), SOUNDSTAGE (spektrum, vektorskop, bredd, mono-under-frekvens).
- **Analys:** ANALYZER (spektrum, kollision kick/bas, rapport, offline-rendering, deterministisk). AI/MCP-verktyg kan läsa/sätta parametrar och rendera.

## Vad vi planerar (föreslå gärna regler som motiverar dem)
Fasverktyg (fasrotation av grundtonen efter filter, övertons-/fasredigerare), kickverktyg (markörer 1/16 och 1/8, svansfrekvens, 35–43 Hz-zon), Analyzer-sida (faskorrelation per 16-del, kollisionsvarning), Low-End shaper (30–400 Hz), FM-källa, fler effekter (disperser, multiband-distorsion, shimmer, widener), live code (Strudel), kunskapsbas för AI.

## Det vi behöver ur guiderna
1. **Regler med tal** vi kan lägga i AI-kunskapen och som varning i analysatorn.
2. **Arbetsflöde:** i vilken ordning och med vilka mätningar en producent gör kick+bas rätt.
3. **Verktyg som saknas** i listan ovan (skriv "SAKNAS → verktygsidé").
4. **Motsägelser** mellan källor (en rad: A säger X, B säger Y).
Terminologi: kick = KICK; bas/lead = INSTR 1–10; filter-envelope per ton = "plocket"; LENGTH = radens av/på-rytm.
