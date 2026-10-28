# Plan: klick-lager 2 och 3 + sampleverktyg (Arbetare 2)

Tolkning: lager 1 är dagens klick. Lager 2 och 3 är nya brusklick med egen nivå, längd, ton, fördröjning och läge (BP/LP/HP). Sampleverktyg = klipp bort början, baklänges, utfasning på kickens sampling.

## Parametrar (alla sist i `addKickToggleParams`, standard = av = gamla ljudet)
| ID | Intervall | Standard |
|---|---|---|
| kickClick2Level / kickClick3Level | 0–100 % | 0 (av) |
| kickClick2Length / kickClick3Length | 1–30 ms | 10 |
| kickClick2Tone / kickClick3Tone | 200–16000 Hz | 2500 |
| kickClick2Delay / kickClick3Delay | 0–100 ms | 0 |
| kickClick2Mode / kickClick3Mode | Bandpass, Lowpass, Highpass | Bandpass |
| kickSampleStart | 0–200 ms | 0 |
| kickSampleReverse | av/på | av |
| kickSampleFade | 0–500 ms | 0 |

## Motor (`KickEngine.cpp`)
- Extra klick läggs till bara om nivån > 0 (inga extra slumptal annars, så hashen är orörd). Fönster: `[delay, delay+length)`, form som klicket, eget filter per röst. Gäller inte i ersätt-läge eller med synten av.
- Sampleverktyg: klipp (`startSamples`), baklänges (läser från slutet), utfasning (linjär sista ms). Längden (`totalSeconds`) följer klippet och lagren.

## Tester (`testKickClickLayersAndSampleTools`)
Av = bit-identiskt; lager tyst före fördröjning, hörs i fönstret, tyst efter (alla tre lägen); klipp, baklänges, utfasning på en ramp.

## UI behöver
Lager 2/3: kickClick{2,3}Level/Length/Tone/Delay/Mode (Mode = val). Sampling: kickSampleStart/Reverse/Fade. Hjälptexter i `Source/Ui/Help.cpp` (Arbetare 1).
