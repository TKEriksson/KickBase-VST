# Presetlista (underlag för fas E0)

En rad per nytt tillstånd som en fas lägger till. Var sparas det: P = preset, J = projekt, G = global inställning, T = egen presettyp.
InitPreset / MCP: behöver InitPreset eller MCP-verktygen (get/set, preset-skrivaren) känna till det?

| Fas | Tillstånd | Sparas | InitPreset / MCP | Kommentar |
|---|---|---|---|---|
| R | `lfonSnap`, `lfonSnapRange` (LFO 1–3) | P | MCP: ja (parametrar) | om SNAP godkänns |
| M | `activeLfos` (mask, standard 7 = LFO 1–3), `LfoCurve4–10`, parametrar `lfo4…lfo10*` (bara instrument) | P, J | MCP: ja (set_modulation lfo 1–10 aktiverar, remove; get_state active_lfos) | LFO:er med kopplingar räknas som aktiva vid laddning. FX har bara 1–3 |
| R | radens längd: `lengthOf/manualOf/extraOf` per rad (mönsterträdets "length"/"extra" i state, .roll och presets) | P, J | MCP: ja (get_state bars/length_mode/content_bars/extra_bars, set_pattern length/trim/repeat_to) | sparas i P (mönsterpreset/radpreset) och J (projekt). Gamla projekt: applyLegacyLengths från patternBars/rowBars |
