# Till Copilot: återstående Analyzer-jobb (AN), 21 okt kväll

Läs först `docs/work/HANDOFF-copilot-2026-10-21.md` (regler, CI, worktrees). Detta dokument gäller bara AN. Svara Tobias på svenska, kort, med `fil:rad`, och skriv rollnamn (projektledaren, Arbetare 1/2, specialisten). Ingen push, ingen merge utan Tobias.

## Var koden finns
- Arbeta i worktree `C:\Users\hunga\Documents\GitHub\KickBase-VST-rel`, gren `content-g` (tip `d6cbcba`). Den är INTE i `release-int` och inte i main. Ny gren från `content-g` är bäst (t.ex. `content-an2`), så att Arbetare 1:s gren inte rörs.
- **Hela content-g måste med i nästa release** (AN + R9b + kbproj + F2 steg 3 + FX2), inte bara dina AN-ändringar. Ordning: (1) projektledaren släpper den pågående releasen (release-int b0be511, utan content-g); (2) projektledaren mergar `content-g` in i `release-int` (på en testgren, löser konflikter, kör CI) och meddelar Tobias; (3) DU utgår från den sammanslagna grenen (be projektledaren om commit-hash) och lägger AN-resten ovanpå; (4) projektledaren kör CI på slutresultatet, och bygger nästa release med allt. Börja alltså inte bygga på gamla `content-g` om du får ett nyare hash från projektledaren.
- OBS: `docs/` är gitignorerad i main men `-rel` har egen kopia av `docs/` (spåras där). AN-specen finns i `KickBase-VST-rel/docs/work/an-spec.md`, mätlistan i `KickBase-VST-rel/docs/research-notes/gaps-to-specs.md` (A1-A10, B1-B13), Arbetare 2:s tröskelgranskning i `KickBase-VST-mx/docs/work/an-threshold-review-A2.md` (redan genomförd, se nedan).
- Mätmotor: `Source/Core/MixAnalysis.h/.cpp` (offline-render, FFT 8192, kick/instrument/master, `Finding` med allvarlighetsgrad; `PhaseMetrics`, `summariseKickBass`, `evaluate`, `describePhase`, `toText`, JSON). AI-verktygen använder samma motor via `Source/Core/ToolLayer.cpp` (`analyse_mix`, `describePhase`). Trösklarna är delade konstanter i `MixAnalysis` (fynd och verktyg ska aldrig glida isär).
- UI: `Source/Ui/AnalyzerPage.h` (underflikar), `KickBassView.h` (vågform/fas/korrelation/spektrum 20-300 Hz), `CollisionsView.h` (fyndlista, grön/gul/röd, band/takter markerade), `SpectrumPage.h` (SPECTRUM, ANALYSIS, REPORT).
- Tester: `Tools/AnTests.cpp` (`antest`), `Tools/Tool.cpp` (`inittest` m.fl.), KickbaseTests. `rendersig*` måste vara 0 diff (ren analys ändrar inget ljud).
- Dokumentation som ska hållas ajour: `Resources/AGENT_GUIDE.md` (MCP/AI-fält), hjälptexter + `helpcheck`, `docs/work/PROGRESS-*.md` (egen `PROGRESS-copilot-an.md`).

## KLART på content-g (rör inte, bygg vidare)
- AN steg 1-4 ur an-spec.md: egen ANALYZER-toppflik med KICK+BASS / COLLISIONS / SPECTRUM / REPORT; A1 faskorrelation per 16-del, A2 hit-till-hit-konsistens, A3 stereo per band.
- Tröskelgranskningen (Arbetare 2) genomförd: limiterfynd på 95:e percentil (varning 6 dB, problem 9 dB), relativ energiviktad fasregel, stereofynd 20-120 Hz med golv -50 dB och minst 2 takter/25 %, lera mäts på kick/bas-spåren med golv -40 dB, gemensamma konstanter.
- "Kick vinner" och "svans tar bas" (`cde9d61`, kategori `kickbass`), lektionsvarningar (RAND-fas på bas, 200 Hz), AGENT_GUIDE + helptexter, `inittest`.

## KVAR (prioritetsordning, billigast och mest nyttigt först)
Alla är ren offline-matematik på renders vi redan gör; ingen ändring i ljudtråden. Ny fynd/fält läggs ALLTID både i UI-listan, `toText`, JSON och AI-verktyget, och beskrivs i `Resources/AGENT_GUIDE.md`.

1. **A8: latensrapport per FX-typ.** Ett test renderar en impuls genom varje effekttyp (lista via `list_effects`; FX2 lade till Dynamic EQ, Octaver, Band Ducker sist) och rapporterar fördröjning i samplar. Ducking är bara samplingsexakt om latensen är 0, så markera varje typ med latens > 0 som tips ("INSTR X FX Y adds N samples: kick ducking drifts"). Ny check i `instrumenttest` eller egen `latencytest`; visas som rad i COLLISIONS eller REPORT.
2. **Limiter-delta och lågband före/efter limitern** (plan-v2:100). Mät nivå i <100 Hz före och efter masterlimitern, och hur många dB limitern tar på just lågbandet. Fynd: "limiter eats the low end by N dB". Mätpunkterna finns i `Master` (`Source/Core/Master.h/.cpp`); lägg ev. kopieringsbuffert i den offline-render som analysen redan gör, inte i processBlock.
3. **Fasdiagram per FX-plats** (plan-v2:100, bredvid A8). Per instrument och FX-slot: korrelation/fasskift 30-150 Hz mellan signalen före och efter slotten (impulssvar eller sinus-svep räcker). Visa som liten rad per slot i KICK+BASS eller egen underflik PHASE. Fynd om slot flyttar basens grundtonsfas > ca 45° mot kicken.
4. **Kick+snare-koincidens** (≈3 dB hopp när kick och snare/klap slår samtidigt): mät summans topp mot kickens ensam på slag där ett andra instrument med högt innehåll startar. Fynd/tips: "kick+snare coincide: +N dB on beat X". Läs hur instrumenten klassas (kick = kickkälla; snare/klap = instrument med mest energi > 500 Hz).
5. **Frekvenszonsvy.** Ny underflik ZONES i AnalyzerPage: stapelrad med energi per zon (sub <60, bas 60-150, lera 150-300, låg-mid 300-1k, mid 1k-4k, hög >4k), per källa (kick, varje instrument, master), grön/gul/röd mot riktvärden ur `docs/kick-bass-school.md` / `psytrance-kick-bass-reference.md`. Återanvänd bandlayouten i `MixAnalysis` (kolla `edges`).
6. **A4 loudness:** LUFS-I och short-term (ITU-R BS.1770 K-vägning + gating, egna filterkoefficienter ur standarden), 4x översamplad true peak, crest. Per källa och master. Fält i REPORT/JSON/verktyg.
7. **A5 transienter + klickdetektor** (envelope 0,5 ms upplösning; hopp > 6 dB över lokal RMS) och **A7 FX-retur per källa** (30-300 och 200-500 Hz per FX-kanal solo; "reverb läcker i basen från instr X"). A6 (aliasing) och A9 (täthet/16-takts-repeterare) är prio 2-3 och kan vänta.
8. **suggested_fixes + phase_findings** (plan-v2:103): strukturerade fält i `Finding`/JSON/`save_mix_report`, samma data i UI, MCP och rapport. Förslag är alltid A/B-kandidater som användaren godkänner, aldrig automatiska ändringar. Arbetare 1 hoppade över detta eftersom fyndens `detail`-text redan innehåller samma råd; gör det sist och bara om det är billigt.

## Regler specifikt för AN
- Ingen automatisk fasändring: bara fakta och förslag (fasverktyg hör till PH).
- Fynd och AI-verktyg delar tröskelkonstanter (en källa i `MixAnalysis`). Lägg aldrig siffror på två ställen.
- Golv mot falskt rött: tyst källa får aldrig ge fynd (se `gmax < -75 dB` "Nothing plays").
- Varje ny mätning: syntetiskt enhetstest (motfas = -1, samma fas = +1, kända latenser) i `AnTests.cpp`, och en rad i `inittest` att Init ger ändliga värden.
- Nya parametrar behövs inte; om något ändå krävs läggs de sist och Init ska inte ändras. Bevisa med `rendersig*` 0 diff.
- Allt som går via CI: be Tobias/projektledaren köra `ci-run.ps1`, lägg inte egna parallella builds.

## Definition of done per steg
1. Koden kompilerar i CI (`ci-run.ps1 -Revs @('<commit>')`), KickbaseTests + antest + inittest + helpcheck gröna, `rendersig*` oförändrade (Init `94f0a1b6bd8a3c7f`, instr 1 `f7139084f716c127`, tung instr 10 `6a3e65e696d14d10`, tb303 instr 10 `03fbeb761547e195`; kontrollera mot senaste körningen i `ci-log.txt` om FX2-typerna ändrat dem).
2. Visuell koll av ANALYZER-fliken av Tobias (skärmbild räcker).
3. Logg i `PROGRESS-copilot-an.md`: vad, varför, resultat, commit.

## Kort tips till Tobias
Läs `MixAnalysis.cpp` kring `PhaseMetrics` och `summariseKickBass` själv (några minuter): där räknas allt Analyzern säger om kick+bas, och alla nya mätare ska följa samma mönster.

## Uppdatering 21 okt natt (projektledaren)
Bas-hash ges av Tobias. Aktuellt läge (hashar, CI 27, release-int-blockeraren, Arbetare 1:s status) står i HANDOFF-copilot-2026-10-21.md under "SLUTSTATUS 21 okt natt". COLLISIONS-vyn har nu axeltitlar och färgförklaring (`Source/Ui/CollisionsView.h` paintField, 6fc8ce1); bygg AN-resten ovanpå den. "Gå till"/Quick-Fix i COLLISIONS är parkerade (`collisions-goto-quickfix-spec.md`), starta inte. Om Cursor tar över: egen `PROGRESS-cursor-<datum>.md`.
