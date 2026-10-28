# FUNDAMENTAL DUCK (recept)

**Problem:** kick och bas ligger på nästan samma grundton (30–60 Hz). Då adderas de i fas eller tar ut varandra, och kicken tappar slag. Sidechain-ducken sänker hela basen; FUNDAMENTAL DUCK sänker bara basens grundton och lämnar mellanregistret (det som "rullar") orört.

**Hur det funkar** (`Source/Core/FundamentalDuck.h`): ett smalt peaking-filter (RBJ, Q 1,4) som ligger på basens senast spelade ton och dras ner medan kicken klingar. Djupet följer kickens envelope gånger en fönstervikt i cent. Av = full bypass, bit-identiskt.

**Parametrar** (instrumentnivå): `fundamentalDuckOn`, Depth 0–24 dB, Window 10–1200 cent, Source (Kick / Instr 1–10 / Any). Per motor: Inherit / Off / Own.

**Recept i presets** (`fundDuck` i `PresetData.inl`):
- Rollande bas (PSY ROLLING 1–4, GLIDE Acid/Dive): Depth 9 dB, Window 200 cent.
- Glide Sub (mycket sub, mer att göra): Depth 12 dB, Window 250 cent.
- Bas-only-presets rörs inte: ducken beror på vilken kick de spelas mot.

**Mätning (audit full, före → efter):** effekten är liten i bandsiffrorna eftersom filtret är smalt och bara verkar medan kicken klingar: 60–250 Hz ≤ 0,1 dB, <60 Hz oförändrat, Glide Sub >5 kHz −64,3 → −61,1 dB. Audit har inget kollisionsmått; kolla i DAW.

**Lyssna i DAW:** slå av/på `fundamentalDuckOn` på Roll Full-On och Glide Sub. På: kickens slag ska bli tydligare utan att basen tunnas ut. Om basen "hackar": minska Depth eller öka Window.
