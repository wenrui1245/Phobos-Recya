$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $PSScriptRoot '../../src/Interop/Version.cpp'))
$start=$source.IndexOf('DEFINE_EXPORT(HRESULT, Phobos_DeliverProtocolBuilding,')
if($start -lt 0){throw 'Missing delivery export'}
$end=$source.IndexOf("`n}",$start)
if($end -lt 0){throw 'Unterminated delivery export'}
$out=Join-Path $PSScriptRoot '../../.private/protocol-delivery'
New-Item -ItemType Directory -Force $out | Out-Null
[IO.File]::WriteAllText((Join-Path $out 'api-body.h'),$source.Substring($start,$end+2-$start),[Text.UTF8Encoding]::new($false))
