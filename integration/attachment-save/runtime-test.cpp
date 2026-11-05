#include <Utilities/SavegameDef.h>
#include <Utilities/Debug.h>
#include <New/Entity/AttachmentSaveFormat.h>
#include <cassert>
#include <cstdio>
void __cdecl Debug::Log(const char*,...){}
#include "production.inc"
int main(){
 // Legacy payloads are consecutive, not individually bounded blocks.
 PhobosByteStream legacy;PhobosStreamWriter w0(legacy);
 AttachmentTypeClass type;type.Serialize(w0);type.Serialize(w0);
 AttachmentClass object;object.Serialize(w0);w0.Save(std::uint32_t{0x12345678});
 PhobosStreamReader r0(legacy);AttachmentTypeClass a,b;AttachmentClass old;
 a.LoadFromStream(r0);b.LoadFromStream(r0);
 assert(r0.Success()&&a.Targetable&&b.Targetable&&a.InheritOwner_DetachToOriginal);
 assert(old.Load(r0,true)&&old.OriginalOwnerIndex==-1);
 std::uint32_t sentinel=0;assert(r0.Load(sentinel)&&sentinel==0x12345678&&r0.ExpectEndOfBlock());
 // Mixed versioned and legacy records do not consume each other's fields.
 PhobosByteStream mixed;PhobosStreamWriter w1(mixed);
 type.Targetable=false;type.InheritOwner_DetachToOriginal=false;type.SaveToStream(w1);type.Serialize(w1);
 object.OriginalOwnerIndex=7;assert(object.Save(w1));object.Serialize(w1);
 PhobosStreamReader r1(mixed);AttachmentTypeClass modern,previous;
 modern.LoadFromStream(r1);previous.LoadFromStream(r1);
 assert(r1.Success()&&!modern.Targetable&&!modern.InheritOwner_DetachToOriginal&&previous.Targetable);
 AttachmentClass loaded,older;assert(loaded.Load(r1,true)&&loaded.OriginalOwnerIndex==7);
 assert(older.Load(r1,true)&&older.OriginalOwnerIndex==-1&&r1.ExpectEndOfBlock());
 PhobosByteStream full;PhobosStreamWriter wf(full);assert(object.Save(wf));
 auto copy=full;std::vector<unsigned char> raw(copy.Size());assert(copy.Read(raw.data(),raw.size()));
 for(size_t n=0;n<raw.size();++n){PhobosByteStream cut;cut.Write(raw.data(),n);PhobosStreamReader reader(cut);AttachmentClass bad;assert(!bad.Load(reader,true));}
 PhobosByteStream future;PhobosStreamWriter ff(future);ff.Save(AttachmentSaveFormat::TypeMagic);ff.Save(std::uint32_t{2});
 PhobosStreamReader rf(future);AttachmentTypeClass badType;badType.LoadFromStream(rf);assert(!rf.Success());
 PhobosByteStream invalid;PhobosStreamWriter wi(invalid);object.OriginalOwnerIndex=-2;assert(object.Save(wi));
 PhobosStreamReader ri(invalid);AttachmentClass badOwner;assert(!badOwner.Load(ri,true)&&!ri.Success());
 PhobosByteStream bytes;bytes.Save(std::uint32_t{0x10203040});PhobosStreamReader peek(bytes);
 std::uint32_t one=0,two=0;assert(peek.Peek(one)&&peek.Peek(two)&&one==two&&bytes.Offset()==0);
 assert(peek.Load(two)&&one==two&&peek.ExpectEndOfBlock());
 puts("PASS: production attachment readers/writers, consecutive legacy records, mixed formats, flags/owner round trip, all truncated object records, unknown versions and non-consuming peek.");
}
