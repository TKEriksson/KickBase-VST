# Wavetables: rankning för rollbas

Status: **mätt**. Kommandot `KickbaseTool tablespectrum` (`Tools/Tool.cpp`, före audit-dispatchen) gör en naiv DFT av `displayFrame(0/8/15)` för varje inbyggd tabell och skriver spektralcentroid (i övertonsnummer) + övertoner 1–12 i dB relativt grundtonen. Obandbegränsad ram, så höga övertoner kan vara aliasrika och rader där grundtonen är ≈0 (Notch Saw f0, Psy Zap f15) är mätartefakter. Jag kan bara mäta; Tobias lyssnar i FL.

Mappning: valet <14 → direkt, 14 = User WAV, >14 = index−1; `classicIndex(shape) = 10+shape`, tabell 12 = saw, 13 = square, 10 = sine.

## Mätt (övertoner relativt h1, dB; c = centroid i övertonsnummer)
| Tabell | Ram 0 | Ram 8 | Ram 15 | Slutsats |
|---|---|---|---|---|
| Classic Saw (12) | c 13.5, h2 −6, h3 −10 | – | – | referens, ljus |
| Saw Soft | c 2.3, h2 −13, h3 −21 | c 6.7, h2 −9, h3 −14 | c 17.5, h2 −5, h3 −8 | pos 0→1 flyttar hela mellanregistret utan filter: **huvud-OSC A** |
| Saw Bright | saw | – | c 27.5, nästan platt (−2..−6) | för ljus, endast lågt |
| Psy Roll | c 6.9, h2 −3, h3 −11 | c 9.5, h2 −7, h3 −8, **h4 −5** | c 10.3, topp vid h6–h7 (−11/−9) | pos ger rörlig övertonstopp 2–7: **bäst som OSC B** |
| Psy Sub Punch | c 1.3, h2/h3 −16 | c 2.4, h3 −4, h5 −11 | c 3.0, h3 0, h5 −6 | bara udda 3/5, svagt mellanregister; sub + punch |
| Psy Acid | nästan sinus (c 1.1) | c 8.0, platt −6..−13 (resonanspuckel kring h8–h11) | c 12.7, −6..−18 | pos = cutoff; 303-karaktär för Forest |
| Psy Thump | sinus | c 2.7, h2 −8 | c 3.3, h2 −4 | mjuk sub-puck, inget mellan |
| Psy Growl A/B/C, Metal, Zap, Hoover | FM-/formantaktiga, ojämna toppar (±10 dB) | | | growl/zap, inte ren roll |
| Two Peaks | c 4.5 | topp h7–h8 (+13) | | ger en kraftig topp om man vill ha formant; risk för skarpt |
| Tilt Bright / Peak Harmonic | | | | enkla ljusare/mörkare saw-varianter |
| Square Soft, Triangle Bright, Odd Spectrum | bara udda | | | ihålig klang, lågt lager |

## Rankning för rollbas (mellan 250 Hz–1 kHz vid grundton ≈ 49 Hz = övertoner 5–20)
1. **Saw Soft** (OSC A): pos ≈ 0.3–0.6 ger c ≈ 5–8, dvs. övertoner över 250 Hz utan att filtret måste skapa dem.
2. **Psy Roll** (OSC B, +12 semi eller 0): rörlig topp h4–h7 ≈ 200–350 Hz vid 49 Hz; just det saknade registret.
3. **Psy Acid** (Forest, tillsammans med filtermodell 1).
4. Psy Sub Punch / Psy Thump: sublager när `sub` inte räcker.
5. Two Peaks pos ≈ 0.5: kandidat för Wide Mid-varianten (formant ≈ h7–h8 ≈ 350–400 Hz) – testas.

## Behövs ny tabell eller synthfunktion?
Nej: det som saknas i dagens basar är inte tabellerna utan att recepten (`rollBass`, `PresetData.inl:129`) inte använder dem. Saw Soft + Psy Roll täcker mellanregistret. Ingen ny tabell föreslås. Mätgap: tabellerna mäts obandbegränsade; faktisk ljudbild vid ton mäts med `audit bass`.
