# .kbproj – spara/öppna projekt (spec, D1–D4 beslutade 21 okt, UI-layout återstår)

Status: bara spec. Ingen kod. Hör till fas IO (plan-v2.md:94), men en liten version går att ta tidigare.

## Mål
En fil på disk som håller hela plugin-läget, så att man kan spara, öppna, skicka vidare och lägga i git utan DAW-projektet.
Liten: ingen ZIP, inget arrangemang, ingen autospara i v1.

## Vad som redan finns
`PluginBase::getStateInformation` / `setStateInformation` (`Source/Core/PluginBase.cpp:523`) skriver redan allt: APVTS-parametrar, preset-namn, lagernamn/färger, pattern, kickens sample, instrumentsamplar, användartabeller, sidechain-kurva, Matrix-kopplingar (`modRoutes`), LFO-kurvor, kickens pitch/amp-envelope. Samma blob används av `createOfflineCopy` (`PluginBase.cpp:1461`). Ett projekt är alltså samma data som DAW-läget, i en fil.

## Beslut (Tobias, 21 okt)
- **D1 XML.** Läsbart och git-vänligt (`createXml` på samma state som `getStateInformation`). Inget binärt format.
- **D2 bädda in** användarsampel och användartabeller (som DAW-läget).
- **D3 UI i toppraden vid AI-server-knappen.** Layoutförslag nedan. Inget UI byggs än.
- **D4 inte i v1:** ingen "senaste projekt" och ingen autospara.

## Förslag (format)
- Filändelse `.kbproj`, innehåll = XML-text (UTF-8). Rotelementet bär `kbprojVersion="1"` och pluginversionen som läsbara attribut (ingen binär header). State-versionen (2 efter R9b/E0) ligger kvar inne i själva state-trädet.
- Spara: JUCE `FileChooser` (async), skriv via `TemporaryFile` så att en avbruten skrivning inte förstör gamla filen.
- Öppna: läs XML, kontrollera rotnamn och `kbprojVersion`, anropa sedan `setStateInformation` på samma data. Fel rot/för ny version = felmeddelande, ingenting ändras.
- Inbäddade samplingar blir base64 i XML (större fil, men självbärande). Storleksgräns vid läsning (t.ex. 256 MB).
- Ingen ny parameter. Ingen ändring av ljudtråden. Rendersig påverkas inte.

## Layout i toppraden (förslag, ingen kod)
Dagens knappar i `HeaderBar` (`Source/Ui/Modules.h:58-66`, `Modules.cpp:77-128`): AI SERVER (84 px), presetväljare, EXPORT, PREVIEWS-gruppen, restart. Att lägga SAVE/OPEN som två egna knappar gör raden för trång vid smal bredd (presetbredden krymper redan i `Modules.cpp:131`).

- **A (rek.): en meny-knapp "PROJECT" som ersätter dagens AI-knapp-platsen och samlar verktygen** i en `PopupMenu` (samma mönster som `showExportMenu`, `Modules.cpp:~148`):
  - Save project… / Open project…
  - Export… (dagens EXPORT-undermeny kan ligga kvar som egen knapp tills vidare)
  - AI server: ON/OFF (bockad rad; statusen syns som liten prick/färg på själva knappen så att man ser den utan att öppna menyn)
  - Senare: Settings, Recent (D4), Verktyg
  Fördelar: en knapp i stället för tre+, plats för framtida punkter, ingen ny bredd. Nackdel: AI-läget kräver ett klick extra, därför statusprick.
- **B:** behåll AI SERVER-knappen som den är och lägg bara SAVE / OPEN som två smala knappar (ca 44 px var) före presetväljaren. Enklast, men tar ~96 px och presetväljaren krymper.
- **C:** menyrad (Project / Tools) som i ett vanligt program. Mest plats att växa i, men tar höjd i en header som redan är låg.

Rek. A. AI-växlaren finns också i `SettingsPage.cpp:269`, så ingen funktion försvinner om den flyttas.

## Öppna frågor kvar
1. A, B eller C för D3?

## Risker/test
- Init-check: ladda Init, spara, ladda om, jämför state-blobben byte för byte. Det är ett ställe att bevisa att formatet är stabilt.
- `persisttest` finns redan för state-rundtur; utöka med fil-rundtur (spara till temp, ladda i ny instans).
- Äldre projekt (före nya parametrar) laddas som idag eftersom samma `setStateInformation` används.
- Säkerhet: filen läses som data. `setStateInformation` måste tåla trasig indata (kontrollera storleksgräns innan läsning; avvisa filer över t.ex. 256 MB).

## Uppskattad storlek
Ett par hundra rader (fildialog + header + fel hantering + test). Beroende: ingen annan fas.
