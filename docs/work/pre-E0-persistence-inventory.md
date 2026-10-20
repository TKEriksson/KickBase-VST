# Förinventering inför E0: vad sparas var?

Status: läsning av nuvarande `main` den 20 oktober 2026. Detta är ett underlag, inte E0-godkännande, inte ett fullständigt test och inte ett beslut om vad som *bör* ingå i ett preset. Kontrollera igen mot de grenar som faktiskt väljs för E0; `content-l`, `content-s`, `content-an` och `content-lc` är separata från `main` i nuläget.

## Överblick

Projekt-/host-state och användarpresets delar en del serialisering, men de är inte samma format eller samma omfattning:

- `PluginBase::getStateInformation` kopierar APVTS-trädet och lägger till metadata och specialdata (`Source/Core/PluginBase.cpp:519`).
- `PluginBase::saveUserPreset` skriver ett separat `KickbasePreset`-XML med sektionsurval (`Source/Core/PluginBase.cpp:824`).
- `loadPresetXml` återställer parametrar i vald sektion och laddar specialdata bara för de sektioner som uttryckligen hanterar den (`Source/Core/PluginBase.cpp:935`).
- APVTS-parametrar omfattar parametriserade delar; den manuella serialiseringen nedan visar vilka icke-parameterobjekt som också sparas.

## Verifierad serialisering

| Tillstånd | Projekt/host-state | Fullt användarpreset | Delpreset | Källkod / kommentar |
|---|---|---|---|---|
| APVTS-parametrar | Alla i `copyState()` | Alla parametrar under sektionen `full` | Filtreras med `Presets::belongsTo` | `PluginBase.cpp:521`, `824–860`; sektioner i `Presets.cpp:195–300` |
| Mönster, rader och GATE | Hela `Pattern`-trädet | Hela `Pattern`-trädet | `pattern`-preset sparar en rad | `PluginBase.cpp:533`, `870–873`; `Pattern.cpp:370–435` |
| Kicksample + kickens pitch/amp-envelopes | Ja | Ja (`full` och `kick`) | Kick-preset | `PluginBase.cpp:534`, `543–544`, `876–878` |
| Instrumentsamples | Alla aktiva samples i `BassSamples` | Alla | Bass-preset tar med valt lagers sample | `PluginBase.cpp:535`, `882–885`, `1305–1328` |
| Egna wavetables | `UserTables` | Alla | Bass-preset tar med tabeller för valt lager | `PluginBase.cpp:346–420`, `536`, `888–890` |
| Sidechain-kurva | Ja | Ja (`full`/`sidechain`) | Sidechain-preset | `PluginBase.cpp:537`, `893`; `Sidechain.cpp:73–96` |
| Modulationsrutter | Ja | Ja (`full`/`lfo`) | LFO-preset | `PluginBase.cpp:538`, `897`; `Modulation.cpp:193–230` |
| LFO-kurvor och `activeLfos` | Ja; kurvor för LFO 1–3 och aktiva extra-LFO:er | Ja (`full`/`lfo`) | LFO-preset | `PluginBase.cpp:539–543`, `898–902` |
| Presetnamn per sektion/rad/FX-kanal | Ja, som slot-metadata | Inte uttryckligen bland XML-fälten som `saveUserPreset` skriver | Inte uttryckligen | `PluginBase.cpp:523–529`; jämför presetformatet `824–907` |
| `previewNote` | Ja | Inte uttryckligen | Inte uttryckligen | `PluginBase.cpp:530`, `565` |
| Egna instrumentnamn | Ja (`layerName1..N`) | Inte uttryckligen | Inte uttryckligen | `PluginBase.cpp:532`, `567–568` |
| Länk mellan presets | Nej i host-state-listan ovan | Valfri `<Link>`; används av instrument-/mönsterpresetflöden | Valfri länk | `PluginBase.cpp:824–841`, `1074–1155` |
| Globala användarinställningar | Separat `Documents/Kickbase/settings.xml`, delas mellan projekt/plugininstanser | Nej | Nej | Se inventeringen nedan; undanta normalt från ljudpreset |

`Pattern` sparar version, notdata, radens längdläge och GATE-steg; ackordintervall läggs till i notraden när de finns (`Pattern.cpp:370–435`). I nuvarande `main` omfattar detta ackord, inte de pågående staplade polyfoninoterna från den separata `content-poly2`-worktreen.

SampleSlot lagrar ljuddata, ursprungligt namn och samplingsfrekvens i trädet (`SampleSlot.cpp:79–112`). User-wavetables lagrar lager, oscillator, namn, stegantal och Base64-kodade 16-bitars data (`PluginBase.cpp:346–420`). Det betyder att projekt/full-preset kan innehålla stor binär data; E0:s “allt påslaget”-test bör kontrollera att både innehåll och metadata kommer tillbaka.

## Separata användarinställningar

`Settings` läser och skriver en gemensam XML-fil under `Documents/Kickbase/settings.xml`; det är inte projektstate eller presetdata. Koden dokumenterar uttryckligen dessa inställningar som delade mellan Kickbase-fönster och projekt (`Source/Core/Settings.h:3–5`; implementation `Settings.cpp:17–61`).

| Inställningsgrupp/nycklar | Scope / beteende | Källa |
|---|---|---|
| `presetsFolder`, `exportsFolder`, `wavetablesFolder`, `libraryFolder` | Användarens valda mappar; delas globalt. Töm värde återgår till standardmapp under `Documents/Kickbase`. | `Settings.h:11–18`, `Settings.cpp:63–87` |
| `uiScale` | Fönsterskala, delas mellan instrument och FX-fönster. | `Settings.h:40–42`, `InstrumentEditor.cpp:45–55`, `FxEditor.cpp:37–47` |
| `reduceAnimations`, `fpsAnalyzer`/`fpsSidechain`/`fpsMaximizer`/`fpsSoundstage`/`fpsEffects`/`fpsMixer`/`fpsTopBar`/`fpsOther`, motsvarande `fill*`, `spectrumAnalysisStep` | Global visnings-/prestandapreferens, inte ljudpreset. Standardvärden utelämnas ur XML. | `Settings.h:44–67`, `Settings.cpp:148–264`, `OptimizationsPanel.cpp` |
| `exportContent` | Minns vilka analys-/exportdelar som valts i Spectrum/Report-UI. | `SpectrumPage.cpp:623` |
| `referencePath` | Valfri referensljudsfil för analyservyn; sparas globalt och läses vid öppning om sökvägen finns. | `SpectrumPage.cpp:278`, `724–757` |
| `chatModel`, `chatApiKey` | Modellval och AI-chattens nyckel; nyckeln går via `protect`/`unprotect` (Windows DPAPI; annat fall base64 enligt implementationen). | `Settings.h:31–37`, `Settings.cpp:108–145` |
| `aiKey` | Nyckel för lokal AI-brygga. Settings-sidan läser/skriver den som vanlig text; skilj den från den skyddade `chatApiKey`. Bör aldrig hamna i preset, rapport eller repo. | `Settings.h:22–27`, `SettingsPage.cpp:72–99` |
| `chatEndpoint` | Testöverskrivning av AI-endpoint; kodkommentaren anger att den bara är för tester. | `AiChat.cpp:186` |

E0 bör hålla isär **återkallbart ljudtillstånd** och **användarens app-/UI-preferenser**. `exportContent` och `referencePath` är analys-UI-tillstånd snarare än ljudtillstånd; om användaren vill att analysrapporten ska bära en viss referens eller exportprofil är det ett separat produktbeslut, inte skäl att lägga in dem i ett vanligt synthpreset. API-/bryggnycklar hör aldrig hemma i ljudpresets.

## Befintliga testytor och luckor att verifiera i E0

- `KickbaseTool presettest` täcker redan kickpreset, radpreset (inklusive noter/GATE/grid/delay och laddning till ett annat lager), FX-kanalpreset och länkade instrument-/mönsterpresets (`Tools/Tool.cpp:1335–1510`).
- Host-testverktyget har state-save/state-load-läge (`Tools/HostTest.cpp:1–6`, `133–163`).
- Dessa befintliga kontroller är inte i sig bevis för en enda full round-trip med alla specialobjekt samtidigt (samples, user tables, envelopes, sidechain-kurva, mod routes/LFO-kurvor, samtliga mönsterdelar och slot-/lager-metadata). E0-planen kräver en sådan riktad “allt på”-kontroll och en host save/load-kontroll.
- För varje post bör testet först skapa ett icke-standardvärde, spara, ändra eller rensa originalet, ladda igen och jämföra det återställda värdet/innehållet. Kontrollera även att data som *inte* hör till ett delpreset lämnas orörd.
- Separera fel i serialisering från designfrågan “ska detta vara preset, projekt, global inställning eller egen presettyp?” Den frågan tillhör Opus/Tobias i E0.

## Nästa steg när E0 startar

1. Uppdatera denna matris mot den beslutade integrationsgrenen, inklusive L/S/AN/LC och polyfoni-UI om de då ingår.
2. Låt Opus/Tobias bekräfta att Settings-posterna ovan är globala preferenser; fråga särskilt om `referencePath`/analysval ska följa projektet.
3. Bygg ett riktat round-trip-test för de specialobjekt som ännu inte har ett gemensamt täckande test.
4. Låt Opus/Tobias besluta preset-/projekt-/global-scope och Init-innehåll innan format eller UI ändras.
