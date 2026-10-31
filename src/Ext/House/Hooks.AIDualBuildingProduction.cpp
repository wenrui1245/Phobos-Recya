#include "Body.h"

#include <BuildingClass.h>
#include <BuildingTypeClass.h>
#include <EventClass.h>
#include <FactoryClass.h>
#include <HouseClass.h>
#include <ScenarioClass.h>

#include <Ext/BuildingType/Body.h>

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
//   1. 主产线忙（原版正在生产一座非防御建筑）时，管理器扫描基地节点，取第一
//      个待建的 Combat 节点作为防御产线任务（可负担性等检查与原版一致）；
//   2. 在第一座建造厂上以独立 FactoryClass 生产（与原版 AI 工厂的创建方式
//      一致：FactoryClass + DemandProduction）；
//   3. 生产完成后在该节点自己的坐标发 Place 事件放置（沿用基地规划的位置），
//      成功后把节点标记为已放置；建造厂被打包/出售时与原版同样视为产线丢失，
//      放弃生产；
//   4. 防御产线生产期间，原版挑选节点时跳过该类型（见 0x4FE401 钩子），
//      防止同一节点被主产线重复建造。

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
				// 本节点已完成使命，标记为已放置，防止之后被重复建造
				for (auto& node : pThis->Base.BaseNodes)
				{
					if (!node.Placed && node.BuildingTypeIndex == defIdx
						&& node.MapCoords == this->AIDefenseNodeCoords)
					{
						node.Placed = true;
						break;
					}
				}

				this->ClearAIDefenseFactory();
				this->ProducingDefenseBuildingTypeIndex = -1;
			}
		}

		return;
	}

	// —— 获取新任务 ——
	// 仅当主产线忙时启用并行（主产线空闲时原版自然会处理包括防御建筑在内的节点）
	if (pThis->ProducingBuildingTypeIndex == -1)
		return;

	if (pThis->ConYards.Count <= 0)
		return;

	if (Unsorted::CurrentFrame < this->AIDefenseNextPlaceFrame)
		return;

	const long money = pThis->Available_Money();

	for (auto& node : pThis->Base.BaseNodes)
	{
		if (node.Placed || node.BuildingTypeIndex < 0
			|| node.BuildingTypeIndex >= BuildingTypeClass::Array.Count
			|| node.BuildingTypeIndex == pThis->ProducingBuildingTypeIndex)
		{
			continue;
		}

		auto const pType = BuildingTypeClass::Array.GetItem(node.BuildingTypeIndex);

		if (pType->BuildCat != BuildCat::Combat)
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

// AI_Building（0x4FE3E0）中的待建节点挑选调用（lea ecx,[ebp+0x5700]; push -1;
// call 0x42EB20，共 0xD 字节）。防御产线正在生产的类型不参与原版挑选（防止
// 同一节点被主产线重复建造），其余行为与原版完全一致。
// 注意必须整体替换这三条指令——若只替换 call 本身，自定义路径返回时原版
// push 的 -1 参数会残留在栈上，导致函数尾声 ret 读到野地址（实测崩溃）。
DEFINE_HOOK(0x4FE401, HouseClass_AI_BuildingProduction_PickNode, 0xD)
{
	enum { AfterPick = 0x4FE40E };

	GET(HouseClass* const, pThis, EBP);

	auto const pExt = HouseExt::Fetch(pThis);
	const int defenseIdx = pExt->ProducingDefenseBuildingTypeIndex;

	if (defenseIdx == -1)
	{
		// 防御产线空闲，无遮蔽需要
		R->EAX(DualBuildingTemp::PickPendingNode(&pThis->Base));
		return AfterPick;
	}

	auto& nodes = pThis->Base.BaseNodes;
	std::vector<BaseNodeClass*> masked;

	for (auto& node : nodes)
	{
		if (!node.Placed && node.BuildingTypeIndex == defenseIdx)
		{
			node.Placed = true;
			masked.push_back(&node);
		}
	}

	auto const pNode = DualBuildingTemp::PickPendingNode(&pThis->Base);

	// 精确还原被遮蔽的节点（不得触碰本就 Placed 的节点）
	for (auto node : masked)
		node->Placed = false;

	R->EAX(pNode);
	return AfterPick;
}
