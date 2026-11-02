from pathlib import Path
import re,sys
root=Path(__file__).resolve().parents[2];out=Path(sys.argv[1]) if len(sys.argv)>1 else root/'.private/missile-water';out.mkdir(parents=True,exist_ok=True)
def body(text,signature):
 start=text.index(signature); brace=text.index('{',start);depth=0
 for i in range(brace,len(text)):
  if text[i]=='{':depth+=1
  elif text[i]=='}':
   depth-=1
   if not depth:return text[start:i+1]+'\n'
 raise AssertionError('Missing production function body')
a=(root/'src/Ext/Bullet/Trajectories/ActualTrajectories/MissileTrajectory.cpp').read_text(encoding='utf-8-sig');b=(root/'src/Ext/Bullet/Trajectories/PhobosTrajectory.cpp').read_text(encoding='utf-8-sig')
parts=[body(a,'bool MissileTrajectory::CanHitGroundAt('),body(b,'TrajectoryCheckReturnType PhobosTrajectory::OnDetonateUpdate(')]
fast=next(x.strip() for x in b.splitlines() if 'if ((subjectToGround &&' in x).split(' //')[0][5:-1]
low=next(x.strip() for x in b.splitlines() if 'if (this->CanHitGroundAt(theTargetCoords)' in x)[4:-1]
assert 'CanHitGroundAt(curCoord)' in fast and fast.endswith('GetCellFloorHeight(curCoord)')
parts+=['bool PhobosTrajectory::FastGround(const CoordStruct& curCoord,bool subjectToGround) const { return '+fast+'; }\n','bool PhobosTrajectory::LowGround(const CoordStruct& theTargetCoords) const { const int cellHeight=MapClass::Instance.GetCellFloorHeight(theTargetCoords); return '+low+'; }\n']
header=(root/'src/Ext/BulletType/Body.cpp').read_text(encoding='utf-8-sig');assert re.search(r'\.Process\(this->TrajectoryType\)[^;]*;',header)
# Existing type data must remain before the optional tail, in the final type field.
assert a.index('this->Serialize(Stm);')<a.index('MissileWaterSurface::Load(Stm, enabled)')
(out/'production-fragments.inc').write_text('\n'.join(parts),encoding='utf-8')
print('PASS: runtime fixture reads actual position policy, low/high-speed height expressions and detonation method from current host source.')
