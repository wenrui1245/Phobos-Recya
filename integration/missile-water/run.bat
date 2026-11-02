@echo off
setlocal
cd /d "%~dp0..\.."
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "MISSILE_TEST_VS=%%i"
call "%MISSILE_TEST_VS%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul
if not %errorlevel%==0 exit /b 1
if not exist .private\missile-water mkdir .private\missile-water
if not defined MISSILE_WATER_PYTHON set "MISSILE_WATER_PYTHON=python"
"%MISSILE_WATER_PYTHON%" integration\missile-water\extract.py .private\missile-water
if not %errorlevel%==0 exit /b 1
cl /nologo /std:c++20 /EHsc /I src /I .private\missile-water /Fe:.private\missile-water\runtime-test.exe /Fo:.private\missile-water\runtime-test.obj integration\missile-water\runtime-test.cpp > .private\missile-water\test-build.log 2>&1
if not %errorlevel%==0 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File integration\check-binary-privacy.ps1 -Binary .private\missile-water\runtime-test.exe
if not %errorlevel%==0 exit /b 1
.private\missile-water\runtime-test.exe
if not %errorlevel%==0 exit /b 1
cl /nologo /utf-8 /std:c++20 /EHsc /Gy /Gz /DSYR_VER=2 /DNOMINMAX /I src /I YRpp /Fe:.private\missile-water\stream-test.exe /Fo:.private\missile-water\ integration\missile-water\stream-test.cpp src\Utilities\Stream.cpp /link /OPT:REF /DEBUG:NONE > .private\missile-water\stream-build.log 2>&1
if not %errorlevel%==0 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File integration\check-binary-privacy.ps1 -Binary .private\missile-water\stream-test.exe
if not %errorlevel%==0 exit /b 1
.private\missile-water\stream-test.exe
exit /b %errorlevel%
