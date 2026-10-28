param([Parameter(Mandatory=$true)][string]$Python,[Parameter(Mandatory=$true)][string]$OutDir,[Parameter(Mandatory=$true)][string]$PrivacyChecker,[string]$SourceRoot='')
$ErrorActionPreference='Stop'
try {
 if(!$SourceRoot){$SourceRoot=Join-Path $PSScriptRoot '../..'}
 $root=(Resolve-Path -LiteralPath $SourceRoot).Path
 $null=New-Item -ItemType Directory -Force -Path $OutDir
 $out=(Resolve-Path -LiteralPath $OutDir).Path
 & $Python (Join-Path $PSScriptRoot 'prepare.py') --root $root --out $out
 if($LASTEXITCODE){throw 'Prepare failed'}
 Push-Location -LiteralPath $out
 try {
  & cl /nologo /std:c++20 /EHsc /W4 runtime-test.cpp /Fe:runtime-test.exe /Fo:runtime-test.obj /link /DEBUG:NONE > build.log 2>&1
  if($LASTEXITCODE){throw 'Compile failed'}
  & powershell -NoProfile -ExecutionPolicy Bypass -File $PrivacyChecker -Binary (Join-Path $out 'runtime-test.exe')
  if($LASTEXITCODE){throw 'Privacy failed'}
  foreach($mode in @('','no-module','no-export','no-policy-export')){& (Join-Path $out 'runtime-test.exe') $mode;if($LASTEXITCODE){throw 'Runtime failed'}}
 }finally{Pop-Location}
}catch{Write-Output 'Target environment regression failed; details remain in the private test output.';exit 1}
