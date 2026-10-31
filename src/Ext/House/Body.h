#pragma once
#include <HouseClass.h>

class FactoryClass;

#include <Ext/HouseType/Body.h>

#include <Utilities/Container.h>
#include <Utilities/Detach.h>
#include <Utilities/TemplateDef.h>

#include <array>

struct PlacingBuildingStruct
{
	BuildingTypeClass* Type;
	BuildingTypeClass* DrawType;
	int Times;
	CDTimerClass Timer;
	CellStruct TopLeft;
	// Mix-ECpack: PlaceBuilding.Extra 类型在挂起等待期间需要保留手动放置时
	// 编码的水面/朝向标志（bit0 = 水面，bit1+ = 朝向），重发 Place 事件时
	// 由 CheckHangUpBuilding 读回；上游 28b62dc9 删除 PlaceType 后不再保存。
	int PlaceFlags = 0;
};

class HouseExt final : public AbstractExt, public Detach::Listener<BuildingClass>
{
public:
	using base_type = HouseClass;

	// deprecated: the pre-rework nested data class is now the extension class itself
	using ExtData [[deprecated("use the extension class itself instead")]] = HouseExt;

	static constexpr DWORD Canary = 0x11111111;

public:
	// typed owner accessor
	HouseClass* OwnerObject() const
	{
		return static_cast<HouseClass*>(this->GetAttachedObject());
	}

	std::vector<UnitClass*> OwnedDeployingUnits;
	PlacingBuildingStruct Common;
	PlacingBuildingStruct Combat;
	int LastRefineryBuildFrame;

	std::vector<BuildingClass*> PowerPlantEnhancers;
	std::vector<BuildingClass*> OwnedLimboDeliveredBuildings;
	std::vector<TechnoClass*> OwnedCountedHarvesters;

	CounterClass LimboAircraft;  // Currently owned aircraft in limbo
	CounterClass LimboBuildings; // Currently owned buildings in limbo
	CounterClass LimboInfantry;  // Currently owned infantry in limbo
	CounterClass LimboVehicles;  // Currently owned vehicles in limbo

	BuildingClass* Factory_BuildingType;
	BuildingClass* Factory_InfantryType;
	BuildingClass* Factory_VehicleType;
	BuildingClass* Factory_NavyType;
	BuildingClass* Factory_AircraftType;

	CDTimerClass CombatAlertTimer;
	CDTimerClass AISuperWeaponDelayTimer;
	CDTimerClass AIFireSaleDelayTimer;

	//Read from INI
	Nullable<bool> RepairBaseNodes[3];

	// FactoryPlants with Allow/DisallowTypes set.
	std::vector<BuildingClass*> RestrictedFactoryPlants;

	int LastBuiltNavalVehicleType;
	int ProducingNavalUnitTypeIndex;

	// AI 防御建筑产线（与主建筑产线并行，镜像 ProducingNavalUnitTypeIndex 的做法）：
	// 主产线沿用原版 ProducingBuildingTypeIndex，防御建筑（BuildCat::Combat）的
	// 产槽独立；工厂为挂在第一座建造厂上的独立 FactoryClass，完成后的放置经由
	// Primary_ForDefenses 走与玩家一致的 Place 事件流程。
	int ProducingDefenseBuildingTypeIndex;
	FactoryClass* AIDefenseFactory;          // 防御产线工厂（瞬态，不入档）
	bool AIDualPickCombatOnly;               // AI_Building 本轮节点挑选只取防御建筑（瞬态）

	void UpdateAIDefenseProduction();
	void ClearAIDefenseFactory();

	// Factories that exist but don't count towards multiple factory bonus.
	int NumAirpads_NonMFB;
	int NumBarracks_NonMFB;
	int NumWarFactories_NonMFB;
	int NumConYards_NonMFB;
	int NumShipyards_NonMFB;

	std::map<int, std::vector<int>> SuspendedEMPulseSWs;

	// standalone? no need and not a good idea
	struct SWExt
	{
		int ShotCount;
	};
	std::vector<SWExt> SuperExts;

	CDTimerClass SpyEffect_RadarJamTimer;

	int ForceEnemyIndex;
	bool ForceOnlyTargetHouseEnemy;
	int ForceOnlyTargetHouseEnemyMode;
	int TeamDelay;
	bool FreeRadar;
	bool ForceRadar;

	bool PlayerAutoRepair;

	std::array<int, 3> BeaconsPlacedOrder;

	std::vector<BuildingClass*> Academies;

	HouseExt(HouseClass* OwnerObject) : AbstractExt(OwnerObject)
		, PowerPlantEnhancers {}
		, OwnedLimboDeliveredBuildings {}
		, OwnedCountedHarvesters {}
		, LimboAircraft {}
		, LimboBuildings {}
		, LimboInfantry {}
		, LimboVehicles {}
		, Factory_BuildingType { nullptr }
		, Factory_InfantryType { nullptr }
		, Factory_VehicleType { nullptr }
		, Factory_NavyType { nullptr }
		, Factory_AircraftType { nullptr }
		, AISuperWeaponDelayTimer {}
		, RepairBaseNodes { }
		, RestrictedFactoryPlants {}
		, LastBuiltNavalVehicleType { -1 }
		, ProducingNavalUnitTypeIndex { -1 }
		, ProducingDefenseBuildingTypeIndex { -1 }
		, AIDefenseFactory { nullptr }
		, AIDualPickCombatOnly { false }
		, CombatAlertTimer {}
		, NumAirpads_NonMFB { 0 }
		, NumBarracks_NonMFB { 0 }
		, NumWarFactories_NonMFB { 0 }
		, NumConYards_NonMFB { 0 }
		, NumShipyards_NonMFB { 0 }
		, AIFireSaleDelayTimer {}
		, SuspendedEMPulseSWs {}
		, SuperExts(SuperWeaponTypeClass::Array.Count)
		, SpyEffect_RadarJamTimer {}
		, ForceEnemyIndex { -1 }
		, ForceOnlyTargetHouseEnemy { false }
		, ForceOnlyTargetHouseEnemyMode { -1 }
		, TeamDelay { -1 }
		, FreeRadar { false }
		, ForceRadar { false }
		, PlayerAutoRepair { true }
		, BeaconsPlacedOrder { 0, 0, 0 }
		, Academies {}
		, OwnedDeployingUnits {}
		, Common { nullptr, nullptr, 0 }
		, Combat { nullptr, nullptr, 0 }
		, LastRefineryBuildFrame { -1 }
	{ }

	bool OwnsLimboDeliveredBuilding(BuildingClass* pBuilding) const;
	void AddToLimboTracking(TechnoTypeClass* pTechnoType);
	void RemoveFromLimboTracking(TechnoTypeClass* pTechnoType);
	int CountOwnedPresentAndLimboed(TechnoTypeClass* pTechnoType) const;
	void UpdateNonMFBFactoryCounts(AbstractType rtti, bool remove, bool isNaval);
	int GetFactoryCountWithoutNonMFB(AbstractType rtti, bool isNaval) const;
	float GetRestrictedFactoryPlantMult(TechnoTypeClass* pTechnoType) const;

	int GetForceEnemyIndex();
	void SetForceEnemyIndex(int EnemyIndex);

	void UpdateVehicleProduction();

	virtual ~HouseExt() = default;

	virtual void LoadFromINIFile(CCINIClass* pINI) override;
	//virtual void Initialize() override;
	virtual void OnDetach(BuildingClass* pTarget, bool removed) override;

	virtual void LoadFromStream(PhobosStreamReader& Stm) override;
	virtual void SaveToStream(PhobosStreamWriter& Stm) override;

private:
	template <typename T>
	void Serialize(T& Stm);
	bool UpdateHarvesterProduction();

public:
	class ExtContainer final : public Container<HouseExt>
	{
	public:
		ExtContainer();
		~ExtContainer();
	};

	static ExtContainer ExtMap;

	static HouseExt* Fetch(const HouseClass* pThis)
	{
		return AbstractExt::Fetch<HouseExt>(pThis);
	}

	static HouseExt* TryFetch(const HouseClass* pThis)
	{
		return AbstractExt::TryFetch<HouseExt>(pThis);
	}

	static bool LoadGlobals(PhobosStreamReader& Stm);
	static bool SaveGlobals(PhobosStreamWriter& Stm);

	static int ActiveHarvesterCount(HouseClass* pThis);
	static int TotalHarvesterCount(HouseClass* pThis);
	static HouseClass* GetHouseKind(OwnerHouseKind kind, bool allowRandom, HouseClass* pDefault, HouseClass* pInvoker = nullptr, HouseClass* pVictim = nullptr);
	static CellClass* GetEnemyBaseGatherCell(HouseClass* pTargetHouse, HouseClass* pCurrentHouse, CoordStruct defaultCurrentCoords, SpeedType speedTypeZone, int extraDistance = 0);
	static void GetAIChronoshiftSupers(HouseClass* pThis, SuperClass*& pSuperCSphere, SuperClass*& pSuperCWarp);

	static void SetForceOnlyTargetHouseEnemy(HouseClass* pThis, int mode = -1);
	static void SetSkirmishHouseName(HouseClass* pHouse);

	static bool IsDisabledFromShell(
	HouseClass const* pHouse, BuildingTypeClass const* pItem);

	static size_t FindOwnedIndex(
	HouseClass const* pHouse, int idxParentCountry,
	Iterator<TechnoTypeClass const*> items, size_t start = 0);

	static size_t FindBuildableIndex(
		HouseClass const* pHouse, int idxParentCountry,
		Iterator<TechnoTypeClass const*> items, size_t start = 0);

	template <typename T>
	static T* FindOwned(
		HouseClass const* const pHouse, int const idxParent,
		Iterator<T*> const items, size_t const start = 0)
	{
		auto const index = FindOwnedIndex(pHouse, idxParent, items, start);
		return index < items.size() ? items[index] : nullptr;
	}

	template <typename T>
	static T* FindBuildable(
		HouseClass const* const pHouse, int const idxParent,
		Iterator<T*> const items, size_t const start = 0)
	{
		auto const index = FindBuildableIndex(pHouse, idxParent, items, start);
		return index < items.size() ? items[index] : nullptr;
	}

	static std::vector<int> AIProduction_CreationFrames;
	static std::vector<int> AIProduction_Values;
	static std::vector<int> AIProduction_BestChoices;
	static std::vector<int> AIProduction_BestChoicesNaval;

	static CanBuildResult BuildLimitGroupUpgradeCheck(const HouseClass* pThis, const TechnoTypeClass* pItem, bool buildLimitOnly, bool includeQueued);
	static bool ReachedBuildLimit(const HouseClass* pHouse, const TechnoTypeClass* pType, bool ignoreQueued);
	static int CountOwnedPresentExt(HouseClass* pHouse, TechnoTypeClass* pTechnoType, bool upgrade = false, bool deploy = false);
	static int CountOwnedPresentWithJumpjet(HouseClass* pHouse, AircraftTypeClass* pAircraftType);
	static int CountOwnedPresentWithDeploy(HouseClass* pHouse, UnitTypeClass* pUnitType, bool deploy = false);
	static int CountOwnedPresentWithDeployOrUpgrade(HouseClass* pHouse, BuildingTypeClass* pBuildingType, bool upgrade = false, bool deploy = false);
	static int CountOwnedNowWithDeployOrUpgrade(HouseClass* pHouse, BuildingTypeClass* pBuildingType, bool upgrade = true, bool deploy = true);
	static bool CheckOwnerBitfieldForCurrentPlayer(TechnoTypeClass* pType);
	static void RecheckOwnerBitfieldForCurrentPlayer();
	static void ReorganizeAllTo(HouseClass* pFromHouse, HouseClass* pToHouse);
	static void __fastcall DecideTechnosFate(HouseClass* pHouse);
	static void CalculatePowerSurplus(HouseClass* pThis);
};

