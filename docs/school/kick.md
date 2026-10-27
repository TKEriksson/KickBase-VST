# Kick: PSY CLEAN

## Motor (kort)
- Frekvens: `tailHz * exp2(pitchSemitoneEnv(t) * pitchAmount / 12)`. Pitch- och ampenvelope har punkter `{tid, värde, tension}`.
- tailHz: `Params::kickTailMidiNote` (`Source/Core/Params.cpp:462-469`), `readKick` (`Params.cpp:561-567`). Med kickKeyFollow Fixed (0) är tailHz = 32.7 · 2^(tune/12).
- Psy Clean-tonerna (Fixed + tune): C ≈ 33, D ≈ 37–38, F ≈ 45–50, G ≈ 50, A ≈ 56 Hz.

## Regler från skolan
Kicken slutar inom ca 1/8 (≈ 206 ms vid 145 BPM); svans 35–50 Hz. Notera: Myloops (se rollbas.md) ger 50–70 Hz för kickens grundton, vilket krockar med skolans 35–50; vi följer skolan och noterar skillnaden.

## Status
Del 1 klar (commit 76ad77e): 8 Psy Clean-kickar i `Source/Core/PresetData.inl` (`psyCleanKick`, kategorin i konstanten `psyCleanCategory = "PSY CLEAN"`). Referens: användarpresetet TK_Kick Psy140 Clean G, det enda Tobias tycker är bra. Mätvärden per kick: se `presets-logg.md` och `docs/work/PROGRESS-w2.md` (Del 1).
