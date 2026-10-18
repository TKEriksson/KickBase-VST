Open303 (the TB-303 filter only)
================================

Source:   https://github.com/RobinSchmidt/Open303
Commit:   313bf0d9ade7c1dcb6b3a74f5ea1780a29d70074 ("added ToDo.txt", 29 March 2024)
Fetched:  18 October 2026
Licence:  MIT, "Copyright (c) 2009 Robin Schmidt (www.rs-met.com)" (License.txt in this folder: line 1 "The Open303 source code is released
          under the terms of the MIT license", line 5 the copyright line, lines 7-23 the licence text). The notice is also in
          THIRD-PARTY.md at the top of the repository and in the plugin's SETTINGS > ABOUT.

Files copied from Source/DSPCode/ (unchanged except for the four points below):
  rosic_TeeBeeFilter.h/.cpp, rosic_OnePoleFilter.h/.cpp, rosic_RealFunctions.h/.cpp, GlobalFunctions.h/.cpp,
  rosic_NumberManipulations.h/.cpp, GlobalDefinitions.h

The only changes (each marked with "Kickbase:" in the file), needed to compile with 64-bit MSVC. A diff of every file here against the
upstream commit above shows exactly these four places and nothing else (License.txt and all other files are byte-identical apart from
line endings):
  1. rosic_NumberManipulations.h, roundToInt (line 22): the x86 inline assembly is used only when _M_IX86 is defined; 64-bit MSVC
     takes the portable #else branch that was already in the file.
  2. rosic_NumberManipulations.h, floorInt (line 58): the same change.
  3. rosic_RealFunctions.h, sinCos (line 124): the x86 assembly is skipped on anything but 32-bit x86 (the portable sin/cos branch
     that was already there for GCC is used).
  4. GlobalFunctions.h, log2 (lines 303 and 308, an #if/#endif around the definition): skipped with a current MSVC, where log2 is a
     compiler intrinsic that cannot be redefined.

Never include these headers outside Source/Core/Tb303Filter.cpp (see "tool includecheck" in docs/ENGINEERING.md): they define macros
(PI, EPS, INF ...) and global functions (exp2, round, sign ...) that change other code.

Used by Source/Core/Tb303Filter.h (TeeBeeFilter in the TB_303 mode, run at twice the sample rate). JC303 (GPLv3) was NOT copied.
