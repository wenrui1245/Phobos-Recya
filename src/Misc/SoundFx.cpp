/* SoundFx.cpp - 按 sound.ini 条目名调用引擎全局音效。
 * 移植自 MagicShiba/MO_ExperienceEnhancements src/misc/SoundFx.cpp（YRpp 化改写）。
 * VocClass::FindIndex / PlayGlobal / AudioController::GetEvent/End 与 MOEx 手工
 * 声明的 0x7514D0 / 0x750920 / 0x406130 / 0x405FD0 为同一引擎函数。
 */
#include "SoundFx.h"

#include <Windows.h>

#include <VocClass.h>
#include <Audio.h>

/* 音频设备是否存活。YRpp 未命名（MOEx ADDR_AUDIO_ENABLED）。 */
#define SFX_AUDIO_ENABLED 0x87E2A0u
/* 音频事件状态字段偏移：4 = 已结束，其余为待播/延时/在播（MOEx OFF_AUDIO_EVENT_STATE）。 */
#define SFX_EVENT_STATE_OFF 0x1Cu

#define SFX_CACHE_N 16

struct SfxCache
{
	char name[24];
	int idx;
};

static SfxCache s_Cache[SFX_CACHE_N];
static int s_CacheN;
static AudioController s_Loop[SFX_LOOP_N];
static int s_LoopActive[SFX_LOOP_N];   /* 通道持有控制器 */

int Sfx_Index(const char* name)
{
	if (!name || !*name) return -1;
	for (int i = 0; i < s_CacheN; i++)
		if (!lstrcmpiA(s_Cache[i].name, name)) return s_Cache[i].idx;

	/* 失败不缓存，允许音效表装载后重试。 */
	int i = VocClass::FindIndex(name);
	if (i < 0) return -1;
	if (s_CacheN < SFX_CACHE_N)
	{
		lstrcpynA(s_Cache[s_CacheN].name, name, (int)sizeof(s_Cache[0].name));
		s_Cache[s_CacheN].idx = i;
		s_CacheN++;
	}
	return i;
}

/* 音量百分比 -> PlayGlobal 的倍率。夹在 0~200：超过 2 倍只是削波，还会盖住
 * 游戏里其它声音。 */
static float Sfx_Vol(int vol)
{
	if (vol < 0) vol = 0;
	if (vol > 200) vol = 200;
	return (float)vol / 100.0f;
}

int Sfx_Play(const char* name, int pan, int vol)
{
	int idx = Sfx_Index(name);
	if (idx < 0) return 0;
	VocClass::PlayGlobal(idx, pan, Sfx_Vol(vol), nullptr);
	return 1;
}

int Sfx_LoopStart(int ch, const char* name, int pan, int vol)
{
	if (ch < 0 || ch >= SFX_LOOP_N) return 0;
	int idx = Sfx_Index(name);
	if (idx < 0) return 0;
	Sfx_LoopStop(ch);

	auto& ctrl = s_Loop[ch];
	ctrl = AudioController();   /* AudioIndex 默认指向 AudioIDXData::Instance */

	VocClass::PlayGlobal(idx, pan, Sfx_Vol(vol), &ctrl);
	/* 未绑定事件时由调用方限频重试。 */
	if (!ctrl.Event) return 0;

	s_LoopActive[ch] = 1;
	return 1;
}

int Sfx_LoopAlive(int ch)
{
	if (ch < 0 || ch >= SFX_LOOP_N || !s_LoopActive[ch]) return 0;
	/* 引擎核对类型与创建代次，不能把复用后的事件当成自己的继续播放。 */
	void* event = s_Loop[ch].GetEvent();
	if (event && *(unsigned*)((char*)event + SFX_EVENT_STATE_OFF) != 4)
		return 1;

	/* GetEvent 可能已清 Event；Stop 仍负责清通道占用与剩余控制器字段。 */
	Sfx_LoopStop(ch);
	return 0;
}

void Sfx_LoopStop(int ch)
{
	if (ch < 0 || ch >= SFX_LOOP_N || !s_LoopActive[ch]) return;
	s_LoopActive[ch] = 0;
	/* 设备关闭时不访问旧事件，只清本地句柄。 */
	if (*(volatile unsigned*)(size_t)SFX_AUDIO_ENABLED)
		s_Loop[ch].End();
	s_Loop[ch].Event = nullptr;
	s_Loop[ch].EventType = nullptr;
}
