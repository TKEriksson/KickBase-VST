# PROGRESS – specialisten (Opus), 2026-10-27

## Gjort
- Anmälde mig till projektledaren som specialist.
- Läste Tobias testlista (`research/TEST2026-10-23/`) med bilder, PROGRESS-copilot p22–28, free-run-patchen och relevant kod.
- Granskade `docs/masterplan.md` och skickade beslutet till projektledaren: **godkänd med ändringar**.
  - Uppspelningspaketet A1–A3 går till Arbetare 1. Specialisten granskar orsaken före fixen och diffen före merge.
  - B1 flyttas till Arbetare 1 och A5 till Arbetare 2, så att de inte krockar i samma filer.
  - T4 tolkas om: en VST3 kan troligen inte stoppa FL eller ändra FL:s tempo. LOOP och BPM gäller därför bara i free-run. Det ska verifieras i JUCE 8.0.15 AudioPlayHead.
  - POP OUT och GHOST NOTES läggs i fas D.
  - Saknade småpunkter läggs till: HeaderBar-timern, `/STACK` (CMakeLists.txt:243), AGENT_GUIDE.
  - Fas F får de riktiga grenarna som inte är mergade: content-d1, content-ph, content-s2, fix-matrix, fix-slicer-test (L, S och K finns inte).
  - Undo-modell: en gemensam juce::UndoManager. APVTS får den, och pattern-ändringar blir UndoableAction med snapshots, en gest per transaktion.
  - Listade FL-tester för paketet.

## Hypotes, ej verifierad i FL
Felet där bara C5 spelas (T10): pianorullens MIDI IN står som standard på INSTRUMENT (PianoRoll.cpp:98, 224). Värdens C5 spelas då live i stället för att starta mönstret (InstrumentProcessor.cpp:540-579). Projektledaren har kodkontrollerat detta. Tobias bestämmer vilket standardvärde MIDI IN ska ha.

## Tobias beslut (2026-10-27)
- När värden spelar startar dess noter alltid mönstret. MIDI IN live gäller bara när värden står still.
- Laddning av huvudpreset tömmer undo-historiken. Laddning av ett del-preset är en ångerbar transaktion.
- Båda besluten är vidarebefordrade till projektledaren.

## Granskning (a) av A1/A2-orsaker (Arbetare 1, PROGRESS-w1.md)
Godkänd med justeringar:
- A1:
  - Hoppa bara över note-on när värden spelar. Note-off ska fortfarande gå till handleLiveMidi, annars fastnar toner.
  - Läs värdens råa isPlaying, inte followingHost, som A3:s forceFreeRun gör falsk.
- A2:
  - Exemplet om Loop 1-2 på en rad med 1 takt var fel. De verkliga felfallen är när loopen inte är en multipel av radens längd, och när loopStart >= rowTicks.
  - window.from ignoreras på StepScheduler.cpp:46.
  - loopEnd ska klippas mot total.

## Granskning av Copilots kod (2026-10-27, klar)
Gjort av specialisten. Jag har bara läst koden, utan att bygga eller testa. Underlaget är `git diff 2ba981c an-master` i KickBase-VST-ai (30 commits).

Fynd:
- C1, låg. Params.h:120 har en kommentar där två kommentarer har klistrats ihop, och Params.cpp:421-437 har fel indrag (även den avslutande `}`). Förslag: städa upp.
- C2, låg. InstrumentProcessor.cpp:145: `}    clock.beginBlock (...)` står på samma rad. Förslag: radbryt.
- C3, låg. PluginBase.h `postNoteAudition`: när FIFO:n (32 platser) är full släpps meddelandet tyst. Om det som tappas är en note-off fastnar tonen. Förslag: låt note-off alltid komma fram, till exempel genom att skicka all-notes-off för raden när rullen stängs.
- C4, låg (bör kontrolleras). Maximizer har fått MODE som knob 5 (Effects.cpp:506, knobsVersion 3). Gamla projekt sparar redan knob 5 för varje slot, så migreringen (Effects.cpp:528, PluginBase.cpp:1317) sätter aldrig något standardvärde. MODE får då det värde som råkade ligga sparat. Är värdet 0.5 blir det troligen Classic, eftersom roundToInt avrundar 0.5 till jämnt tal. Har sloten tidigare haft en annan typ kan det bli Smooth, och då ändras ljudet. Förslag: test med ett v2-projekt som har maximizer och knob5 = 1.0.
- C5, låg. InstrumentSynth.cpp:440-447 `addMonoWide`: fördröjningslinjen skrivs bara när width > 1. Om man vrider WIDTH från 1 och uppåt spelas först gamla samplingar, upp till 17 stycken, vilket kan ge ett litet klick. Förslag: skriv alltid `line[widthDelayPos] = x` före `if`.
- C6, info. Fas F-grenarna har commits märkta "(ej byggt)": ad768d0 (S2b), 4dad3e2 och d254e2d (slicertest). De måste byggas och testas innan de mergas.
- Redan i planen: Perch-buggen är bekräftad i koden (AiMemory.cpp:333-338, uppdatering på plats och sedan `remove(0)` på rad 74), se A5. Autocomplete på svenska med trasiga åäö är T7.
- Ingen nyckel loggas: grep på DBG/Logger tillsammans med key/header/Authorization gav inga träffar.
- OK: de nya instrumentparametrarna ligger sist och kom i samma release (a109d31), så ordningen ändrades aldrig mellan två releaser. Shimmer och Low Multiplier ligger sist i EffectType.

Listan är skickad till projektledaren. Jag föreslog en liten städfas med prio efter A och B, och att C4 verifieras tidigt.

## Nästa
- Skriva `docs/work/undo-spec.md` när fas C startar.
- Granska orsaksanalysen och diffen för A1–A3 när Arbetare 1 är klar.
