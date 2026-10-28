# Presets-logg

En rad per preset. Kolumner: namn | kategori | nyckelparametrar | mätt | ändrat och varför | höll inte.

| Namn | Kategori | Nyckelparametrar | Mätt | Ändrat/varför | Höll inte |
|---|---|---|---|---|---|
| Psy Clean (8 kickar) | PSY CLEAN | Fixed + tune: C 33, D 37–38, F 45–50, G 50, A 56 Hz | audit kick, se PROGRESS-w2 Del 1 | commit 76ad77e | – |
| Rolling Classic/Tight/Deep/Bright/Fat/Soft/Snap (befintliga) | (ingen) | `rollBass` (`PresetData.inl:129`): saw + OSC B låg nivå, sub 0.1–0.3, drive 0.12–0.35 | centroid 81–95 Hz, <60 Hz −2.3..−3.2 dB, 250–1k −13.6..−15.4, 1–5k −23..−25, side −34.8; nästan identiska | inget ändrat (gamla index rörs ej) | saknar mellanregister och rörelse; låter lika |
| Hi-Tech Zap, Wide Reese | (ingen) | – | Zap centroid 257 Hz; Reese side −26.4 | – | – |
| Full-On Roll | PSY ROLLING BASS | Saw Soft pos 0.95 + Psy Roll +12 st, cutoff 1200, drive 0.15 | audit (lager 0 enbart): centroid 135, 250–1k −9.6, mono | commit följer | nära målet (150–300 / −6..−9) |
| Wide Mid | PSY ROLLING BASS | som Full-On, cutoff 1500, drive 0.05, nivå 0.52 | centroid 145, 250–1k −10.0 | commit följer | ingen unison ännu (kan läggas till) |
| Dekel Groove | PSY ROLLING BASS | Saw Soft pos 1.0 + Psy Roll, cutoff 1500, drive 0.10 | centroid 75, 250–1k −14.7 | commit följer | mörkare grooveversion, når inte målet |
| Forest 1/32 | PSY ROLLING BASS | Psy Acid pos 0.9, TB-303-filter, res 0.45, cutoff 2200 | centroid 67, 250–1k −21.2 | commit följer | mörk acid; mät i DAW |
| Roll Full-On | PSY ROLLING | PSY CLEAN C-kick + Full-On Roll-bas, distorsion-send, Duck 0.85, 4 takter med oktavhopp | audit full: peak −3,3, RMS −11,0, centroid 89, 250–1k −12,1, DC 0 | commit följer | audit mäter hela mixen inkl. kick |
| Roll Wide | PSY ROLLING | Psy Clean D + Wide Mid-bas, chorus-send, 2 takter | peak −3,3, centroid 92, side −120 (mono) | commit följer | stereo kommer bara från chorusen i DAW |
| Roll Dekel | PSY ROLLING | Psy Clean F Soft + Dekel Groove, eko-send | peak −2,9, centroid 82 | commit följer | mörk |
| Roll Forest | PSY ROLLING | Psy Clean A Punch + Forest 1/32 (TB-303), litet rum | peak −3,2, centroid 131, 250–1k −11,2 | commit följer | ljusast av fyra |
| Glide Sub | GLIDE BASSES | PSY CLEAN C-kick + Saw Soft/Sub Punch-bas, sub 0.25, glid rotton→kvint→oktav, eko ej | audit full: centroid 78, DC 0, peak ca −1,5 (efter nivåsänkning) | commit följer | mjuk sub; mät i DAW |
| Glide Acid | GLIDE BASSES | PSY CLEAN D-kick + Psy Acid, TB-303, korta slides, eko-send | peak −1,2, RMS −11, centroid 84 | commit följer | acid-slides |
| Glide Dive | GLIDE BASSES | PSY CLEAN F Soft + Saw Soft/Psy Roll, oktavfall 200–300 %, chorus-send | peak −2,0, centroid 84 | commit följer | tydliga dyk |
| Roll 1–4, Glide Sub/Acid/Dive | PSY ROLLING, GLIDE BASSES | + FUNDAMENTAL DUCK (Kick-källa): 9 dB/200 cent, Glide Sub 12 dB/250 cent | audit full före/efter: 60–250 Hz ≤0,1 dB, övrigt oförändrat (smalt filter) | commit följer | se fundamental-duck.md; effekten hörs bara i DAW |
| Duck Demo | GLIDE BASSES (sist) | D-kick (36,7 Hz) + lång sub-bas på x±1/x+3 (±100 cent från kicken, dissonant), FUNDAMENTAL DUCK 12 dB/200 cent, ingen send | audit full: peak −0,9, RMS −11,4, centroid 87 (duck på; skillnaden på/av hörs i DAW) | commit följer | demo: grundtonerna krockar och sväver |
| New Init 1 | (sist) | fabriks-Init + FUNDAMENTAL DUCK 9 dB/200 cent, duck-rattar som i Init | audit: som Init (peak −0,3, centroid 78) | commit följer | startpunkt med duck |
| New Init 2 | (sist) | Sub + Acid, halftime över två takter, LFO 1 → acid-filter | peak −0,3, RMS −10,6, centroid 73 | commit följer | gles |
| New Init 3 | (sist) | Bara sub, offbeat | peak −3,4, RMS −10,1, centroid 64 | commit följer | renast |
| New Init 4 | (sist) | Sub + mellanbas, rullande | peak −1,6, RMS −9,2, centroid 74 | commit följer | |
| New Init 5 | (sist) | Mellanbas + acid, LFO 1 → acid-filter, två takter | peak −1,0, RMS −11,3, centroid 118 | commit följer | ljusast |

| Mid Growl, Mid Pluck, Mid Wide | MID BASS | 1 lager, oktav 1, såg + 2:a tabell; audit peak −10…−10,5 dB, ingen klippning | Arbetare 2 |
| Hi Zap, Hi Acid, Hi Lead | HI BASS | oktav 2, högre filter; peak −11…−12 dB | Arbetare 2 |
| Synth Pad, Synth Pluck, Synth Stab | SYNTH | unison/pluck/stab; peak −10…−12 dB | Arbetare 2 |

| Minimal/Goa/Dark Psy/Dance/Trance/Techno/Empirikal Kick | MINIMAL, GOA, DARK PSY, DANCE, TRANCE, TECHNO, EMPIRIKAL | 1 kick per genre via psyCleanKick; svans 35 Hz (Goa, Dark Psy) till 52 Hz (Techno), peak −2,8…−3,4 dB | Arbetare 2 |
| Minimal Sub/Pluck, Goa Roll/Acid, Dark Psy Growl/Sub, Dance Pump/Pluck, Trance Roll/Pluck, Techno Sub/Stab, Empirikal Reese/Growl | samma sju | 2 basar per genre via registerBass; peak −9…−12 dB, ingen klippning | Arbetare 2 |
| Unison Reese / Wide / Dark | UNISON | S3 Unison/Reese via registerBass: två sågor, OSC B finstämd ±10–12 cent, unison 3–7, detune 12–25 cent; peak −8,4…−9,7 dB, DC 0, ingen klippning (audit bass). Ej lyssnat | Arbetare 2 |
