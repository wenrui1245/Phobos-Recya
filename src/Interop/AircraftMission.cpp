#include "AircraftMission.h"
#include <Ext/Aircraft/Body.h>

int AircraftMissionTurningRadius(AircraftClass*);
namespace {
using namespace AircraftMissionABI;
bool __cdecl Eligible(AircraftClass* p) {
    return p && p->IsAlive && p->Health>0 && !p->InLimbo && !p->Team && !p->Airstrike && !p->IsALoaner;
}
bool __cdecl Suppressed(AircraftClass* p){return p->IsUnderEMP() || p->Deactivated || AircraftExt::Fetch(p)->IsTempImmobilized;}
double __cdecl Radius(AircraftClass* p){return AircraftMissionTurningRadius(p);}
void __cdecl Prepare(AircraftClass* p) {
    if(p->CurrentMission==Mission::Enter && p->DockNowHeadingTo){p->SendToEachLink(RadioCommand::NotifyUnlink);p->DockNowHeadingTo=nullptr;}
    p->IsLocked=false;p->IsReturningFromAttackRun=false;p->SetArchiveTarget(nullptr);
}
void __cdecl Steer(AircraftClass* p,const CoordStruct* at){p->IsLocked=false;p->Locomotor->Move_To(*at);}
bool __cdecl Return(AircraftClass* p) {
    if(!p->TryNearestDockBuilding(&p->Type->Dock,0,0))return false;
    p->SetTarget(nullptr);p->SetArchiveTarget(nullptr);p->IsLocked=false;p->EnterIdleMode(false,true);
    return p->DockNowHeadingTo!=nullptr;
}
bool __cdecl Acquire(AircraftClass* p,const CoordStruct* at) {
    if(p->IsArmed() && !p->TargetingTimer.HasTimeLeft()){auto point=*at;p->TargetAndEstimateDamage(point,ThreatType::Area);}
    return p->Target!=nullptr;
}
int __cdecl Fire(AircraftClass* p,AbstractClass* target) {
    if(!p->IsArmed())return 0;
    const int index=p->SelectWeapon(target);
    if(index<0 || !p->GetWeapon(index)->WeaponType || p->GetFireError(target,index,false)!=FireError::OK)return 0;
    AircraftExt::Fetch(p)->CurrentAircraftWeaponIndex=index;
    p->ShouldLoseAmmo=true;
    const int delay=std::max(1,p->GetROF(index));
    AircraftExt::FireWeapon(p,target);
    return delay;
}
const Services native{sizeof(Services),Revision,Eligible,Suppressed,Radius,Prepare,Steer,Return,Acquire,Fire};
}
int AircraftMissionBridge::Call(AircraftClass* p,unsigned event,const CoordStruct* at,AbstractClass* target,int value) {
    static const auto dispatch=[]()->AircraftMissionABI::Dispatch {
        const auto module=GetModuleHandleA("Kaleidoscope.dll");
        return module?reinterpret_cast<AircraftMissionABI::Dispatch>(GetProcAddress(module,"Kaleidoscope_AircraftMissionV1")):nullptr;
    }();
    return dispatch?dispatch(&native,p,event,at,target,value):0;
}
