# Plan: sample-editor-kärna (Arbetare 2, rel-0.3)

Mål: lagrets sampling får en AMP-kurva och en PITCH-kurva över tid (som kickens AMP/PITCH), var och en med på/av. Av = exakt gamla ljudet.

## Parametrar (sist i `InstrumentField`, efter `sampleSource`)
| Fält | ID-suffix | Standard | Betydelse |
|---|---|---|---|
| `sampleAmpOn` | SampleAmpOn | av | AMP-kurvan på/av |
| `sampleAmpMs` | SampleAmpMs | 300 ms (10..2000) | kurvans längd; efter den är nivån 0 |
| `sampleAmpShape` | SampleAmpShape | 0 (-1..1) | 0 = linjär nedtoning, + = snabbt fall (konvex), − = långsamt fall |
| `samplePitchOn` | SamplePitchOn | av | PITCH-kurvan på/av |
| `samplePitchSemis` | SamplePitchSemis | 12 (-48..48) | startavvikelse i halvtoner, glider mot 0 |
| `samplePitchMs` | SamplePitchMs | 100 ms (5..2000) | glidtid |
| `samplePitchShape` | SamplePitchShape | 0 (-1..1) | som AMP |

## Motorn (`InstrumentSynth.cpp`, sample-grenen)
- Tid sedan anslag räknas i samples (`sampleAge`, nollas i `noteOn`).
- AMP: nivå × `curve(1 - age/ms, shape)`; 0 efter ms.
- PITCH: hastighet × `2^(semis * curve(1 - age/ms, shape) / 12)` (gäller icke-slice, även multisample). Slice-röster rörs inte av PITCH (originalhastighet); AMP gäller även dem.
- `samplePosition` rörs bara när samplingen faktiskt spelas (DELAY från Arbetare 1 ska stoppa den; faktorn läggs efter min kärna).
- Av = ingen multiplikation alls, ljudet bit-identiskt.

## Tester
Av = identiskt med förut (render-hash orörd); AMP efter `ms` är tyst; PITCH +12 första samplet går dubbel hastighet.

## UI behöver (Arbetare 1)
Sju parametrar: `bass{N}SampleAmpOn/Ms/Shape`, `bass{N}SamplePitchOn/Semis/Ms/Shape` via `ParamID::bass (layer, InstrumentField::…)`. Kurvorna ritas som kickens AMP/PITCH; samma formel finns som `InstrumentSynth::sampleCurve (float remaining, float shape)` (statisk, ren) så att UI kan rita exakt det som låter.
