# Reese-bas: unison och recept

Status: **mätt, ej lyssnat** (Arbetare 2 kan bara mäta; Tobias lyssnar i DAW).

## Hur det fungerar i Kickbase
- Unison finns redan per OSC (A/B/C): 1–7 röster, detune, blend, slumpfas, pan, stereo, width. Ingen ny kod behövdes.
- Klassisk Reese = två (eller tre) lätt förstämda sågor som slår mot varandra. Gör det med OSC A + OSC B, B finstämd +12 cent, valfritt C −12 cent som tredje lager.
- UI varnar när VOICES × unison > 24 (`Modules.cpp:1273`).

## Recept
1. Sub 0, noise 0 (låt Reese bära sig själv; lägg sub separat vid behov).
2. OSC A och B: saw, unison 3–5, detune ca 20 cent. B `bFine` +12, valfritt C `cFine` −12.
3. Mono eller få röster (VOICES 1–3), hög sustain, filter cutoff 600–1500 med låg envAmount.
4. Kicken duckar basen; EqHp 150–200 Hz på sendkanalen så att sidan under det är ren.

## Mätning (`KickbaseTool reesetest`, Debug, 4 s, 48 kHz, block 512, tre toner vid VOICES>1)
| Recept | CPU/s | alloc | peak dB | DC | lowSide dB |
|---|---|---|---|---|---|
| 1+1 (ingen unison) | 0.159 | 0 | −5.4 | 0.000 | −120 |
| 3+3, 20 cent | 0.146 | 0 | −5.0 | 0.000 | −25.1 |
| 5+5 | 0.176 | 0 | −4.8 | 0.000 | −26.2 |
| 7+7 | 0.156 | 0 | −4.7 | 0.000 | −26.6 |
| 7+7+7 | 0.163 | 0 | −4.7 | 0.000 | −26.6 |
| 7+7, VOICES 3 | 0.167 | 0 | −5.1 | 0.000 | −25.0 |

- 0 allokeringar på ljudtråden, DC 0, allt ändligt. `blocksizetest` identiskt ljud för block 32–2048.
- CPU skiljer sig knappt mellan 1+1 och 7+7+7 (Debug-bygge, mäts mot hela kedjan), så unison är inte flaskhalsen.
- **lowSide ca −25 dB**: unison ger stereoinnehåll även i basen. Vill man ha mono under ca 150 Hz: sänk stereo/width på OSC eller använd EqHp/mono-lågband på mixen. Det är ett val, inte ett fel.
- Gap: inget funnet, ingen motorkod ändrad.
