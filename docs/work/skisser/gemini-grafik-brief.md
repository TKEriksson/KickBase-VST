# Brief till Gemini: grafik till Kickbase (för 0.5)

Kickbase är en mörk VST3-synth för psytrance (kick + bas). Gränssnittet är mörkt, tekniskt, med cyan och fuchsia som accenter. Nu ritas allt i kod; vi vill ha riktiga bilder för en snyggare känsla.

## Fasta ramar
- Stil: mörk, matt/glas, subtil metall eller kolfiber, diskret brus. Inget färgglatt, inget som drar blicken från rattar och grafer.
- Palett (hex): bakgrund #0d1018 och #0b0d14, panel #0e111a och #121622, kant #232a3b, cyan #22d3ee, ljus cyan #67e8f9, fuchsia #e879f9, dämpad text #94a3b8.
- Bilderna ska gå att lägga under text och rattar utan att störa: låg kontrast, jämn ljushet.
- Måste vara fria att använda i ett AGPL-projekt, ingen text eller logotyper i bilden.

## Leveranser (börja med 1–3)
1. Bakgrund för hela fönstret: 2360 x 2056 px (2x av 1180 x 1028), PNG, mörk, nästan jämn.
2. Paneltextur som kan upprepas (sömlös/tile): 512 x 512 px, PNG, för modulernas bakgrund.
3. Glasyta för rubrikfält/flikrad: 1024 x 128 px, PNG med alfa, svag glans högst upp.
4. (Senare) Ikoner som SVG: play, högtalare, hjälp (?), plus, stäng, 24 x 24 enhetsruta, enfärgade (vita, vi färgar i koden).
5. (Senare) Rattar: förslag på knopp som SVG, utan markeringsstreck (strecket ritar vi i koden).

## Så här kontrollerar vi
Lägg in i en testbygge, titta i Lemon på normal och liten fönsterstorlek. Bilden ska inte göra text eller grafer svårare att läsa.

## JOG-hjulet som LP-spelare (Tobias idé)
Idag ritas hjulet i koden (JogPanel.h:234-290): en 140 x 140 px skiva med sockel och en nål. I koden roterar vi skivan när man drar i den, så bilden ska vara en platt, rak uppifrån-vy som kan snurras.

Filer att be om:
1. Skivan: PNG, kvadratisk, 560 x 560 px (4x), transparent bakgrund utanför skivan, helt rakt uppifrån, mittpunkten exakt i bildens mitt. Skivans kant går ända ut till bildens kant. Ingen skugga i bilden (vi lägger skuggan i koden, annars snurrar den med).
2. Sockel/skivspelarplatta: PNG, 640 x 640 px, transparent bakgrund, med hål/plats i mitten där skivan läggs.
3. Pickup-arm (tonarm): PNG, transparent bakgrund, rakt uppifrån, ligger rakt (0 grader), svängpunkten i bildens övre högra hörn, ca 400 x 120 px. Vi vinklar den i koden.

Prompt att klistra in i Gemini (en bild i taget, byt ut det som står i hakparentes):

> Skapa en platt bild rakt uppifrån av [en vinylskiva / en skivspelarplatta / en pickup-arm] för ett mörkt VST-plugin-gränssnitt. Stil: matt svart, diskret cyan (#22d3ee) detalj, fin ringstruktur i spåren, subtil glans, tekniskt och minimalistiskt. Ingen text, inga logotyper, inga skuggor, inget bakgrundsmönster. [Skivan: etikett i mitten med en enkel cyan ring och ett litet hål i exakt mitten, skivan fyller hela kvadraten.] Transparent bakgrund, PNG, [560 x 560 / 640 x 640 / 400 x 120] pixlar.

## Filkrav (gäller alla bilder)
- Format: PNG med alfakanal (transparens) för allt som ligger ovanpå något annat. SVG för enkla ikoner. Inte JPG (ingen transparens) och inte WebP.
- Stora i förhållande till användning (2x–4x), vi skalar ner i koden så det blir skarpt.
- Rakt uppifrån, inga skuggor eller reflektioner som beror på ljusriktning, eftersom bilden kan roteras.
- Under 1 MB per bild om det går. Lägg filerna i en mapp och skicka sökvägen, så lägger vi in dem via BinaryData.
- Gemini gör ibland "falsk" transparens (rutmönster). Kontrollera att bakgrunden verkligen är genomskinlig, annars be om "ren vit bakgrund" så tar vi bort den.
