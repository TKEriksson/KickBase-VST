# Release 0.4 – utkast till PDF-innehåll

Skrivet av Arbetare 2 ur git log (slice-roll, fx-matrix, pl-jog). Utkast: projektledaren och Tobias justerar. Utanför 0.4 (flyttat till 0.5): (p), (q), (w), FX punkt 4.

## Effekter och mixer
- **FX-matris**: gamla SEND-länkar flyttas automatiskt till FX-matrisen när ett projekt laddas, och INS visas som en egen rad.
- **Ta bort FX-kanal**: i FX-popupen finns "- remove FXn" på sista kanalen. Har kanalen effekter eller sends frågar den först; Ctrl+Z återställer effekter, EQ, kompressor, nivå och sends.
- **PATTERN-raderna**: FX-ratten är borta, VOL är större och FX-knappen visar "N x FX".
- **MIX-fliken** (m): vågen har full bredd och ligger överst, ratterna under; WIDE/NARROW är borta.

## Utseende
- Gemensam kantglöd, tydligare menynivåer, hover i menyer, glasknappar och glas-PREVIEWS.
- **Glödstyrka** Av/Svag/Stark i SETTINGS (av som standard).
- Mer luft i menyer och jämnare avstånd.
- Stor OUT-ratt.
- Ratt och etikett sitter ihop som ett block.
- SWING/HUMAN/DELAY: en ratt per rad.
- SAMPLE är uppdelad i tre rutor: PITCH, AMP och REVERSE.

## Diagram och vyer
- **OSC OUTPUT** (h): klicka MODE-etiketten tills det står OUTPUT för att se den riktiga utsignalen (phase, warp och unison med detune/blend).
- **TIMING-diagram** (l): alla sju motorerna på en gemensam tidsaxel med volymkonturen. Dra vänsterkanten (POS DELAY) och högerkanten (LENGTH). FOCUS/OVERVIEW.
- **Litet tidsdiagram på INSTR** (f): skrivskyddat; klick öppnar TIMING.
- **FM-diagram** (k): vågform och index över tid.
- **Harmoniska editorn** (r): tooltips på varje fält och en HELP-flik.

## Provlyssning och JOG
- **En provlyssning i taget** (u): PREVIEW släcks när man provlyssnar lokalt (högtalare, LISTEN, tangent, slice, JOG).
- **JOG TEST BPM** (t): egen ratt för tempot vid JOG-test.
