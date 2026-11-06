@echo off
setlocal
cd /d "%~dp0..\.."
if "%~1"=="" exit /b 2
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "FOG_VOXEL_VS=%%i"
if not defined FOG_VOXEL_VS exit /b 1
call "%FOG_VOXEL_VS%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
"%~1" integration\fog-voxel\prepare.py
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /d1trimfile:"%CD%" /std:c++20 /EHsc /Gy /Gz /DSYR_VER=2 /DNOMINMAX /I .private\fog-voxel-test /I integration\fog-voxel /I src /I YRpp /Fe:.private\fog-voxel-test\test.exe /Fo:.private\fog-voxel-test\ integration\fog-voxel\test.cpp src\Misc\FoggedVoxel.cpp src\Utilities\Stream.cpp /link /OPT:REF /DEBUG:NONE > .private\fog-voxel-test\build.log 2>&1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File integration\check-binary-privacy.ps1 -Binary .private\fog-voxel-test\test.exe
if errorlevel 1 exit /b 1
.private\fog-voxel-test\test.exe
exit /b %errorlevel%
