from pathlib import Path
root = Path(__file__).resolve().parents[2]
stub = root/'.private/fog-voxel-test'
stub.mkdir(parents=True,exist_ok=True)
(stub/'Phobos.Git.h').write_text('// No version stamp in isolated tests.\n')
for name in ['BuildingClass.h','FootClass.h','ColorScheme.h','ConvertClass.h','ScenarioClass.h','TacticalClass.h']:
    (stub/name).write_text('#include "GameStub.h"\n')

# Compile the real constructor hook body too: earlier tests called Freeze directly
# and never exercised the native register/stack extraction that failed in game.
import re
source=(root/'src/Misc/Hooks.AI.cpp').read_text(encoding='utf-8-sig')
match=re.search(r'DEFINE_HOOK\([^\n]*FoggedObjectClass_CTOR_FreezeVoxel[^\n]*\)\s*\{(.*?)\n\}',source,re.S)
assert match, 'constructor hook missing'
(stub/'ctor-hook.inc').write_text('unsigned RunFrozenConstructorHook(MockRegisters* R)\n{' + match.group(1) + '\n}\n')

(stub/'Utilities').mkdir(exist_ok=True)
(stub/'Utilities/Swizzle.h').write_text('#include "MockSwizzle.h"\n')
entry=re.search(r'DEFINE_HOOK\([^\n]*BuildingClass_FreezeInFog_PrepareVoxel[^\n]*\)\s*\{(.*?)\n\}',source,re.S)
assert entry, 'common freeze entry hook missing'
(stub/'entry-hook.inc').write_text('unsigned RunFreezeEntryHook(MockEntryRegisters* R)\n{' + entry.group(1) + '\n}\n')
