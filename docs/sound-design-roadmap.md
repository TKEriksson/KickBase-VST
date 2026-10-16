# Roadmap: ljuddesign, renderloop och AI-styrning (efter v1)

Kontext för agenten: efter v1 vill vi bygga en skill som hjälper oss att göra riktigt bra presets och förbättra synthen, och sedan låta användare styra synthen med AI. Inget av detta byggs nu, men designa v1 så att det blir lätt att lägga till.

## Att ta hänsyn till redan i v1

- **Parametrar på ett ställe**: ett register med id, namn, intervall, skalning (lin/log/exp) och default, som UI, presets och DSP alla läser från. Dokumentation ska kunna genereras därifrån.
- **DSP-kärnan frikopplad från plugin-wrappern**, så att den kan köras headless från en CLI (offline-rendering utan DAW).
- **Deterministisk rendering**: seedbar slump för noise, drift och unison-faser. Samma preset ska rendera bit-identiskt varje gång.
- **Versionsfält i presetformatet** från start, så att gamla presets kan migreras när parametrar ändras.
- **Rika parameterbeskrivningar i registret**: enhet (Hz, ms, dB), en mening om vad parametern gör soniskt och vilka andra parametrar den samverkar med. Registret blir källan till MCP-schema, dokumentation och skill (fas 4).
- **Parameterändringar via en trådsäker väg** (lock-free-kö eller pluginets parameter-API), så att även externa källor senare kan ändra parametrar utan att röra audiotråden.

## Fas 2: skill (SKILL.md + referensfiler)

1. Parameterkarta: alla parametrar, intervall, skalning, vad de gör i DSP-koden och hur de samverkar.
2. Presetformat: schema och exempelpresets.
3. Ljuddesignrecept per kategori (pads, plucks, bas, leads, FM osv.) med referenspresets och en motivering till varför de låter bra.
4. Tobbes smak: genrer, vad som är bra och vad som ska undvikas.

## Fas 3: renderloop (den viktigaste delen, eftersom agenten inte kan höra)

- **Offline-renderare (CLI)**: preset + fast MIDI-fras → WAV.
- **Analysskript**: spektrogram som PNG, spectral centroid, LUFS/peak, DC-offset, aliasing i toppen, stereobredd per band, envelope-kurva.
- **Jämförelse mot referens-WAV**.

Målet är att agenten själv ska kunna iterera (justera → rendera → analysera) och samtidigt hitta DSP-buggar: aliasing, klick i envelopes, denormals och nivåhopp mellan röster.

## Fas 4: AI-styrning via MCP (byggs före fas 5)

Användaren kopplar sin egen Claude-app (Claude Desktop, Claude Code) eller annan MCP-klient till synthen. AI:n körs på användarens eget abonnemang, så **ingen API-nyckel behövs** och det kostar oss ingenting. Fungerar med AI-appar på datorn, inte med webbversioner (de når inte localhost).

Bygg först ett **gemensamt verktygslager** (verktygen nedan plus `set_pattern`, `set_sidechain_curve` m.fl. för sequencern). Både MCP och AI-chatten i fas 5 använder samma lager, så verktygen skrivs bara en gång. Fas 4 byggs först eftersom den är enklare, gratis för användarna och fungerar som testbädd för verktygen innan chatten läggs ovanpå.

**Arkitektur: brygg-app (rekommenderad)**

```
Claude-app ⇄ (MCP över stdio) ⇄ KickBase-brygga ⇄ (lokal socket/named pipe) ⇄ plugin-instanser i DAW:en
```

- En liten separat körbar fil som användaren registrerar i sin Claude-app en gång. Claude startar den själv.
- Pluginet öppnar ingen nätverksport. Varje instans registrerar sig hos bryggan via lokal IPC.
- Bryggan listar alla körande instanser ("KickBase i spår 3", "KickBase i spår 7") och agenten väljer vilken den styr.
- Bryggan fungerar även utan DAW: läsa och skriva presetfiler, köra offline-rendering (fas 3).
- Alternativ: HTTP-server på localhost direkt i pluginet. Enklare, men kräver öppen port i DAW:en och kan blockeras av vissa DAW:ar eller macOS-sandboxing.

**Verktyg**
- `list_instances`: körande plugin-instanser
- `list_parameters`: id, namn, enhet, intervall, skalning, sonisk beskrivning och relationer (genereras från registret)
- `get_state` / `set_parameters`: batch-ändringar, i både normaliserade värden och riktiga enheter
- `load_preset` / `save_preset` / `list_presets`
- `render_preview`: offline-rendering av en ton eller fras, returnerar analys och spektrogram (återanvänder fas 3)
- `undo`

**Resurser**: parameterdokumentation, signalflöde och ljuddesignguiden (från fas 2).

**Krav och fallgropar**
- Agentens ändringar får aldrig röra audiotråden direkt, utan går via den trådsäkra vägen från v1.
- Ändringar rapporteras till DAW:en (begin/end edit), så att automation och undo fungerar.
- AI-styrning är opt-in med en knapp i pluginets UI. IPC endast lokalt.
- Instruktion för användaren: hur bryggan läggs till i Claude Desktop och Claude Code.

## Fas 5: AI-chatt direkt i UI:t (valfri, ovanpå fas 4)

En AI-knapp i pluginet där användaren skriver till sin AI och ber den justera ljud och mönster ("gör basen mer rolling", "lägg en fill i takt 4"). Chatten använder samma verktygslager som fas 4. Skillnaden är att pluginet självt anropar modellen, så här krävs en API-nyckel. Användare med Claude-abonnemang kan i stället använda fas 4 utan nyckel.

**Teknik**
- **Bring your own key**: användaren anger sin egen API-nyckel. Den sparas i OS:ets nyckelring (Keychain/Credential Manager), aldrig i presets eller projektfiler.
- **Leverantörsoberoende**: ett tunt adapterlager för Claude, OpenAI och lokala modeller (t.ex. Ollama, som inte kräver nyckel).
- **Nätverksanrop på en egen tråd**, aldrig i audio- eller UI-tråden. Strömma svaret.
- **Kontext**: skicka hela tillståndet som JSON (parametrar, mönster, BPM, tonart) med varje förfrågan.

**UX**
- Visa ändringarna innan de appliceras (t.ex. "Cutoff 800 → 1400 Hz") med Godkänn/Ångra, eller applicera direkt med ett tydligt ett-klicks-ångra.
- A/B-knapp mellan före och efter AI:ns ändring.
- Låt ändrade rattar glöda kort så att man ser vad som hände. Gäller även ändringar via MCP i fas 4.
- Placering: en AI-knapp i toppraden bredvid EXPORT som öppnar en utfällbar panel, eller gör om statusraden längst ner ("Hover over any control…") till ett inmatningsfält med en panel som fälls upp för svaret.