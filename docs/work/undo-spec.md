# Spec fas C: ångra / gör om (T14)

Skriven av projektledaren i specialistrollen, 2026-10-27. Implementeras av Arbetare 1. Bygger på masterplanen (Fas C) och Tobias beslut: huvudpreset tömmer historiken, del-preset är EN transaktion.

## Mål
Ctrl+Z ångrar, Ctrl+Shift+Z och Ctrl+Y gör om. En ångring backar en hel **gest** (en knobbdragning, en pianorullsgest), aldrig ett litet steg. Gäller först pianorullen, sedan parametrar.

## Grundregler
1. **En** `juce::UndoManager undoManager` i `PluginBase` (bredvid `apvts`, `PluginBase.h:239`). Deklareras **före** `apvts` så den lever längre. Både Kickbase och Kickbase FX ärver den (FX använder bara parameterdelen).
2. Allt sker på **meddelandetråden**. Ljudtråden rörs inte; `Pattern` skrivs redan bara från meddelandetråden.
3. `undoManager.setMaxNumberOfStoredUnits (30000, 200)` (200 transaktioner räcker; gränsen skyddar minnet). Mät minnet av en radögonblicksbild (se nedan) och skriv det i PROGRESS-w1.md.
4. Ångring får aldrig skapa en ny undopost: kör `undo()`/`redo()` och låt UndoManager själv skydda mot återkoppling. Ett UI som reagerar på ändringen (`patternChanged()`) får inte skriva tillbaka.
5. Tidsstämpel/namn: `beginNewTransaction (juce::String name)` med kort engelskt namn ("Edit notes", "Move notes", "Knob: <param>"), används för tooltip/menyrad "Undo <namn>".

## Parametrar
- Ge UndoManager till APVTS-konstruktorn: `PluginBase.cpp:32` `apvts (*this, &undoManager, "Parameters", ...)`. Biblioteket spelar då in ändringar via `AudioProcessorParameter`/`ParameterAttachment`; läs `juce_AudioProcessorValueTreeState.cpp` i 8.0.15 (hur `setValueNotifyingHost` och `ParameterAttachment::beginGesture` samverkar med undo) **innan** du bygger något eget, och dokumentera vad du hittar i PROGRESS-w1.md.
- **Gestgräns:** varje knob/slider/ModSlider börjar en ny transaktion i `mouseDown` (eller via `onDragStart`/`beginChangeGesture`), inte per värdeändring. Hittar du ett gemensamt ställe (`KnobComponent`, `Components.cpp:328`-ish, och de ~32 attachments i `Ui/`), lägg `beginNewTransaction` där en gång. Modulationsdjup (`dragStartAmount`) är samma sak: en gest = en transaktion.
- Automation från värden ska **inte** hamna i undo. Kontrollera att `ParameterAttachment` bara spelar in användargester; annars sätt `undoManager`-skrivning av bara vid `beginGesture`..`endGesture`.
- Modulationsmatrisen och val i rullgardiner (ComboBox) = en transaktion per ändring.

## Pattern (pianorullen)
Ingen ny datamodell. Använd befintligt API.

- **Primitiv:** `Pattern::rowToValueTree (layer)` / `rowFromValueTree (layer, tree)` (`Pattern.h:274-275`) ger en komplett radögonblicksbild (toner, staplar, ackord, glide, GATE, längder). Ny klass `PatternRowAction : juce::UndoableAction` håller `layer`, `before`, `after` (båda `ValueTree`). `perform()` skriver `after`, `undo()` skriver `before`; båda anropar `patternChanged()` (eller motsvarande) så UI och `refreshLengths()` uppdateras. Raden skrivs med `rowFromValueTree`, vilket redan hanterar längd/`contentOf`.
- **Hur man fångar före/efter:** `PatternEdit`-hjälpare: `begin (layers...)` tar `before` för berörda lager, `commit ()` tar `after`, hoppar över om lika (`isEquivalentTo`) och lägger en `PatternRowAction` per berört lager i **samma** transaktion. Lager som en gest rör vet pianorullen redan (markering, ritad ton, flytt mellan rader = flera lager).
- **Gestgränser i `PianoRoll`:** `begin` i `mouseDown` (`PianoRoll.cpp:1814`) när gesten kommer att ändra något, `commit` i `mouseUp` (`:2258`). Dessutom: ta bort/klistra in/dubblera/kvantisera/transponera/radera (tangenter och högerklicksmenyn ~`:1887`) = en `begin`+`commit` runt åtgärden. Högerdrag som ritar glidestreck (D1, `0515d2f`) = en transaktion; menyval utan drag = egen transaktion per menyval.
- **Längdändringar** (`setManualBars`, `setExtraBars`, trimma rad, `+`): ingår i radens ögonblicksbild; kontrollera att `rowToValueTree` bär `manual`/`extra`. Om inte, utöka snapshoten (versionsgräns: ändra inte `currentVersion` om det går att undvika, bara en intern ögonblicksbild) och testa.
- **SAMPLE/SLICER-underrader** (`RowRegistry`): samma sak per lager-slot; kontrollera att `rowToValueTree` täcker `numRowSlots`, inte bara 0–10.
- **Spela/preview får inte spela in.** Förhandsvisning av en ton (klick/spel) är ingen ändring.
- Minne: en rad är litet (noter är sparse). Om en snapshot ändå blir stor (>~200 KB) logga det och föreslå delta; bygg inte delta nu.

## Live Code, AI och mallar
- **Live Code-körning** (kör-knappen): `begin` före, `commit` efter ⇒ EN transaktion oavsett hur många lager/parametrar koden rör. Parametrar den sätter fångas av APVTS-undo, mönsterlager av `PatternEdit` (alla lager som koden berör; enklast: alla `numRowSlots`-rader, hoppa över oförändrade).
- **AI-chattens verktyg**: ingen ångring i historiken (Tobias beslut 27 okt: chatten har sitt eget "undo"-verktyg och tokens är redan använda, så en ångring blir förvirrande). Ursprunglig plan var en transaktion "AI edit" per svar.
- `applyTemplate` / `applyLayerTemplate` / `setKickSteps` från UI: en transaktion.

## Presets
- **Huvudpreset** (`Presets::Section::full`, `loadFactoryPreset`, `loadUserPreset`, init `PluginBase.cpp:1269`, projektladdning): efter lyckad laddning `undoManager.clearUndoHistory()`. Ingen ångring över preset-byte. Detsamma vid `setStateInformation` (värden laddar projektet).
- **Del-preset** (kick, instrument, bas, mönsterrad, motor osv. via `loadPresetXml:1272` med `section != full`): **EN transaktion**: `beginNewTransaction`, ladda, klart. Parameterändringarna fångas av APVTS-undo (säkerställ att `loadPresetXml` skriver parametrar via `setValueNotifyingHost`/`ValueTree`-vägen som UndoManager ser, och att den inte i sig anropar `beginNewTransaction` flera gånger). Mönsterdelen och eventuellt Live Code-text/instrumentsamplar/användartabeller (`liveCodeToValueTree`, `instrumentSamplesFromValueTree`, `userTablesFromValueTree`) tas med som `UndoableAction` med före/efter-träd av samma typ som `PatternRowAction` (en generisk `TreeAction` med `std::function<void(const ValueTree&)> apply`). Om något av dessa är för tungt (stora samplar): dokumentera och låt just den delen **inte** vara ångerbar, hellre än att hålla stora buffertar; skriv det i PROGRESS-w1.md och i FL-testlistan.
- Preset-laddning har redan skydd för ljudtråden (`PluginBase.cpp`/`Presets.cpp:340`); ändra inget där utöver undo-anropen.

## Tangentbord och meny
- Ctrl+Z = `undoManager.undo()`, Ctrl+Shift+Z och Ctrl+Y = `redo()`. Fånga i editorns `keyPressed` (högsta nivå) **och** i `PianoRoll::keyPressed` (när den har fokus). Textfält (Live Code-editorn, chatten, preset-namn) behåller sin egen textångring: låt dem hantera tangenterna först (`TextEditor`/`CodeEditorComponent` har egen UndoManager).
- **Historikdialog (Tobias 27 okt, ersätter Undo/Redo-rader i menyn):** "History..." i Meny öppnar en dialog (icke-modal, går att ha öppen medan man jobbar; samma stil som övriga dialoger). Den listar alla transaktioner, nyast överst. Ångrade steg (redo-delen) visas gråa ovanför en markering "Now", och utförda steg visas under den. Underlaget är `getRedoDescriptions()`/`getUndoDescriptions()`. Lyssna på UndoManager (`ChangeBroadcaster`) så listan uppdateras direkt.
  - Knappar under listan: **Undo**, **Redo** och **Restore**. Restore går till markerat steg: `undo()` N gånger bakåt eller `redo()` N gånger framåt. Dubbelklick på en rad gör samma sak som Restore.
  - Gråa knappar när de inte går att använda (`canUndo()`/`canRedo()`).
  - Namnen ska säga vad som ändrats, till exempel "Edit notes: Instr 1", "Knob: Filter Cut", "Live Code run", "AI edit" och "Load preset: Kick". Tidsstämpeln (hh:mm:ss) läggs i början av transaktionsnamnet, eftersom UndoManager bara bär en sträng per transaktion.
  - När huvudpreset laddas töms listan (se Presets). Visa då en rad "History cleared: <preset>", som bara står i UI:t och inte går att ångra.
- FL Studio kan ta Ctrl+Z först när editorn saknar tangentbordsfokus. Det är en DAW-gräns; notera i FL-testlistan att pianorullen/knobbarna måste ha fokus (Tobias bekräftar vid FL-testet).

## Samspel / kantfall (testa eller dokumentera varje)
1. Ångra efter att värden ändrat en parameter (automation): undo ändrar bara användarens senaste gest.
2. Ångra en pianorullsändring efter att raden raderats/ändrats av Live Code: `PatternRowAction::undo` skriver hela radens före-bild; det är OK (gäller senaste transaktionen först, LIFO).
3. Preset laddas under pågående gest: avbryt gesten (`commit` med oförändrat → tomt), töm historiken.
4. Undo av glidestreck (D1): ingår i radens snapshot; kontrollera att `rowToValueTree` bär glidestrecken (annars utöka). MIDI-exporten av D1-streck: kolla varningen (`RollFiles::exportMidi`), det var inte verifierat.
5. Undo av längdändring: UI-längdkontroller (`refreshLengths`) uppdateras via samma `patternChanged()`.
6. Tom transaktion: `commit` utan skillnad lägger **ingen** post (annars blir Ctrl+Z ett "tyst" steg).
7. FX-versionen: bara parametrar, ingen pianorulle; bygg Kickbase FX och kontrollera att det kompilerar och att Ctrl+Z fungerar på knobbar.

## Tester (minimala)
- `StateTests`: (a) ändra en parameter i en gest, `undo()` ⇒ ursprungsvärde, `redo()` ⇒ nytt; tre `setValue` i samma gest ⇒ ETT undo-steg. (b) `PatternEdit`: tre toner i en gest, `undo()` ⇒ raden som före, `redo()` ⇒ efter; oförändrad gest ⇒ `canUndo()` oförändrad. (c) `loadFactoryPreset` ⇒ `canUndo()==false`. (d) del-preset ⇒ EN `undo()` återställer parametrar **och** mönsterrad.
- Ingen UI-test krävs; FL-testlistan (PROGRESS-w1.md): dra en knobb mycket och ångra (går tillbaka till före gesten), rita/flytta/radera flera toner och ångra ett steg i taget, kör Live Code och ångra, ladda huvudpreset (Ctrl+Z gör inget), ladda en kick-del-preset och ångra.

## Ordning (Arbetare 1)
1. UndoManager + APVTS-argument + läs bibliotekskällan (parametrar kan då redan fungera, men gestgränserna saknas).
2. `PatternRowAction`/`PatternEdit` + pianorullen (`mouseDown/Up`, tangenter, menyn).
3. Ctrl+Z/Y-tangenter + historikdialogen (Meny → History...).
4. Gestgräns för knobbar/sliders/ModSlider.
5. Live Code/AI/mallar = en transaktion.
6. Preset-regler (huvud = töm, del = en).
7. Tester, PROGRESS-w1.md, FL-testlista.

Biblioteksnotis till Tobias (CLAUDE.md): läs själv `juce::UndoManager` (`modules/juce_data_structures/undomanager/juce_UndoManager.h`) och avsnittet om `beginNewTransaction`/`UndoableAction`; det är ~15 minuter och ger hela bilden.
