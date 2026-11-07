#include "Body.h"

#include <AircraftClass.h>
#include <AircraftTypeClass.h>
#include <BuildingClass.h>
#include <BuildingTypeClass.h>
#include <HouseClass.h>

#include <Ext/Rules/Body.h>

// AI 飞机生产停机坪门控
//
// 原生 HouseClass::AIAircraftProduction（0x4FF210）决策只看小队任务单缺口、
// CanBuild 与现金，完全不看停机坪容量：pad 满时照样生产新飞机，出厂后徘徊
// 找不到泊位（FindNearestDock 兜底悬停），白白占用产线与现金；而机场是否
// 补建由主建筑队列决定，与飞机生产互不感知。
//
// 本钩子在候选机型判定处（0x4FF41A，CanBuild 通过后的首条指令）加一道
// Dock 容量门控：机型的 Dock 列表（TechnoTypeClass::Dock，与
// AircraftClass::FindNearestDock 0x41A160 的泊位判据一致——本方建筑类型
// ∈ 机型的 Dock 列表）对应的本方存活建筑 NumberOfDocks 总量，减去本方
// 现存需要停靠且 Dock 列表与之重叠的飞机数，无剩余则跳过该机型
// （跳 0x4FF489 循环尾处理下一机型）。Dock 列表为空的机型不受限；全部
// 候选被门控时 ProducingAircraftTypeIndex 经原生汇合路径保持 -1，下一轮
// 再试。判定只读共享模拟状态，无随机，联机各机一致。
//
// [AI]AI.AircraftDockGate 控制开关（默认开启）。

namespace
{
	bool DockListContains(const TypeList<BuildingTypeClass*>& list, BuildingTypeClass* pType)
	{
		for (int i = 0; i < list.Count; ++i)
			if (list.Items[i] == pType)
				return true;
		return false;
	}

	bool HasFreeDockFor(HouseClass* pHouse, AircraftTypeClass* pAType)
	{
		const auto& dockList = pAType->Dock;
		if (dockList.Count <= 0)
			return true;

		int capacity = 0;
		auto& buildings = pHouse->Buildings;
		for (int i = 0; i < buildings.Count; ++i) {
			auto* pBld = buildings.Items[i];
			if (!pBld || !pBld->IsAlive || pBld->InLimbo || !pBld->Type)
				continue;
			if (DockListContains(dockList, pBld->Type))
				capacity += pBld->Type->NumberOfDocks;
		}
		if (capacity <= 0)
			return false;

		int occupied = 0;
		for (int i = 0; i < AircraftClass::Array.Count; ++i) {
			auto* pAir = AircraftClass::Array.Items[i];
			if (!pAir || !pAir->IsAlive || pAir->InLimbo || pAir->Owner != pHouse)
				continue;
			const auto& airDocks = pAir->Type->Dock;
			if (airDocks.Count <= 0)
				continue;
			// 只统计与本机型争用同一批 pad 的飞机（Dock 列表有交集）。
			for (int k = 0; k < airDocks.Count; ++k) {
				if (airDocks.Items[k] && DockListContains(dockList, airDocks.Items[k])) {
					++occupied;
					break;
				}
			}
		}
		return occupied < capacity;
	}
}

// 候选机型循环（CanBuild 通过后的首条指令 mov ecx,Array.Items，5 字节）。
// 寄存器语义：esi=机型下标、edi=所属方（下一轮循环 0x4FF403 重载）。
DEFINE_HOOK(0x4FF41A, HouseClass_AIAircraftProduction_DockGate, 0x5)
{
	if (!RulesExt::Global()->AIAircraftDockGate)
		return 0;
	GET(HouseClass* const, pHouse, EDI);
	GET(int, typeIndex, ESI);
	auto* pAType = AircraftTypeClass::Array.Items[typeIndex];
	if (!pAType || HasFreeDockFor(pHouse, pAType))
		return 0;
	return 0x4FF489;
}
