# Plan: reverse per not (Arbetare 2, rel-0.3)

Mål: lagrets sampling kan spela varje ton baklänges. Av = exakt gamla ljudet.

- Ny parameter sist i `InstrumentField`: `sampleReverse` (Bool, av) → ID `bass{N}SampleReverse`.
- Motorn (`InstrumentSynth.cpp`, noteOn + sample-grenen): startposition speglas (START räknas från slutet), steget går bakåt; förbi början loopas från slutet (LOOP) eller samplingen tystnar. Gäller hela samplingen och multisampling (hastighet, KEY och PITCH-kurvan som vanligt).
- Skivor (slicer) har redan sin egen `slicerReverse`; `sampleReverse` rörs inte där.
- Test: baklänges är tyst där ljudet låg först, ljudet kommer sist, tyst efter slutet; av = identiskt.

UI behöver: en på/av-knapp REVERSE vid samplingen (`bass{N}SampleReverse` via `ParamID::bass (layer, InstrumentField::sampleReverse)`). Vågformsmarkören `getSampleFraction()` går bakåt.
