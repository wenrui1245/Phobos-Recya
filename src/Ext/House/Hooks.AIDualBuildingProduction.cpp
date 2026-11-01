#include "Body.h"

#include <BuildingClass.h>
#include <BuildingTypeClass.h>
#include <EventClass.h>
#include <FactoryClass.h>
#include <HouseClass.h>
#include <ScenarioClass.h>

#include <Ext/BuildingType/Body.h>

namespace DualBuildingTemp
{
	// 0x42EB20 = BaseClass::取第一个待建节点（-1 = 不限类型），返回节点指针
	inline BaseNodeClass* PickPendingNode(BaseClass* pBase)
	{
		return reinterpret_cast<BaseNodeClass*(__thiscall*)(BaseClass*, int)>(0x42EB20)(pBase, -1);
	}

	// 挂接式升级/插件类型（PowersUp 系列，需要宿主建筑）不归防御产线：它们
	// 依赖宿主存在才能放置，由原版主产线的升级放置流程处理
	inline bool IsHostedUpgradeType(BuildingTypeClass* pType)
	{
		return !BuildingTypeExt::Fetch(pType)->PowersUp_Buildings.empty();
	}

	// 供 AI_Building 的两处节点挑选调用（0x4FE401 / 0x4FE493）使用：临时遮蔽
	// 所有应由防御产线承接的待建节点，让原版挑选取到第一个非防御节点，随后
	// 精确还原（不得触碰本就 Placed 的节点）
	inline BaseNodeClass* PickExcludingDefense(HouseClass* pHouse)
	{
		auto& nodes = pHouse->Base.BaseNodes;
		std::vector<BaseNodeClass*> masked;

		for (auto& node : nodes)
		{
			if (node.Placed || node.BuildingTypeIndex < 0
				|| node.BuildingTypeIndex >= BuildingTypeClass::Array.Count)
			{
				continue;
			}

			auto const pType = BuildingTypeClass::Array.GetItem(node.BuildingTypeIndex);

			if (pType->BuildCat == BuildCat::Combat && !IsHostedUpgradeType(pType))
			{
				node.Placed = true;
				masked.push_back(&node);
			}
		}

		auto const pNode = PickPendingNode(&pHouse->Base);

		for (auto node : masked)
			node->Placed = false;

		return pNode;
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
//      "已入队则移除重挑"的再挑选）都会遮蔽这些节点，主产线永远选不到防御
//      建筑。挂接式升级/插件类型（PowersUp 系列）除外——它们仍由原版主产线
//      以宿主感知的方式放置；
//   2. 在第一座建造厂上以独立 FactoryClass 生产（与原版 AI 工厂的创建方式
//      一致：FactoryClass + DemandProduction）；
//   3. 生产完成后在该节点自己的坐标发 Place 事件放置（沿用基地规划的位置），
//      成功后把节点标记为已放置；反复放置失败（如目标位置被永久堵死）达
//      上限后放弃该节点，防止产线被卡死；建造厂被打包/出售时与原版同样视为
//      产线丢失，放弃生产。

// 防御产线工厂生命周期：获取任务（扫描基地节点）、创建（挂在第一座建造厂上）、
// 完成放置、清理。
void HouseExt::UpdateAIDefenseProduction()
{
	auto const pThis = this->OwnerObject();
	const int defIdx = this->ProducingDefenseBuildingTypeIndex;

	// —— 既有任务的生命周期 ——
	if (defIdx != -1)
	{
		// 建造厂全部消失（被卖/打包且无后续建造厂）：与原版一致，产线随工厂丢失
		if (pThis->ConYards.Count <= 0)
		{
			this->ClearAIDefenseFactory();
			this->ProducingDefenseBuildingTypeIndex = -1;
			return;
		}

		// 工厂所有者失效 → 放弃
		if (this->AIDefenseFactory && !this->AIDefenseFactory->Owner)
		{
			this->ClearAIDefenseFactory();
			this->ProducingDefenseBuildingTypeIndex = -1;
			return;
		}

		if (!this->AIDefenseFactory)
		{
			auto const pType = BuildingTypeClass::Array.GetItem(defIdx);
			auto const pFactory = GameCreate<FactoryClass>();

			if (!pFactory->DemandProduction(pType, pThis, false))
			{
				GameDelete(pFactory);
				this->ProducingDefenseBuildingTypeIndex = -1;
				return;
			}

			this->AIDefenseFactory = pFactory;
			// 防御产线的 Place 事件经由 Primary_ForDefenses 路由到本工厂
			pThis->SetPrimaryFactory(pFactory, AbstractType::BuildingType, false, BuildCat::Combat);
			return;
		}

		// 生产完成 → 在节点坐标发 Place 事件放置（带重试间隔，避免事件与
		// 疏散尝试每帧刷屏）
		if (this->AIDefenseFactory->Object && this->AIDefenseFactory->IsDone())
		{
			if (Unsorted::CurrentFrame < this->AIDefenseNextPlaceFrame)
				return;

			this->AIDefenseNextPlaceFrame = Unsorted::CurrentFrame + 30;

			if (auto const pType = abstract_cast<BuildingTypeClass*>(this->AIDefenseFactory->Object->GetType()))
			{
				const int place = pType->Naval ? 1 : 0;
				const auto arrayIndex = pType->GetArrayIndex();
				EventClass::OutList.Add(EventClass(pThis->ArrayIndex, EventType::Place,
					AbstractType::Building, arrayIndex, place, this->AIDefenseNodeCoords));
			}

			// 放置成功后对象会离开工厂
			if (!this->AIDefenseFactory->Object)
			{
				this->MarkAIDefenseNodePlaced();
				this->ClearAIDefenseFactory();
				this->ProducingDefenseBuildingTypeIndex = -1;
				this->AIDefensePlaceAttempts = 0;
			}
			else if (++this->AIDefensePlaceAttempts >= 8)
			{
				// 反复放置失败（位置被永久堵死、缺宿主等）：放弃本节点，防止
				// 产线被一个永远无法放置的任务卡死
				this->MarkAIDefenseNodePlaced();
				this->ClearAIDefenseFactory();
				this->ProducingDefenseBuildingTypeIndex = -1;
				this->AIDefensePlaceAttempts = 0;
			}
		}

		return;
	}

	// —— 获取新任务 ——
	// 所有防御建筑需求一律由本产线承接（原版挑选已通过 0x4FE401/0x4FE493 钩子
	// 排除 Combat 节点），不依赖主产线是否空闲；挂接式升级/插件类型不承接
	if (pThis->ConYards.Count <= 0)
		return;

	if (Unsorted::CurrentFrame < this->AIDefenseNextPlaceFrame)
		return;

	const long money = pThis->Available_Money();

	for (auto& node : pThis->Base.BaseNodes)
	{
		if (node.Placed || node.BuildingTypeIndex < 0
			|| node.BuildingTypeIndex >= BuildingTypeClass::Array.Count)
		{
			continue;
		}

		auto const pType = BuildingTypeClass::Array.GetItem(node.BuildingTypeIndex);

		if (pType->BuildCat != BuildCat::Combat || DualBuildingTemp::IsHostedUpgradeType(pType))
			continue;

		if (pThis->CanBuild(pType, false, false) == CanBuildResult::Unbuildable)
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
		// 防御产线的 Place 事件经由 Primary_ForDefenses 路由到本工厂
		pThis->SetPrimaryFactory(pFactory, AbstractType::BuildingType, false, BuildCat::Combat);
		return;
	}
}

void HouseExt::ClearAIDefenseFactory()
{
	if (!this->AIDefenseFactory)
		return;

	if (this->OwnerObject()->Primary_ForDefenses == this->AIDefenseFactory)
		this->OwnerObject()->SetPrimaryFactory(nullptr, AbstractType::BuildingType, false, BuildCat::Combat);

	if (this->AIDefenseFactory->Object)
		this->AIDefenseFactory->AbandonProduction();

	GameDelete(this->AIDefenseFactory);
	this->AIDefenseFactory = nullptr;
}

void HouseExt::MarkAIDefenseNodePlaced()
{
	auto const pThis = this->OwnerObject();
	const int defIdx = this->ProducingDefenseBuildingTypeIndex;

	if (defIdx == -1)
		return;

	for (auto& node : pThis->Base.BaseNodes)
	{
		if (!node.Placed && node.BuildingTypeIndex == defIdx
			&& node.MapCoords == this->AIDefenseNodeCoords)
		{
			node.Placed = true;
			break;
		}
	}
}

// AI_Building（0x4FE3E0）中的待建节点挑选调用（lea ecx,[ebp+0x5700]; push -1;
// call 0x42EB20，共 0xD 字节）。应由防御产线承接的节点不参与原版挑选，其余
// 行为与原版完全一致。
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
// （push -1; lea ecx,[ebp+0x5700]; call 0x42EB20，共 0xD 字节）。此处同样
// 需要遮蔽——此前漏挂导致防御节点经重挑漏回主产线（实测主产线仍在造防御）。
DEFINE_HOOK(0x4FE493, HouseClass_AI_BuildingProduction_RepickNode, 0xD)
{
	enum { AfterRepick = 0x4FE4A0 };

	GET(HouseClass* const, pThis, EBP);

	R->EAX(DualBuildingTemp::PickExcludingDefense(pThis));
	return AfterRepick;
}
