@echo off
setlocal
cd /d "%~dp0..\.."
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PROTOCOL_BUILD_VS=%%i"
if not defined PROTOCOL_BUILD_VS exit /b 1
call "%PROTOCOL_BUILD_VS%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
set "_CL_=/Z7 /MP3"
set "_LINK_=/DEBUG:NONE"
powershell -NoProfile -ExecutionPolicy Bypass -File integration\protocol-delivery\prepare.ps1
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /std:c++20 /EHsc /I .private\protocol-delivery /Fe:.private\protocol-delivery\runtime-test.exe /Fo:.private\protocol-delivery\runtime-test.obj integration\protocol-delivery\runtime-test.cpp
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File integration\check-binary-privacy.ps1 -Binary .private\protocol-delivery\runtime-test.exe
if errorlevel 1 exit /b 1
.private\protocol-delivery\runtime-test.exe
exit /b %errorlevel%
