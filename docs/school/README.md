# Preset-skolan (Arbetare 2)

Samlat vad vi lär oss om att bygga presets, så arbetet kan fortsätta senare. Kort och faktabaserat; skrivs löpande.

## Index
| Fil | Innehåll |
|---|---|
| [kick.md](kick.md) | PSY CLEAN-kickar: vad som gör Psy140 Clean G bra, tailHz per ton |
| [rollbas.md](rollbas.md) | Rollbas-utredningen: uppbyggnad, mätning av nuvarande, förslag, källor |
| [wavetables.md](wavetables.md) | De ~120 tabellerna: rankning och rekommendation för rollbas |
| [kick-bass-samspel.md](kick-bass-samspel.md) | Sidechain, notlängd, svans mot grundton |
| [mixning-fx-sidechain.md](mixning-fx-sidechain.md) | FX-kedja, sidechain-inställning, MONO BELOW |
| [genrer.md](genrer.md) | Genrepresets v1 (7 genrer × 1 kick + 2 basar): bygge, tabell, mätning |
| [minimal.md](minimal.md) | MINIMAL (utredning ej gjord, v1-presets finns) |
| [goa.md](goa.md) | GOA (utredning ej gjord, v1-presets finns) |
| [presets-logg.md](presets-logg.md) | En rad per preset: kategori, nyckelparametrar, mätt, ändrat, höll inte |

Äldre, flyttas inte (andra filer länkar dit): [../kick-bass-school.md](../kick-bass-school.md), [../psytrance-kick-bass-reference.md](../psytrance-kick-bass-reference.md), [../sound-design-roadmap.md](../sound-design-roadmap.md).

## Kategorier
Kick = PSY CLEAN. Bas = PSY ROLLING BASS. Färdigt huvudpreset (kick + bas + FX) = PSY ROLLING. MINIMAL och GOA gäller kick, bas och huvudpreset. Namnlistor per kategori står (versaler) i `docs/work/PROGRESS-w2.md`; projektledaren kopplar in dem vid merge.

## Så bygger man ett preset, steg för steg
1. **Läs**: skolan och referensen ovan, rätt ämnesfil här, `docs/work/LESSONS.md`. Gäller ett bibliotek: läs dess dokumentation först.
2. **Mät utgångsläget**: `KickbaseTool audit <kick|bass|full> all` (`Tools/PresetAudit.inl:40`, verktyget `build\Debug\KickbaseTool_artefacts\Debug\KickbaseTool.exe`). Notera kolumnerna.
3. **Bygg**: nya presets sist i listan i `Source/Core/PresetData.inl` (inga index flyttas, inga nya parametrar/ID:n). Basera tonen på tailHz i Hz, inte gissning.
4. **Audit igen** och iterera ett steg i taget mot skolans mål (svans 35–50 Hz, kick ≤ 1/8, mono under ~120 Hz, DC 0, ducking). Släng det som inte håller, anteckna varför i `presets-logg.md`.
5. **Kick + bas ihop** (`render_preview`, `analyse_mix`) för basar och huvudpresets.
6. **Bygg**: `build.bat Debug all` grönt (PowerShell: `cmd /c ".\build.bat Debug all"`).
7. **FL-rad** i `docs/work/fl-testlista.md` (Tobias lyssnar; vi kan bara mäta).
8. **Logga** i `docs/work/PROGRESS-w2.md` och `presets-logg.md`; commit med Co-Authored-By-raden. Aldrig push/merge.
