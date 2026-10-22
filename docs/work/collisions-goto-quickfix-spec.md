# COLLISIONS: "Gå till" och Quick-Fix (Tobias idé 2026-10-21)

Idé: gör COLLISIONS-vyn (Source/Ui/CollisionsView.h) till ett arbetsverktyg, inte bara en lista.

1. **Gå till**: klick på ett fynd ("INSTR 1 and INSTR 2 collide at 80-100 Hz") hoppar synthen till rätt ställe: instrumentets flik, dess EQ, med en markör vid fyndets frekvens (t.ex. 90 Hz, mitten av bandet i f.lowHz/f.highHz).
   - Behövs: ett navigeringsanrop från vyn till editorn (välj instrument, välj flik) och en markör/överlagring i EQ-grafen. Fyndet har redan lowHz/highHz och källor.
   - Litet till medelstort, rent UI, inga nya parametrar.
2. **Quick-Fix-knapp** vid varje varning: "Auto-EQ Notch" (skär ett smalt band hos den svagare källan) eller "Sidechain Fix" (mer ducking för basen).
   - Ändrar ljudparametrar, så följ samma regel som dagens fasfynd: förslag, A/B-lyssning, inget ändras förrän Tobias godkänner (se "SUGGESTED FIXES" i CollisionsView.h). Ångra via undo.
   - Behöver ett val av vilken källa som ska vika (den svagare), och att EQ-bandet återanvänds om det redan finns.
   - Medelstort till stort. Gör "Gå till" först, sedan Quick-Fix.
3. Ordning: 1) Gå till. 2) Quick-Fix med A/B. Tas efter release och LC, helst när ny veckolimit kommer.
