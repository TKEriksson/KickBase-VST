# Glidebas: så fungerar det och recept

Status: **mätt, ej lyssnat** (Arbetare 2 kan bara mäta; Tobias lyssnar i DAW).

## Hur glide fungerar i Kickbase
- Glide är en **flagga per ton** (`Pattern.h:127`), inte en parameter. Parametern `F::glide` är oanvänd (`Params.cpp:305`).
- En ton med glide-flaggan **glider från föregående tons tonhöjd till sin egen** och glidet varar **tonens egen längd** (`InstrumentProcessor.cpp:1061`: `glideTo (pitch, längd i samplar)`). Ingen ny stämma startas, så ampenveloppen går vidare (legato).
- Glidet sker i log2(Hz), alltså jämnt i halvtoner (`InstrumentSynth.h:260`).
- I preset-texten: `x+7~:200` = normal ton, +7 halvtoner, glide, längd 200 % av ett steg. `:procent` är andel av ett steg (16-delsgrid), så glidtid = steg × procent.
- Glidet hörs bara om tonen är **tillräckligt lång**: 12 ticks (~80 ms vid 145 bpm) hinner knappt höras; 60+ ticks (en sextondel ≈ 100 ms; 200 % ≈ 200 ms) hörs tydligt.

## Recept (allmänt)
1. Mono-bas (voices 1), hög sustain (0.7–0.8) och lång amp-decay så att tonen inte dör innan glidet är klart.
2. Håll filtret öppet nog (cutoff 700–1800) och låg envAmount; annars klipper filterenveloppen glidet.
3. Glide-tonen ska följa en normal ton; först en rotton, sedan `~` till kvint (+7), oktav (+12) eller ner (−5).
4. Längd 100–300 %: 100 = snabbt "plopp", 200–300 = tydligt dyk.
5. Kicken ducka basen (Duck 0.85) och EqHp 150–200 Hz på sendkanalen.

## GLIDE BASE (Tobias fil)
Sinus-bas med sub 0.3 och två glide-toner (tick 108 och 312) på 12 ticks. **Fix:** glide-tonerna förlängda till 60 ticks (`108/60/0/1`, `312/60/0/1`) så att glidet hörs; allt annat orört. Backup: `GLIDE BASE.mofp.bak` i samma mapp.

## Presets (kategori GLIDE BASSES, `PresetData.inl`: glideBass1–3)
| Namn | Bas | Mönster | Mätt (peak / RMS / centroid) |
|---|---|---|---|
| Glide Sub | Saw Soft pos 0.3 + Sub Punch, sub 0.25 | rotton, kvint, oktav, glid ner | −0.3→ ca −1.5 / −11 / 78 |
| Glide Acid | Psy Acid, TB-303-filter | korta 100 %-slides mellan sextondelar | −1.2 / −11 / 84 |
| Glide Dive | Saw Soft 0.6 + Psy Roll | oktavfall 200–300 % | −2.0 / −11 / 84 |
