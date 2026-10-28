# Kick + bas: samspel

Fakta från skolan (`docs/kick-bass-school.md:39,56`) och referensen (`docs/psytrance-kick-bass-reference.md`):
- Kicken vinner där de möts: första bas-16-delen startar medan kick-svansen hörs. Verktyg: `bassNDuck`, `scMode` = Kick envelope, `scLength`, `set_sidechain_curve`.
- Ingen basdel får ringa in i nästa kicktransient (resonant filter / lång release): klipp före nästa kick (`bassNGate`, `bassNGateSmooth`, filterdecay, `bassNDuck`).
- Kick slutar inom ~1/8. Basnoter ≤ 1/16. Första basnoten velocity 60–85 %. Random phase 0 (fasen ska vara densamma varje not). Bas mono under ~120 Hz.
- Startvärden (referensen): cutoff 150–400 Hz, filterenv +2..+4 oktaver, env decay 30–90 ms, resonans 10–30 %, gate 40–80 % av 16-delen.
- Kort mönster: K + tre 16-delar (NI-forumet), där första bas-16-delen ligger lägre i velocity (~80).

## Svans mot grundton
Basens grundton ska ligga nära kickens svansfrekvens (Sokolovskiy: stäm basroten mot kickens tonhöjd, svävning under 1 Hz). Annars slår de två mot varandra på första 16-delen. Detta väljer toner: kick G (≈ 50 Hz) → basrot G (49 Hz) osv.

## Fas
VCV-forumet: kickens pitchenvelope ska vila på grundtonen i andra halvan så fasen mot första 16-delen blir lugn. Ableton-forumet: noter ska starta i nollgenomgång (fas 0 varje not, ingen random phase).

Mätning: `render_preview part=kick/bass/full` och `analyse_mix` (fasmätning saknas fortfarande, se referensens tabell "SAKNAS").
