param([string]$Python = '') # Retained for older commands; preparation now uses PowerShell.
$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$taskOut = Join-Path $taskRoot '.private/ai-building-production'
New-Item -ItemType Directory -Path $taskOut -Force | Out-Null
& (Join-Path $PSScriptRoot 'prepare.ps1') -Out $taskOut
$taskVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$taskLookup = New-Object Diagnostics.ProcessStartInfo
$taskLookup.FileName = $taskVsWhere
$taskLookup.Arguments = '-latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -utf8'
$taskLookup.UseShellExecute = $false
$taskLookup.RedirectStandardOutput = $true
$taskLookup.StandardOutputEncoding = [Text.Encoding]::UTF8
$taskLookup.CreateNoWindow = $true
$taskProcess = [Diagnostics.Process]::Start($taskLookup)
$taskVS = $taskProcess.StandardOutput.ReadToEnd().Trim()
$taskProcess.WaitForExit()
if ($taskProcess.ExitCode -ne 0 -or !$taskVS) { throw 'VS C++ x86 toolchain not found.' }
$taskProcess.Dispose()
$taskVcvars = Join-Path $taskVS 'VC/Auxiliary/Build/vcvarsall.bat'
$taskBuild = '@echo off' + "`r`nchcp 65001 >nul`r`ncall `"$taskVcvars`" x86 >nul`r`nif errorlevel 1 exit /b 1`r`n" +
    'cl /nologo /std:c++20 /utf-8 /EHsc /O1 /MT /W4 /DNOMINMAX /d1trimfile:"' + $taskRoot + '" /I "' + $PSScriptRoot + '" "' + (Join-Path $taskOut 'controller-tests.cpp') +
    '" /Fe:"' + (Join-Path $taskOut 'controller-tests.exe') + '" /Fo:"' + (Join-Path $taskOut 'controller-tests.obj') +
    '" /link /DEBUG:NONE /INCREMENTAL:NO' + "`r`nexit /b %errorlevel%`r`n"
$taskScript = Join-Path $taskOut 'build.cmd'
[IO.File]::WriteAllText($taskScript, $taskBuild, [Text.UTF8Encoding]::new($false))
& $env:ComSpec /d /c $taskScript *> (Join-Path $taskOut 'build.log')
if ($LASTEXITCODE -ne 0) { throw 'Controller test build failed; see the private build log.' }
& (Join-Path $taskRoot 'integration/check-binary-privacy.ps1') -Binary (Join-Path $taskOut 'controller-tests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Controller test binary privacy check failed.' }
& (Join-Path $taskOut 'controller-tests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Controller regression failed.' }
