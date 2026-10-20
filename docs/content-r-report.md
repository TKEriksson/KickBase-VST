# Fas R – rapport (gren content-r, bas content-d2 c0ef101)

Inget är pushat. Sista commit: se `git log content-r -1`.

## Gjort per steg
- **R1–R3**: pianorulle (kryss-y, scrollbarens klick, vänsterkant, Alt = sudd, SELECT ALL, + BAR), ENV-header, håll-provlyssning (hold-audition), LISTEN vid PITCH/AMP.
- **R4–R7**: analyzer-lista, export per instrument och "without effects", LFO-fas fortsätter vid tempoändring (Modulation.cpp phaseOffset). SNAP är byggd enligt spec.
- **R9 (radens längd)**: ingen TOTAL LENGTH. Varje rad har LENGTH (AUTO n eller n 🔒), högerklick på radetiketten, bekräftelse vid förkortning, + BAR. Steg 4: MCP `get_state` (bars, length_mode, content_bars, extra_bars) och `set_pattern` length/trim/repeat_to.
- **Fasslut**: de gamla parametrarna patternBars/rowBars heter "(unused)", har ingen hjälptext och finns kvar bara för gamla projekt (v6 → `applyLegacyLengths`). LENGTH-knappens text sätts nu i konstruktorn (StepSequencer.cpp:26), inte först vid första timern.

## Tester
- persisttest (ny sektion 9: låst rad, AUTO + extra, gammalt v6-projekt), helpcheck, lengthtest, analysetest, blocksizetest, audiotest, KickbaseTests: gröna.
- rendersig, rendersigreal och FX-raderna: identiska med baslinjen.
- **Avvikelse**: rendersigwide (4 tunga instrumentrader) och rendersigtb303 skiljer. Det är inte trimRow (återställt och ombyggt). Trolig orsak: R9 ändrade mönsterinnehållet i scenariot (`applyTemplate` fyller `fillBars`; `Scenarios::makeHeavy` lägger glide-toner bara i takt 0, som nu loopar varje takt). Ingen ljudmotorkod ändrad. Obevisat. Baslinjerna är orörda; Opus avgör om de görs om.
- HostTest: gammal build sparar, ny laddar: 0 av 4351 jämförda parametrar skiljer. Ny → ny identiskt (Kickbase och FX).
- `--selftest-link` skrev ingenting; oklart om den kördes.

## Bilder (42 st, före = content-d2-tippen byggd i samma miljö, efter = content-r)
2 av 42 identiska. Sett: fx_tab0 (gammal + ny) och inst_upper0_tab0 (ny).
- fx_tab0–8, inst_upper0_tab0/8 (~96k resp. ~188k px): TOTAL LENGTH-menyn borta, LENGTH-knapp per rad, mönstret följer innehållet (2 takter, repeat dämpad), pianorullens körer för anpassade rader, sidoknapparna "<" ">" och FOLLOW. Avsett.
- inst_n*_tab20–28 (521 px), tab22 (1529 px), tab23 (37837 px), inst_upper1_tab10/18 (1476 px): ENV-headern och ENV-sektionen från R1–R3 (förklarade ur koden; inst_n1_tab23 sedd, ENV-fliken ser rätt ut). Avsett.
- Kända ostabila element, ej regressioner: liten röd klippt text i inst_n10_tab23, inst_upper3_tab30/38 (~175 px), pixel (514,177).

## Uppskjutet
- SNAP-mockupens detaljer → fas L.
- WARP ENV-varning ska in i plan-v2 under S.
- Help-städning: klar.
- Ej klickat igenom: bekräftelsedialogen vid förkortning och högerklicksmenyn (på Tobias lista).

## Testlista för Tobias (bara det som hörs/syns)
1. **LENGTH-meny**: klicka LENGTH på en rad; välj AUTO och ett fast antal takter. Knappen visar "AUTO n" eller "n 🔒".
2. **Förkorta**: sätt en rad med toner till färre takter än noterna. En bekräftelse ska komma; Avbryt lämnar raden orörd, OK tar bort noterna efter slutet.
3. **Högerklick på radetiketten**: samma meny som LENGTH-knappen.
4. **Låst rad**: klick i pianorullen efter radens slut ska inte göra något.
5. **AUTO**: lägg en not i takt 5, raden växer till 5; ta bort den, raden krymper.
6. **+ BAR**: lägger till en tom takt i en AUTO-rad och den består.
7. **SNAP**: dra/placera toner och kolla att de snappar enligt vald nivå.
8. **LFO-fas**: kör en LFO och ändra BPM under uppspelning; fasen ska fortsätta utan hopp.
9. **Håll-provlyssning**: håll ner förhandsknappen (PREVIEWS/LISTEN); ton hålls och släpps när du släpper.
10. **Export per instrument**: exportera en instrumentrad, med och utan effekter, och lyssna på filerna.
