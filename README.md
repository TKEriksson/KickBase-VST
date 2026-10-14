# MyOwnFruit

En VST3-synth byggd med [JUCE](https://juce.com) 8.

## Bygga

Kräver Visual Studio Build Tools med C++-verktygen (CMake och Ninja följer med).

```
build.bat            # Release
build.bat Debug
```

JUCE laddas ner automatiskt första gången.

## Installera

Kopiera mappen `build\Release\MyOwnFruit_artefacts\Release\VST3\MyOwnFruit.vst3`
till `C:\Program Files\Common Files\VST3\` (kräver admin) och scanna om plugins i din DAW.

Den fristående appen ligger i `build\Release\MyOwnFruit_artefacts\Release\Standalone\MyOwnFruit.exe`.
