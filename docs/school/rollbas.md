# Rollbas-utredning (psytrance)

Status: utredning, **inget byggt** tills projektledaren sagt ok. Jämförelse med Tobias Gemini-svar görs när det kommer. Allt nedan är mätt (audit) eller hämtat med källa; förslag är markerade som förslag. Jag kan bara mäta, inte lyssna; Tobias lyssnar i FL.

## 1. Vad en bra rollbas består av
- **Sub + mellanregister.** Sub (sinus/mjuk saw på kickens grundton) bär golvet; mellanregistret (250 Hz–1 kHz, övertoner 2–7) gör att basen hörs på små högtalare. Lägg gärna lätt mättnad (Gearnews, Gearspace).
- **Envelope.** Kort puckel per 16-del: amp decay ca 60–90 ms, sustain lågt, release 20–40 ms (Sokolovskiy). Filterenvelope ger "pluck": +2..+4 oktaver, decay 30–90 ms.
- **Filter.** LP 24 dB/oct, cutoff 150–400 Hz, resonans 10–30 % (referensen). TB-303-modellen (`filterModel` 1) för acid-karaktär.
- **Mättnad.** Lätt rör/mjuk drive före filtret eller efter; mer bara på mellanlagret.
- **Mono under ~120 Hz**, bredd bara på mellan/höjd (Armada, Gearnews).
- **Fas.** Samma fas varje not (random 0, phase reset); starta vid nollgenomgång (Ableton-forumet).

## 2. Samspel med kicken
Se [kick-bass-samspel.md](kick-bass-samspel.md) och [mixning-fx-sidechain.md](mixning-fx-sidechain.md). Kärna: sidechain är inte valfri för 16-delsrull (Producer School); kick klar inom ~1/8; första basnoten velocity 60–85 %; basrot nära kickens svansfrekvens; ingen basringning in i nästa kick.

## 3. Mätning av nuvarande basar (`audit bass`, bas solo)
Rolling Classic/Tight/Deep/Bright/Fat/Soft/Snap är praktiskt taget identiska: centroid 81–95 Hz; <60 Hz −2.3..−3.2 dB, 60–250 −3.2..−4.2, 250–1k −13.6..−15.4, 1–5k −23.2..−25.3, >5k ≈ −35; dc 0; side ≈ −34.8; lowsd ≈ −40.4. Undantag: Hi-Tech Zap (centroid 257, <60 −9.7), Wide Reese (side −26.4).
**Vad som är fel**: nästan ren låg sinus/saw, inget mellanregister (250–1k är >10 dB under bas); alla använder samma recept via `rollBass` (`Source/Core/PresetData.inl:129-136`): saw (tabell 12) + valfri OSC B (10/12/13) låg nivå, sustain 0.7–0.9, amp decay 120 / release 12 ms (nästan ingen pluck från ampen), ingen unison/warp/fas/random, ingen rörelse. Skillnaderna är små och inte hörbara via siffror. Ingen av dem sätter FX, sidechain, filtermodell eller voices.
**Vad en bra har** (mål, förslag): centroid ca 150–300 Hz; 250–1k inom ca −6..−9 dB från bas; 1–5k −15..−20; mono under 120 Hz (lowsd/side nära dagens); tydlig envelopepuckel; annorlunda karaktär mellan varianterna (centroid och mellanband skiljer >3 dB).

## 4. Förslag: 4 basar (PSY ROLLING BASS), kompletta inställningar
Alla: mono, phase reset varje not, random 0, voices 1, MONO BELOW ≈ 120 Hz, sub sinus på basroten (stäm mot kickens tailHz), FX: lätt drive + EQ (HP 30 Hz), sidechain `scMode` = Kick envelope, bassNDuck 0.7, scLength ≈ 60 ms (16-del ≈ 103 ms vid 145 BPM, återhämtning ≤ 60 ms). Värdena är startvärden att iterera mot audit, inte facit.

1. **Full-On Roll** (145 BPM, recept A): OSC A Saw Soft pos 0.4; OSC B Psy Roll pos 0.3 level 0.5, +12 semi; sub 0.35; filter LP cutoff ≈ 250 Hz, resonans 20 %, envAmount +3 okt, envDecay 45 ms, velToEnv 0.5; amp A 0.5 ms / D 90 / S 0.1 / R 25; drive 0.25; gate 60 %, första velocity 75 %.
2. **Dekel Groove** (134 BPM, recept E): OSC A Saw Soft pos 0.2; OSC B Psy Sub Punch pos 0.5; sub 0.4; cutoff ≈ 180 Hz, resonans 15 %, envDecay 60 ms; långsam LFO på cutoff; swing 10–15 %, luckor i mönstret; gate 50 %.
3. **Forest 1/32** (mörk, recept G): OSC A Saw Soft pos 0.1; OSC B Psy Acid pos 0.3 level 0.4; filterModel TB-303, cutoff ≈ 300 Hz, resonans 30 %, envDecay 30 ms; 1/32-rull, amp D 50 / R 20; drive 0.35.
4. **Wide Mid** (mellanlager med bredd): OSC A Saw Soft; OSC B saw, unison 3, detune liten, stereo; sub 0.35 mono; cutoff ≈ 350 Hz, envAmount +2 okt, envDecay 70 ms; bredd bara via mellanlager (mono < 120 Hz via MONO BELOW).

## 5. Behövs ny tabell eller synthfunktion? (förslag, inget byggs)
- Ny tabell: troligen inte (se [wavetables.md](wavetables.md)). Avgörs av mätningen av tabellerna.
- Möjlig funktion: `rollBass(...)` (`PresetData.inl:129`) sätter inte unison/detune/warp/fas/filtermodell/FX/sidechain; en bredare hjälpfunktion i `PresetData.inl` (ingen ny parameter) räcker.
- Mätgap: fasmätning kick↔bas vid första 16-delen saknas (referensen, tabell "SAKNAS").

## 6. Källor (länk → vad som togs)
- NI-forumet, https://community.native-instruments.com/discussion/28700/psytrance-style-kick-and-bass/p10 → K + tre 16-delar, första bas-16-delen velocity ≈ 80.
- Producer School, https://theproducerschool.com/blogs/featured-blogs/5-essential-bass-patterns-that-define-hard-house-and-trance-music → sidechain obligatoriskt för 16-delsrull; kick klar inom ~2 steg.
- Ableton-forumet, https://forum.ableton.com/viewtopic.php?p=1083205 → noter startar vid nollgenomgång; kick upp till ~1/8.
- VCV ShapeMaster, https://community.vcvrack.com/t/shapemaster-psytrance-kick-patch/14414 → kick exakt 1/8; pitchenvelopens andra halva på grundtonen.
- Armada, https://www.armadamusic.com/university/music-production-articles/how-to-mix-your-kick-and-bass-5-must-know-methods → mono < 120 Hz, side-lågpass, bredd bara på mellan/höjd.
- Gearnews, https://www.gearnews.com/kick-and-bass-workshop-studio/ → mono < 100 Hz; mättnad/lager för små högtalare.
- Renoise, https://forum.renoise.com/t/the-mixing-thread/68794/19 → mono åtminstone under 200–300 Hz.
- Gearspace, https://gearspace.com/threads/psytrance-bass-compression.973695/ → lätt rörmättnad på kick OCH bas.
- Myloops, https://www.myloops.net/how-to-make-psytrance-kick-and-bass-work-together → crossover 90 Hz; sidechain; kickgrundton 50–70 Hz (krockar med skolans 35–50; ej sammanslaget).
- Sokolovskiy, https://dsokolovskiy.com/blog/all/psytrance-bassline-synthesis/ → amp decay 60–90 ms / sustain 0 / release 20–40 ms; bas-delay 5–15 ms; kompressor 0.1 ms/8:1/8–12 dB/80–100 ms; återhämtning 50–60 ms vid 16-delsrull; basrot mot kick (svävning < 1 Hz); saw en oktav ned + LP 24 dB + decay ≈ 30 %, cutoff ≈ 300 Hz.
- Ej öppnade än: Elektronauts https://www.elektronauts.com/t/psytrance-kick-bass-parameters/253875, MusicRadar https://musicradar.com/tuition/tech/how-to-make-a-trance-bass-sound-263010, SoundBridge https://soundbridge.io/mixing-kick-and-bass.
- Interna: `docs/kick-bass-school.md`, `docs/psytrance-kick-bass-reference.md` (startvärden, recept A–G, verifieringslista).

Inga preset eller samples kopierade; bara teknik.
