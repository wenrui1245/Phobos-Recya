from pathlib import Path
import re
r=Path(__file__).resolve().parents[2]
o=r/'.private/attachment-save';o.mkdir(parents=True,exist_ok=True)
(o/'Phobos.Git.h').write_text('// Isolated stream tests do not stamp a DLL version.\n',encoding='ascii')
def function(text,signature):
 a=text.index(signature);b=text.index('{',a);depth=0
 for j in range(b,len(text)):
  if text[j]=='{':depth+=1
  elif text[j]=='}':
   depth-=1
   if not depth:return text[a:j+1]+'\n'
 raise ValueError(signature)
parts=[]
for name,path in [('AttachmentClass','src/New/Entity/AttachmentClass.cpp'),('AttachmentTypeClass','src/New/Type/AttachmentTypeClass.cpp')]:
 text=(r/path).read_text(encoding='utf-8-sig')
 sig=('bool ' if name=='AttachmentClass' else 'void ')+name+'::Serialize('
 serial=function(text,sig)
 fields=re.findall(r'\.Process\(this->(\w+)\)',serial)
 assert not {'OriginalOwnerIndex','Targetable','InheritOwner_DetachToOriginal'} & set(fields)
 decl=['class '+name+' {public:']+['std::uint32_t '+f+'=0x1000;' for f in fields]
 if name=='AttachmentClass':
  decl+=['int OriginalOwnerIndex=-1;','template<class T> bool Serialize(T&);','bool Load(PhobosStreamReader&,bool);','bool Save(PhobosStreamWriter&)const;']
  methods=[function(text,'bool AttachmentClass::Load('),function(text,'bool AttachmentClass::Save(')]
 else:
  decl+=['bool Targetable=false,InheritOwner_DetachToOriginal=true;','template<class T> void Serialize(T&);','void LoadFromStream(PhobosStreamReader&);','void SaveToStream(PhobosStreamWriter&);']
  methods=[function(text,'void AttachmentTypeClass::LoadFromStream('),function(text,'void AttachmentTypeClass::SaveToStream(')]
 parts+=['\n'.join(decl)+'\n};','template<class T>\n'+serial]+methods
(o/'production.inc').write_text('\n'.join(parts),encoding='utf-8')
bridge=(r/'src/Locomotion/AttachmentLocomotionClass.cpp').read_text(encoding='utf-8-sig')
(o/'bridge.inc').write_text(function(bridge,'bool AttachmentLocomotionClass::ShouldBeOnBridge()'),encoding='utf-8')
print('Extracted actual attachment save/load methods and verified legacy field separation.')
