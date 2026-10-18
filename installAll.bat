@echo off
rem Kopierar de byggda filerna till ratt stalle: bada VST3-pluginsen (Kickbase och Kickbase FX) och MCP-programmet.
rem   installAll.bat [VST-bygge] [MCP-bygge]       (standard: RelWithDebInfo och Release)
rem   Exempel:  installAll.bat                      VST3 fran RelWithDebInfo, MCP fran Release om det finns (annars RelWithDebInfo)
rem             installAll.bat Release Release      allt fran Release
rem Gor foljande, i den har ordningen:
rem   1. MCP-programmet (KickbaseMcp.exe) -> %LOCALAPPDATA%\Kickbase, och Claude Desktops konfiguration (install-mcp.ps1).
rem      Claude Desktop maste vara helt avslutat (Avsluta i aktivitetsfaltet), annars ar filen last.
rem   2. VST3-filerna -> C:\Program Files\Common Files\VST3 (install.ps1). Det kraver administrator: ett UAC-fonster visas.
rem      FL Studio maste vara stangt.
rem Bygg forst med build.bat (samma konfiguration), t.ex.  build.bat RelWithDebInfo all

setlocal
set VSTCONFIG=%1
if "%VSTCONFIG%"=="" set VSTCONFIG=RelWithDebInfo
set MCPCONFIG=%2
if "%MCPCONFIG%"=="" set MCPCONFIG=Release
set HERE=%~dp0

if not exist "%HERE%build\%VSTCONFIG%\Kickbase_artefacts\%VSTCONFIG%\VST3\Kickbase.vst3" (
    echo Hittar inget bygge av Kickbase i build\%VSTCONFIG%. Kor forst:  build.bat %VSTCONFIG% all
    exit /b 1
)
if not exist "%HERE%build\%MCPCONFIG%\KickbaseMcp_artefacts\%MCPCONFIG%\KickbaseMcp.exe" (
    echo Inget MCP-bygge i build\%MCPCONFIG%: anvander build\%VSTCONFIG% i stallet.
    set MCPCONFIG=%VSTCONFIG%
)
if not exist "%HERE%build\%MCPCONFIG%\KickbaseMcp_artefacts\%MCPCONFIG%\KickbaseMcp.exe" (
    echo Hittar inte KickbaseMcp.exe i build\%MCPCONFIG%. Kor forst:  build.bat %MCPCONFIG% all
    exit /b 1
)

echo.
echo === 1/2: MCP-programmet (build\%MCPCONFIG%) ===
powershell -NoProfile -ExecutionPolicy Bypass -File "%HERE%install-mcp.ps1" %MCPCONFIG%
if errorlevel 1 echo OBS: MCP-programmet kopierades inte (se meddelandet ovan). Fortsatter med VST3-filerna.

echo.
echo === 2/2: VST3-filerna (build\%VSTCONFIG%, administrator) ===
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process powershell -Verb RunAs -Wait -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-Command','& ''%HERE%install.ps1'' %VSTCONFIG%; Read-Host ''Tryck Enter for att stanga'''"

echo.
echo Klart. Starta om Claude Desktop (MCP) och scanna om plugins i FL Studio om de inte syns.
endlocal
