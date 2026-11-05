@echo off
setlocal
cd /d "%~dp0..\.."
if "%~1"=="" exit /b 2
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "ATTACH_SAVE_VS=%%i"
if not defined ATTACH_SAVE_VS exit /b 1
call "%ATTACH_SAVE_VS%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
"%~1" integration\attachment-save\prepare.py
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /std:c++20 /EHsc /Gy /Gz /DSYR_VER=2 /DNOMINMAX /I src /I YRpp /I .private\attachment-save /Fe:.private\attachment-save\runtime-test.exe /Fo:.private\attachment-save\ integration\attachment-save\runtime-test.cpp src\Utilities\Stream.cpp /link /OPT:REF /DEBUG:NONE > .private\attachment-save\build.log 2>&1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File integration\check-binary-privacy.ps1 -Binary .private\attachment-save\runtime-test.exe
if errorlevel 1 exit /b 1
.private\attachment-save\runtime-test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /EHsc /I .private\attachment-save /Fe:.private\attachment-save\bridge-test.exe /Fo:.private\attachment-save\bridge-test.obj integration\attachment-save\bridge-test.cpp > .private\attachment-save\bridge-build.log 2>&1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File integration\check-binary-privacy.ps1 -Binary .private\attachment-save\bridge-test.exe
if errorlevel 1 exit /b 1
.private\attachment-save\bridge-test.exe
exit /b %errorlevel%
