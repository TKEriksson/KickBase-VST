# Genrepresets (v1)

Byggda 2026-10-28 av Arbetare 2: per genre 1 kick + 2 basar, sist i `kickPresets()`/`bassPresets()` (`Source/Core/PresetData.inl`). Kategori sätts av namnprefix i `Source/Core/PresetCategories.cpp:factoryCategory`. Ingen genreutredning med källor är gjord ännu; värdena är startpunkter att lyssna på, inte facit.

## Hur de är byggda
- **Kick**: `psyCleanKick(ton, svep, tid, längd, attack, nivå, klick, drive)`. Svansens Hz = 32,7·2^(ton/12). Psy-genrer (Goa, Dark Psy) ligger vid 35 Hz; de andra högre (Minimal 48, Dance 50, Trance 47, Techno 52, Empirikal 40 Hz) eftersom de inte följer psyets 35–50 Hz-mål.
- **Bas**: `registerBass(...)`, ett lager, såg på OSC A, andra tabell på OSC B, unison för bredd. Oktav 1 för sub/rull/growl, oktav 2 för pluck/stab/acid.

| Genre | Kick | Basar | Tanken |
|---|---|---|---|
| MINIMAL | Minimal Kick (mjuk, lång svans) | Minimal Sub, Minimal Pluck | Sparsamt, mjukt, lite filterrörelse |
| GOA | Goa Kick | Goa Roll, Goa Acid | Klassisk rullande bas, syra med resonans |
| DARK PSY | Dark Psy Kick (längre, drive) | Dark Psy Growl, Dark Psy Sub | Mörkt, drivet, långsamt filter |
| DANCE | Dance Kick (kort, klick) | Dance Pump, Dance Pluck | Pumpande, lätt att sidechaina |
| TRANCE | Trance Kick | Trance Roll, Trance Pluck | Bred unison, ljus pluck |
| TECHNO | Techno Kick (hård, mycket klick) | Techno Sub, Techno Stab | Hård kick, kort stab |
| EMPIRIKAL | Empirikal Kick | Empirikal Reese, Empirikal Growl | Reese-detune och growl |

## Mätt (audit)
Kickar: peak −2,8…−3,4 dB, ingen klippning. Basar: peak −9…−12 dB, ingen klippning. Ej lyssnat; se `docs/work/fl-testlista.md`.

## Nästa steg
Genreutredning (tempo, kickkaraktär, basrörelse, källor) för [minimal.md](minimal.md) och [goa.md](goa.md), därefter finjustera efter Tobias lyssning i Lemon.
