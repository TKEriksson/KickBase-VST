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
