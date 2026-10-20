# Presetlista (underlag för fas E0)

En rad per nytt tillstånd som en fas lägger till. Var sparas det: P = preset, J = projekt, G = global inställning, T = egen presettyp.
InitPreset / MCP: behöver InitPreset eller MCP-verktygen (get/set, preset-skrivaren) känna till det?

| Fas | Tillstånd | Sparas | InitPreset / MCP | Kommentar |
|---|---|---|---|---|
| R | `lfonSnap`, `lfonSnapRange` (LFO 1–3) | P | MCP: ja (parametrar) | om SNAP godkänns |
