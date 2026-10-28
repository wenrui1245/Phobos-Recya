#pragma once
#include <TechnoClass.h>
#include <WeaponTypeClass.h>

// Optional semantic API: do not infer physical submersion from cloak or read a
// different DLL's extension layout. Modules are loaded before tactical drawing;
// no late injection/hot reload is supported by this cached consumer.
namespace UnderwaterInterop {
inline int WeaponPolicy(WeaponTypeClass* weapon) {
    using Query=int(__cdecl*)(WeaponTypeClass*);
    static const Query query=[]() -> Query {
        const auto module=GetModuleHandleA("Kaleidoscope.dll");
        return module?reinterpret_cast<Query>(GetProcAddress(module,"Kaleidoscope_GetUnderwaterWeaponPolicy")):nullptr;
    }();
    return query && weapon?query(weapon):-1;
}
inline bool IsPhysicallySubmerged(TechnoClass* unit) {
    using Query=int(__cdecl*)(TechnoClass*);
    static const Query query=[]() -> Query {
        const auto module=GetModuleHandleA("Kaleidoscope.dll");
        return module?reinterpret_cast<Query>(GetProcAddress(module,"Kaleidoscope_GetUnderwaterDepth")):nullptr;
    }();
    return query && query(unit)>0;
}
}
