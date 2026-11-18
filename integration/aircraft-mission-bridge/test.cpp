#include <Interop/AircraftMission.h>
#include <GeneralDefinitions.h>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdio>
class AbstractClass {};
struct Timer {bool Waiting=false;bool HasTimeLeft(){return Waiting;}};
struct Loco {CoordStruct Point;void Move_To(CoordStruct p){Point=p;}};
struct Weapon {bool Present=true;};struct Slot{Weapon* WeaponType;};
struct TypeData {int Dock=0;};
class AircraftClass:public AbstractClass {
public:
 bool IsAlive=true,InLimbo=false,Team=false,Airstrike=false,IsALoaner=false,Deactivated=false,EMP=false,IsLocked=true,IsReturningFromAttackRun=true,ShouldLoseAmmo=false,Armed=true,Airport=true;
 int Health=100,Shots=0,Unlinks=0,Scans=0;Mission CurrentMission=Mission::Enter;
 AbstractClass *Target=nullptr,*Candidate=nullptr,*DockNowHeadingTo=nullptr,*Archive=nullptr;
 TypeData Data;TypeData* Type=&Data;Loco Flight;Loco* Locomotor=&Flight;Timer TargetingTimer;Weapon WeaponData;Slot WeaponSlot{&WeaponData};FireError Error=FireError::OK;
 bool IsUnderEMP(){return EMP;}bool IsArmed(){return Armed;}
 void SendToEachLink(RadioCommand){++Unlinks;}void SetTarget(AbstractClass* t){Target=t;}void SetArchiveTarget(AbstractClass* t){Archive=t;}
 bool TryNearestDockBuilding(int*,int,int){return Airport;}void EnterIdleMode(bool,bool){DockNowHeadingTo=Airport?this:nullptr;}
 void TargetAndEstimateDamage(CoordStruct&,ThreatType){++Scans;Target=Candidate;}
 int SelectWeapon(AbstractClass*){return 1;}Slot* GetWeapon(int){return &WeaponSlot;}FireError GetFireError(AbstractClass*,int,bool ignore){assert(!ignore);return Error;}int GetROF(int){return 9;}
};
struct AircraftExt {
 bool IsTempImmobilized=false;int CurrentAircraftWeaponIndex=0;
 static AircraftExt State;static AircraftExt* Fetch(AircraftClass*){return &State;}
 static void FireWeapon(AircraftClass* a,AbstractClass*){++a->Shots;}
};AircraftExt AircraftExt::State;
int AircraftMissionTurningRadius(AircraftClass*){return 321;}
bool Missing=false;int Calls=0;
int __cdecl Forward(const AircraftMissionABI::Services*,AircraftClass*,unsigned,const CoordStruct*,AbstractClass*,int);
HMODULE FakeModule(const char*){return Missing?nullptr:reinterpret_cast<HMODULE>(1);}
FARPROC FakeProc(HMODULE,const char* name){assert(!strcmp(name,"Kaleidoscope_AircraftMissionV1"));return reinterpret_cast<FARPROC>(&Forward);}
#define GetModuleHandleA FakeModule
#define GetProcAddress FakeProc
#include "actual.h"
int __cdecl Forward(const AircraftMissionABI::Services* s,AircraftClass* a,unsigned event,const CoordStruct*,AbstractClass*,int value){assert(s->Size==40&&s->Version==1&&a&&event==AircraftMissionABI::Order&&value==17);++Calls;return 23;}
int main(int argc,char**){
 AircraftClass a;if(argc>1){Missing=true;assert(!AircraftMissionBridge::Call(&a,AircraftMissionABI::Order,nullptr,nullptr,17)&&!Calls);puts("PASS: absent Kaleidoscope dispatcher preserves native path.");return 0;}
 assert(native.Eligible(&a));a.Team=true;assert(!native.Eligible(&a));a.Team=false;a.EMP=true;assert(native.Suppressed(&a));a.EMP=false;
 assert(native.MinimumRadius(&a)==321);a.DockNowHeadingTo=&a;native.PrepareFlight(&a);assert(a.Unlinks==1&&!a.DockNowHeadingTo&&!a.IsLocked&&!a.IsReturningFromAttackRun);
 CoordStruct point{1,2,3};native.Steer(&a,&point);assert(a.Flight.Point==point);
 a.Airport=false;assert(!native.TryReturn(&a));a.Airport=true;assert(native.TryReturn(&a));
 a.Candidate=&a;assert(native.Acquire(&a,&point)&&a.Scans==1);a.TargetingTimer.Waiting=true;native.Acquire(&a,&point);assert(a.Scans==1);
 a.Error=FireError::RANGE;assert(!native.Fire(&a,&a)&&!a.Shots);a.Error=FireError::OK;assert(native.Fire(&a,&a)==9&&a.Shots==1&&a.ShouldLoseAmmo&&AircraftExt::State.CurrentAircraftWeaponIndex==1);
 assert(AircraftMissionBridge::Call(&a,AircraftMissionABI::Order,nullptr,nullptr,17)==23&&Calls==1);
 puts("PASS: actual thin native services compiled under /Gz with explicit C callbacks; eligibility, suppression, steering, docking, targeting timer, weapon checks and dispatcher forwarding. No orbit settings/state in Phobos.");
}
