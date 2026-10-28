#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
enum class Layer {Underground=0,Surface=1,Ground=2,Air=3,Top=4};
struct AbstractClass {bool techno=true;};
struct ObjectClass:AbstractClass {int height=0;bool IsSurfaced(){return height>-20;}};
struct Projectile {bool AA=false,AU=false;};
struct WeaponTypeClass {struct Projectile* Projectile=nullptr;int Policy=-1;};
struct WeaponStruct {WeaponTypeClass* WeaponType=nullptr;};
struct TechnoClass:ObjectClass {
 Layer layer=Layer::Ground;int depth=0;bool SpecialTracked=false;WeaponStruct primary,slot;bool PrimaryPresent=true,SecondaryPresent=true;
 Layer InWhichLayer(){return layer;}WeaponStruct* GetWeapon(int index){assert(index==0 || index==1);return index==0?(PrimaryPresent?&primary:nullptr):(SecondaryPresent?&slot:nullptr);}
};
template<class T,class U>T abstract_cast(U* p){return p && p->techno?static_cast<T>(p):nullptr;}
static bool modulePresent=true,exportPresent=true,policyExportPresent=true;static int modules=0,resolves=0,queries=0,fetches=0;
static int __cdecl Query(TechnoClass* t){++queries;return t?t->depth:0;}
static int __cdecl Policy(WeaponTypeClass* w){return w?w->Policy:-1;}
static void* Module(const char* name){++modules;assert(!strcmp(name,"Kaleidoscope.dll"));return modulePresent?reinterpret_cast<void*>(1):nullptr;}
static void* Proc(void*,const char* name){++resolves;if(!strcmp(name,"Kaleidoscope_GetUnderwaterWeaponPolicy"))return policyExportPresent?reinterpret_cast<void*>(&Policy):nullptr;assert(!strcmp(name,"Kaleidoscope_GetUnderwaterDepth"));return exportPresent?reinterpret_cast<void*>(&Query):nullptr;}
#define GetModuleHandleA Module
#define GetProcAddress Proc
#include "depth-consumer.h"
#include "TargetEnvironment.h"
namespace TechnoExt {static TechnoClass* Fetch(TechnoClass* t){++fetches;return t;}}
namespace BulletTypeExt {static Projectile* Fetch(Projectile* p){return p;}}
namespace SelectAutoTarget_Context {bool AU=false;}
struct REGISTERS {
 TechnoClass *firer,*target;WeaponTypeClass* weapon;bool automatic=false;bool firing=false;Layer layer=Layer::Ground;
 template<class T>T ESI(){return reinterpret_cast<T>(automatic?target:firer);}
 template<class T>T EDI(){return reinterpret_cast<T>(firing?reinterpret_cast<void*>(weapon):automatic?reinterpret_cast<void*>(firer):reinterpret_cast<void*>(target));}
 template<class T>T EAX(){return static_cast<T>(firing?static_cast<int>(layer):automatic?target->height:1);}
 template<class T>T EBX(){return reinterpret_cast<T>(target);}
};
#define DEFINE_HOOK(address,name,size) unsigned name(REGISTERS* R)
#define GET(type,name,reg) auto name=R->reg<type>()
#include "host-hooks.h"
#include "weapon-pick.h"
int main(int argc,char** argv){
 if(argc>1 && !strcmp(argv[1],"no-module"))modulePresent=false;
 if(argc>1 && !strcmp(argv[1],"no-export"))exportPresent=false;
 if(argc>1 && !strcmp(argv[1],"no-policy-export"))policyExportPresent=false;
 const bool available=modulePresent && exportPresent;
 Projectile projectile;WeaponTypeClass weapon{&projectile};TechnoClass firer,target;firer.slot.WeaponType=&weapon;
 REGISTERS regs{&firer,&target,&weapon};unsigned cases=0;
 for(Layer layer:{Layer::Underground,Layer::Ground,Layer::Surface,Layer::Air,Layer::Top})
 for(int depth:{-1,0,1,128,256})for(int height:{-512,-128,-21,-20,-19,0,256})
 for(bool au:{false,true})for(bool aa:{false,true})for(bool special:{false,true})for(int policy:{-1,0,1}){
  weapon.Policy=policy;
  target.layer=layer;target.depth=depth;target.height=height;target.SpecialTracked=special;
  projectile.AU=au;projectile.AA=aa;SelectAutoTarget_Context::AU=au;
  const bool underground=layer==Layer::Underground,air=layer==Layer::Air || layer==Layer::Top;
  const bool water=available && !underground && !air && depth>0;
  const auto kind=TargetEnvironment::Classify(&target);
  assert((kind==TargetEnvironment::Kind::Underground)==underground);
  assert((kind==TargetEnvironment::Kind::Air)==air);
  assert((kind==TargetEnvironment::Kind::Underwater)==water);
  assert(TargetEnvironment::IsUnderground(&target)==underground);
  regs.automatic=regs.firing=false;
  const auto cursor=TechnoClass_MouseOverObject_AttackUnderGround(&regs);
  const bool rejected=water && policyExportPresent && policy==0;
  assert(cursor==(!rejected && (height>-20 || water || au)?0x700246u:0x70056Cu));
  regs.automatic=true;fetches=0;
  const auto automatic=TechnoClass_CanAutoTargetObject_AU(&regs);
  assert(automatic==((height>=-20 || water || au || special)?0x6F7E24u:0x6F894Fu));
  assert(fetches==((height>=-20 || water || au)?0:1));
  regs.firing=true;regs.layer=layer;
  const auto final=TechnoClass_CanFire_AntiUnderground(&regs);
  assert(final==((rejected || (underground?!au:air?!aa:false))?0x6FC86Au:0x6FC762u));
  ++cases;
 }
 ObjectClass object;object.techno=false;object.height=-128;
 assert(TargetEnvironment::RequiresAU(&object));object.height=0;assert(!TargetEnvironment::RequiresAU(&object));
 assert(TargetEnvironment::Classify(nullptr)==TargetEnvironment::Kind::Surface);
 WeaponTypeClass first{&projectile},second{&projectile};firer.primary.WeaponType=&first;firer.slot.WeaponType=&second;
 for(Layer layer:{Layer::Underground,Layer::Ground,Layer::Surface,Layer::Air})for(int depth:{0,128})
 for(int a:{-1,0,1})for(int b:{-1,0,1})for(bool one:{false,true})for(bool two:{false,true}){
  target.layer=layer;target.depth=depth;first.Policy=a;second.Policy=b;firer.PrimaryPresent=one;firer.SecondaryPresent=two;
  const bool water=available && layer!=Layer::Underground && layer!=Layer::Air && depth>0;
  const bool hasOne=one && !(water && policyExportPresent && a==0),hasTwo=two && !(water && policyExportPresent && b==0);
  assert(PickWeaponIndex(&firer,&target,&target,0,1,false,false)==(!hasOne && !hasTwo?-1:!hasTwo?0:!hasOne?1:-7));
 }
 assert(modules<=2 && resolves<=(modulePresent?2:0));
 assert((queries>0)==available);
 printf("PASS: %u production classifier/cursor/AI/fire combinations; AU/AA unchanged for native media, water independent of negative height, legacy fallback and lazy SpecialTracked lookup preserved.\n",cases);
}
