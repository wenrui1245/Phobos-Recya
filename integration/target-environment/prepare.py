from pathlib import Path
import argparse,shutil
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
for name in ['TargetEnvironmentModel.h','TargetEnvironment.h']:
 s=(a.root/'src/Utilities'/name).read_text(encoding='utf-8-sig')
 if name=='TargetEnvironment.h':s='\n'.join(x for x in s.splitlines() if not x.startswith('#include <'))
 (a.out/name).write_text(s,encoding='utf-8')
s=(a.root/'src/Interop/Underwater.h').read_text(encoding='utf-8-sig');(a.out/'depth-consumer.h').write_text('\n'.join(x for x in s.splitlines() if not x.startswith('#include <')),encoding='utf-8')
def hook(source,name):
 pos=source.index(name);start=source.rfind('DEFINE_HOOK(',0,pos);brace=source.index('{',pos);level=1;end=brace+1
 while level:
  if source[end]=='{':level+=1
  elif source[end]=='}':level-=1
  end+=1
 return source[start:end]+'\n'
s=(a.root/'src/Ext/Techno/Hooks.cpp').read_text(encoding='utf-8-sig')
body=hook(s,'TechnoClass_MouseOverObject_AttackUnderGround')+hook(s,'TechnoClass_CanAutoTargetObject_AU')
s=(a.root/'src/Ext/Techno/Hooks.Firing.cpp').read_text(encoding='utf-8-sig');body+=hook(s,'TechnoClass_CanFire_AntiUnderground')
(a.out/'host-hooks.h').write_text(body,encoding='utf-8')
picker=(a.root/'src/Ext/Techno/WeaponHelpers.cpp').read_text(encoding='utf-8-sig')
start=picker.index('int TechnoExt::PickWeaponIndex(');end=picker.index('\n\tauto const pWeaponTwo',start)
(a.out/'weapon-pick.h').write_text(picker[start:end].replace('int TechnoExt::PickWeaponIndex(','int PickWeaponIndex(')+'\nreturn -7;\n}\n',encoding='utf-8')
shutil.copyfile(Path(__file__).with_name('runtime-test.cpp'),a.out/'runtime-test.cpp')
print('Prepared production environment classifier, depth consumer and three complete host callbacks.')
