param([string]$Out='.private/ai-building-production',[string]$SourceRoot=(Join-Path $PSScriptRoot '../..'))
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($SourceRoot)
if(![IO.Path]::IsPathRooted($Out)){$Out=Join-Path $root $Out}
$null=New-Item -ItemType Directory -Force -Path $Out
$source=[IO.File]::ReadAllText((Join-Path $root 'src/Ext/House/Hooks.AIDualBuildingProduction.cpp'))
if([regex]::Matches($source,'DEFINE_HOOK\(').Count -ne 3){throw 'Review changed AI building hook declarations before preparing tests'}
$controller=$source.Substring(0,$source.IndexOf('DEFINE_HOOK('))
$controller=[regex]::Replace($controller,'(?m)^#include[^\n]*\n','')
if($controller.Contains('EventClass::OutList')){throw 'Local AI input queue must not be used for simulation placement'}
$dispatch=$source.Substring($source.IndexOf('DEFINE_HOOK(0x4FEA3C,'))
$dispatch=[regex]::Replace($dispatch,'DEFINE_HOOK\([^\n]+\)','unsigned DispatchDefenseForTest(HouseClass* pThis, BaseNodeClass* pNode)')
$dispatch=[regex]::Replace($dispatch,'(?m)^\s*GET(?:_STACK)?\([^\n]+\);','')
foreach($pair in @(@('(0x4FB0E0)','(&NativePlace)'),@('(0x42E780)','(&NativeBuilt)'),@('(0xA8B238)','(&NativeFlag)'),@('(0x89C310)','(&NativeRepickCell)'))){
    if([regex]::Matches($controller,[regex]::Escape($pair[0])).Count -ne 1){throw 'Review changed native AI boundary before preparing tests'}
    $controller=$controller.Replace($pair[0],$pair[1])
}
$naval=[IO.File]::ReadAllText((Join-Path $root 'src/Ext/House/Hooks.AINavalProduction.cpp'))
$start=$naval.IndexOf('DEFINE_HOOK(0x4CA0A1,');$end=$naval.IndexOf("`nDEFINE_HOOK(",$start+1)
if($start -lt 0 -or $end -lt $start){throw 'Native abandon dispatch missing'}
$guard=$naval.Substring($start,$end-$start)
$guard=[regex]::Replace($guard,'DEFINE_HOOK\([^\n]+\)','unsigned AbandonSlotGuard(FactoryClass* pThis)')
$guard=[regex]::Replace($guard,'(?m)^\s*GET\([^\n]+\);','')
$body=[IO.File]::ReadAllText((Join-Path $root 'src/Ext/House/Body.cpp'))
$fields=@('ProducingDefenseBuildingTypeIndex','AIDefenseFactory','AIDefenseNodeCoords','AIDefenseNextPlaceFrame','AIDefensePlaceAttempts','AIDefenseAbandonedNodes')
$pattern=($fields|ForEach-Object {[regex]::Escape('.Process(this->'+$_+')')}) -join '\s*'
$match=[regex]::Match($body,$pattern)
if(!$match.Success){throw 'Defense state no longer has one contiguous save block'}
$save='template<class T> void HouseExt::TestSerialize(T& Stm) { Stm'+$match.Value+"; }`n"
$test="#include `"Fakes.h`"`n"+$guard+"`n"+$controller+"`n"+$dispatch+"`n"+$save+"`n#include `"Tests.h`"`n"
[IO.File]::WriteAllText((Join-Path $Out 'controller-tests.cpp'),$test,[Text.UTF8Encoding]::new($false))
Write-Output 'Prepared actual controller, native boundaries, abandon dispatch and save fields.'
