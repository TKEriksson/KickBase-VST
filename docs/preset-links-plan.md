# Länkade presets: plan (ej byggt än)

Beställt av Tobias 2026-10-17, i den här ordningen. Bygg i steg, ett i taget, med tester.

## Läget nu (efter commit "Pattern preset per row")

- **Pattern preset** (`Presets::Section::pattern`, `layer` = instrument 0-9 eller `Pattern::kickLayer`): en rad. Sparar toner + pianorulle + GATE-steg
  (`Pattern::rowToValueTree`), och parametrarna för raden (`belongsTo`: grid, GATE-ratten/LENGTH, gateOn/Grid/Depth/Smooth, delay, swing,
  humanize, rowBars, rowLoop; kicken: rowGridKick, rowBarsKick, rowLoopKick). Filen har `scope="row"` och `rowKind="kick|instrument"`.
  Knappen sitter i kortet för det valda instrumentet (`PatternModule::presets`, `PresetButton::setLayer`).
- **Instrument-preset** (`Section::bass`, `layer`): ljudet. **Kick-preset** (`Section::kick`).
- **FX Preset** (`Section::effects`): ALLA effektkanaler på en gång (+ sändningarna). Det är det som måste ändras (steg 1).
- Namn på laddat preset per rad: `PluginBase::rowPresetNames`, per instrument `bassPresetNames`, övrigt `presetNames`.
- Fabrikspresets för Pattern visas inte i menyn längre (FILL har rytmerna).

## Önskat

1. **FX Preset per effektkanal.** `Section::effects` med `layer` = kanal 0-10. Sparar kanalens 5 platser (typ + rattar), kanalens EQ och
   kompressor, nivå/duck. Parameter-ID skrivs som för FX1 (`fxA...`) och flyttas till kanalen man laddar i. Knappen på EFFECTS-fliken följer den
   valda kanalen. Äldre filer (alla kanaler) döljs i menyn (som mönster-presets: `scope="channel"`).
2. **Instrument- och kick-preset kan länka en effekt.** Spara-dialogen får en ruta "Save its effect too". Är kanalen instrumentet skickar till
   inte sparad som FX Preset (inget namn) frågas efter ett namn och FX Preset sparas först. Instrument-presetet får `<Link kind="effect" name="..." amount="...">`
   (inte effekten själv). Samma för kicken.
   - **Ladda:** hitta FX Preset med det namnet. Lägg det på första lediga effektkanal (`PluginBase::fxChannelUsageMask`), sätt instrumentets
     `fxChannel`/`fxAmount`. Finns ingen ledig kanal: fråga **Replace FX n / Skip the effect / Cancel**. Finns inte presetet längre:
     varna och ladda utan effekt.
3. **Pattern preset kan länka ett instrument** (och för kickens rad: en kick). Spara-dialogen får "Include the instrument". Raden har inget namn
   på sitt instrument-preset (`bassPresetNames` är "Default" eller tom): fråga efter ett namn och spara instrument-presetet först (som i sin tur
   länkar sin effekt enligt 2). Mönster-presetet får `<Link kind="instrument" name="...">`.
   - **Ladda:** finns länk: fråga **Replace the instrument / Keep my instrument (only the pattern) / Cancel** så att inget instrument går
     förlorat. Länken pekar på en fil: saknas den, varna och ladda mönstret.
4. Beslut att ta (rekommendation inom parentes): länka med **namn** (som beställt) eller bädda in en kopia (länk, men spara filnamnet och visa
   tydligt om det saknas). Byter man namn på ett preset bryts länken: lägg en rad i menyn "Rename" som uppdaterar länkarna? (nej: håll det enkelt).

## Tester

- Spara FX Preset för kanal 3, ladda i kanal 7: platser, EQ och komp följer med, kanal 3 orörd.
- Instrument med länkad effekt: ladda när FX1 är upptagen -> hamnar på FX2; när alla 11 är upptagna -> dialog (testa via ett anrop utan UI).
- Pattern preset med länkat instrument: ladda med/utan "Keep my instrument"; saknad länk.
- `rendersig` ska vara oförändrad.

## Att tänka på

- PresetButton har redan `setLayer`, `setEmptyLabel`, bekräftelsedialog (`AlertWindow::showAsync`, första knappen ger 1) och en meny som kan stå kvar öppen.
- `PluginBase::getUserPresets (section, layer)` filtrerar på `scope`/`rowKind`: gör likadant för kanalpresets.
- Effekternas parameter-ID: `Effects::slotParamID`, `Effects::channelParamID`, `Effects::isEffectParameter`.
