"""Compile the production controller itself against a deterministic fake engine."""
import argparse
from pathlib import Path
import re

p = argparse.ArgumentParser()
p.add_argument('--out', required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
source = (root / 'src/Ext/House/Hooks.AIDualBuildingProduction.cpp').read_text(encoding='utf-8-sig')
assert source.count('DEFINE_HOOK(') == 3
controller = source[:source.index('DEFINE_HOOK(')]
controller = re.sub(r'^#include[^\n]*\n', '', controller, flags=re.M)
assert 'EventClass::OutList' not in controller
dispatch = source[source.index('DEFINE_HOOK(0x4FEA3C,'):]
dispatch = re.sub(r'DEFINE_HOOK\([^\n]+\)', 'unsigned DispatchDefenseForTest(HouseClass* pThis, BaseNodeClass* pNode)', dispatch, count=1)
dispatch = re.sub(r'^\s*GET(?:_STACK)?\([^\n]+\);', '', dispatch, flags=re.M)
# Substitute only the verified engine ABI boundaries. Keep the actual decision
# and lifetime code intact; never reserve game addresses in the test process.
for old, new in [('(0x4FB0E0)', '(&NativePlace)'), ('(0x42E780)', '(&NativeBuilt)'),
                 ('(0xA8B238)', '(&NativeFlag)'), ('(0x89C310)', '(&NativeRepickCell)')]:
    assert controller.count(old) == 1, 'Native boundary changed: update the fixture explicitly'
    controller = controller.replace(old, new)
naval = (root / 'src/Ext/House/Hooks.AINavalProduction.cpp').read_text(encoding='utf-8-sig')
start = naval.index('DEFINE_HOOK(0x4CA0A1,')
guard = naval[start:naval.index('\nDEFINE_HOOK(', start + 1)]
# Exercise the actual abandon dispatch, not a second implementation of its guard.
guard = re.sub(r'DEFINE_HOOK\([^\n]+\)', 'unsigned AbandonSlotGuard(FactoryClass* pThis)', guard, count=1)
guard = re.sub(r'^\s*GET\([^\n]+\);', '', guard, flags=re.M)
serialize = (root / 'src/Ext/House/Body.cpp').read_text(encoding='utf-8-sig')
fields = ['ProducingDefenseBuildingTypeIndex', 'AIDefenseFactory', 'AIDefenseNodeCoords',
          'AIDefenseNextPlaceFrame', 'AIDefensePlaceAttempts', 'AIDefenseAbandonedNodes']
pattern = r'\s*'.join(re.escape('.Process(this->' + name + ')') for name in fields)
m = re.search(pattern, serialize)
assert m, 'Defense job must be saved as one complete, contiguous stream block'
save = 'template<class T> void HouseExt::TestSerialize(T& Stm) { Stm' + m[0] + '; }\n'
out = Path(a.out)
out.mkdir(parents=True, exist_ok=True)
(out / 'controller-tests.cpp').write_text('#include "Fakes.h"\n' + guard + '\n' + controller + '\n' + dispatch + '\n' + save + '\n#include "Tests.h"\n', encoding='utf-8')
print('Prepared actual controller, native abandon dispatch and save fields.')
