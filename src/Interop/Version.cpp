#include "Version.h"
#include <Ext/SWType/Body.h>
#include <Ext/House/Body.h>
#include <Ext/BuildingType/Body.h>
#include <BuildingClass.h>
#include <MapClass.h>
#include <CellClass.h>
#include "Utilities/Debug.h"

DEFINE_EXPORT(HRESULT, GetInteropAPIVersion, InteropAPIVersion* pVersion)
{
	if (!pVersion)
		return E_POINTER;

	pVersion->major = INTEROP_API_VERSION_MAJOR;
	pVersion->minor = INTEROP_API_VERSION_MINOR;
	pVersion->patch = INTEROP_API_VERSION_PATCH;

	return S_OK;
}

// ============================================================================
// Deprecated API implementation example (commented out for future use)
// ============================================================================
/*
DEFINE_EXPORT(int, SomeOldAPI_Deprecated, int param1)
{
	// Trigger fatal error with explanation.
	Debug::Fatal("SomeOldAPI_Deprecated has been removed in Interop API v2.0.0 (was available in v1.0.0-v1.x.x). "
		"Please update your code to use the replacement API.");
	return 0;  // unreachable
}
*/

DEFINE_EXPORT(HRESULT, Phobos_QuerySuperWeaponAvailability,
 HouseClass* house, SuperWeaponTypeClass* type, BOOL* available)
{
 if(!available)return E_POINTER;
 *available=FALSE;
 // Validate identities against live host registries before touching extensions.
 if(!house || !type)return E_INVALIDARG;
 if(HouseClass::Array.GetItemOrDefault(house->ArrayIndex)!=house
  || SuperWeaponTypeClass::Array.GetItemOrDefault(type->ArrayIndex)!=type)return E_INVALIDARG;
 const auto ext=SWTypeExt::TryFetch(type);
 if(!ext || !house->Type || !HouseExt::TryFetch(house))return E_UNEXPECTED;
 *available=ext->IsAvailable(house)?TRUE:FALSE;
 return S_OK;
}

// Direct protocol delivery: no SuperClass grant, launch, shot count or cameo.
DEFINE_EXPORT(HRESULT, Phobos_DeliverProtocolBuilding,
 HouseClass* house, BuildingTypeClass* type, BOOL limbo, int id)
{
 if(!house || !type || id<0 || (limbo!=FALSE && limbo!=TRUE))return E_INVALIDARG;
 if(HouseClass::Array.GetItemOrDefault(house->ArrayIndex)!=house
  || BuildingTypeClass::Array.GetItemOrDefault(type->ArrayIndex)!=type
  || !HouseExt::TryFetch(house) || !BuildingTypeExt::TryFetch(type) || house->Defeated)return E_INVALIDARG;
 if(limbo && type->BuildLimit>0){
  int count=house->CountOwnedNow(type);
  if(type->UndeploysInto)count+=house->CountOwnedNow(type->UndeploysInto);
  if(count>=type->BuildLimit)return S_OK; // Same consumed/no-create outcome as ApplyLimboDelivery.
 }
 if(limbo){
  auto* building=static_cast<BuildingClass*>(type->CreateObject(house));
  if(!building)return E_OUTOFMEMORY;
  BuildingTypeExt::CreateLimboBuilding(building,type,house,id);return S_OK;
 }
 const auto center=house->GetBaseCenter();
 auto* cell=MapClass::Instance.TryGetCellAt(center);if(!cell)return S_FALSE;
 auto* building=static_cast<BuildingClass*>(type->CreateObject(house));if(!building)return E_OUTOFMEMORY;
 // Native trigger CreateBuildingAt / Phobos 6E427D construction ordering.
 const bool buildup=type->LoadBuildup()!=nullptr;
 building->QueueMission(Mission::Construction,false);building->NextMission();
 auto coords=cell->GetCoords();
 if(!building->ForceCreate(coords)){building->UnInit();return S_FALSE;}
 if(!buildup){building->BeginMode(BStateType::Idle);building->QueueMission(Mission::Guard,false);building->NextMission();building->Place(false);}
 return S_OK;
}
