#include "Version.h"
#include <Ext/SWType/Body.h>
#include <Ext/House/Body.h>
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
