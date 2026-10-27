# Wavetables: rankning för rollbas

Status: **analytisk** rankning utifrån definitionerna i `Source/Core/Wavetable.cpp` (ca 120 inbyggda tabeller). Spektrummätning av varje tabell (harmonisk avtrappning) är inte körd än; görs innan basarna byggs (förslag: liten verktygskommand som renderar varje tabell vid pos 0/0.5/1 på 49 Hz och skriver centroid + harmoniska 1–16 i dB). Rankningen nedan är alltså hypoteser som mätningen ska bekräfta eller ändra.

Mappning: valet <14 → direkt, 14 = User WAV, >14 = index−1; `classicIndex(shape) = 10+shape`, tabell 12 = saw, 13 = square, 10 = sine.

## Kandidater (hög → låg lämplighet)
| Tabell | Rad | Varför | Anmärkning |
|---|---|---|---|
| Saw Soft | `Wavetable.cpp:291` | amplitud k^−alpha, alpha 2.2→0.8: mjuk→ljus, ren harmonisk trappa | huvudoscillator; pos styr mellanregistret utan filter |
| Psy Roll | `:515` | k^−1.15 med topp kring övertone 2–7 (pos) | ger "rörelse" i mellanregistret som filtret annars måste skapa; bra som OSC B |
| Psy Sub Punch | `:520` | grundton + 3:e/5:e/7:e via pos | sub + punch, mest för lågt register / tight |
| Psy Acid | `:531` | resonansformad envelope (q 9), cutoff via pos | 303-aktig, för Forest/acid-varianten |
| Saw Bright | `:296` | alpha 1.0→0.3, upp till 160 övertoner | för ljus utan filter; risk för aliasing/skärande, endast lågt i mix |
| Square Soft / Triangle Bright | `:301,:306` | bara udda övertoner | hålig klang, bra som lågt lager för "ihålig" bas |
| Classic Saw (12) | – | referens | det alla nuvarande Rolling-basar använder |
| Psy Growl A–C, Zap, Metal | `:525–` | FM-/vokalliknande | för growl-/zap-bas, inte för ren roll |

## Saknas
- Tabell med **tydlig 2:a–4:e övertonstopp som är stabil över pos** (som Psy Roll men fast vid låg frekvens) för bas som bär på små högtalare.
- Ren **sub-sinus med mild tredje övertonssättning** finns som Psy Sub Punch; ingen med mjuk 2:a övertons-"filt". Kan ersättas av `sub` + `drive`.

## Rekommendation
Ingen ny tabell behövs för v1 av rollbasen: Saw Soft + Psy Roll + sub räcker. Ny tabell eller funktion föreslås bara om mätningen visar att mellanregistret (250–1k) fortfarande ligger >10 dB under <60 Hz.
