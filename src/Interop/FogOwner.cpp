#include "FogOwner.h"

#include <HouseClass.h>
#include <MapClass.h>
#include <Utilities/Macro.h>

HouseClass* PhobosFogOwner = nullptr;

DEFINE_EXPORT(HRESULT, Phobos_Fog_SetOwner, HouseClass* pHouse)
{
	if (PhobosFogOwner == pHouse) return S_OK;
	PhobosFogOwner = pHouse;
	if (pHouse)
	{
		// FULL-BLACK baseline via direct field writes. ReshroudMap (0x50BD10
		// -> 0x577AB0) only touches per-house legacy fields (+0x10C/0x110/0x114)
		// and never writes the global CellClass fog fields in this fork - it
		// is a no-op for the global fog (probe: PRE flags stayed 00000003).
		CellClass** cells = *reinterpret_cast<CellClass***>(
			reinterpret_cast<unsigned char*>(0x87F7E8) + 0x13C);
		if (cells)
		{
			for (int idx = 0; idx < 0x40000; ++idx)
			{
				CellClass* c = cells[idx];
				if (!c) continue;
				unsigned f = static_cast<unsigned int>(c->Flags);
				f |= static_cast<unsigned int>(CellFlags::Fogged);
				f &= ~(static_cast<unsigned int>(CellFlags::CenterRevealed) | static_cast<unsigned int>(CellFlags::EdgeRevealed));
				c->Flags = static_cast<CellFlags>(f);
				unsigned al = static_cast<unsigned int>(c->AltFlags);
				al &= ~(static_cast<unsigned int>(AltCellFlags::Mapped) | static_cast<unsigned int>(AltCellFlags::NoFog));
				c->AltFlags = static_cast<AltCellFlags>(al);
				c->Visibility = -2;
				// 必须与原版 CellClass 构造一致（0x47BC5B 处两字段都初始化为 0xFE）：
				// FogCell（0x4ACC50）的递归级联只在“邻居无揭示标志且 Foggedness != -2”
				// 时向邻居递归。这里若写 0，首次迷雾级联会递归穿透全图（深度可达 26 万层），
				// 栈溢出——表现为游戏卡死后瞬间闪退且无任何崩溃报告。写 -2 让级联立即终止。
				c->Foggedness = -2;
				c->ShroudCounter = 0;
				reinterpret_cast<void (__thiscall*)(CellClass*)>(0x486C50)(c); // ClearFoggedObjects
			}
		}
	}
	return S_OK;
}
DEFINE_EXPORT(HRESULT, Phobos_Fog_FullReveal)
{
	typedef void (__thiscall* RevealAllFn)(void* self, HouseClass* house);
	if (!HouseClass::CurrentPlayer) return E_FAIL;
	reinterpret_cast<RevealAllFn>(0x577D90)(
		reinterpret_cast<void*>(0x87F7E8), HouseClass::CurrentPlayer);
	// Belt and braces: normalize every cell to fully-visible and clear
	// leftover fogged ghosts (the fork's Reveal hook may leave residue and
	// queued fog-refill cells can re-fog right after we return)
	CellClass** cells = *reinterpret_cast<CellClass***>(
		reinterpret_cast<unsigned char*>(0x87F7E8) + 0x13C);
	if (cells)
	{
		for (int idx = 0; idx < 0x40000; ++idx)
		{
			CellClass* c = cells[idx];
			if (!c) continue;
			unsigned f = static_cast<unsigned int>(c->Flags);
			f &= ~static_cast<unsigned int>(CellFlags::Fogged);
			f |= static_cast<unsigned int>(CellFlags::CenterRevealed)
				| static_cast<unsigned int>(CellFlags::EdgeRevealed);
			c->Flags = static_cast<CellFlags>(f);
			unsigned al = static_cast<unsigned int>(c->AltFlags);
			al |= static_cast<unsigned int>(AltCellFlags::Mapped)
				| static_cast<unsigned int>(AltCellFlags::NoFog);
			c->AltFlags = static_cast<AltCellFlags>(al);
			c->Visibility = -1;
			c->Foggedness = -1;
			// CellClass::ClearFoggedObjects (drops frozen building ghosts)
			reinterpret_cast<void (__thiscall*)(CellClass*)>(0x486C50)(c);
		}
	}
	// MarkNeedsRedraw + minimap rebuild
	reinterpret_cast<void (__thiscall*)(void*)>(0x657CE0)(
		reinterpret_cast<void*>(0x87F7E8));
	return S_OK;
}

DEFINE_EXPORT(HRESULT, Phobos_Fog_RevealCell, int x, int y, HouseClass* pHouse)
{
	// engine-native reveal for one cell (history rebuild); the FogOwner
	// filter inside the RevealFogShroud hook lets the owner's own calls pass
	CellStruct cs = { static_cast<short>(x), static_cast<short>(y) };
	reinterpret_cast<bool (__thiscall*)(void*, CellStruct&, HouseClass*, bool)>(0x4A9CA0)(
		reinterpret_cast<void*>(0x87F7E8), cs, pHouse, true);
	
		// Visibility normalization: the engine calc leaves freshly revealed
		// cells at V=-2 until neighbors re-run; history rebuild has no such
		// follow-up, so normalize fully-clear cells right here.
		{
			CellStruct cs2 = { static_cast<short>(x), static_cast<short>(y) };
			CellClass* c2 = reinterpret_cast<CellClass* (__thiscall*)(void*, CellStruct& )>(0x5657A0)(reinterpret_cast<void*>(0x87F7E8), cs2);
			if (c2 && c2->Foggedness == -1)
			{
				unsigned f = static_cast<unsigned int>(c2->Flags);
				f |= static_cast<unsigned int>(CellFlags::CenterRevealed) | static_cast<unsigned int>(CellFlags::EdgeRevealed);
				f &= ~static_cast<unsigned int>(CellFlags::Fogged);
				c2->Flags = static_cast<CellFlags>(f);
				c2->Visibility = -1;
			}
		}return S_OK;
}
// Call after modifying fog fields directly: repaints the fog layer and
// the minimap (GScreenClass::MarkNeedsRedraw(2) + radar rebuild).
DEFINE_EXPORT(HRESULT, Phobos_Fog_NotifyCellsChanged)
{
	auto* map = reinterpret_cast<void*>(0x87F7E8);
	reinterpret_cast<void (__thiscall*)(void*)>(0x657CE0)(map);
	return S_OK;
}

DEFINE_EXPORT(HRESULT, Phobos_Fog_RevealSight, int cx, int cy, int sight, HouseClass* pHouse, bool increase)
{
	if (!pHouse) return E_FAIL;
	if (sight < 0) sight = 0;
	if (sight > 40) sight = 40;
	const long long r2 = static_cast<long long>(sight) * sight;
	typedef bool (__thiscall* RevealFn)(void*, CellStruct&, HouseClass*, bool);
	auto reveal = reinterpret_cast<RevealFn>(0x4A9CA0);
	auto map = reinterpret_cast<void*>(0x87F7E8);
	for (int dy = -sight; dy <= sight; ++dy)
	{
		const int y = cy + dy;
		if (y < 0 || y >= 512) continue;
		for (int dx = -sight; dx <= sight; ++dx)
		{
			const int x = cx + dx;
			if (x < 0 || x >= 512) continue;
			if (dx * dx + dy * dy > r2) continue;
			CellStruct cs = { static_cast<short>(x), static_cast<short>(y) };
			reveal(map, cs, pHouse, increase);
		}
	}
	
		// Post-pass: the engine Visibility calc can leave fully-clear cells
		// (Foggedness==-1 with all neighbors revealed) stuck at V=-2 when the
		// whole circle is revealed in one pass. Normalize them so units
		// standing there are not drawn inside shroud.
		for (int dy = -sight; dy <= sight; ++dy)
		{
			const int y = cy + dy;
			if (y < 0 || y >= 512) continue;
			for (int dx = -sight; dx <= sight; ++dx)
			{
				const int x = cx + dx;
				if (x < 0 || x >= 512) continue;
				if (dx * dx + dy * dy > r2) continue;
				CellStruct cs2 = { static_cast<short>(x), static_cast<short>(y) };
				CellClass* c2 = reinterpret_cast<CellClass* (__thiscall*)(void*, CellStruct& )>(0x5657A0)(reinterpret_cast<void*>(0x87F7E8), cs2);
				if (!c2) continue;
				if (c2->Foggedness != -1) continue;
				unsigned f = static_cast<unsigned int>(c2->Flags);
				f |= static_cast<unsigned int>(CellFlags::CenterRevealed) | static_cast<unsigned int>(CellFlags::EdgeRevealed);
				f &= ~static_cast<unsigned int>(CellFlags::Fogged);
				c2->Flags = static_cast<CellFlags>(f);
				c2->Visibility = -1;

				// drop frozen building ghosts: CleanFog only clears the Fogged bit,
				// the FoggedObjects vector (semi-transparent frozen images) survives
				reinterpret_cast<void (__thiscall*)(CellClass*)>(0x486C50)(c2);				}
		}
		reinterpret_cast<void (__thiscall*)(void*)>(0x657CE0)(reinterpret_cast<void*>(0x87F7E8));return S_OK;
}
DEFINE_EXPORT(HRESULT, Phobos_Fog_SpreadFog, int cx, int cy)
{
	// engine-native fog refill cascade: FogCell per cell (sets Fogged,
	// freezes buildings into FoggedObjects, hides units) + recursive
	// spread into neighbors that are unrevealed and not yet fully fogged;
	// pacing comes from the vanilla FogRate timer in LogicClass
	CellStruct cs = { static_cast<short>(cx), static_cast<short>(cy) };
	reinterpret_cast<void (__thiscall*)(void*, const CellStruct&)>(0x4ACC50)(
		reinterpret_cast<void*>(0x87F7E8), cs);
	reinterpret_cast<void (__thiscall*)(void*)>(0x657CE0)(
		reinterpret_cast<void*>(0x87F7E8));
	return S_OK;
}