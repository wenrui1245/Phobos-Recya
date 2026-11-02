param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$taskOut = Join-Path $taskRoot '.private/ai-building-production'
New-Item -ItemType Directory -Path $taskOut -Force | Out-Null
& $Python (Join-Path $PSScriptRoot 'prepare.py') --out $taskOut
if ($LASTEXITCODE -ne 0) { throw 'Controller preparation failed.' }
$taskVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$taskVS = & $taskVsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$taskVcvars = Join-Path $taskVS 'VC/Auxiliary/Build/vcvarsall.bat'
$taskBuild = '@echo off' + "`r`ncall `"$taskVcvars`" x86 >nul`r`nif errorlevel 1 exit /b 1`r`n" +
    'cl /nologo /std:c++20 /utf-8 /EHsc /O1 /MT /W4 /I "' + $PSScriptRoot + '" "' + (Join-Path $taskOut 'controller-tests.cpp') +
    '" /Fe:"' + (Join-Path $taskOut 'controller-tests.exe') + '" /Fo:"' + (Join-Path $taskOut 'controller-tests.obj') +
    '" /link /DEBUG:NONE /INCREMENTAL:NO' + "`r`nexit /b %errorlevel%`r`n"
$taskScript = Join-Path $taskOut 'build.cmd'
[IO.File]::WriteAllText($taskScript, $taskBuild)
& $env:ComSpec /d /c $taskScript *> (Join-Path $taskOut 'build.log')
if ($LASTEXITCODE -ne 0) { throw 'Controller test build failed; see the private build log.' }
& (Join-Path $taskOut 'controller-tests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Controller regression failed.' }
