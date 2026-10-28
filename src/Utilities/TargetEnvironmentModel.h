#pragma once
namespace TargetEnvironment {
enum class Kind { Surface, Air, Underground, Underwater };
// Native locomotor Underground is authoritative. Explicit water state comes
// from its owner, never from a sign test on the body's drawing height.
constexpr Kind Resolve(bool underground,bool airborne,int underwaterDepth) {
    return underground?Kind::Underground:airborne?Kind::Air:underwaterDepth>0?Kind::Underwater:Kind::Surface;
}
constexpr bool RequiresAU(Kind kind,bool nativeSurfaced) {
    return kind!=Kind::Underwater && !nativeSurfaced;
}
constexpr bool HeightGate(Kind kind,int nativeHeight,bool hasAU,bool specialTracked) {
    return kind==Kind::Underwater || nativeHeight>=-20 || hasAU || specialTracked;
}
}
