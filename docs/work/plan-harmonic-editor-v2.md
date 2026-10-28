# Harmonics-editorn v2 (Serum-stil, ingår i rel-0.3)

Status: etapp 1-3 klara i `pl-presets` (b3ddf32, 675213c, 5557277), kvar: test i Lemon. Plan, projektledaren. Tobias 28 okt: projektledaren tar den (Arbetare 1 tar sample-editorn).
Berör: `Source/Ui/HarmonicEditor.cpp/.h` (öppnas från `OscPanel.cpp:207`). Ingen ny parameter, ingen ändring i `WavetableEdit` eller ljudvägen: editorn bygger fortfarande om User-tabellen via `PluginBase::installWavetable`.

## Etapp 1: större editor och levande vågform
- Fönstret växer från 680x330 till ca 900x560. Överst en vågformsvy (vald frame, utgångspunkt + ändring) som ritas om medan man drar, under den amplitud- och fasbanorna.
- Vågformen räknas ur samma `WavetableEdit::apply` på en frame (billigt, en frame, UI-tråden), så den följer spektrumet direkt; själva tabellbygget har kvar sin 100 ms-fördröjning.
- Knapparna "No fundamental" och "Reset" tas bort. Grundtonen tas bort genom att dra stapel 1 längst ner; dubbelklick på en stapel nollställer den, dubbelklick i vågformsvyn nollställer allt.
- Dra över flera staplar ritar en sammanhängande kurva (interpolerar kolumnerna mellan två musrörelser, annars hoppar man över staplar vid snabb dragning).

## Etapp 2: zoom och penslar
- Zoom: antal visade övertoner 16 / 32 / 64 (rullgardin eller Ctrl+hjul), staplarna blir bredare.
- Penslar (rad ovanför): DRAW (som idag), LINE (dra en rak linje mellan två punkter), SMOOTH (jämnar ut grannar), RANDOM (slumpar inom intervallet, deterministisk med frö).

## Etapp 3: presets
- Rullgardin med färdiga spektrum: SAW (1/k), SQUARE (udda, 1/k), TRIANGLE (udda, 1/k², växlande fas), ODD, EVEN, "BASS WARM" (få övertoner). Varje preset sätter bara gainDb/phaseDeg, ingen fabrikslista att migrera.
- Formler senare (eget beslut).

## Tester
- Etapp 1: bygg + befintliga tester (editorn är ren UI, ljudvägen orörd, rendersig oförändrad). Tobias provar i Lemon: öppna (högerklick på vågen > Edit harmonics), dra, hör ändringen, stäng, ladda om projektet (User-tabellen ska komma tillbaka som förut).
- Ingen ny hjälptext krävs om inga parametrar tillkommer; verktygstips på alla kontroller.
