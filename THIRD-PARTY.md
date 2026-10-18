# Third-party software and content

Kickbase is free software under the **GNU Affero General Public License, version 3** (see `LICENSE`). It is built with, or can
download, the following third-party material. Each item keeps its own licence.

## Built into the plugins

| Component | Licence | Notes |
|---|---|---|
| **JUCE 8.0.15** (https://juce.com) | AGPLv3 (Kickbase uses the AGPLv3 option of JUCE's dual licence) | Fetched at build time by CMake (`FetchContent`, tag 8.0.15). `LICENSE.md` in the JUCE source tree describes the dual licence. |
| Libraries bundled inside JUCE's modules (FLAC, Ogg Vorbis, zlib, and others the linked modules use) | Their own permissive licences | The licence texts are in JUCE's source tree, for example `modules/juce_audio_formats/codecs/flac/Flac Licence.txt`. |
| **Steinberg VST 3 SDK 3.8.0** (bundled with JUCE as `modules/juce_audio_processors_headless/format_types/VST3_SDK`) | MIT, "Copyright (c) 2025, Steinberg Media Technologies GmbH" (`LICENSE.txt` in that folder) | VST is a trademark of Steinberg Media Technologies GmbH. |
| **Open303** (Robin Schmidt, https://github.com/RobinSchmidt/Open303) | MIT | Not yet included: the MIT notice is added here, word for word, together with the first file that uses it (the TB-303 filter). |

## Downloaded by the user, never part of this repository

| Content | Licence | Notes |
|---|---|---|
| **Adventure Kid Waveforms (AKWF)**, https://www.adventurekid.se/akrt/waveforms/adventure-kid-waveforms/ | CC0 1.0 (public domain dedication) | Installed on request from SETTINGS (or `install-wavetables.ps1`) from https://github.com/schollz/AKWF-FREE into `Documents\Kickbase\Wavetables\AKWF`. |

## Own material

`Resources/InitPreset.mofp` (the factory preset Init) was made by the project owner and is part of the project under the same licence.

## Sound libraries (planned)

Sound files are never stored in this repository. A sample library on the user's computer carries a `manifest.json` per package with the
source, URL, author, licence and date of every file. Only CC0 (or an equally permissive licence with its notice) may be shipped with
Kickbase; material that is only "royalty-free for your own music" is marked PERSONAL and stays on the user's computer.
