from pathlib import Path
r=Path(__file__).resolve().parents[2];o=r/'.private/attachment-death';o.mkdir(parents=True,exist_ok=True)
a=(r/'src/New/Entity/AttachmentClass.cpp').read_text(encoding='utf-8-sig');b=(r/'src/Ext/Techno/Body.cpp').read_text(encoding='utf-8-sig')
def function(text,sig):
 start=text.index(sig);opening=text.index('{',start);depth=0
 for j in range(opening,len(text)):
  if text[j]=='{':depth+=1
  elif text[j]=='}':
   depth-=1
   if depth==0:return text[start:j+1]+'\n'
 raise AssertionError(sig)
s='\n'.join(function(a,x) for x in ['void AttachmentClass::UpdateAll()','void AttachmentClass::Destroy(TechnoClass* pSource)','void AttachmentClass::PointerGotInvalid(void* ptr, bool removed)','void AttachmentClass::InvalidatePointer(void* ptr)'])
s+=function(b,'void TechnoExt::DestroyAttachments(TechnoClass* pThis, TechnoClass* pSource)')
start=a.index('void AttachmentClass::AI()');end=a.index('AttachmentTypeClass* pType =',start)
s+=a[start:end]+'++Updates;if(OnAI)OnAI(this);\n}\n'
(o/'production.inc').write_text(s,encoding='utf-8');print('Extracted current production cleanup, invalidation, global update and orphan AI path.')
