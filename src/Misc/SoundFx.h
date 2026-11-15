#pragma once
/* SoundFx.h - 按 sound.ini 条目名调用引擎全局音效。
 * 移植自 MagicShiba/MO_ExperienceEnhancements src/misc/SoundFx（YRpp 化改写）。
 * 名字由引擎解析，音量与声像直接传给 PlayGlobal；不碰模拟状态。
 * 循环音自持 AudioController；调用方检查存活并限频重起。
 */

#define SFX_LOOP_N 2   /* 循环通道数（0=雨，1=雪） */

/* 名字 -> 音效下标（带 16 项缓存；失败不缓存，允许音效表装载后重试）。 */
int Sfx_Index(const char* name);

/* 一次性播放。vol = 音量百分比（0~200，>100 削波）。成功返回 1。 */
int Sfx_Play(const char* name, int pan, int vol);

/* 通道 ch(0..SFX_LOOP_N-1) 起循环音。失败（含未绑定事件）返回 0，调用方限频重试。 */
int Sfx_LoopStart(int ch, const char* name, int pan, int vol);

/* 通道是否还在播放。事件结束/设备失效会自清并返回 0。 */
int Sfx_LoopAlive(int ch);

/* 停止并释放通道。 */
void Sfx_LoopStop(int ch);
