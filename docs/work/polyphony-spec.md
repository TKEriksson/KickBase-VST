# Polyfoni: flera toner på samma starttick (spec, ej byggd)

Mål: en rad kan ha staplade toner på en tick (ackord med fri längd/velocity per ton, glide bredvid en ton). Idag: en `Pattern::Note` per tick (Pattern.h:104-), ackord = intervall i `Note::chord` (delad längd/velocity).

## Lagring
- `Note` får `extra[maxStack-1]` (maxStack = 4): `{on, pitch, length, velocity, slide}` atomärt, i samma block (`Pattern.h:104`). Huvudtonen är kvar som idag => en rad utan staplar är bit-för-bit som förut.
- Ingen ny blockallokering; blocket växer ca 3x (384 toner/takt). Alternativ om minnet oroar: extras i eget lazy-block per takt (+1 pekartabell).
- Sparfil: ett `/e` fält efter ackordsfältet i radsträngen (Pattern.cpp:411), skrivs bara när extras finns. Äldre versioner ignorerar det.
- `chord` (intervall) behålls; ackord + staplar räknas båda mot VOICES (`Pattern::maxSimultaneous`, klar).

## Glide
- En glide-ton på samma tick som en annan ton med annan tonhöjd återanvänder dess röst: ingen extra VOICE, räknas inte i dialogen/varningen (maxSimultaneous gör redan så för glide som startar under en klingande ton).

## Ritning (PianoRoll)
- Glide-läget på + klick på upptagen tick => lägger till en glide-ton (idag ersätts tonen).
- Vanligt läge + klick på upptagen tick med annan tonhöjd => ny stapel (idag ersätts/flyttas den); samma tonhöjd => markerar den.
- `NoteMap` (tick -> data) blir (tick,index) -> data; markering, flytt, ändra längd, radera, kopiera/klistra, MIDI-export/import och ToolLayer läser per ton. Största arbetet: PianoRoll.cpp (readNotes/applyNotes 774-810, selection är `std::set<int>` => `std::set<NoteKey>`).
- VOICES-dialogerna (klara) triggas av samma räknare.

## Motor
- `StepScheduler.cpp:55` triggar huvudton + chord; lägg en loop över extras (noteOn med egen längd/velocity). Glide-extra => `noteOn` med slide mot rösten som klingar.
- Ljudtråden läser bara atomer, ingen allokering.

## Bakåtkompatibilitet
- Gamla mönster och Init-presetet: extras är alla `on=false` => samma beteende. Test: `rendersig*` 0 diff, rundtur spara/läs, tom/korrupt `/e` kraschar inte.

## Kostnad (grov)
- Lagring + sparfil + motor + tester: ~1 session. UI (NoteKey genom hela PianoRoll, ritning, kopiera, MIDI, undo): ~2 sessioner. Totalt ~3 sessioner; risken ligger i UI-markeringen.
- Billigare variant (~1,5 session): bara glide-stapel (1 extra ton per tick, bara slide) och ackord som idag.
