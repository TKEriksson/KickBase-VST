# Handoff: Claude research synthesis ↔ projektledare

**ÅTERTAGET 20 okt 10:47 av projektledaren** (ingen aktivitet sedan 10:09, svarade inte på meddelande). Allt kvarvarande görs av projektledaren. Vaknar du: skriv Status med klockslag här och vänta på ny Inbox-rad.

## Uppdrag och avgränsning

- Läsa synthens design och sound-design-roadmap, därefter guider, transkriptioner, PDF:er, specs och övrigt läsbart material i de två research-mapparna.
- Besöka citerade externa sidor när de går att nå och markera källor som inte gick att läsa.
- Göra materialet användbart för KickBase: ljuddesignförslag, mätbar felsökning och konkreta ändringar via nuvarande MCP-verktyg.
- Spara en tät, källmärkt sammanställning i MCP-guiden (`docs/psytrance-kick-bass-reference.md`) och vid behov precisera AI-arbetsflödet i `docs/AGENT_GUIDE.md`.
- Ingen DSP-, UI- eller MCP-funktionsändring ingår; dokumentationsändringar behöver inte byggas eller testas.

## Preliminära iakttagelser

- Källorna motsäger varandra om huruvida kickens svans ska stämmas exakt med basens grundton, ligga några halvtoner ovanför eller väljas utan hänsyn till tonarten. Det bör beskrivas som alternativ att mäta och lyssna på, inte som en automatisk regel.
- KickBase-analysen kan rapportera frekvensöverlapp, maskering, stämning, subrumble och limiterarbete. Den mäter inte fas-/stereoproblem; AI:n måste säga när underlag saknas och inte hitta på en orsak.
- Bilden `research/TEST2026-10-20/plugintips_with_instant_phase_shift.bmp` visar en extern additive-synth-diskussion om separat realtidskontroll av partialernas pitch, nivå och fas. Det är en möjlig framtida idé, inte en funktion som ska tillskrivas KickBase.
- Råden ska skilja mellan källbelagda observationer, generella startvärden och personliga/stilmässiga val. Samma ändring ska renderas och jämföras före/efter.

## Önskad handoff från projektledaren

Kunskapsbasen är tänkt att spara Claude framtida tokenarbete: i stället för att läsa råtranskript och mockuper kan den läsa den befintliga MCP-resursen `get_guide topic=psytrance`. Den har en kort princip-/diagnostikdel plus ett täckningsindex med takeaway per källa.

**Viktiga beslut i sammanställningen:**
- Källorna är oense om kickens tail ska matcha basroten, ligga några halvtoner över eller väljas som ett fast sweet spot. Guiden rekommenderar jämförande test, inte automatisk retuning.
- `analyse_mix`/rapporten kan stödja kollisions-, maskerings-, stämnings-, rumble- och limiterdiagnos men bevisar inte fas-/stereoproblem. Rapporten ska skilja fakta från hypotes och smak.
- PULSES/Psylab/Bézier/LFO-bilder, AI-server/chattmockuper och additiv-synthbild är inspirationsmaterial, inte bevis på aktuella KickBase-kontroller.

**Filer ändrade av Claude:**
- `docs/psytrance-kick-bass-reference.md` — källsyntes, felsökningskarta, MCP-arbetsflöde, källindex och besökta researchlänkar.
- `docs/AGENT_GUIDE.md` — mätningsbegränsningar och försiktighet kring fas/stämning.
- Den här handoff-filen.

**Källstatus:** 21 transkript, två PDF:er och sju Markdown/specs har sammanfattats; bildfamiljerna har granskats visuellt. Relevanta artiklar gick att läsa. 16 YouTube-sidor svarade men videoinnehållet gick inte att läsa; en citerad sida gav 403 och sex gav 404. Inga ljudprov kunde verifieras från stillbilder.

Ändringarna är dokumentation. Inga DSP-/UI-/MCP-funktioner ändrades; ingen build/test behövs för dessa dokument.

Lägg gärna en daterad instruktion nedan om särskild prioritet, granskningsfråga eller om du vill att en separat `SKILL.md` skapas. Jag rapporterar slutstatus här och i chatten.

---

## Inbox

### 2026-10-20 · Projektledaren (kickbase-vst-27): svar och uppdelning
Bra iakttagelser, särskilt (1) kick-svans-motsägelsen och (2) att analysen inte mäter fas/stereo. Läs [README.md](README.md) (regler, ny tidsgräns/status-sektion) och [SYNTH-CONTEXT.md](SYNTH-CONTEXT.md).
**Uppdelning så att ni inte gör samma jobb (Codex gör parallellt `docs/research-notes/`, regel→reglage-mappning per tema):**
- **Du äger:** `docs/psytrance-kick-bass-reference.md` och AI-arbetsflödet i `docs/AGENT_GUIDE.md` (AI-vänlig, kort, källmärkt). Rör inte `docs/research-notes/` (Codex) eller `Source/`/`Tests/`.
- **Codex äger:** `docs/research-notes/*` (kick-design, bass-design, kick-bass-phase-lock, mix-master-fx, arrangement-workflow). När de finns: länka dit från referensen i stället för att kopiera.
**Prioritet för dig:**
**Status 10:03:** README och synthkontext lästa. Punkterna 2–4 är klara: `Mät först, gissa aldrig`, mätluckor som `SAKNAS → verktygsidé`, tre testbara kick-tail-alternativ och additiv-bilden som framtida idé. Punkt 1 (länkar i `guidesonline.txt.txt`) pågår.
**Status 10:08:** Källindexet anger nu en takeaway för samtliga 21 transkript, båda PDF:erna och sju specs; bildfamiljerna är också sammanfattade. Länkkontrollen av `guidesonline.txt.txt` återstår.
**Status 10:10:** Projektledaren har lagt nästa research-uppdrag hos mig efter länkkontrollen: Low-End Shaper, FM-squelch/lead och Psy Multi-FX. Jag tar det då, utan att gå in i Codex-materialet före dess.
1. Besök de externa länkarna i `transcripts/guidesonline.txt.txt` (Gemini svarar inte, så det ligger kvar hos dig; hoppa över länkar du inte når). Märk `[verifierad]`/`[osäker]`/`[död]`.
2. Skriv i referensen en kort sektion "Mät först, gissa aldrig": vilka påståenden våra verktyg kan belägga (frekvensöverlapp, maskering, stämning, subrumble, limiterarbete) och vilka de inte kan (fas, stereo). Varje saknad mätning → en rad "SAKNAS → verktygsidé" i en ny sektion `## Saknade mätningar` sist i referensen. Det blir underlag för analysator-fas AN/PH.
3. Kick-svans-motsägelsen: skriv den som tre alternativ (exakt grundton / några halvtoner över / oberoende av tonart), med hur man testar var och en via MCP (rendera före/efter, samma seed) och när varje passar.
4. Additiv-bilden: bara en rad under "framtida idéer", ingen funktionsöverdrift (som du skrev).
**NYTT 10:10 (projektledaren):** Codex, Copilot och Gemini antas ha nått limit och gör inget mer. Jag tar tillbaka deras uppdrag och lägger Uppdrag 2 hos dig, efter länkkollen: skriv `docs/research-notes/fx-and-fm-ideas.md` enligt texten i `CODEX-CLAUDE-HANDOFF.md` under "Uppdrag 2" (Low-End Shaper, FM squelch/lead med minimal operatorstruktur, Psy Multi-FX mot vår effektlista; JUCE-implementation, CPU-risk, AGPL-licens, prio 1–3). Format enligt README, max ~150 rader, `[osäker]` på det du inte kunnat verifiera. Du äger nu även `docs/research-notes/`. Rapportera 5 rader.
**Status:** skriv `Status: <klockslag>` här vid start och efter varje punkt. **Rapport:** 5 rader här när klart. Ändrar du referensen kraftigt, committa inte; jag samlar commits.

### 2026-10-20 · Projektledaren (kickbase-vst-27): uppdrag FXMAP (plugin-tips till bygge)
Status: 15:14 start (Sonnet-session; läser transkripten direkt, skriver docs/research-notes/plugin-tips-to-builds.md).
Status: 15:21 klar (ej committat).
**Resultat:** `docs/research-notes/plugin-tips-to-builds.md` (80 rader, 40 tabellrader).
1. Läst direkt: alla effekttunga transkript och båda PDF:erna (via pdftotext); PDF:ernas artikellänkar öppnades inte. tips.txt, guidesonline och PDF:erna är AI-sammanställda och märkta `[osäker]` om inget transkript säger samma sak.
2. Varje rad har källa med märkning, vad tipset gör, "hos oss" (HAVE med effekttyp/reglage, B/A-nr eller NYTT), BYGG/SKIPPA/SENARE med motiv och prio 1–3.
3. Prio 1: disperser (B7), multiband-distorsion (B6), polaritet (B1), kompressor med detektor-HPF, limiter-delta/lågband-jämförelse. De flesta tipsen finns redan eller har B-nr; 5 är NYTT utan fas.
4. Topp 5 utan fas: detektor-HPF på kompressor, limiter-delta + lågbandsloudness, per-band-ducking, kompressor MIX/parallell, kick+snare-sammanfall (analys).
5. Att verifiera i koden: reverbens pre-delay, kompressorns MIX, echo dotted/triplet-sync, distortionens kurvtyper (`list_effects`). Inget i Source/Tests rört, ingen commit.
