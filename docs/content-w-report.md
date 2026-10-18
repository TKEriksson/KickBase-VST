# Fas W: Morph-knapp för wavetable-position (gren `content-w`, från `content-d1`)

Skriven av Sonnet 18 okt 2026 (spec godkänd av Opus). Tobias önskade en Morph-knapp som i Pigments: med Morph på tonar POS över mellan vågformerna, med Morph av hoppar den mellan dem.
**Med standardvärden är ljudet bit för bit som förut** (Morph är på): `rendersig`, `rendersigreal`, `rendersigwide` och `rendersigtb303` är identiska med baslinjerna.

## 1. Vad som byggts

- **Parametrar:** `bassNAMorph`, `bassNBMorph`, `bassNCMorph` (bool, standard på), sist i `InstrumentField` (`aMorph`, `bMorph`, `cMorph`) med hjälpfunktionen `ParamID::oscMorphField(osc)`. Visningsnamn "Instr N Osc 1 Morph" (som övriga oscillatorparametrar, som i gränssnittet säger OSC 1–3).
  30 nya rader i `docs/audit/params-instrument.txt`; gamla projekt utan fälten får standardvärdet (på).
- **Motorn:** `Wavetable::sample(position, phase, maxHarmonics, morph)`. Med Morph av slås frame upp direkt (`Wavetable::stepFrame`), utan interpolation, efter all modulering och per sampling precis före tabelluppslaget.
  Positionen snäpps till `round(pos × (steps−1)) / (steps−1)`, och frame blir `round(steg × 15 / (steps−1))`.
- **Antal steg** (`Wavetable::getSteps`): Basic 4 (sinus, triangel, såg, fyrkant = frames 0/5/10/15), övriga inbyggda 16, en tabell av en WAV så många cykler filen hade (högst 16), en enkel cykel 1, `fromFrames` utan uppgift 16.
- **Sparas med projektet:** attributet `steps` på tabellens `Table`-nod (projekt, Full-preset och instrument-preset). Saknas det (äldre projekt) blir det 16; äldre byggen ignorerar attributet.
- **Gränssnittet:** MORPH-knappen i raden SIN TRI SAW SQR [MORPH] WAV i OSC-panelen, med tooltip. Utan Morph ritas den spelade kurvan på den frame som spelas. Hjälptext `bassAMorph` (B och C använder den) och en rad i AGENT_GUIDE:
  "Steps jump; fast modulation of POS can click."

## 2. Tester

- **Enhet** (`KickbaseTests`, Wavetable): 4 cykler ger 4 steg, en enkel cykel 1, Basic 4; Morph av på ett stegs egen plats = Morph på (under −120 dB); Morph av håller samma våg genom hela steget; Morph på blandar (≠ Morph av mellan två steg);
  Basic hoppar mellan sinus, triangel, såg och fyrkant; `fromFrames` bär antalet steg, saknas det blir det 16.
- **`persisttest` 4b:** en WAV med 4 cykler, Morph av, spara och ladda (projekt) och instrument-preset till ett annat instrument: 4 steg, Morph av, samma ljud med Morph av (under −70 dB; tabellen lagras som 16 bitar).
- `blocksizetest`, `audiotest` (0 allokeringar) och `boundarytest` gröna; `helpcheck`: 0 utan förklaring.
- **Gränssnitt:** 42 skärmbilder mot `content-d1` (`Tools/pngdiff.py`): 15 identiska, de övriga skiljer bara i OSC-vyn (x 45–885, y 172–387), MORPH-knappen får plats. En bild (MATRIX, `tab26`) har en pixel utanför rutan som skiljer (den kända flackande pixeln).

## 3. Anmärkningar

- **Klick:** Morph av ger hårda hopp. Snabb modulering av POS (LFO) kan klicka, som i Pigments. Står i hjälptexten. En 1–2 ms mjukning kan läggas till om det hörs.
- **Init-instrumentet använder tabellen Classic Sine**, inte Basic (där POS gör något). Tobias vill ha Basic som standard för ett nytt instrument; att ändra fabrikspresetet "Init" ändrar `rendersigreal` (regel 4), så det är en fråga till Opus och Tobias, inte gjort här.
