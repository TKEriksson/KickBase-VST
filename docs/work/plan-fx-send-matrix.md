# FX send-matris och 12 FX-kanaler (plan, efter rel-0.3)

Status: plan, ingen kod. Beslutad av Tobias 28 okt: 12 kanaler som tak, send per kanal med eget belopp. Byggs efter rel-0.3.

## Mål
- Varje ljudkälla (kick + instrument 1-10 = 11 st) kan skicka till flera FX-kanaler samtidigt, med eget belopp per kanal. 0 = av.
- Upp till **12 FX-kanaler** (idag 11, `Effects.h:23` `numEffectChannels`). Tanken: en kanal per källa (11) + en 12:e för t.ex. egen sidechain.
- Start med 3 synliga kanaler; **+** och **−** lägger till/tar bort, 1-12.
- FX-väljaren per källa (idag None, FX1-FX11) blir en lista med ett litet reglage per kanal. Bara kanaler som finns visas.
- Knapparna på FX-sidan (FX1..FX11) blir en dropdown.

## Idag
- Varje källa har en `fxChannel` + `fxAmount` + `fxSend` (SEND-läge): `InstrumentProcessor.cpp:992-1011`, `Params.h:65-68, 112, 145`. En kanal åt gången.
- Kanalanvändning: `fxChannelUsageMask()` (`PluginBase.h:48`), `fxChannelHasEffects`, `firstFreeFxChannel` (`PluginBase.h:139-140`), `MixerPage.cpp:263-363`.

## Förslag
1. **Bakåtkompatibilitet:** `fxChannel`/`fxAmount`/`fxSend` behåller sin betydelse. Gamla presets låter bit för bit likadant (kontrolleras med rendersig). Listan visar den gamla kanalen och beloppet som reglaget för just den kanalen.
2. **Nya parametrar sist** (ID, preset-index och hashar ändras inte): 11 källor × 12 kanaler = 132 sendbelopp (0..1, standard 0), vanliga parametrar så de går att automatisera. Hjälptext för alla (helpcheck), `Help::forParameter`.
3. **Kanalantal** som sparat läge (inte parameter). Kanaler som används av ett laddat preset syns alltid, även om bara 3 syns som start.
4. **Ta bort kanal** som har effekter eller sends: fråga först, ingenting försvinner tyst.
5. Kanal 12 behöver samma stöd som övriga i mixern, FX-pluginet och sidechain-routingen.

## Att kolla först (ej undersökt än)
- Hur hårt "11" sitter: arrayer, mixern (`MixerPage.cpp`), FX-pluginet (`Source/Fx`), sparade presets, sidechain-ingångar.
- Ljudvägen när en källa skickar till flera kanaler: torrt/blött, SEND-läge och gain-staging per kanal.
- CPU: kanaler utan effekter ska inte kosta något.

## Tester
- paramtest/helpcheck för de 132 nya parametrarna; rendersig oförändrad (alla sends 0); ny test: två sends från samma källa ger summan av var för sig; gammalt preset med en kanal låter likadant; blocksizetest.
- Tobias testar i Lemon: sidechain på kanal 12, flera sends från kicken, gamla presets.

## Beslut 28 okt (Tobias): UI och summa
- Summa: effektivt belopp per kanal = gamla fxAmount (om kanalen matchar) + matrisen. Gamla presets blir bit-identiska när matrisen är 0.
- Gamla matrisen i UI: popup från FX-väljaren per källa, en rad per kanal som finns: `FXn [på/av] slider %`. Överst/nederst en rad `+ add FX channel`. Knappen visar ett sammandrag (t.ex. "FX1 50%, FX3 30%" eller "None"). Gamla INS/SEND-kopplingen visas som en vanlig rad.
- På/av per rad är bara en UI-genväg, inga extra parametrar: av = beloppet 0 och slidern disabled; på = slidern går att dra. Senaste värdet kommer ihåg medan fönstret är öppet, inte i presetet.
- Kanalantal är globalt: `+ add FX channel` i popupen lägger till en kanal överallt (nya kanalen har send 0). Ta bort görs bara på FX-sidan, med fråga om effekter/sends finns.
- Senare: mixer-lik matris som översikt (samma parametrar), M-knapp per kanal på mixersidan.
- Beslutat (Tobias 28 okt): M-knapp (mute) per FX-kanal i mixern, tystar hela kanalen för alla källor, sends ligger kvar. 12 nya parametrar (fxMute1..12, standard av), sist i layouten, hjälptexter, automatiserbara. Steg 3 eller senare, inte rel-0.3. Tester: muteat = tyst, ej muteat = oförändrat (rendersig), ingen klick vid mute (ramp, blocksizetest).
