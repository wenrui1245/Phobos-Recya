/* LightConvFix.cpp - 光照变更后的每格 LightConvert 刷新（灯光抗锯齿）。
 * 移植自 MagicShiba/MO_ExperienceEnhancements src/misc/LightConvFix.cpp（GPL 项目内引用，来源无许可证）。
 * 原 LightConv_TriggerTint（按比例染色）属 LightingPreset 范围，不移植。
 */
#include "LightConvFix.h"

#include <Windows.h>
#include <cstddef>

#include <ScenarioClass.h>
#include <CellClass.h>
#include <MapClass.h>
#include <Utilities/Debug.h>
#include <Phobos.h>

/* ---------------- 引擎访问点 ----------------
 * YRpp 已命名：MapClass::Instance(0x87F7E8)、ScenarioClass::Instance(0xA8B230)、
 * AmbientCurrent、NormalLighting.Tint(R/G/B)、CellClass::InitLightConvert、
 * CellClass::LightConvert。
 * YRpp 未命名（裸偏移，来源 MOEx 实测）：
 *   MapClass+0x13C = 格子指针数组基址（512*512 槽，引擎越界判定同值）。 */

#define LF_OFF_BASE    0x13Cu   /* MapClass+0x13C = 格子指针数组基址 */

/* UpdateCellLighting (0x4AE4C0)：fastcall，ECX = MapClass 对象（0x87F7E8）。
 * YRpp 把它声明成无参静态，ECX 语义没有体现，为与 MOEx 行为逐字一致
 * 这里保留裸函数指针。 */
typedef void (__fastcall *LF_FnScen)(void* scen);

static LF_FnScen s_CellUpd = (LF_FnScen)(size_t)0x4AE4C0u;

/* 偏移锁定：字段对不上立刻编译失败，退回裸偏移。 */
static_assert(offsetof(ScenarioClass, AmbientCurrent) == 0x352Cu, "ScenarioClass::AmbientCurrent offset mismatch");
static_assert(offsetof(ScenarioClass, NormalLighting) == 0x3534u, "ScenarioClass::NormalLighting offset mismatch");
static_assert(offsetof(CellClass, LightConvert) == 0x34u, "CellClass::LightConvert offset mismatch");

static int s_Init, s_On = 1;
static int s_PendingRebuild = 0;   /* 读档/新开局就绪后补刷一轮 */
static int s_Busy;                 /* 保留 MOEx 语义位：池 Count 被临时压过时禁止重建 */
static unsigned s_LastAmb, s_LastR, s_LastG, s_LastB;
static unsigned s_LastTick, s_LastSig;  /* 刷新日志去重（规模 + 1 秒） */
static int s_HaveSig;

/* 注意：LightConvert 池的 Count 地址按 MOEx 实测 = 0x87F6A8
 * （YRpp 的 LightConvertClass::Array 定义在 0x87F698，Count 落点与实测差 4 字节，
 * 为保守起见这里沿用 MOEx 的裸地址；本文件只有日志规模去重用到 Count 概念，
 * 染色功能未移植，实际上不依赖这两个地址）。 */

static unsigned LF_CellBase()
{
	/* MapClass::Instance 是引用（对象即 0x87F7E8 处），取地址得到格子数组宿主。 */
	auto base = reinterpret_cast<unsigned char*>(&MapClass::Instance);
	if (!base)
		return 0;
	return *reinterpret_cast<unsigned*>(base + LF_OFF_BASE);
}

int LightConv_WorldReady()
{
	return LF_CellBase() != 0;
}

static void LF_EnsureInit()
{
	if (s_Init) return;
	s_Init = 1;
	s_On = Phobos::Config::LightConvert_AutoFix ? 1 : 0;
	Debug::Log(s_On ? "lightingfix: ON\n" : "lightingfix: OFF (engine original, no auto refresh)\n");
}

int LightConv_Enabled()
{
	LF_EnsureInit();
	return s_On;
}

/* 光照签名：AmbientCurrent + tint 三色，变了才值得全格扫。 */
static void LF_StoreSig()
{
	auto const pScen = ScenarioClass::Instance;
	if (!pScen) return;
	s_LastAmb = (unsigned)pScen->AmbientCurrent;
	s_LastR = (unsigned)pScen->NormalLighting.Tint.Red;
	s_LastG = (unsigned)pScen->NormalLighting.Tint.Green;
	s_LastB = (unsigned)pScen->NormalLighting.Tint.Blue;
	s_HaveSig = 1;
}

static int LF_SigChanged()
{
	auto const pScen = ScenarioClass::Instance;
	if (!pScen) return 0;
	if (s_HaveSig
		&& (unsigned)pScen->AmbientCurrent == s_LastAmb
		&& (unsigned)pScen->NormalLighting.Tint.Red == s_LastR
		&& (unsigned)pScen->NormalLighting.Tint.Green == s_LastG
		&& (unsigned)pScen->NormalLighting.Tint.Blue == s_LastB)
		return 0;
	return 1;
}

/* 全格重建：InitLightConvert(null) 内部走 CalculateLightSourceLighting
 * (0x484180) 读 AmbientCurrent 与 tint，每格算出自己那份颜色。 */
static void LF_RefreshAll(int force)
{
	unsigned base, slot, old, cells = 0, rebuilt = 0, nSeen = 0, i;
	unsigned tick, sig;
	void* c;
	void* conv;
	void* seen[512]; /* 地形用到的不同 convert 件数上限 */

	base = LF_CellBase();
	if (!base) return;

	for (slot = 0; slot < 512u * 512u; slot++)
	{
		c = *reinterpret_cast<void**>(base + slot * 4);
		if (!c) continue;
		cells++;
		old = (unsigned)(size_t)*reinterpret_cast<void**>((unsigned char*)c + 0x34);
		reinterpret_cast<CellClass*>(c)->InitLightConvert(nullptr, 0x10000, 0, 1000, 1000, 1000);
		conv = *reinterpret_cast<void**>((unsigned char*)c + 0x34);
		if ((unsigned)(size_t)conv != old)
			rebuilt++;
		if (!conv) continue;
		for (i = 0; i < nSeen; i++)
			if (seen[i] == conv) break;
		if (i == nSeen && nSeen < 512) seen[nSeen++] = conv;
	}

	sig = cells ^ (rebuilt << 3);
	tick = GetTickCount();
	if (!force && sig == s_LastSig && tick - s_LastTick < 1000) return;
	s_LastSig = sig;
	s_LastTick = tick;

	auto const pScen = ScenarioClass::Instance;
	Debug::Log("lightconv: cells=%u rebuilt=%u convs=%u amb=%d\n",
		cells, rebuilt, nSeen, pScen ? pScen->AmbientCurrent : 0);
}

/* ---------------- 钩子体（由 .cpp 底部的 DEFINE_HOOK 调用） ---------------- */

/* 两处引擎调用点（call UpdateCellLighting，钩子替换掉的就是这条 call）的公共体：
 * 自己调原函数完成标量重算，环境光/tint 签名变了才全格重建 convert，返回被覆盖
 * call 的下一条。
 *
 * 关闭开关也必须自己调原函数后返回 after，不能返回 0 交给 Syringe 重放：站点
 * 这 5 字节就是 `E8 rel32`，桩池副本不重定位 rel32，换地址执行后目标平移成野
 * 地址（MOEx notes/崩溃定位.md 的教训）。 */
static DWORD LF_HookBody(DWORD after)
{
	LF_EnsureInit();
	s_CellUpd(reinterpret_cast<void*>(0x87F7E8u)); /* MapClass::Instance */
	if (!s_On || s_Busy) return after;
	if (LF_SigChanged()) LF_RefreshAll(0);
	LF_StoreSig();
	return after;
}

DWORD LightConv_OnRecalc()
{
	return LF_HookBody(0x53ADE0u); /* 0x53ADDB + 5 */
}

DWORD LightConv_OnStep()
{
	return LF_HookBody(0x55B4CBu); /* 0x55B4C6 + 5 */
}

void LightConv_NotifyLoadReady()
{
	/* 读档就绪 / 新开局：签名可能已"对齐"，强制补刷一轮。 */
	s_PendingRebuild = 1;
}

void LightConv_RebuildCells()
{
	LF_EnsureInit();
	s_CellUpd(reinterpret_cast<void*>(0x87F7E8u));
	LF_RefreshAll(1);
	LF_StoreSig();
}

void LightConv_AfterMapTick()
{
	if (s_PendingRebuild)
	{
		s_PendingRebuild = 0;
		LightConv_RebuildCells();
	}
}

/* ---------------- Syringe 钩子 ---------------- */

#include <Helpers/Macro.h>

/* 0x53ADDB(5B): RecalcLighting 内 call UpdateCellLighting。 */
DEFINE_HOOK(0x53ADDB, LightConv_Recalc, 0x5)
{
	return LightConv_OnRecalc();
}

/* 0x55B4C6(5B): 环境光渐变步进内 call UpdateCellLighting。 */
DEFINE_HOOK(0x55B4C6, LightConv_Step, 0x5)
{
	return LightConv_OnStep();
}

/* 引擎把"世界已就绪"置位的两条指令（新开局/读档各一条，站点与 MOEx LightingPreset 同）：
 * 只置标志，AfterMap 钩里补刷。 */
DEFINE_HOOK(0x67E6C4, LightConv_LoadReady, 0x6)
{
	LightConv_NotifyLoadReady();
	return 0;
}

DEFINE_HOOK(0x683E75, LightConv_ScenStart, 0x7)
{
	LightConv_NotifyLoadReady();
	return 0;
}
