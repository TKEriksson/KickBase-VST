# Fas P – rapport (gren content-p)

Inget är pushat, mergat eller Release-byggt.

## Gjort per steg
- **Steg 1–4**: se docs/work/PROGRESS.md (röster, ackord i mönstret, MIDI/MCP, pianorullens CHORD-meny).
- **Steg 5**: `polytest` (ny), `perftest` med röster och ackord, VOICES-varning i UI (KnobComponent::setWarning: etiketten blir orange, tooltip "WARNING: ..." när VOICES ≥ 2 och VOICES × summerad unison för aktiva oscillatorer > 24).

## Tester
- polytest: gick igenom.
  - Stöld (steal) jämfört med samma toner utan stöld: HF-energi > 5 kHz −30,2 dB mot −29,7 dB, största hopp 0,8884 i båda. Inget extra klick.
  - Oanvänd röst låter som ny instans (krav 1): RMS −0,5 dB, HF −24,7 dB, identiskt.
  - VOICES 4 → 1 medan toner ljuder (krav 2): ljudande röster 4 → 1, största hopp 0,0723 efter mot 0,0725 före. Tonar ut utan klick.
  - Seed och seed per röst (krav 7): OK.
  - Ackordets start i processorn, med swing och humanize: sample 7266 för grundton och ackord.
- rendersig ×4: 0 rader skillnad (sparad som -p5). blocksizetest, instrumenttest, persisttest: 0 FEL.
- Krav 6: params-fx och FX-rendering oförändrade (täcks av rendersig).

## Minne och prepareToPlay
sizeof InstrumentVoices 17 kB, InstrumentSynth 2 kB.

| Samplingsfrekvens | 80 röster | Hela instansen | Varav skapad | Varav prepareToPlay | prepareToPlay-tid |
|---|---|---|---|---|---|
| 48 kHz | 6,0 MB | 150,5 MB | 71,8 MB | 78,8 MB | 7 ms |
| 192 kHz | 24,7 MB | 402,3 MB | 71,5 MB | 330,8 MB | 27 ms |

Det mesta av instansminnet fanns före fas P. Fas P lade till ungefär 7/8 av röstsiffrorna. Testets gräns gäller därför rösternas egen andel (50 MB), och prepareToPlay-tiden (0,5 s).

## CPU (perftest, sekunder CPU per sekund ljud)
| Fall | CPU | Mot en röst |
|---|---|---|
| 3 instrument, VOICES 4 + treklanger | 0,188 | 2,54 × |
| 10 tunga instrument, en röst | 0,480 | – |
| 10 tunga instrument, VOICES 8 + ackord | 3,171 | 6,61 × |

Värsta fallet går inte i realtid. Därför finns VOICES-varningen.

## Designanteckningar
- Poly ≠ mono med avsikt: i poly får varje ton en nollställd röst, i mono fortsätter tonen från nuvarande nivå. VOICES 2 utan ackord låter alltså inte exakt som VOICES 1.
- Kicken har inga ackord.

## Krav 4: var tonerna läses och skapas
- Ackordmedvetna: Pattern.cpp/h, RollFiles.cpp, ToolLayer.cpp (get_state, repeat_to rad 834), PianoRoll.cpp/h, InstrumentProcessor.cpp:910–912, testerna.
- Läser toner utan att behöva ackordet: PluginBase.cpp:59 och 234, StepScheduler.cpp, FxProcessor.cpp:241 och 247 (kick- och lager 0-händelser), LengthMenu.h:42, PresetButton.cpp:211, stegsekvenserns ritning.
- Skapar toner utan ackord (standard {}, avsiktligt): Presets.cpp:170 och 406, StepSequencer.cpp:1150 (FILL).
- Gamla XML-laddaren (Pattern.cpp:533) läser det gamla formatet, som aldrig hade ackord.

## Testlista för Tobias i DAW (bara det som hörs/syns)
1. Pianorullen: markera toner, högerklicka på tom plats → Chord → Major. Konturerna syns ovanför tonen; statusraden visar "CHORD +4 +7".
2. VOICES 1: konturerna är svaga och menyn säger "needs VOICES 2 or more". Bara grundtonen hörs.
3. VOICES 2 och 8: ackordet hörs. Snabba toner med VOICES 2 (stöld): inga klick.
4. Höj unison på flera oscillatorer och VOICES: VOICES-etiketten blir orange och tooltipen varnar.
5. Sänk VOICES från 8 till 1 medan ackord spelar: rösterna tonar ut utan klick.
6. Importera en MIDI-fil med ackord: ackorden hamnar på rätt toner.
7. Kopiera och klistra in toner med ackord; flytta dem med piltangenterna: ackordet följer med.
8. Spara projektet, stäng och öppna: ackord och VOICES finns kvar.
9. CPU-mätaren i DAW:n med några instrument på VOICES 4 + ackord.
