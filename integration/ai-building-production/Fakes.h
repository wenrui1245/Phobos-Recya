#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>
#include <stdexcept>
using DWORD = unsigned long;
inline DWORD NativeFlag = 1;

struct CellStruct
{
	short X = 0, Y = 0;
	bool operator==(const CellStruct&) const = default;
	static const CellStruct Empty;
};
inline const CellStruct CellStruct::Empty {-1, -1};
inline CellStruct NativeRepickCell;
enum class AbstractType { Building, BuildingType, Unit };
enum class BuildCat { DontCare, Combat };
enum class CanBuildResult { TemporarilyUnbuildable = -1, Unbuildable, Buildable };
struct FactoryClass;
struct HouseExt;
struct BuildingTypeClass;
struct BuildingClass { bool Deactivated = false, HasPower = true; };
struct BaseNodeClass
{
	int BuildingTypeIndex = 0;
	CellStruct MapCoords {10, 20};
	bool Placed = false;
	int Attempts = 0;
};
template<class T> struct FakeArray
{
	std::vector<T> Items;
	int Count = 0;
	T GetItem(int i) const { return Items.at(i); }
	void Add(T v) { Items.push_back(v); Count = static_cast<int>(Items.size()); }
};
struct BaseClass { FakeArray<BaseNodeClass> BaseNodes; std::set<int> Built; };
struct BuildingTypeClass
{
	inline static FakeArray<BuildingTypeClass*> Array;
	BuildCat BuildCat = BuildCat::Combat;
	int ArrayIndex = 0;
	bool Naval = false, Hosted = false, Buildable = true, HasBuilder = true;
	int Cost = 300;
	BuildingClass Builder;
	BuildingClass* FindFactory(bool occupied, bool power, bool buildable, const struct HouseClass*)
	{
		assert(occupied && power && buildable);
		return HasBuilder && Builder.HasPower && !Builder.Deactivated ? &Builder : nullptr;
	}
	int GetActualCost(const struct HouseClass*) const { return Cost; }
};
struct BuildingTypeExt
{
	std::vector<int> PowersUp_Buildings;
	static BuildingTypeExt* Fetch(BuildingTypeClass* pType)
	{
		static BuildingTypeExt ext;
		ext.PowersUp_Buildings = pType->Hosted ? std::vector<int>{1} : std::vector<int>{};
		return &ext;
	}
};
struct TechnoClass
{
	BuildingTypeClass* Type;
	bool InLimbo = true;
	BuildingTypeClass* GetType() const { return Type; }
	AbstractType WhatAmI() const { return AbstractType::Building; }
};
struct UnitClass : TechnoClass { };
struct HouseClass
{
	HouseExt* Ext = nullptr;
	bool Human = false, Defeated = false, Production = true, Repick = false;
	FakeArray<int> ConYards;
	BaseClass Base;
	FactoryClass* Primary_ForDefenses = nullptr;
	int ProducingBuildingTypeIndex = 77;
	int Money = 10000;
	bool IsControlledByHuman() const { return Human; }
	bool IsValidBaseNode(BaseNodeClass*) const { return Repick; }
	int Available_Money() const { return Money; }
	CanBuildResult CanBuild(BuildingTypeClass* t, bool, bool) const
	{ return t->Buildable ? CanBuildResult::Buildable : CanBuildResult::Unbuildable; }
	void SetPrimaryFactory(FactoryClass* f, AbstractType, bool, ::BuildCat) { Primary_ForDefenses = f; }
};
namespace Unsorted { inline int CurrentFrame = 0; }
inline bool DemandSucceeds = true, PlaceSucceeds = true, PlaceDeletesFactory = true;
inline int PlaceCalls = 0, CreatedFactories = 0, DeletedFactories = 0;
inline CellStruct PlacedAt;
inline std::set<FactoryClass*> LiveFactories;
inline std::vector<TechnoClass*> PlacedObjects;
unsigned AbandonSlotGuard(FactoryClass*);
struct HouseExt
{
	HouseClass* House;
	int ProducingDefenseBuildingTypeIndex = -1;
	FactoryClass* AIDefenseFactory = nullptr;
	CellStruct AIDefenseNodeCoords;
	int AIDefenseNextPlaceFrame = 0, AIDefensePlaceAttempts = 0;
	std::vector<CellStruct> AIDefenseAbandonedNodes;
	int ProducingNavalUnitTypeIndex = 9;
	explicit HouseExt(HouseClass* h) : House(h) { h->Ext = this; }
	HouseClass* OwnerObject() const { return House; }
	static HouseExt* TryFetch(HouseClass* h) { return h ? h->Ext : nullptr; }
	static HouseExt* Fetch(HouseClass* h) { return TryFetch(h); }
	void UpdateAIDefenseProduction();
	bool TryStartAIDefenseProduction(const BaseNodeClass& node);
	void ClearAIDefenseFactory();
	void MarkAIDefenseNodePlaced();
	void OnDetach(FactoryClass*, bool);
	template<class T> void TestSerialize(T&);
};
struct FactoryClass
{
	HouseClass* Owner = nullptr;
	TechnoClass* Object = nullptr;
	bool IsSuspended = true, IsManual = false, Done = false;
	int Progress = 0, Paid = 0;
	FactoryClass() { LiveFactories.insert(this); ++CreatedFactories; }
	~FactoryClass()
	{
		assert(LiveFactories.erase(this) == 1);
		++DeletedFactories;
		if (Owner)
		{
			if (Owner->Primary_ForDefenses == this) Owner->Primary_ForDefenses = nullptr;
			if (Owner->Ext) Owner->Ext->OnDetach(this, true);
		}
		delete Object;
	}
	bool DemandProduction(BuildingTypeClass* t, HouseClass* h, bool)
	{
		if (!DemandSucceeds) return false;
		Owner = h; Object = new TechnoClass {t}; return true;
	}
	bool IsDone() const { return Done; }
	void Suspend(bool) { IsSuspended = true; }
	void Unsuspend(bool) { if (!Done) IsSuspended = false; }
	void Tick()
	{
		if (Object && !IsSuspended && !Done && Owner->Money >= 10)
		{
			Owner->Money -= 10; Paid += 10;
			if (++Progress == 3) { Done = true; IsSuspended = true; }
		}
	}
	void AbandonProduction()
	{
		// Execute the real modified hook's native-slot dispatch before emulating
		// 0x4CA0DA. A defense cancellation must skip the main-slot write.
		if (AbandonSlotGuard(this) != 0x4CA0E3) Owner->ProducingBuildingTypeIndex = -1;
		Owner->Money += Paid;
		delete Object; Object = nullptr;
	}
};
template<class T> T* GameCreate() { return new T; }
template<class T> void GameDelete(T* p) { delete p; }

// Engine call boundaries only; controller logic is compiled from production.
inline bool __fastcall NativeBuilt(BaseClass* b, void*, int i) { return b->Built.contains(i); }
inline bool __fastcall NativePlace(HouseClass* h, void*, AbstractType, int, bool, const CellStruct* cell)
{
	++PlaceCalls; PlacedAt = *cell;
	if (!PlaceSucceeds) return false;
	auto f = h->Primary_ForDefenses;
	assert(f && LiveFactories.contains(f) && f->Done);
	f->Object->InLimbo = false;
	PlacedObjects.push_back(f->Object); f->Object = nullptr;
	for (int i = 0; i < h->Base.BaseNodes.Count; ++i)
		if (h->Base.BaseNodes.Items[i].MapCoords == *cell) h->Base.Built.insert(i);
	if (PlaceDeletesFactory) delete f;
	return true;
}
