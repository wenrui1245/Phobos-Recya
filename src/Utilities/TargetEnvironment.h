#pragma once
#include <TechnoClass.h>
#include <Helpers/Cast.h>
#include <Interop/Underwater.h>
#include "TargetEnvironmentModel.h"

// Targeting classification only. Keep native rendering/locomotor layers and
// coordinates unchanged; preserve the host's old fallback without a provider.
namespace TargetEnvironment {
inline Kind Classify(TechnoClass* target,Layer nativeLayer) {
    if(nativeLayer==Layer::Underground)return Kind::Underground;
    if(nativeLayer==Layer::Air || nativeLayer==Layer::Top)return Kind::Air;
    const int depth=target && UnderwaterInterop::IsPhysicallySubmerged(target)?1:0;
    return Resolve(false,false,depth);
}
inline Kind Classify(TechnoClass* target) {
    return target?Classify(target,target->InWhichLayer()):Kind::Surface;
}
inline bool IsUnderground(TechnoClass* target) {return Classify(target)==Kind::Underground;}
inline bool RejectWeapon(WeaponTypeClass* weapon,TechnoClass* target) {
    return target && Classify(target)==Kind::Underwater && UnderwaterInterop::WeaponPolicy(weapon)==0;
}
inline bool RequiresAU(ObjectClass* object) {
    if(object->IsSurfaced())return false;
    return RequiresAU(Classify(abstract_cast<TechnoClass*>(object)),false);
}
inline bool HeightGate(TechnoClass* target,int nativeHeight) {
    if(nativeHeight>=-20)return true;
    return HeightGate(Classify(target),nativeHeight,false,false);
}
}
