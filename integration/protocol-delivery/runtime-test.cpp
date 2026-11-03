#line 1 "delivery-api-test.cpp"
#include <windows.h>
#include <cassert>
#include <cstdio>
#define DEFINE_EXPORT(ret,name,...) ret __stdcall name(__VA_ARGS__)
enum class Mission {Construction,Guard}; enum class BStateType {Idle};
struct BuildingTypeClass;struct HouseClass;struct BuildingClass;
template<class T>struct Registry {T* Item=nullptr;T* GetItemOrDefault(int index){return index==0?Item:nullptr;}};
struct BuildingClass {
 bool Limbo=false,Deleted=false,Constructing=false,Placed=false,Forced=false;
 void QueueMission(Mission m,bool){Constructing=m==Mission::Construction;}
 void NextMission(){} bool ForceCreate(int&){assert(Constructing);Forced=true;return PlaceOK;}
 void UnInit(){Deleted=true;}void BeginMode(BStateType){}void Place(bool){Placed=true;}
 static inline bool PlaceOK=true;
};
BuildingClass Product;bool Allocate=true;bool ExtOK=true;int Created=0,LastID=-1;
struct BuildingTypeClass {int ArrayIndex=0,BuildLimit=0;BuildingTypeClass* UndeploysInto=nullptr;bool Buildup=false;
 static inline Registry<BuildingTypeClass> Array;
 void* CreateObject(HouseClass*){if(!Allocate)return nullptr;++Created;Product={};return &Product;}
 void* LoadBuildup(){return Buildup?this:nullptr;}
};
struct HouseClass{int ArrayIndex=0,Count=0;bool Defeated=false;static inline Registry<HouseClass> Array;
 int CountOwnedNow(BuildingTypeClass*){return Count;}int GetBaseCenter(){return 0;}
};
struct HouseExt {static void* TryFetch(HouseClass* h){return ExtOK?h:nullptr;}};
struct BuildingTypeExt {static void* TryFetch(BuildingTypeClass* t){return ExtOK?t:nullptr;}
 static void CreateLimboBuilding(BuildingClass* b,BuildingTypeClass*,HouseClass*,int id){b->Limbo=true;LastID=id;}
};
struct Cell {int GetCoords(){return 0;}};
struct MapClass {static MapClass Instance;bool CellOK=true;Cell C;Cell* TryGetCellAt(int){return CellOK?&C:nullptr;}};MapClass MapClass::Instance;
#include "api-body.h"

int main(){
 HouseClass house;BuildingTypeClass type;HouseClass::Array.Item=&house;BuildingTypeClass::Array.Item=&type;
 assert(Phobos_DeliverProtocolBuilding(nullptr,&type,TRUE,1)==E_INVALIDARG);
 assert(Phobos_DeliverProtocolBuilding(&house,&type,TRUE,-1)==E_INVALIDARG);
 type.ArrayIndex=1;assert(Phobos_DeliverProtocolBuilding(&house,&type,TRUE,1)==E_INVALIDARG);type.ArrayIndex=0;
 ExtOK=false;assert(Phobos_DeliverProtocolBuilding(&house,&type,TRUE,1)==E_INVALIDARG);ExtOK=true;
 Allocate=false;assert(Phobos_DeliverProtocolBuilding(&house,&type,TRUE,1)==E_OUTOFMEMORY);Allocate=true;
 assert(Phobos_DeliverProtocolBuilding(&house,&type,TRUE,7)==S_OK&&Product.Limbo&&LastID==7);
 type.BuildLimit=1;house.Count=1;int before=Created;assert(Phobos_DeliverProtocolBuilding(&house,&type,TRUE,7)==S_OK&&Created==before);type.BuildLimit=0;
 MapClass::Instance.CellOK=false;assert(Phobos_DeliverProtocolBuilding(&house,&type,FALSE,0)==S_FALSE);MapClass::Instance.CellOK=true;
 BuildingClass::PlaceOK=false;assert(Phobos_DeliverProtocolBuilding(&house,&type,FALSE,0)==S_FALSE&&Product.Deleted);
 BuildingClass::PlaceOK=true;assert(Phobos_DeliverProtocolBuilding(&house,&type,FALSE,0)==S_OK&&Product.Placed&&!Product.Constructing&&!Product.Limbo);
 type.Buildup=true;assert(Phobos_DeliverProtocolBuilding(&house,&type,FALSE,0)==S_OK&&!Product.Placed&&Product.Constructing);
 puts("PASS: actual exported API body, identities, allocation, limbo IDs/limit, blocked delivery, normal/buildup placement; no superweapon service exists in fixture.");
}
