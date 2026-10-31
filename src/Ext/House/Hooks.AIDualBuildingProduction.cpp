#include "Body.h"

#include <BuildingClass.h>
#include <BuildingTypeClass.h>
#include <FactoryClass.h>
#include <HouseClass.h>

#include <Ext/BuildingType/Body.h>

// AI 双建筑产线（主建筑 + 防御建筑并行）
//
// 原版 AI 的建筑生产只有单一产槽（HouseClass::ProducingBuildingTypeIndex，
// HouseClass::AI_Building @ 0x4FE3E0 负责"从基地节点队列挑一个待建节点 →
// 写入产槽"），主建筑与防御建筑（BuildCat::Combat）共用，因此 AI 无法像玩家
// 那样同时建造主要建筑物与防御建筑物。本文件镜像 Hooks.AINavalProduction
// （AI 载具/船只双产线）的思路：
//
//   1. HouseExt::ProducingDefenseBuildingTypeIndex 为防御建筑独立产槽；
//   2. AI_Building 入口（0x4FE3E9）在"两条产线都有任务"时才跳过挑选，主产线
//      忙时本轮节点挑选只允许命中防御建筑节点（临时遮蔽非 Combat 待建节点的
//      Placed 标记，挑选完成后恢复）；
//   3. 产槽写入点（0x4FE3E0 尾部 0x4FEA3C）按所选类型是否为 Combat 路由到
//      对应产槽；
//   4. 防御产线的生产在第一座建造厂上以独立 FactoryClass 进行（与原版 AI
//      工厂的创建方式一致：FactoryClass + DemandProduction），完成后的放置
//      经由 Primary_ForDefenses 发 Place 事件走 AutoPlaceBuilding，与玩家
//      放置流程一致；建造厂被打包/出售时与原版同样视为产线丢失，放弃生产。

namespace DualBuildingTemp
{
	// 0x42EB20 = BaseClass::按条件取第一个待建节点（-1 = 不限类型），返回节点指针
	inline BaseNodeClass* PickPendingNode(BaseClass* pBase)
	{
		return reinterpret_cast<BaseNodeClass*(__thiscall*)(BaseClass*, int)>(0x42EB20)(pBase, -1);
	}
}

// AI_Building 入口的"主产线是否空闲"检查（原版 cmp [ebp+0x564C],-1 / jne 尾部，共 0xD 字节）。
// 改为：两条产线都有任务才跳过；主产线忙而防御产线空闲时继续执行，但本轮节点
// 挑选只允许命中防御建筑（见 0x4FE409 钩子）。
DEFINE_HOOK(0x4FE3E9, HouseClass_AI_BuildingProduction_DualQueue, 0xD)
{
	enum { Continue = 0x4FE3F6, Done = 0x4FEA48 };

	GET(HouseClass* const, pThis, EBP);

	if (pThis->IsControlledByHuman())
	{
		// 玩家维持原版行为（含原版检查本身的语义）
		auto const pExt = HouseExt::Fetch(pThis);
		pExt->AIDualPickCombatOnly = false;

		if (pThis->ProducingBuildingTypeIndex != -1)
			return Done;
		return Continue;
	}

	auto const pExt = HouseExt::Fetch(pThis);

	// 防御产线工厂生命周期管理（每 AI tick）
	pExt->UpdateAIDefenseProduction();

	const bool normalBusy = pThis->ProducingBuildingTypeIndex != -1;
	const bool defenseBusy = pExt->ProducingDefenseBuildingTypeIndex != -1;
	pExt->AIDualPickCombatOnly = normalBusy && !defenseBusy;

	return (normalBusy && defenseBusy) ? Done : Continue;
}

// AI_Building 中的待建节点挑选调用（lea ecx,[ebp+0x5700]; push -1; call 0x42EB20，
// 共 0xD 字节）。整体接管：需要遮蔽时先遮蔽、调用引擎挑选、再精确还原。
// 注意必须整体替换这三条指令——若只替换 call 本身，自定义路径返回时原版
// push 的 -1 参数会残留在栈上，导致函数尾声 ret 读到野地址（实测崩溃）。
DEFINE_HOOK(0x4FE401, HouseClass_AI_BuildingProduction_PickNode, 0xD)
{
	enum { AfterPick = 0x4FE40E };

	GET(HouseClass* const, pThis, EBP);

	auto const pExt = HouseExt::Fetch(pThis);

	// 已在两条产线上生产的类型不得再次入选，否则同一节点会被重复排队；
	// 无任何需要遮蔽的情形时保持与原版完全一致
	const int normalIdx = pThis->ProducingBuildingTypeIndex;
	const int defenseIdx = pExt->ProducingDefenseBuildingTypeIndex;
	const bool needMask = pExt->AIDualPickCombatOnly || normalIdx != -1 || defenseIdx != -1;

	if (!needMask)
	{
		R->EAX(DualBuildingTemp::PickPendingNode(&pThis->Base));
		return AfterPick;
	}

	auto& nodes = pThis->Base.BaseNodes;
	std::vector<BaseNodeClass*> masked;

	for (auto& node : nodes)
	{
		const bool inProduction = (node.BuildingTypeIndex == normalIdx && normalIdx != -1)
			|| (node.BuildingTypeIndex == defenseIdx && defenseIdx != -1);
		const bool notCombat = node.BuildingTypeIndex >= 0
			&& node.BuildingTypeIndex < BuildingTypeClass::Array.Count
			&& BuildingTypeClass::Array.GetItem(node.BuildingTypeIndex)->BuildCat != BuildCat::Combat;

		if (!node.Placed && (inProduction || (pExt->AIDualPickCombatOnly && notCombat)))
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

// AI_Building 尾部：把选中的节点类型写入主产槽。改按 BuildCat 路由：
// Combat 且防御产槽空闲 → 写入防御产槽；否则照常写入主产槽。
DEFINE_HOOK(0x4FEA3C, HouseClass_AI_BuildingProduction_RouteSlot, 0xC)
{
	enum { Done = 0x4FEA48 };

	GET(HouseClass* const, pThis, EBP);
	GET_STACK(const BaseNodeClass* const, pNode, 0x10);

	auto const pExt = HouseExt::Fetch(pThis);
	const int typeIndex = pNode ? pNode->BuildingTypeIndex : -1;

	if (pThis->IsControlledByHuman())
	{
		pThis->ProducingBuildingTypeIndex = typeIndex;
		return Done;
	}

	if (typeIndex >= 0 && typeIndex < BuildingTypeClass::Array.Count
		&& BuildingTypeClass::Array.GetItem(typeIndex)->BuildCat == BuildCat::Combat
		&& pExt->ProducingDefenseBuildingTypeIndex == -1)
	{
		pExt->ProducingDefenseBuildingTypeIndex = typeIndex;
		return Done;
	}

	// 兜底：该类型已在防御产线上生产时不得重复写入主产槽
	if (typeIndex != -1 && typeIndex == pExt->ProducingDefenseBuildingTypeIndex)
		return Done;

	pThis->ProducingBuildingTypeIndex = typeIndex;
	return Done;
}

// 防御产线工厂生命周期：创建（挂在第一座建造厂上）、完成放置、清理。
void HouseExt::UpdateAIDefenseProduction()
{
	auto const pThis = this->OwnerObject();
	const int defIdx = this->ProducingDefenseBuildingTypeIndex;

	if (defIdx == -1)
	{
		this->ClearAIDefenseFactory();
		return;
	}

	// 建造厂全部消失（被卖/打包且无后续建造厂）：与原版一致，产线随工厂丢失
	if (pThis->ConYards.Count <= 0)
	{
		this->ClearAIDefenseFactory();
		this->ProducingDefenseBuildingTypeIndex = -1;
		return;
	}

	// 工厂的所有者失效（如唯一建造厂被变卖重建导致注册的 Factory 失效）→ 放弃
	if (this->AIDefenseFactory && !this->AIDefenseFactory->Owner)
	{
		this->ClearAIDefenseFactory();
		this->ProducingDefenseBuildingTypeIndex = -1;
		return;
	}

	if (!this->AIDefenseFactory)
	{
		auto const pConYard = pThis->ConYards.GetItem(0);

		if (!pConYard)
			return;

		auto const pType = BuildingTypeClass::Array.GetItem(defIdx);
		auto const pFactory = GameCreate<FactoryClass>();
		pFactory->DemandProduction(pType, pThis, false);
		this->AIDefenseFactory = pFactory;

		// 防御产线的 Place 事件经由 Primary_ForDefenses 路由到本工厂
		pThis->SetPrimaryFactory(pFactory, AbstractType::BuildingType, false, BuildCat::Combat);
		return;
	}

	// 生产完成 → 自动放置（发出的 Place 事件经 Primary_ForDefenses 命中本工厂，
	// 与玩家"工厂生产完成后放置"的处理一致）；放置成功后对象离开工厂
	if (this->AIDefenseFactory->Object && this->AIDefenseFactory->IsDone())
	{
		if (auto const pBuilding = abstract_cast<BuildingClass*>(this->AIDefenseFactory->Object))
			BuildingTypeExt::AutoPlaceBuilding(pBuilding);

		// 工厂对象已被取走（放置成功）→ 本轮完成
		if (!this->AIDefenseFactory->Object)
		{
			this->ClearAIDefenseFactory();
			this->ProducingDefenseBuildingTypeIndex = -1;
		}
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
