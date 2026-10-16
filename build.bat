@echo off
rem Bygger Kickbase med MSVC + Ninja (CMake och Ninja foljer med Visual Studio Build Tools).
rem Anvandning: build.bat [Debug|RelWithDebInfo|Release] [all]   (standard: Release)
rem   Utan "all" byggs bara det som behovs for att prova: instrumentet (VST3 + app), FX-pluginet och testerna.
rem   "all" bygger dessutom utvecklingsverktygen (KickbaseMcp / MyOwnFruitTool) och KickbaseHostTest, vilket tar langre tid.

setlocal
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release

for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSPATH=%%i
if not defined VSPATH (
    echo Hittade ingen Visual Studio med C++-verktyg.
    exit /b 1
)

call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

cmake -S "%~dp0." -B "%~dp0build\%CONFIG%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% || exit /b 1
if /i "%2"=="all" (
    cmake --build "%~dp0build\%CONFIG%" || exit /b 1
) else (
    cmake --build "%~dp0build\%CONFIG%" --target MyOwnFruit_VST3 MyOwnFruit_Standalone MyOwnFruitFX_VST3 MyOwnFruitTests || exit /b 1
)

echo.
"%~dp0build\%CONFIG%\MyOwnFruitTests_artefacts\%CONFIG%\MyOwnFruitTests.exe" || exit /b 1

echo.
echo Klart! Instrument: build\%CONFIG%\MyOwnFruit_artefacts\%CONFIG%\VST3\Kickbase.vst3
echo        Effekt:     build\%CONFIG%\MyOwnFruitFX_artefacts\%CONFIG%\VST3\Kickbase FX.vst3
echo        App:        build\%CONFIG%\MyOwnFruit_artefacts\%CONFIG%\Standalone\Kickbase.exe
