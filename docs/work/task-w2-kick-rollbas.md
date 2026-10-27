# Uppgift till Arbetare 2: kickar i PSY CLEAN G-stil + riktig rollbas

Från projektledaren, 27 okt 2026 (Tobias önskemål). Du arbetar bara i worktree `KickBase-VST-w2`, branch `w2`. Pusha aldrig, mergea aldrig, rör inte main. Svenska, korta rapporter med `fil:rad`. Egen logg: `docs/work/PROGRESS-w2.md`. Fel loggas i `docs/work/LESSONS.md`.

Läs först: `docs/kick-bass-school.md` (skolan), `docs/psytrance-kick-bass-reference.md`, `docs/sound-design-roadmap.md`, skillen `kickbase-sound-design`, `docs/work/preset-checklist.md`, `docs/work/PROGRESS-w2.md`.

## Bakgrund
Tobias: av alla kickar är bara **TK_Kick Psy140 Clean G** bra. Den ligger som användarpreset i `C:\Users\hunga\Documents\Kickbase\Presets\Kick\TK_Kick Psy140 Clean G.mofp` (även `TK_Kick Seismic Clean G.mofp`). Våra fabrikskickar (Clean Full-On m.fl. i `Source/Core/PresetData.inl:422`) är bara mätta, aldrig lyssnade. Dessutom: ingen av våra basar, särskilt rullande bas, duger.

## Del 1: kickar i samma stil (först)
1. Läs parametrarna i Psy140 Clean G (och Seismic Clean G) och mät med `KickbaseTool audit kick`. Skriv ner vad som gör den bra (svans, pitch-envelope, klick, mättnad).
2. Bygg 6–8 varianter som är lika men inte identiska: andra toner (minst C, D, F, A, utöver G), olika karaktär (punchigare, längre svans, mjukare attack, lite mer sub). Basera varje ton på tailHz för toppen i Hz, inte gissning.
3. Lägg dem sist i kicklistan (inga index får flyttas, inga nya parametrar/ID:n). Namn i stil med "Psy Clean C", "Psy Clean C Tight".
4. Mät varje med audit. Tobias lyssnar i FL; skriv raderna i `docs/work/fl-testlista.md`.

## Del 2: lär dig en riktig rollbas (utredning, bygg inget förrän jag godkänt)
Skriv en kort utredning i `docs/work/rollbas-utredning.md`:
- Vad en bra psytrance-rollbas består av (sub + mellanregister, envelope, filter, mättnad, mono under ~120 Hz).
- Samspel med kicken: sidechain (hur mycket, release), notlängd, tonen på 1/16 efter kicken, kickens svans mot basens grundton.
- Effekter och inställningar: FX-kedja, sidechain-inställning i Kickbase, soundstage/MONO BELOW. Hämta konkret från skolan `docs/kick-bass-school.md` och referensen.
- Mät vad våra nuvarande 6 rollbasar gör fel (audit bass) och vad en bra borde ha.
Föreslå sedan 3–4 basar med kompletta inställningar (inkl. FX och sidechain, så att kick+bas fungerar ihop), men bygg dem först när jag sagt ok.

Sök även på nätet (WebSearch/WebFetch): tutorials, producentgenomgångar och forum om psytrance rolling bass, kick+bass-sidechain och mixning. Skriv källorna (länk + vad du tog från dem) i utredningen. Kopiera inga preset eller samples; vi lär oss tekniken. Målet är en riktigt bra bas, så var grundlig.

## Regler
Preset-hash och ordning får inte ändras för gamla index. `build.bat Debug all` ska vara grönt före commit. Commit-slut: "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>". Rapportera när del 1 är klar eller om du är blockerad; fortsätt annars.

## Kategorier (tillägg)
Tobias vill ha kategorier i presetmenyerna (Arbetare 1 utreder hur). Nya kickar i Psy Clean-stil ska få kategorin **PSY CLEAN**, de nya basarna kategorin **PSY ROLLING BASS**. Håll kategorinamnet i en enda konstant/kolumn så det är lätt att koppla in när formatet är bestämt.

## Tillägg: GLIDE BASE och kategorin GLIDE BASSES (Tobias 27 okt)
Titta på Tobias användarpreset "GLIDE BASE" (mappen `C:\Users\hunga\Documents\Kickbase\Presets\`), mät och lyssna-fixa den så den låter bra och korrekt. Gör sedan kategorin **GLIDE BASSES** med några coola huvudpresets (kick + bas) där basen använder glide. Läs hur glide fungerar i synthen (DESIGN.md, bassens glide-parametrar) och skriv receptet i `docs/school/` (t.ex. `glide-bass.md`). Kvalitet före kvantitet. Kategorinamnet i den gemensamma konstanttabellen. FL-rader i fl-testlista.
