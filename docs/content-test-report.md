# content-test: testlista för Tobias

Testgren `content-test` = `content-l` (innehåller faserna R, M, P och L2/L3/L2b). Ingen push och ingen merge.

**VST3 (Release, commit 3f4c9f3):** mappen `C:\Users\hunga\Documents\Kickbase-content-test\` har `Kickbase.vst3`, `Kickbase FX.vst3` och appen `Kickbase.exe`. Kopiera VST3-mapparna till `C:\Program Files\Common Files\VST3\` eller lägg till mappen som sökväg i din DAW.

Detaljerna finns i `docs/content-r-report.md`, `docs/content-p-report.md` och i `docs/work/PROGRESS.md` (M, L).

## Testlista (det som hörs och syns)

1. **R, LENGTH**: Klicka på LENGTH på en rad och välj AUTO och sedan 3 takter. Knappen ska visa "AUTO n" eller "3 🔒". Om du förkortar en rad som har toner efter slutet ska en bekräftelse komma, och Avbryt ska lämna raden orörd.
2. **R, AUTO och + BAR**: Lägg en ton i takt 5 så att raden växer till 5 takter, och ta sedan bort tonen så att raden krymper igen. + BAR ska lägga till en tom takt som ligger kvar.
3. **R, LFO-fas**: Låt en LFO gå och ändra BPM under uppspelning. Svepet ska fortsätta utan hopp.
4. **R, export per instrument**: Exportera en instrumentrad både med och utan effekter, och lyssna på filerna.
5. **M, LFO-fliken**: Lägg till LFO:er med + ADD LFO, upp till 10 stycken. Fäll ihop ett kort. Ta bort en LFO som har kopplingar med × och kontrollera att du får en fråga först.
6. **M, egen kurva**: Dubbelklicka på en minikurva eller välj EDIT POINTS, ändra punkterna och lyssna på modulationen.
7. **M, spara**: Spara projektet med LFO 4–10 i bruk, stäng det och öppna igen. LFO:erna och kopplingarna ska finnas kvar.
8. **P, ackord**: Markera toner i pianorullen, högerklicka och välj Chord → Major. Med VOICES 1 hörs bara grundtonen. Med VOICES 2–8 hörs hela ackordet, och det ska inte klicka i snabba toner.
9. **P, VOICES ned**: Sänk VOICES från 8 till 1 medan ett ackord spelar. Rösterna ska tona ut utan klick.
10. **P, MIDI**: Importera en MIDI-fil med ackord och kontrollera att tonerna hamnar rätt. Kopiera, klistra in och flytta toner med piltangenterna: ackordet ska följa med.
11. **L2, ENV-rutorna**: Alla fyra rutor har ON, SYNC, LISTEN och DURATION. Sätt ENV 1 på SYNC och ändra tempot: längden ska följa tempot. Stäng av FILTER ENV med ON och lyssna efter att filtersvepet försvinner.
12. **L2b, DRAG**: Dra DRAG från ENV 1 eller FILTER ENV till en ratt, till exempel filtrets cutoff. Ratten ska röra sig med envelopen. Spara projektet, öppna igen och kontrollera att kopplingen finns kvar.
13. **L3, PATTERN-flikraden**: NOTE END, SWING och HUMANIZE ligger till höger i PATTERN-flikens rad och påverkar hela mönstret. Lyssna på SWING.
14. **L3, AI SERVER**: Knappen "AI SERVER (MCP): ON/OFF" i headern ska slå av och på bryggan och visa samma läge som inställningssidan.
15. **CPU och laddning**: Kör ett äldre projekt (från före de här faserna) och kontrollera att det låter som förut. Titta på DAW:ns CPU-mätare med några instrument på VOICES 4 med ackord och några LFO:er.
