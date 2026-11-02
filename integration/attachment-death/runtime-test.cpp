#include <New/Entity/AttachmentRuntime.h>
#include <algorithm>
#include <memory>
#include <functional>
#include <cassert>
#include <cstdio>
struct AttachmentClass;struct TechnoExt;
enum class Mission {None,Guard};
struct Weapon {int ID=1;};
struct OptionWeapon {Weapon* Value=nullptr;Weapon* Get()const{return Value;}};
struct OptionMission {Mission Value=Mission::None;Mission Get()const{return Value;}};
struct AttachmentTypeClass {OptionWeapon DestructionWeapon_Child;bool InheritDestruction=true;OptionMission ParentDestructionMission;};
struct TechnoClass {TechnoExt* Ext=nullptr;bool InLimbo=false,Dead=false;int Missions=0;void QueueMission(Mission,bool){++Missions;}};
struct Timer {int Remaining=1;void Stop(){Remaining=0;}};
struct AttachmentClass {
 inline static std::vector<AttachmentClass*> Array;AttachmentTypeClass* Type;TechnoClass *Parent,*Child;Timer RespawnTimer;int Updates=0;
 std::function<void(AttachmentClass*)> OnAI;
 AttachmentClass(AttachmentTypeClass* t,TechnoClass* p,TechnoClass* c):Type(t),Parent(p),Child(c){AttachmentRuntime::Lifetimes<AttachmentClass>()->Add(this);Array.push_back(this);}
 ~AttachmentClass(){AttachmentRuntime::Lifetimes<AttachmentClass>()->Remove(this);Array.erase(std::remove(Array.begin(),Array.end(),this),Array.end());}
 AttachmentTypeClass* GetType(){return Type;}
 void AI();static void UpdateAll();void Destroy(TechnoClass*);void InvalidatePointer(void*);static void PointerGotInvalid(void*,bool);
};
struct TechnoExt {
 AttachmentClass* ParentAttachment=nullptr;std::vector<std::unique_ptr<AttachmentClass>> ChildAttachments;
 inline static int Kills=0,Fires=0;inline static std::function<void(TechnoClass*)> OnFire,OnKill;
 static TechnoExt* TryFetch(TechnoClass* t){return t?t->Ext:nullptr;}static TechnoExt* Fetch(TechnoClass* t){assert(t&&t->Ext);return t->Ext;}
 static void FireWeaponAtSelf(TechnoClass* t,Weapon*){assert(t&&!t->Dead);++Fires;if(OnFire)OnFire(t);}
 static void Kill(TechnoClass* t,TechnoClass*){assert(t&&!t->Dead);++Kills;t->Dead=true;AttachmentClass::PointerGotInvalid(t,true);if(OnKill)OnKill(t);}
 static void DestroyAttachments(TechnoClass*,TechnoClass*);
};
template<class T>void AnnounceInvalidPointer(T*& p,void* gone){if(p==gone)p=nullptr;}
#include "production.inc"
int main(){
 for(bool inherit:{false,true}){
  AttachmentTypeClass type;type.InheritDestruction=inherit;type.ParentDestructionMission.Value=Mission::Guard;
  TechnoExt pe,ce;TechnoClass parent{&pe},child{&ce};auto entry=std::make_unique<AttachmentClass>(&type,&parent,&child);ce.ParentAttachment=entry.get();
  AttachmentClass::PointerGotInvalid(&parent,false);assert(entry->Parent==&parent&&ce.ParentAttachment==entry.get());
  AttachmentClass::PointerGotInvalid(&parent,true);assert(!entry->Parent&&!ce.ParentAttachment&&entry->Child==&child&&entry->RespawnTimer.Remaining==0);
  const int killed=TechnoExt::Kills;AttachmentClass::UpdateAll();assert(!entry->Child&&TechnoExt::Kills-killed==int(inherit)&&child.Dead==inherit&&child.Missions==int(!inherit));
  AttachmentClass::UpdateAll();assert(TechnoExt::Kills-killed==int(inherit)&&entry->Updates==0);
 }
 // A destruction weapon can remove the child; do not kill or command it again.
 {AttachmentTypeClass type;Weapon w;type.DestructionWeapon_Child.Value=&w;TechnoExt pe,ce;TechnoClass p{&pe},c{&ce};AttachmentClass entry(&type,&p,&c);ce.ParentAttachment=&entry;
  const int killed=TechnoExt::Kills;TechnoExt::OnFire=[](TechnoClass* t){TechnoExt::Kill(t,nullptr);};entry.Destroy(&p);assert(!entry.Child&&!ce.ParentAttachment&&TechnoExt::Kills==killed+1);TechnoExt::OnFire={};}
 // Reentrant owner teardown sees an empty owner list; local ownership survives.
 {AttachmentTypeClass type;Weapon w;type.DestructionWeapon_Child.Value=&w;TechnoExt pe,ce;TechnoClass p{&pe},c{&ce};pe.ChildAttachments.push_back(std::make_unique<AttachmentClass>(&type,&p,&c));ce.ParentAttachment=pe.ChildAttachments[0].get();
  TechnoExt::OnFire=[&](TechnoClass*){assert(pe.ChildAttachments.empty());TechnoExt::DestroyAttachments(&p,nullptr);};const int killed=TechnoExt::Kills;TechnoExt::DestroyAttachments(&p,nullptr);assert(pe.ChildAttachments.empty()&&TechnoExt::Kills==killed+1);TechnoExt::OnFire={};}
 // An orphan cleanup callback can delete the current attachment itself.
 {AttachmentTypeClass type;Weapon w;type.DestructionWeapon_Child.Value=&w;TechnoExt ce;TechnoClass c{&ce};auto entry=std::make_unique<AttachmentClass>(&type,nullptr,&c);ce.ParentAttachment=entry.get();TechnoExt::OnFire=[&](TechnoClass*){entry.reset();};AttachmentClass::UpdateAll();assert(!entry&&c.Dead);TechnoExt::OnFire={};}
 // A snapshot token must reject a deleted entry even if its address is reused.
 {AttachmentTypeClass type;TechnoExt e;TechnoClass p{&e};alignas(AttachmentClass) unsigned char storage[sizeof(AttachmentClass)];auto first=std::make_unique<AttachmentClass>(&type,&p,nullptr);auto* second=new(storage)AttachmentClass(&type,&p,nullptr);bool replaced=false;
  first->OnAI=[&](AttachmentClass*){if(!replaced){second->~AttachmentClass();second=new(storage)AttachmentClass(&type,&p,nullptr);replaced=true;}};
  AttachmentClass::UpdateAll();assert(first->Updates==1&&second->Updates==0);AttachmentClass::UpdateAll();assert(second->Updates==1);second->~AttachmentClass();}
 assert(AttachmentClass::Array.empty());
 struct ShutdownProbe{};struct LateOwner {~LateOwner(){assert(!AttachmentRuntime::Lifetimes<ShutdownProbe>());puts("PASS: registry closes before late owner destructors without leaking storage.");}};static LateOwner lateOwner;auto* shutdown=AttachmentRuntime::Lifetimes<ShutdownProbe>();assert(shutdown);
 puts("PASS: production orphan cleanup/invalidation/destruction/update; inherit death and survivor mission, no respawn, repeated updates, child death weapon, reentrant owner teardown, deleting the current entry, and same-address reuse.");
}
