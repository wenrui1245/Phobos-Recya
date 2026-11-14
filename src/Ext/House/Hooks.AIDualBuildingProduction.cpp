#include "Body.h"

#include <algorithm>

#include <BuildingClass.h>
#include <BuildingTypeClass.h>
#include <FactoryClass.h>
#include <HouseClass.h>
#include <MapClass.h>
#include <ScenarioClass.h>

#include <Ext/BuildingType/Body.h>

namespace DualBuildingTemp
{
	// 放置尝试次数上限：超过即放弃该节点
	constexpr int MaxPlaceAttempts = 8;
	constexpr int PlaceRetryInterval = 30;

	// UnitFromFactory is also the native Place-event handler. The YRpp declaration
	// discards its bool return. Verified at 0x4FB512 (AL = success, ret 0x10),
	// with a false return at 0x4FB5BA. Use that result, not a possibly freed factory.
	inline bool IsHostedUpgradeType(BuildingTypeClass*);
	inline BuildingClass* FindUpgradeHost(HouseClass*,BuildingTypeClass*);
	inline bool PlaceDefense(HouseClass* pHouse, BuildingTypeClass* pType, const CellStruct& coords)
	{
		auto location=coords;
		if(IsHostedUpgradeType(pType)) {
			auto* host=FindUpgradeHost(pHouse,pType);if(!host)return false;
			location=host->GetMapCoords();
		}
		return reinterpret_cast<bool(__thiscall*)(HouseClass*, AbstractType, int, bool, const CellStruct*)>(0x4FB0E0)
			(pHouse, AbstractType::Building, pType->ArrayIndex, pType->Naval, &location);
	}

	inline bool UsesDefenseLine(HouseClass* pHouse)
	{
		return !pHouse->IsControlledByHuman() && pHouse->Production;
	}

	inline bool CanPickNode(HouseClass* pHouse, int index)
	{
		auto& node = pHouse->Base.BaseNodes.Items[index];
		if (reinterpret_cast<bool(__thiscall*)(BaseClass*, int)>(0x42E780)(&pHouse->Base, index))
			return false;

		if (!*reinterpret_cast<const DWORD*>(0xA8B238) || !node.Placed)
			return true;

		if (!pHouse->IsValidBaseNode(&node))
			return false;

		node.MapCoords = *reinterpret_cast<const CellStruct*>(0x89C310);
		return true;
	}

	inline bool IsHostedUpgradeType(BuildingTypeClass* pType)
	{
		return pType->PowersUpBuilding[0] || !BuildingTypeExt::Fetch(pType)->PowersUp_Buildings.empty();
	}
	// Match native/host upgrade placement eligibility instead of treating the
	// upgrade's base-plan coordinates as an empty foundation to build on.
	inline BuildingClass* FindUpgradeHost(HouseClass* house,BuildingTypeClass* type)
	{
		for(auto* host:house->Buildings)
			if(host && host->IsAlive && host->Health>0 && host->IsOnMap && !host->InLimbo
 && !HouseExt::Fetch(house)->OwnsLimboDeliveredBuilding(host)
 && host->CanUpgradeBuilding(type,house) && host->GetMapCoords()!=CellStruct::Empty
				&& host->CurrentMission!=Mission::Selling)return host;
		return nullptr;
	}
	inline bool IsDefenseLineType(BuildingTypeClass* pType)
	{
		return pType->BuildCat == BuildCat::Combat;
	}

	inline bool IsDefenseLineNode(const BaseNodeClass& node)
	{
		if (node.BuildingTypeIndex < 0 || node.BuildingTypeIndex >= BuildingTypeClass::Array.Count)
			return false;

		return IsDefenseLineType(BuildingTypeClass::Array.GetItem(node.BuildingTypeIndex));
	}

	// Skip only the exact defense job already accepted by the native scheduler,
	// or a job abandoned after its placement retries. Future Combat nodes must
	// remain in the native sequence, including -1 defense / -2 stop / -3 wall.
	inline BaseNodeClass* PickAvailableBuildingNode(HouseClass* pHouse)
	{
		auto& nodes = pHouse->Base.BaseNodes;

		for (int i = 0; i < nodes.Count; ++i)
		{
			auto& node = nodes.Items[i];

			if (UsesDefenseLine(pHouse) && IsDefenseLineNode(node))
			{
				auto const pExt = HouseExt::Fetch(pHouse);
				if (pExt->AIDefenseFactory
					&& pExt->ProducingDefenseBuildingTypeIndex == node.BuildingTypeIndex
					&& pExt->AIDefenseNodeCoords == node.MapCoords)
				{
					continue;
				}

				if (std::find(pExt->AIDefenseAbandonedNodes.begin(), pExt->AIDefenseAbandonedNodes.end(),
					node.MapCoords) != pExt->AIDefenseAbandonedNodes.end())
				{
					continue;
				}
			}

			// 节点位置已有建筑 → 跳过（与原版一致）
			if (CanPickNode(pHouse, i))
				return &node;
		}

		return nullptr;
	}
}

// AI 双建筑产线（主建筑 + 防御建筑并行）
//
// 原版 AI 的建筑生产只有单一产槽（HouseClass::ProducingBuildingTypeIndex），
// 主建筑与防御建筑（BuildCat::Combat）共用，AI 无法像玩家那样同时建造主要
// 建筑物与防御建筑物。
//
// Native AI_Building chooses and prepares the next node first. Only its final
// dispatch is split into ordinary and Combat production. Never scan the whole
// plan for future Combat buildings: CanBuild is not the AI's development order,
// and doing so can start a late superweapon before the first defense marker.
// The ordinary line may pass the exact defense job already in progress, while
// the next unclaimed defense node waits for the defense slot to become free.

// Run in the deterministic simulation on every peer. Do not enqueue AI Place
// commands in OutList: it is a local input queue, not a placement acknowledgement.
void HouseExt::UpdateAIDefenseProduction()
{
	auto const pThis = this->OwnerObject();
	const int defIdx = this->ProducingDefenseBuildingTypeIndex;
	if (pThis->IsControlledByHuman() || pThis->Defeated || pThis->ConYards.Count <= 0)
	{
		this->ClearAIDefenseFactory();
		return;
	}

	if (!pThis->Production)
	{
		if (this->AIDefenseFactory && this->AIDefenseFactory->Object)
			this->AIDefenseFactory->Suspend(false);
		return;
	}

	// —— 既有任务的生命周期 ——
	if (defIdx != -1)
	{
		if (defIdx < 0 || defIdx >= BuildingTypeClass::Array.Count || !this->AIDefenseFactory)
		{
			// A factory removed by the game is invalidated through Detach. Never
			// recreate an unpaid/default-position job from only its saved type index.
			this->ClearAIDefenseFactory();
			return;
		}

		auto const pType = BuildingTypeClass::Array.GetItem(defIdx);
		auto const pFactory = this->AIDefenseFactory;
		if (pFactory->Owner != pThis || !pFactory->Object || pFactory->Object->GetType() != pType)
		{
			this->ClearAIDefenseFactory();
			return;
		}

		// Match native factory startup and BuildingExt::UpdateFactoryQueues.
		// A disabled/unpowered yard suspends progress without losing paid work.
		auto const pBuilder = pType->FindFactory(true, true, true, pThis);
		if (!pBuilder || pBuilder->Deactivated || !pBuilder->HasPower
			|| (pThis->Primary_ForDefenses && pThis->Primary_ForDefenses != pFactory))
		{
			pFactory->Suspend(false);
			return;
		}

		pThis->SetPrimaryFactory(pFactory, AbstractType::BuildingType, false, BuildCat::Combat);
		if (!pFactory->IsDone())
		{
			if (pFactory->IsSuspended && !pFactory->IsManual)
				pFactory->Unsuspend(false);
			return;
		}

		if (Unsorted::CurrentFrame < this->AIDefenseNextPlaceFrame)
			return;

		// 基地计划的节点坐标可能落在可视边界之外：在该位置放置的建筑会成为
		// 不可见、不可选中、无法被攻击的"幽灵建筑"，使战败计数永远无法归零。
		// (-1,-1) 是原版重建任务哨兵，由 UnitFromFactory 从工厂出口找位，不受影响。
		if (this->AIDefenseNodeCoords.X != -1 && this->AIDefenseNodeCoords.Y != -1
			&& !MapClass::Instance.IsWithinUsableArea(this->AIDefenseNodeCoords, false))
		{
			this->AIDefenseAbandonedNodes.push_back(this->AIDefenseNodeCoords);
			this->ClearAIDefenseFactory();
			return;
		}

		this->AIDefenseNextPlaceFrame = Unsorted::CurrentFrame + DualBuildingTemp::PlaceRetryInterval;
		// Native placement can delete the factory, including through Phobos hooks.
		// OnDetach clears our pointer; do not dereference pFactory after this call.
		if (DualBuildingTemp::PlaceDefense(pThis, pType, this->AIDefenseNodeCoords))
		{
			this->MarkAIDefenseNodePlaced();
			this->ClearAIDefenseFactory();
		}
		else if (++this->AIDefensePlaceAttempts >= DualBuildingTemp::MaxPlaceAttempts)
		{
			this->AIDefenseAbandonedNodes.push_back(this->AIDefenseNodeCoords);
			this->ClearAIDefenseFactory();
		}

		return;
	}

	// Idle: wait for native AI_Building to dispatch a job. No independent scan.
}

bool HouseExt::TryStartAIDefenseProduction(const BaseNodeClass& node)
{
	auto const pThis = this->OwnerObject();
	if (!DualBuildingTemp::UsesDefenseLine(pThis) || pThis->Defeated || pThis->ConYards.Count <= 0
		|| this->ProducingDefenseBuildingTypeIndex != -1 || this->AIDefenseFactory
		|| pThis->Primary_ForDefenses || !DualBuildingTemp::IsDefenseLineNode(node)
		|| Unsorted::CurrentFrame < this->AIDefenseNextPlaceFrame)
	{
		return false;
	}

	if (std::find(this->AIDefenseAbandonedNodes.begin(), this->AIDefenseAbandonedNodes.end(),
		node.MapCoords) != this->AIDefenseAbandonedNodes.end())
	{
		return false;
	}

	// 同 UpdateAIDefenseProduction：越界节点会导致幽灵建筑，直接废弃。
	// (-1,-1) 是原版重建任务哨兵，由 UnitFromFactory 从工厂出口找位，不受影响。
	if (node.MapCoords.X != -1 && node.MapCoords.Y != -1
		&& !MapClass::Instance.IsWithinUsableArea(node.MapCoords, false))
	{
		this->AIDefenseAbandonedNodes.push_back(node.MapCoords);
		return false;
	}

	auto const pType = BuildingTypeClass::Array.GetItem(node.BuildingTypeIndex);
 if(DualBuildingTemp::IsHostedUpgradeType(pType) && !DualBuildingTemp::FindUpgradeHost(pThis,pType))return false;
	if (pThis->CanBuild(pType, false, false) == CanBuildResult::Unbuildable)
		return false;

	auto const pBuilder = pType->FindFactory(true, true, true, pThis);
	if (!pBuilder || pBuilder->Deactivated || !pBuilder->HasPower)
		return false;

	if (pType->GetActualCost(pThis) > pThis->Available_Money())
	{
		this->AIDefenseNextPlaceFrame = Unsorted::CurrentFrame + DualBuildingTemp::PlaceRetryInterval;
		return false;
	}

	// The caller has already run native node selection, defense-marker expansion
	// and power insertion. Do not repeat IsValidBaseNode or repick its coordinates.
	this->ProducingDefenseBuildingTypeIndex = node.BuildingTypeIndex;
	this->AIDefenseNodeCoords = node.MapCoords;
	this->AIDefensePlaceAttempts = 0;
	auto const pFactory = GameCreate<FactoryClass>();
	if (!pFactory->DemandProduction(pType, pThis, false))
	{
		GameDelete(pFactory);
		this->ClearAIDefenseFactory();
		return false;
	}

	this->AIDefenseFactory = pFactory;
	pThis->SetPrimaryFactory(pFactory, AbstractType::BuildingType, false, BuildCat::Combat);
	pFactory->Unsuspend(false);
	return true;
}

void HouseExt::ClearAIDefenseFactory()
{
	auto const pFactory = this->AIDefenseFactory;
	if (pFactory)
	{
		if (this->OwnerObject()->Primary_ForDefenses == pFactory)
			this->OwnerObject()->SetPrimaryFactory(nullptr, AbstractType::BuildingType, false, BuildCat::Combat);

		// Keep the identity until AbandonProduction returns: the abandon hooks
		// must distinguish this line from the native building production slot.
		// If ownership has changed, only relinquish our reference. Its new owner
		// (or native teardown) is responsible for destroying that factory.
		if (pFactory->Owner == this->OwnerObject())
		{
			if (pFactory->Object)
				pFactory->AbandonProduction();

			GameDelete(pFactory);
		}
		this->AIDefenseFactory = nullptr;
	}

	this->ProducingDefenseBuildingTypeIndex = -1;
	this->AIDefenseNodeCoords = CellStruct::Empty;
	this->AIDefensePlaceAttempts = 0;
}

void HouseExt::OnDetach(FactoryClass* pTarget, bool removed)
{
	if (removed && this->AIDefenseFactory == pTarget)
		this->AIDefenseFactory = nullptr;
}

void HouseExt::MarkAIDefenseNodePlaced()
{
	auto const pThis = this->OwnerObject();
	const int defIdx = this->ProducingDefenseBuildingTypeIndex;

	if (defIdx == -1)
		return;

	auto& nodes = pThis->Base.BaseNodes;

	for (int i = 0; i < nodes.Count; ++i)
	{
		auto& node = nodes.Items[i];

		if (node.BuildingTypeIndex == defIdx
			&& node.MapCoords == this->AIDefenseNodeCoords)
		{
			node.Placed = true;
			node.Attempts = 0;
			break;
		}
	}
}

// AI_Building（0x4FE3E0）中的待建节点挑选调用（lea ecx,[ebp+0x5700]; push -1;
// call 0x42EB20，共 0xD 字节），只跳过已接单的防御节点。
// 注意必须整体替换这三条指令——若只替换 call 本身，自定义路径返回时原版
// push 的 -1 参数会残留在栈上，导致函数尾声 ret 读到野地址（实测崩溃）。
DEFINE_HOOK(0x4FE401, HouseClass_AI_BuildingProduction_PickNode, 0xD)
{
	enum { AfterPick = 0x4FE40E };

	GET(HouseClass* const, pThis, EBP);

	R->EAX(DualBuildingTemp::PickAvailableBuildingNode(pThis));
	return AfterPick;
}

// AI_Building 中"挑选的节点已在生产队列中则移除并重新挑选"的再挑选调用
// （push -1; lea ecx,[ebp+0x5700]; call 0x42EB20，共 0xD 字节），同样换成
// 过滤版挑选循环。
DEFINE_HOOK(0x4FE493, HouseClass_AI_BuildingProduction_RepickNode, 0xD)
{
	enum { AfterRepick = 0x4FE4A0 };

	GET(HouseClass* const, pThis, EBP);

	R->EAX(DualBuildingTemp::PickAvailableBuildingNode(pThis));
	return AfterRepick;
}

// Original: mov eax,[esp+10h]; mov ecx,[eax]; mov [ebp+564Ch],ecx.
// This is AFTER native wall/defense-marker expansion and power-node insertion.
// Non-Combat buildings/upgrades replay all three instructions. Combat jobs are
// handed to the independent slot without touching ProducingBuildingTypeIndex.
DEFINE_HOOK(0x4FEA3C, HouseClass_AI_BuildingProduction_DispatchDefense, 0xC)
{
	enum { AfterDispatch = 0x4FEA48 };
	GET(HouseClass* const, pThis, EBP);
	GET_STACK(BaseNodeClass* const, pNode, 0x10);
	if (!pNode || !DualBuildingTemp::UsesDefenseLine(pThis) || !DualBuildingTemp::IsDefenseLineNode(*pNode))
		return 0;

	HouseExt::Fetch(pThis)->TryStartAIDefenseProduction(*pNode);
	return AfterDispatch;
}
