@echo off
setlocal
cd /d "%~dp0..\.."
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "MISSILE_TEST_VS=%%i"
call "%MISSILE_TEST_VS%\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64 >nul
if not %errorlevel%==0 exit /b 1
if not exist .private\attachment-death mkdir .private\attachment-death
if not defined ATTACHMENT_TEST_PYTHON set "ATTACHMENT_TEST_PYTHON=python"
"%ATTACHMENT_TEST_PYTHON%" integration\attachment-death\extract.py
if not %errorlevel%==0 exit /b 1
cl /nologo /std:c++20 /EHsc /I src /I .private\attachment-death /Fe:.private\attachment-death\runtime-test.exe /Fo:.private\attachment-death\runtime-test.obj integration\attachment-death\runtime-test.cpp > .private\attachment-death\test-build.log 2>&1
if not %errorlevel%==0 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File integration\check-binary-privacy.ps1 -Binary .private\attachment-death\runtime-test.exe
if not %errorlevel%==0 exit /b 1
.private\attachment-death\runtime-test.exe
exit /b %errorlevel%
