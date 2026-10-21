# Third-party software and content

Kickbase is free software under the **GNU Affero General Public License, version 3** (see `LICENSE`). It is built with, or can
download, the following third-party material. Each item keeps its own licence.

## Built into the plugins

| Component | Licence | Notes |
|---|---|---|
| **JUCE 8.0.15** (https://juce.com) | AGPLv3 (Kickbase uses the AGPLv3 option of JUCE's dual licence) | Fetched at build time by CMake (`FetchContent`, tag 8.0.15). `LICENSE.md` in the JUCE source tree describes the dual licence. |
| Libraries bundled inside JUCE's modules (FLAC, Ogg Vorbis, zlib, and others the linked modules use) | Their own permissive licences | The licence texts are in JUCE's source tree, for example `modules/juce_audio_formats/codecs/flac/Flac Licence.txt`. |
| **Steinberg VST 3 SDK 3.8.0** (bundled with JUCE as `modules/juce_audio_processors_headless/format_types/VST3_SDK`) | MIT, "Copyright (c) 2025, Steinberg Media Technologies GmbH" (`LICENSE.txt` in that folder) | VST is a trademark of Steinberg Media Technologies GmbH. |
| **Open303** (Robin Schmidt, https://github.com/RobinSchmidt/Open303), the TB-303 filter (`TeeBeeFilter`) | MIT, "Copyright (c) 2009 Robin Schmidt (www.rs-met.com)" | Source files in `Source/ThirdParty/open303/` (upstream commit 313bf0d9ade7c1dcb6b3a74f5ea1780a29d70074, 29 March 2024; see the README.txt there for the four small changes needed for 64-bit MSVC). The notice below applies. |
| **Strudel** (`@strudel/core`, `@strudel/mini` 1.2.6, https://codeberg.org/uzu/strudel), the pattern engine behind LIVE CODE | AGPL-3.0-or-later, "Copyright (C) Strudel contributors" | Bundled into `Resources/strudel/strudel.all.js` (built by `Resources/strudel/build-bundle.sh` from `package-lock.json`). The banner is at the top of that file. The source is the file plus the bundle script in this repository and the upstream repository. |
| **kabelsalat** (`@kabelsalat/core` 0.4.0, `@kabelsalat/lib` 0.4.1, `@kabelsalat/web` 0.4.1), pulled in by Strudel | AGPL-3.0-or-later | Part of the same bundle. Only loaded, no audio is made with it. |
| **fraction.js** 5.3.4 (https://github.com/rawify/Fraction.js), pulled in by Strudel | MIT | Part of the same bundle. |
| **QuickJS** (the JavaScript engine inside JUCE's `JavascriptEngine`) | MIT | Comes with JUCE 8.0.15. It runs the Strudel bundle. |
| **esbuild** 0.28.2 (https://esbuild.github.io) | MIT | Build tool only (`build-bundle.sh`); its code is not shipped in the plugin. |

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

## Open303 licence notice

The Open303 source code (`Source/ThirdParty/open303/`) is released under the terms of the MIT licence. The notice, word for word from
`License.txt` in the upstream repository at commit 313bf0d9ade7c1dcb6b3a74f5ea1780a29d70074:

```
The Open303 source code is released under the terms of the MIT license:

The MIT License

Copyright (c) 2009 Robin Schmidt (www.rs-met.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```
