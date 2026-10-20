# Spec: gratis AI-chatt via Gemini + guidad nyckel

Mål: en ny användare får fungerande AI-chatt på några klick, utan att veta vad en API-nyckel är. Egen Anthropic-nyckel kan kopplas senare.

## Regler
- Ingen nyckel i repot eller binären. Användaren skapar sin egen (gratis) nyckel; den lagras krypterat som idag (`Settings::setChatApiKey`).
- Kolla Googles AKTUELLA gratisvillkor/gränser (ai.google.dev, Google AI Studio) innan texten i UI:t skrivs; lova inget som inte står där. Skriv gränsen kort i UI:t.
- Anthropic-flödet ska fungera exakt som idag (regression-säkert).

## Delar
1. Leverantör: val Anthropic / Gemini i nyckelsidan (`Source/Ui/AiChatPage.cpp:197`, `Source/Core/AiChat.cpp`). Gemini API: generateContent med function calling mappad på samma verktyg som ToolLayer redan exponerar. Isolera leverantörsskillnader bakom ett litet interface så att resten av chatten inte vet vilken som körs.
2. Guide-knapp "Skaffa gratis med guide": öppnar AI Studios nyckelsida i webbläsaren, visar 3 korta steg, inklistringsruta, och testar nyckeln direkt (ett litet anrop) med tydligt fel/OK.
3. Fel: ogiltig nyckel, slut på gratiskvot (429) och nätverksfel ger begripliga meddelanden, aldrig krasch eller frysning (anrop på bakgrundstråd).
4. Test: leverantörsgränssnittet med en fejkad HTTP-svarsfil (parse av text + tool call + fel). Inga riktiga nyckel- eller nätanrop i testerna.

## Utanför
Egen proxy-server med delad nyckel (senare, kostar drift). Lokal modell (Ollama).

## Ordning
Efter release nr 2 och Tobias test av L/S. Egen gren från main.
