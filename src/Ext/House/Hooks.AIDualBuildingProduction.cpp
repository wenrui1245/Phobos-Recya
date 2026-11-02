#include "Body.h"

#include <algorithm>

#include <BuildingClass.h>
#include <BuildingTypeClass.h>
#include <FactoryClass.h>
#include <HouseClass.h>
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
	inline bool PlaceDefense(HouseClass* pHouse, BuildingTypeClass* pType, const CellStruct& coords)
	{
		return reinterpret_cast<bool(__thiscall*)(HouseClass*, AbstractType, int, bool, const CellStruct*)>(0x4FB0E0)
			(pHouse, AbstractType::Building, pType->ArrayIndex, pType->Naval, &coords);
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

	// 挂接式升级/插件类型（PowersUp 系列，需要宿主建筑）不归防御产线：它们
	// 依赖宿主存在才能放置，由原版主产线的升级放置流程处理
	inline bool IsHostedUpgradeType(BuildingTypeClass* pType)
	{
		return !BuildingTypeExt::Fetch(pType)->PowersUp_Buildings.empty();
	}

	// 由防御产线承接的类型：BuildCat=Combat 且非挂接式
	inline bool IsDefenseLineType(BuildingTypeClass* pType)
	{
		return pType->BuildCat == BuildCat::Combat && !IsHostedUpgradeType(pType);
	}

	inline bool IsDefenseLineNode(const BaseNodeClass& node)
	{
		if (node.BuildingTypeIndex < 0 || node.BuildingTypeIndex >= BuildingTypeClass::Array.Count)
			return false;

		return IsDefenseLineType(BuildingTypeClass::Array.GetItem(node.BuildingTypeIndex));
	}

	// 供 AI_Building 的两处节点挑选调用（0x4FE401 / 0x4FE493）使用。
	//
	// 逐句复刻原版挑选循环（0x42EB50 的语义：已建判定 → [0xA8B238] 全局旗标 →
	// Placed → 0x50CAD0 复选），只额外硬性跳过由防御产线承接的节点。
	// 不能用"临时置 Placed 再还原"来遮蔽：Placed 的实际语义是"该节点已建造"，
	// 原版对 Placed 节点仍会经 0x50CAD0 复选重新选中，且 [0xA8B238] 为 0 时
	// 会完全无视 Placed 直接选中——遮蔽不可靠（实测主产线仍会造出防御建筑）。
	inline BaseNodeClass* PickExcludingDefense(HouseClass* pHouse)
	{
		auto& nodes = pHouse->Base.BaseNodes;

		for (int i = 0; i < nodes.Count; ++i)
		{
			auto& node = nodes.Items[i];

			// 防御产线承接的节点不参与主产线挑选
			if (UsesDefenseLine(pHouse) && IsDefenseLineNode(node))
				continue;

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
// 实现要点：完全不介入原版 AI_Building（0x4FE3E0）的挑选-入队管线（该管线
// 按"单产槽"设计，任何旁路写入都会使其队列状态自相矛盾而堵塞）。防御产线
// 作为一条独立的旁路存在：
//
//   1. 所有待建的 Combat 节点一律由管理器承接为防御产线任务（可负担性等
//      检查与原版一致）；原版 AI_Building 内的两处节点挑选（首轮挑选与
//      "已入队则移除重挑"的再挑选）都换成本文件的挑选循环，硬性跳过这些
//      节点，主产线永远选不到防御建筑。挂接式升级/插件类型（PowersUp 系列）
//      除外——它们仍由原版主产线以宿主感知的方式放置；
//   2. 通过独立 FactoryClass 生产（DemandProduction + Unsuspend），
//      实际建造厂由原版 FindFactory 查找；不占用建造厂的主产线 Factory 字段；
//   3. 生产完成后在该节点自己的坐标调用原版放置函数（沿用基地规划的位置），
//      成功后把节点标记为已放置；反复放置失败（如目标位置被永久堵死）达
//      上限后放弃该节点，防止产线被卡死；建造厂被打包/出售时与原版同样视为
//      产线丢失，放弃生产。

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

	// —— 获取新任务 ——
	// 所有防御建筑需求一律由本产线承接（原版挑选已通过 0x4FE401/0x4FE493 钩子
	// 排除 Combat 节点），不依赖主产线是否空闲；挂接式升级/插件类型不承接
	if (pThis->Primary_ForDefenses)
		return;

	if (Unsorted::CurrentFrame < this->AIDefenseNextPlaceFrame)
		return;

	const long money = pThis->Available_Money();
	auto& nodes = pThis->Base.BaseNodes;

	for (int i = 0; i < nodes.Count; ++i)
	{
		auto& node = nodes.Items[i];

		if (!DualBuildingTemp::IsDefenseLineNode(node))
			continue;

		// 与原版挑选一致：节点位置已有建筑（含已建成本类型）不重复建造
		if (!DualBuildingTemp::CanPickNode(pThis, i))
			continue;

		// 曾因反复放置失败被放弃的节点不再承接
		if (std::find(this->AIDefenseAbandonedNodes.begin(), this->AIDefenseAbandonedNodes.end(),
			node.MapCoords) != this->AIDefenseAbandonedNodes.end())
		{
			continue;
		}

		auto const pType = BuildingTypeClass::Array.GetItem(node.BuildingTypeIndex);

		if (pThis->CanBuild(pType, false, false) == CanBuildResult::Unbuildable)
			continue;

		auto const pBuilder = pType->FindFactory(true, true, true, pThis);
		if (!pBuilder || pBuilder->Deactivated || !pBuilder->HasPower)
			continue;

		if (pType->GetActualCost(pThis) > money)
		{
			this->AIDefenseNextPlaceFrame = Unsorted::CurrentFrame + 30;
			return;
		}

		// 记录任务并启动生产
		this->ProducingDefenseBuildingTypeIndex = node.BuildingTypeIndex;
		this->AIDefenseNodeCoords = node.MapCoords;
		this->AIDefensePlaceAttempts = 0;

		auto const pFactory = GameCreate<FactoryClass>();

		if (!pFactory->DemandProduction(pType, pThis, false))
		{
			GameDelete(pFactory);
			this->ProducingDefenseBuildingTypeIndex = -1;
			return;
		}

		this->AIDefenseFactory = pFactory;
		// The native placement handler routes Combat products through this slot.
		pThis->SetPrimaryFactory(pFactory, AbstractType::BuildingType, false, BuildCat::Combat);
		pFactory->Unsuspend(false);
		return;
	}
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
// call 0x42EB20，共 0xD 字节），换成本文件的挑选循环（硬性跳过防御产线节点）。
// 注意必须整体替换这三条指令——若只替换 call 本身，自定义路径返回时原版
// push 的 -1 参数会残留在栈上，导致函数尾声 ret 读到野地址（实测崩溃）。
DEFINE_HOOK(0x4FE401, HouseClass_AI_BuildingProduction_PickNode, 0xD)
{
	enum { AfterPick = 0x4FE40E };

	GET(HouseClass* const, pThis, EBP);

	R->EAX(DualBuildingTemp::PickExcludingDefense(pThis));
	return AfterPick;
}

// AI_Building 中"挑选的节点已在生产队列中则移除并重新挑选"的再挑选调用
// （push -1; lea ecx,[ebp+0x5700]; call 0x42EB20，共 0xD 字节），同样换成
// 过滤版挑选循环。
DEFINE_HOOK(0x4FE493, HouseClass_AI_BuildingProduction_RepickNode, 0xD)
{
	enum { AfterRepick = 0x4FE4A0 };

	GET(HouseClass* const, pThis, EBP);

	R->EAX(DualBuildingTemp::PickExcludingDefense(pThis));
	return AfterRepick;
}
