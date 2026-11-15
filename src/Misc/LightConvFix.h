#pragma once
/* LightConvFix.h - 光照变更后的每格 LightConvert 刷新（灯光抗锯齿）。
 * 移植自 MagicShiba/MO_ExperienceEnhancements src/misc/LightConvFix.cpp。
 *
 * 每格绘制用的 CellClass::LightConvert 是建格子时按当时的 AmbientCurrent / tint
 * 算好颜色的，引擎只在场景初始化与零散事件点重建它，而 UpdateCellLighting 只重算
 * 每格标量 —— 于是改过光照后没被事件碰到的格子仍用旧 convert 绘制，与邻居形成
 * 2:1 阶梯锯齿（原版同病）。本模块在两处光照重算调用点后按需全格重建。
 *
 * 只读模拟数据 + 改渲染资源，不碰模拟状态，联机安全。
 */

/* 光照修复是否开启（Phobos.ini [Phobos] LightConvert.AutoFix）。 */
int LightConv_Enabled();

/* 逐格重建 convert（标量重算 + 全数组 InitLightConvert），不看开关。
 * 读档/新开局就绪后由 AfterMap 钩补刷一次（pending 标志见 .cpp）。 */
void LightConv_RebuildCells();

/* 世界是否就绪（格子指针数组非空）。 */
int LightConv_WorldReady();

/* AfterMap 每帧调用：pending 时补刷一轮。 */
void LightConv_AfterMapTick();
