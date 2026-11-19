@echo off
setlocal
cd /d "%~dp0..\.."
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "BRIDGE_VS=%%i"
call "%BRIDGE_VS%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul
if not "%errorlevel%"=="0" exit /b 1
if not exist .private\aircraft-mission-bridge mkdir .private\aircraft-mission-bridge
if not defined AIRCRAFT_BRIDGE_PYTHON set "AIRCRAFT_BRIDGE_PYTHON=python"
"%AIRCRAFT_BRIDGE_PYTHON%" -c "from pathlib import Path;p=Path('src/Interop/AircraftMission.cpp').read_text(encoding='utf-8-sig');Path('.private/aircraft-mission-bridge/actual.h').write_text(p[p.index('int AircraftMissionTurningRadius'):],encoding='utf8')"
if not "%errorlevel%"=="0" exit /b 1
cl /nologo /std:c++20 /utf-8 /EHsc /Gz /DNOMINMAX /DSYR_VER=2 /I src /I YRpp /I .private\aircraft-mission-bridge /Fe:.private\aircraft-mission-bridge\test.exe /Fo:.private\aircraft-mission-bridge\test.obj integration\aircraft-mission-bridge\test.cpp /link /DEBUG:NONE >.private\aircraft-mission-bridge\build.log 2>&1
if not "%errorlevel%"=="0" exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File integration\check-binary-privacy.ps1 -Binary .private\aircraft-mission-bridge\test.exe
if not "%errorlevel%"=="0" exit /b 1
.private\aircraft-mission-bridge\test.exe
if not "%errorlevel%"=="0" exit /b 1
.private\aircraft-mission-bridge\test.exe missing
if not "%errorlevel%"=="0" exit /b 1
