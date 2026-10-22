# AI-minne: agenten lär sig av fel och resultat (Tobias förslag, 22 okt)

Status: Implementationen utökas på `ai-extras` efter Tobias önskemål 22 okt om fler lookbacksteg, uppskattad tokenkostnad, arbetsminne och AI-granskning av feedback. Tidigare feedbackminne/Release-build är installerad lokalt; se progress/handoff för aktuell commitstatus.

## Mål
Agenten blir en bättre "personlig" medproducent: den lär sig programmets begränsningar och vad som funkar och inte, och kan alltid gå tillbaka och se hur den löst saker tidigare.

## Förslag 1: felminne
- Varje gång ett verktygsanrop ger fel (t.ex. Strudel "expecting ')'", `set_pattern` "row is locked to 4 bars") sparas felet i en lokal fil som agenten alltid har koll på: verktyg, indata (kort), felmeddelande, och vad som löste det om det blev löst.
- Filen ligger i `Documents\Kickbase\` (t.ex. `Memory\lessons.md` eller `.json`), delas av chatten och MCP-bryggan (samma fil, samma format).
- Agenten läser den i början av varje samtal (via `get_guide` eller ett eget verktyg `get_memory`), så att samma fel inte upprepas. Håll den kort: dedupe likadana fel, räkna förekomster, behåll senaste/mest användbara.

## Förslag 2: uppföljning av resultat
- Efter att agenten gjort en ändring kan den fråga hur resultatet blev, med en enkel dialog (t.ex. "Blev det bättre? Bra / Sämre / Annorlunda" plus valfri kommentar) i chatten.
- Svaret sparas tillsammans med vad som gjordes (parametrar, mönster, mål) i samma minne: "så här löste jag det förra gången och det funkade/funkade inte".
- Agenten kan söka i minnet innan den löser något liknande (`search_memory` eller del av `get_memory`).

## Att bestämma när det byggs
- Format och storlek (tokenkostnad: bara relevanta poster in i kontexten, inte hela filen).
- Integritet: inga API-nycklar, inga projektfiler i minnet. Tobias kan se, redigera och radera minnet (knapp i CHATS-menyn eller SETTINGS).
- Per användare, lokalt, aldrig uppladdat. Skriv det i guiden (`Resources/AGENT_GUIDE.md`) så att agenten vet att minnet finns och hur det används.
- Enkel dialogknapp i UI:t (efter ett verktygsanrop som ändrat ljudet) och motsvarande verktyg för MCP.

## Tillägg 22 okt: betygsätt direkt i AI:ns svar (Tobias)
- När AI:n har ändrat något och skriver sin sammanfattning ("Nu har jag gjort följande ..." med punktlista) ska SAMMA ruta ha betygsknappar längst ner: t.ex. 👍 / 👎 / "Delvis", plus ett litet fält för kommentar ("Vad blev fel?"). Gäller alla svar där AI:n gjort ändringar (verktygsanrop som ändrat ljudet), inte vanliga frågesvar.
- Betyget ersätter den separata uppföljningsdialogen i Förslag 2: ett klick, ingen extra fråga. AI:n kan fortfarande fråga i text, men knapparna finns alltid.
- Sparas i minnesfilen tillsammans med vad som gjordes (verktygsanrop, parametrar, mål) och betyg/kommentar, så AI:n kan se vad som funkade och inte. Ett dåligt betyg är lika viktigt som ett fel (jfr felminnet).
- UI: knapparna ritas i assistant-bubblan i `AiChatPage` (transcripten), bara på det senaste/varje svar med ändringar, och byts mot en bock/vald markering efter klick. Betyg kan ändras.

### Ändring 22 okt: betygskala 1-10 (ersätter 👍/👎/Delvis ovan)
- Betyget är 1-10 (tio små klickbara steg, eller en slider som klickas), direkt i AI:ns svarsruta. Under skalan ett fält "Vad blev du inte nöjd med?" (valfritt, kan skickas med eller utan text).
- Sparas som tal (1-10) + kommentar + vad AI:n gjorde. AI:n tolkar lågt betyg (t.ex. 1-4) som "gör annorlunda nästa gång", högt (8-10) som "så här funkar det bra". Kommentaren är viktigast att ha med när betyget är lågt.

## Tillägg 22 okt: tokenbudget, inställningar och AI-styrning (Tobias)

- **Automatisk inkludering är AV som standard.** Fråga inte användaren vid start. Om den aktiveras ska feedbackkontexten läggas till i modellförfrågningar; var transparent med att den kan kosta tokens.
- **Begränsa kontext separat från lagringen.** I en AI-inställningspopup väljer användaren hur många av de senaste posterna som får läsas in (t.ex. 1, 5, 8, 10 eller 20; standard 8). En post är kort och har redan fältgränser. Använd antal poster i stället för MB som promptgräns, eftersom en MB prompt vore oproportionerligt stor och inte motsvarar tokens.
- **Lokal retention:** spara allt betygsatt feedback lokalt, upp till en konfigurerbar JSON-filstorlek (föreslagen standard 100 MB, valbara gränser). När gränsen överskrids tas äldsta poster bort först. Lagringsgränsen får inte ändra promptgränsen.
- Popup ska visa automatisk inkludering, vald lookback och lagringsgräns, samt separata bekräftade åtgärder för **CLEAR CURRENT CHAT** (bara öppna konversationens meddelanden; sparade chats och feedbackminne kvar) och **CLEAR SAVED FEEDBACK** (hela det delade lokala minnet för chatten och MCP).
- AI/MCP ska kunna hantera inställningarna genom uttryckliga verktyg för att läsa och ändra minnesinställningar samt rensa feedbackminnet. Ändra/rensa inte inställningar eller data utan att användaren ber om det; fråga först innan rensning. Beskriv verktygen och integritets-/tokenbeteendet i `Resources/AGENT_GUIDE.md`.
- `get_memory` ska fortfarande gå att anropa manuellt när minnet är avstängt automatiskt. Guiden ska säga att agenten inte får läsa in minnet automatiskt när inställningen är av, och bara anropa `get_memory` om användaren ber om det eller minnet tydligt hjälper den aktuella uppgiften.
- Tester: standard AV; inställningar överlever omstart; vald lookback respekteras; automatisk promptkontext av/på fungerar; överstor lagring tar bort äldsta poster först; rensning avbryts utan ändring och rensar hela delade minnet efter bekräftad begäran; MCP-verktyg validerar inställningsvärden.

## Uppdaterad design 22 okt (ersätter lookback-stegen ovan)

- **Två minnestyper:** feedback (`feedback.json`) innehåller betyg, kommentar och vad som ändrades; arbetsminne (`work.json`) innehåller kort symptom + fungerande återhämtningsstrategi efter ett löst fel. Råa loggar, källkod, projektfiler och credentials sparas aldrig. Liknande arbetslärdomar dedupliceras och förekomstantalet ökas.
- **Lookback:** 10, 25, 100, 250, 500, 1000, 10000 eller ALL, separat för feedback och arbetslärdomar. Standard 10. ALL begär alla, men resultatet begränsas till en säker uppskattad tokenbudget; returnera antal utelämnade poster.
- **Tokenestimat:** visa ungefärlig kostnad beräknad från faktiskt lagrad text (tecken/4 plus metadata). Det är en uppskattning; provider-tokenisering varierar. API-chattar är stateless: inläst minne ingår i senare kontextbegäranden också, även om en leverantör kan cachea vissa delar. Förklara detta i UI:t.
- **Inläsning:** automatisk inkludering av vardera typen är av som standard. MCP och inbyggd AI får `get_memory`/`get_work_memory` för hämtning på begäran; läs inte automatiskt när respektive inställning är av, om inte användaren ber om det eller minnet tydligt hjälper. Filerna stannar lokalt; poster som läses skickas till vald AI.
- **Arbetsminne:** instruera AI:n att anropa `save_work_lesson` endast efter att den löst ett fel genom ett annat tillvägagångssätt. Lagra redigerad symptom-/lösningstext, inte varje misslyckat försök eller fulla tool arguments.
- **Feedbackåterkoppling:** efter att feedback sparats ska inbyggda AI:n analysera rating/kommentar och föreslå en kort plan om en ändring verkar rimlig. Den review-körningen får bara använda läsverktyg; den får inte ändra ljud. Användaren måste uttryckligen godkänna planen innan AI:n tillämpar den. MCP-guiden och `save_feedback`-resultatet ska instruera samma beteende.
- **Inställningar:** feedback och arbetsminne får separata automatiska inläsningsval, lookback och 1/10/50/100 MB lagringsgränser, äldsta först. Rensa aktuell chatt, feedbackminne och arbetsminne separat med bekräftelse.
