@echo off
rem Bygger spiket med MSVC + Ninja, max 4 parallella jobb. Kor sedan:
rem   build\StrudelSpike_artefacts\Release\StrudelSpike.exe js\strudel.all.js
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSPATH=%%i
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set JUCEDIR=%STRUDEL_JUCE_DIR%
if "%JUCEDIR%"=="" set JUCEDIR=%~dp0..\..\..\KickBase-VST\build\Release\_deps\juce-src
cmake -S "%~dp0." -B "%~dp0build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSTRUDEL_JUCE_DIR=%JUCEDIR% || exit /b 1
cmake --build "%~dp0build" -j 4
