# Mixning: FX, sidechain, MONO BELOW

## Konkreta värden (källor i rollbas.md)
- Mono under ~120 Hz (Armada, Gearnews 100 Hz, Renoise 200–300 Hz som övre gräns); skolan/referensen: 120 Hz. Bredd bara på basens mellan- och höjdregister, lågpass/avskär sidokanalen.
- Crossover ca 90 Hz mellan sub och mellanlager (Myloops).
- Sidochain/ducking: kompressor 0.1 ms attack, 8:1, 8–12 dB reduktion, 80–100 ms release (Myloops/Sokolovskiy). Vid 16-delsrull: återhämtning 50–60 ms eller grundare djup, annars pumpar det.
- Lätt rörmättnad på både kick och bas (Gearspace).
- Basdelay 5–15 ms efter kicken (Sokolovskiy).

## I Kickbase
- Sidechain per lager: `bassNDuck` (djup), `scMode` (Kick envelope), `scLength`; kurva via `set_sidechain_curve`. Parameternamn och intervall: `list_parameters prefix=bass`.
- FX per lager: `fxSend` / `sendTo(...)` i `PresetData.inl` (t.ex. rad ~301, 346, 377, 386). Soundstage/MONO BELOW: se mixerns/masterns läge i `docs/kick-bass-school.md`.
- Öppet: exakta Kickbase-värden för `scLength`/`bassNDuck` per bas fastställs när basarna byggs och mäts (`analyse_mix`); skrivs då in här.
