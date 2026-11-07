#include "Body.h"

#include <BuildingClass.h>
#include <BuildingTypeClass.h>
#include <FactoryClass.h>
#include <HouseClass.h>
#include <MapClass.h>

#include <Ext/Rules/Body.h>

#include <cmath>

// AI 主建筑产线放置失败降级
//
// 原版 [General]MaximumBuildingPlacementFailures（RulesClass+0xE48）只覆盖
// BuildingClass::KickOutUnit 内 FlushForPlacement==1（地基仅被己方单位临时
// 占用、可被赶走）这一种失败；AI 建造真正卡死的两类失败不计数：
//
//   1. GetCoordFromBaseCenter 找不到合法落点，或 FlushForPlacement==2
//      （覆盖物/地形物件/既有建筑阻挡）、Unlimbo 失败 → KickOutResult::Failed：
//      UpdateFactory（0x4500F0）弃单删厂后，同一节点进入
//      "重新生产→再放置失败"的无限循环，反复占用产线与现金；
//   2. FlushForPlacement==1（地基被己方单位临时占用）→ Busy：
//      工厂保留，每 PlacementDelay*900 帧重试一次，永不放弃。
//
// 本文件复用原版 BaseNode::Attempts 字段（随 BaseClass 存档）与原版
// "删除节点"语义（压缩 BaseNodes），把计数拓宽到上述两类失败；超过
// [AI]AI.BuildingPlacementFailureLimit 后删除该节点并按 Failed 路径清理
// 工厂，使建造队列跳过放不下的节点继续推进。原版窄口径（Flush==1 计数）
// 仍照常工作，两条计数互不冲突。
//
// 与双线建造（Hooks.AIDualBuildingProduction）的关系：Combat 节点由
// 0x4FEA3C 分流到独立防御产线，不经过 UpdateFactory 的 KickOut，本文件
// 的计数对防御线不可见；防御线仍使用其自带的 MaxPlaceAttempts。
//
// 与 Kaleidoscope 的关系：其 AI 施工（ConstructionKickOut）在放置成功后
// 把原生 Succeeded 包装为 Busy 返回（延迟完工）。此时产品已 Unlimbo 在图，
// 而原生 Busy（Flush==1）的产品必然仍在工厂内（InLimbo），故用产品是否
// 在图区分两者：在图 → 清零计数并交还原生重试计时等待完工，绝不超限
// 清理；在厂 → 按失败计数。不使用节点坐标匹配——那对地基（FoundationData）
// 不含 (0,0) 格的建筑会失配（建筑不占据 topLeftCell 逻辑格），曾导致延迟
// 完工被误计为失败直至超限清理活工厂（联机闪退）。全部判定来自共享模拟
// 状态，不使用本地随机，联机各机判定一致。

namespace
{
	// BaseClass::GetBaseNode（0x42EB20）：与 KickOutUnit 的 Building 分支
	// （0x444F49-0x444F62，push [pType+0xDF8] 即 BuildingTypeClass::ArrayIndex）
	// 完全一致的取节点方式——按产品建筑类型匹配节点，而非取第一个未完成节点。
	// 双线建造的 AIBaseConstructionUpdate 挑选器会跳过防御节点，若用 -1 取
	// "第一个未完成节点"，Attempts 会记到与产品无关的节点上；按类型匹配则
	// 始终与 KickOutUnit 的放置目标一致。
	BaseNodeClass* GetProductNode(HouseClass* pHouse, BuildingClass* pProduct)
	{
		if (!pProduct)
			return nullptr;
		return reinterpret_cast<BaseNodeClass* (__thiscall*)(BaseClass*, int)>(0x42EB20)
			(&pHouse->Base, pProduct->Type->ArrayIndex);
	}

	// 放置成功判定：原生 Building 产线的 Busy 只来自 FlushForPlacement==1
	// （地基被己方单位占位），该路径产品尚未 Unlimbo，必然 InLimbo；产品已
	// 在地图上的 Busy 只可能是 Kaleidoscope 的延迟完工（其 ConstructionKickOut
	// 把原生 Succeeded 包装为 Busy 返回）。因此用产品是否在图区分，不用节点
	// 坐标匹配——节点 MapCoords 处的 GetBuilding 对地基（FoundationData）不含
	// (0,0) 格的建筑会失配（建筑不占据 topLeftCell 逻辑格），曾导致延迟完工
	// 被误计为失败直至超限清理了活工厂（联机闪退）。
	// 产品是否已在地图上（延迟完工态）。InLimbo==false 即已 Unlimbo 成功。
	bool IsProductOnMap(BuildingClass* pProduct)
	{
		return pProduct && pProduct->IsAlive && !pProduct->InLimbo;
	}

	// 与原生 KickOutUnit 失败路径（0x44524C 一带）相同的节点删除语义：
	// 指针定位 + 前移压缩，保持其余节点顺序稳定。
	void RemoveBaseNode(HouseClass* pHouse, BaseNodeClass* pNode)
	{
		auto& nodes = pHouse->Base.BaseNodes;
		if (!nodes.Items || !pNode)
			return;
		const int index = static_cast<int>(pNode - nodes.Items);
		if (index < 0 || index >= nodes.Count)
			return;
		for (int i = index; i < nodes.Count - 1; ++i)
			nodes.Items[i] = nodes.Items[i + 1];
		--nodes.Count;
	}

	bool ShouldCountFailure(HouseClass* pHouse)
	{
		return pHouse && !pHouse->IsControlledByHuman()
			&& RulesExt::Global()->AIBuildingPlacementFailureLimit > 0;
	}
}

// UpdateFactory Failed 分支入口（KickOut 返回 Failed：找不到合法落点）。
// 原生两条指令 mov edx,[edi] 在处理结束后重放；随后原生照常弃单删厂。
DEFINE_HOOK(0x4501FF, BuildingClass_UpdateFactory_FailedPlacement, 0x2)
{
	GET(BuildingClass* const, pBuilder, ESI);
	auto* pHouse = pBuilder->Owner;
	if (!ShouldCountFailure(pHouse))
		return 0;
	// Failed 分支时工厂尚未弃单（原生 AbandonProduction 在 0x45022C），
	// factory->Object 仍是本次放置失败的产品。
	auto* pFactory = pBuilder->Factory;
	auto* pProduct = pFactory && pFactory->Object ? abstract_cast<BuildingClass*>(pFactory->Object) : nullptr;
	if (auto* pNode = GetProductNode(pHouse, pProduct)) {
		if (++pNode->Attempts > RulesExt::Global()->AIBuildingPlacementFailureLimit)
			RemoveBaseNode(pHouse, pNode);
	}
	return 0;
}

// UpdateFactory Busy 分支入口（KickOut 返回 Busy）。原指令
// mov eax,RulesClass::Instance 处理结束后重放。
DEFINE_HOOK(0x4501CB, BuildingClass_UpdateFactory_BusyPlacement, 0x5)
{
	GET(BuildingClass* const, pBuilder, ESI);
	auto* pHouse = pBuilder->Owner;
	auto* pFactory = pBuilder->Factory;
	if (!ShouldCountFailure(pHouse) || !pFactory)
		return 0;
	auto* pProduct = pFactory->Object ? abstract_cast<BuildingClass*>(pFactory->Object) : nullptr;
	if (IsProductOnMap(pProduct)) {
		// 产品已在地图上：Kaleidoscope 延迟完工，不是放置失败。清零计数并
		// 交还原生重试计时流程；无论节点是否存在都不做超限清理——此时工厂
		// 与产品是活对象，弃单删厂会产生悬空引用。
		if (auto* pNode = GetProductNode(pHouse, pProduct))
			pNode->Attempts = 0;
		return 0;
	}
	// 产品仍在工厂内（InLimbo）：真正的放置失败（Flush==1 己方单位占位）。
	auto* pNode = GetProductNode(pHouse, pProduct);
	if (!pNode)
		return 0;
	if (++pNode->Attempts > RulesExt::Global()->AIBuildingPlacementFailureLimit) {
		// 放弃该节点：删除节点后跳入原生 Failed 清理序列（0x450226：
		// AbandonProduction → 虚析构删除 FactoryClass → Factory=0 → 汇合点）。
		// 必须复用原生序列而不是手工复刻——第三方运行时组件（EC/LLF 等）
		// 可能替换对象的虚表，手搓 GameDelete 会绕过虚析构的额外清理逻辑，
		// 曾导致确定性野调用闪退（建筑构造/析构链上 EIP 落入垃圾代码）。
		// 进入点寄存器：esi=builder（保持）、ebx=0（函数入口即清零且全程
		// 不变），与原生语义一致。
		RemoveBaseNode(pHouse, pNode);
		return 0x450226;
	}
	return 0;
}

// ===== AI 兜底落位搜索 =====
//
// 原生 HouseClass::GetCoordFromBaseCenter（0x5060B0）的搜索空间受限于
// Base.Cells_24 环带候选（按评分排序），每个候选只沿"周围 BaseSpacer 最
// 空旷的方向"走 ≤3 步 ×2 轮（第二轮 AIBaseSpacing=0）。小地图上候选全被
// 阻挡、或选中的方向指向不可建区时返回空坐标，KickOutUnit 在两处放弃：
//
//   0x445025：节点已有坐标但 IsBaseSpacer 为假，原地重搜失败 → 直接 Failed；
//   0x445614：无节点或搜索坐标为空 → （升级建筑另有删节点流程）→ Failed。
//
// 本节在两处注入兜底搜索：围绕基地中心的稠密螺旋（固定次序、无随机，联机
// 确定性），判定与原生内层接受条件逐项一致（IsBuildableCell 地基外扩矩形 +
// CanCreateHere + |高度差|<3 + IsBaseSpacer）。找到后写回栈上坐标
// （var_114）与 EAX/EBP（/EBX），统一跳回 0x4451AA，由原生完成
// node->MapCoords 写回、FlushForPlacement 与 Unlimbo 全流程；未找到则重放
// 原指令走原生 Failed（进入上文的降级计数）。海军类型走原生 NearbyLocation
// 水面搜索，不适用本机制。

namespace
{
	using IsBuildableCellFn = bool(__thiscall*)(MapClass*, RectangleStruct*, int);
	using IsBaseSpacerFn = bool(__thiscall*)(HouseClass*, BuildingTypeClass*, CellStruct*);

	bool FallbackEnabled(HouseClass* pHouse)
	{
		return pHouse && !pHouse->IsControlledByHuman()
			&& RulesExt::Global()->AIPlacementFallbackSearch;
	}

	// 与 GetCoordFromBaseCenter 内层接受判定（0x5060B0 尾部）逐项一致。
	bool CanPlaceAt(HouseClass* pHouse, BuildingTypeClass* pType, const CellStruct& cell,
		int spacing, int baseLevel, int width, int height)
	{
		const RectangleStruct rect { cell.X - spacing, cell.Y - spacing,
			width + 2 * spacing, height + 2 * spacing };
		auto* pRect = const_cast<RectangleStruct*>(&rect);
		if (!reinterpret_cast<IsBuildableCellFn>(0x586780)(&MapClass::Instance, pRect, pHouse->ArrayIndex))
			return false;
		if (!pType->CanCreateHere(cell, pHouse))
			return false;
		auto* pCell = MapClass::Instance.TryGetCellAt(cell);
		if (!pCell || std::abs(baseLevel - static_cast<int>(pCell->Level)) >= 3)
			return false;
		return reinterpret_cast<IsBaseSpacerFn>(0x50B750)(pHouse, pType, &const_cast<CellStruct&>(cell));
	}

	bool FindFallbackCell(BuildingClass* pBuilder, HouseClass* pHouse, BuildingTypeClass* pType, CellStruct& out)
	{
		auto center = pHouse->Base.Center;
		if (center == CellStruct::Empty)
			center = pBuilder->GetMapCoords();
		auto* pCenterCell = MapClass::Instance.TryGetCellAt(center);
		if (!pCenterCell)
			return false;
		const int baseLevel = pCenterCell->Level;
		const int width = pType->GetFoundationWidth();
		const int height = pType->GetFoundationHeight(false);

		int spacing = RulesClass::Instance->AIBaseSpacing;
		if (pType->ProtectWithWall || pType->WantsExtraSpace)
			++spacing;
		const int maxRadius = Math::clamp(RulesExt::Global()->AIPlacementFallbackSearchRadius, 1, 32);

		// 两轮：先按原生间距（含 ProtectWithWall/WantsExtraSpace 的 +1）搜，
		// 全部失败后 spacing=0 贴紧重试（同原生第二轮的口径）。
		for (int pass = 0; pass < 2; ++pass) {
			const int sp = pass ? 0 : spacing;
			for (int r = 1; r <= maxRadius; ++r) {
				// 螺旋环固定次序（每边 2r 格、不含重复角）：上边左→右、
				// 右边上→下、下边右→左、左边下→上。
				for (int side = 0; side < 4; ++side) {
					for (int k = 0; k < 2 * r; ++k) {
						int dx = 0, dy = 0;
						switch (side) {
						case 0: dx = -r + k; dy = -r; break;
						case 1: dx = r; dy = -r + k; break;
						case 2: dx = r - k; dy = r; break;
						default: dx = -r; dy = r - k; break;
						}
						const CellStruct cell { static_cast<short>(center.X + dx),
							static_cast<short>(center.Y + dy) };
						if (CanPlaceAt(pHouse, pType, cell, sp, baseLevel, width, height)) {
							out = cell;
							return true;
						}
					}
				}
			}
			if (spacing == 0)
				break;
		}
		return false;
	}

	void FillCoordFromCell(CoordStruct& coord, const CellStruct& cell)
	{
		// 原生坐标口径：(cell<<8)+0x80；注意不能截断为 short——地图 255 格时
		// (255<<8)+0x80 = 0xFF80，截断会变负数破坏原生 >>8 还原。
		coord.X = (static_cast<int>(cell.X) << 8) + 0x80;
		coord.Y = (static_cast<int>(cell.Y) << 8) + 0x80;
		coord.Z = 0;
	}
}

// KickOutUnit 坐标为空路径（0x4451C6 → 0x445614，唯一跳入源，寄存器语义：
// esi=builder、edi=product、ebx=节点或空、eax/ebp=坐标 X/Y）。原指令
// mov edx,[esp+1Ch] / mov al,[edx+0E88h] 在放弃时重放，维持升级建筑的
// 原生删节点流程。
DEFINE_HOOK(0x445614, BuildingClass_KickOutUnit_PlacementFallback_Search, 0xA)
{
	GET(BuildingClass* const, pBuilder, ESI);
	auto* pHouse = pBuilder->Owner;
	if (!FallbackEnabled(pHouse))
		return 0;
	GET_STACK(BuildingTypeClass*, pType, 0x1C);
	if (!pType || pType->PowersUpBuilding[0] || pType->Naval)
		return 0;
	REF_STACK(CoordStruct, coord, STACK_OFFSET(0x140, -0x114));
	CellStruct found {};
	if (!FindFallbackCell(pBuilder, pHouse, pType, found))
		return 0;
	FillCoordFromCell(coord, found);
	R->EAX(coord.X);
	R->EBP(coord.Y);
	// 跳回 0x445150（而非 0x4451AA）：该块会从栈上 a2 重载节点指针、把
	// var_114 坐标写回 node->MapCoords（0x445177-0x44519F）后再进入检查。
	// 若直接跳 0x4451AA 会跳过节点写回，节点坐标丢失导致 AI 视该节点为
	// 未完成而重复生产同型建筑。节点为空时该块直接滑入 0x4451AA（EAX/EBP
	// 由本钩子已设置，语义不变）。
	return 0x445150;
}

// KickOutUnit 节点坐标失效重搜失败路径（0x445025 的 jz def_443CAD）。此路
// 径节点必存在（ebx=&node->MapCoords），需把 EBX 恢复为节点指针（栈上 a2）。
DEFINE_HOOK(0x445025, BuildingClass_KickOutUnit_PlacementFallback_Rescan, 0x6)
{
	GET(BuildingClass* const, pBuilder, ESI);
	auto* pHouse = pBuilder->Owner;
	if (!FallbackEnabled(pHouse))
		return 0;
	GET_STACK(BuildingTypeClass*, pType, 0x1C);
	if (!pType || pType->Naval)
		return 0;
	REF_STACK(CoordStruct, coord, STACK_OFFSET(0x140, -0x114));
	GET_STACK(BaseNodeClass*, pNode, 0x14);
	CellStruct found {};
	if (!FindFallbackCell(pBuilder, pHouse, pType, found))
		return 0;
	FillCoordFromCell(coord, found);
	R->EAX(coord.X);
	R->EBP(coord.Y);
	R->EBX(reinterpret_cast<int>(pNode));
	return 0x445150;
}
