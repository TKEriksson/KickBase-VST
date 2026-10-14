@echo off
rem Bygger MyOwnFruit med MSVC + Ninja (CMake och Ninja foljer med Visual Studio Build Tools).
rem Anvandning: build.bat [Debug|Release]   (standard: Release)

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
cmake --build "%~dp0build\%CONFIG%" || exit /b 1

echo.
echo Klart! VST3: build\%CONFIG%\MyOwnFruit_artefacts\%CONFIG%\VST3\MyOwnFruit.vst3
echo        App:  build\%CONFIG%\MyOwnFruit_artefacts\%CONFIG%\Standalone\MyOwnFruit.exe
