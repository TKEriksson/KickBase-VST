# Handoff: CYCLES (cycles per frame)

Branch: `main` (huvudmappen, ingen separat worktree). Lokala commits efter fda9b7d är opushade (1f20023 … 962b637). Pusha inte utan Tobias.

## Status
CYCLES-parametern (per osc, 1–32x, standard 1.0 = oförändrat ljud) är inskriven men OSTÄDAD/ocommitad:
- `Source/Core/Params.h`: `aCycles/bCycles/cCycles` sist före `count`, `oscCyclesField(osc)`.
- `Source/Core/Params.cpp`: namnlistan, registrering i `addMaxTimeParam`, `o.cyclesPerFrame` i OscParams-mappningen.
- `Source/Core/InstrumentSynth.h/.cpp`: `cyclesPerFrame`; fasen går `hz/N`, mip-nivån väljs efter `hz/N`.
- `Source/Ui/OscPanel.cpp`: CYCLES-ratt (12:e), `resized()` har nu 7 celler per rad.
- Bygget pågick sist (första försöket föll på `setTooltip`, ändrat till `setDescription`). Kontrollera att `build.bat Release` går igenom och att testerna passerar.

## Kvar
1. Bygg + kör `build/Release/KickbaseTests_artefacts/Release/KickbaseTests.exe`.
2. Skriv ett litet test: standard 1.0 ger samma utdata som förut; N>1 ger långsammare fas och ändliga värden.
3. Kolla layouten visuellt (2 × 6 rattar; HARMONICS är en liten knapp i titelraden).
4. Uppdatera `docs/work/plan-arbetsfordelning-rel03.md` (CYCLES klar) och `docs/work/PROGRESS.md` / `PROGRESS-pl.md` (0.4.1, 0.4.2, extra-takt-fixen, CYCLES; 2–3 KB).
5. Committa (avsluta med `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`). Rör inte `Source/Ui/DESIGNS.zip`.
6. Installera med `.\installAll.bat Release Release` (Lemon stängd) om Tobias vill testa.

## Senare (0.5)
Dragbar modulationsring, lyssna-knapp per motor, 3D-vy med fler frames, EMPTY som standardpreset + kick TK_Kick Psy140 Clean G, LIBRARIES (väntar på klartecken). Valfritt: auto-gissa CYCLES vid import.

## Tobias önskemål (0.5)
- CYCLES 1-32 behålls. Lägg till snap till hela steg (läge, så att kontinuerlig modulation fortfarande går).
- Föreslå CYCLES automatiskt vid laddning av wavetable/WAV (återanvänd cykellängdsdetekteringen från 0.4.1; frame-längd / grundperiod). Idag sätts inget automatiskt.
