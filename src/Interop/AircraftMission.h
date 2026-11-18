#pragma once
#include <windows.h>
#include <GeneralStructures.h>
class AircraftClass;
class AbstractClass;

namespace AircraftMissionABI {
inline constexpr unsigned Revision=1;
enum Event : unsigned { Enabled,Arrival,Attack,Guard,Patrol,Update,Order,Stop,DeployInput };
struct Services {
    unsigned Size,Version;
    bool (__cdecl* Eligible)(AircraftClass*);
    bool (__cdecl* Suppressed)(AircraftClass*);
    double (__cdecl* MinimumRadius)(AircraftClass*);
    void (__cdecl* PrepareFlight)(AircraftClass*);
    void (__cdecl* Steer)(AircraftClass*,const CoordStruct*);
    bool (__cdecl* TryReturn)(AircraftClass*);
    bool (__cdecl* Acquire)(AircraftClass*,const CoordStruct*);
    int (__cdecl* Fire)(AircraftClass*,AbstractClass*);
};
static_assert(sizeof(Services)==40);
using Dispatch=int(__cdecl*)(const Services*,AircraftClass*,unsigned,const CoordStruct*,AbstractClass*,int);
}
namespace AircraftMissionBridge {
int Call(AircraftClass*,unsigned,const CoordStruct* =nullptr,AbstractClass* =nullptr,int =0);
}
