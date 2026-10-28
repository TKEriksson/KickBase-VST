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
