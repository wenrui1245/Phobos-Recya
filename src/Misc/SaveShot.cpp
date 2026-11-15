/* SaveShot.cpp - 存档截图管理（核心版）。
 * 移植自 MagicShiba/MO_ExperienceEnhancements src/gui/SaveShot.cpp + Shot.cpp（核心裁剪）。
 * 与原版的差异：
 *  - 配置走 Phobos.ini [Phobos.SaveShot]，由 Weather_AfterMap 之外零引擎钩子复用；
 *  - 预览不重排对话框控件，固定叠加在面板右缘内侧；
 *  - 框标题/占位文字用引擎 DrawText（对话框阶段字体已初始化），不带矢量字引擎；
 *  - 不做读档接管（s_Loading 状态机随对话框重做一并裁掉）。
 */
#include "SaveShot.h"

#include <Windows.h>

#include <CCINIClass.h>
#include <Surface.h>
#include <Drawing.h>
#include <BitFont.h>
#include <Utilities/Debug.h>
#include <Phobos.h>

#include "PixelRender.h"

/* ---------------- 常量 ---------------- */

#define SS_TICK_MS   1000      /* 兜底取图间隔 */
#define SS_DEF_W     320       /* 缩略图宽（高度按视口比例） */
#define SS_MIN_W     96
#define SS_MAX_W     640
#define SS_DEF_PREV  200       /* 预览框宽 (px) */
#define SS_MIN_PREV  120

/* 模板原始尺寸 (DLU) 与换算失败时的兜底像素尺寸 */
#define SS_TPL_CX    533
#define SS_TPL_CY    369
#define SS_TPL_PX_W  800
#define SS_TPL_PX_H  600

/* FileEntryClass: Name 是 wchar_t[0x80], Filename 落在 +0x100（MOEx 实测偏移）。 */
#define SS_FE_FILE   0x100

/* 对话框控件 id (读档模板 183 / 存档模板 692) */
#define SS_ID_LIST_LOAD 1317
#define SS_ID_LIST_SAVE 1319

#define SS_NAME_MAX  64

/* 场景已开局标志。YRpp 未命名（MOEx ADDR_SCEN_STARTED）。 */
#define SS_SCEN_STARTED 0xA8ED5Cu

/* ---------------- 状态 ---------------- */

static int s_Init = 0;
static int s_Enable = 1;
static int s_W = SS_DEF_W;
static int s_PrevW = SS_DEF_PREV;       /* 预览框宽 (px) */

static unsigned short* s_Pix = NULL;    /* 缓存: 当前局面的缩略图 */
static int s_PixW = 0, s_PixH = 0;
static int s_Aspect = 750;              /* 视口高宽比（千分比，默认 3:4） */
static unsigned s_LastTick = 0;

static char s_SaveDir[MAX_PATH];        /* 存档目录 (= 上次存档所在目录) */
static char s_BaseName[SS_NAME_MAX];    /* 本次存档主名 (无扩展名) */

/* 对话框状态 */
static void* s_Dlg = NULL;
static int   s_DlgList = 0;
static int   s_PanelW = 0, s_PanelH = 0, s_Px = 0, s_Py = 0;
static unsigned s_LastScan = 0;

/* 一张缩放好的 16 位图 (px 为空 = 没图) */
typedef struct { unsigned short* px; int w, h; } SS_Pic;

static SS_Pic s_PicA;                   /* 当前画面 */
static SS_Pic s_PicB;                   /* 选中存档的截图 */
static char   s_ThumbName[SS_NAME_MAX]; /* s_PicB 缓存的是哪一档 (只做比较) */

/* ---------------- 小工具 ---------------- */

static void SS_Log(const char* a, const char* b)
{
	Debug::Log("saveshot: %s%s\n", a, b ? b : "");
}

/* 拆目录与主名 (主名去掉扩展名)。dir/base 都可传 NULL。 */
static void SS_SplitPath(const char* path, char* dir, char* base)
{
	int n = lstrlenA(path), cut = -1, i, k;

	for (i = n - 1; i >= 0; i--)
		if (path[i] == '\\' || path[i] == '/') { cut = i; break; }
	if (dir)
	{
		for (k = 0; k < cut && k < MAX_PATH - 1; k++) dir[k] = path[k];
		dir[k] = 0;
	}
	if (base)
	{
		for (k = 0, i = cut + 1; path[i] && path[i] != '.' && k < SS_NAME_MAX - 1; )
			base[k++] = path[i++];
		base[k] = 0;
	}
}

/* dir (空则用存档目录) 下的 Thumbs\<base>.bmp。 */
static void SS_ThumbPath(char* out, unsigned cap, const char* dir, const char* base)
{
	_snprintf_s(out, cap, _TRUNCATE, "%s\\Thumbs\\%s.bmp", dir[0] ? dir : s_SaveDir, base);
}

static void SS_DefaultDir()
{
	if (s_SaveDir[0]) return;
	auto gd = Phobos::GetProgramDirectory().string();
	if (gd.empty()) return;
	_snprintf_s(s_SaveDir, sizeof(s_SaveDir), _TRUNCATE, "%s\\Saved Games", gd.c_str());
}

/* 选中项的 FileEntry +0x100: 引擎存的文件名 (可能是相对路径, 也可能只有名字)。 */
static int SS_SelectedFile(HWND list, char* out)
{
	int cur, k;
	unsigned fe;
	const char* nm;

	out[0] = 0;
	if (!list) return 0;
	cur = (int)SendMessageA(list, LB_GETCURSEL, 0, 0);
	if (cur < 0) return 0;
	fe = (unsigned)(size_t)SendMessageA(list, LB_GETITEMDATA, (WPARAM)cur, 0);
	if (!fe || !PR_IsPlausiblePtr((const void*)(size_t)fe)) return 0;
	nm = (const char*)(size_t)(fe + SS_FE_FILE);
	for (k = 0; k < 31 && nm[k]; k++)
		if ((unsigned char)nm[k] < 0x20 || (unsigned char)nm[k] > 0x7E) return 0;
	if (k <= 0) return 0;
	memcpy(out, nm, (size_t)k);
	out[k] = 0;
	return 1;
}

/* ---------------- 像素 (跟随引擎位宽: 565 / 555) ---------------- */

static unsigned SS_R(unsigned short c) { return (c >> (PR_GBits == 6 ? 11 : 10)) & 31u; }
static unsigned SS_G(unsigned short c) { return (c >> 5) & ((PR_GBits == 6) ? 63u : 31u); }
static unsigned SS_B(unsigned short c) { return c & 31u; }

static unsigned short SS_Pack(unsigned r, unsigned g, unsigned b)
{
	unsigned gmax = (PR_GBits == 6) ? 63u : 31u;
	if (r > 31u) r = 31u;
	if (g > gmax) g = gmax;
	if (b > 31u) b = 31u;
	return (unsigned short)((r << (PR_GBits == 6 ? 11 : 10)) | (g << 5) | b);
}

/* 已 Lock 的 16 位面: 填一块 (自动裁到面内) / 画 t 像素矩形边框。 */
static void SS_FillPx(unsigned char* base, int pitch, int W, int H,
	int x, int y, int w, int h, unsigned short c)
{
	int i;

	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > W) w = W - x;
	if (y + h > H) h = H - y;
	if (w <= 0 || h <= 0) return;
	for (i = 0; i < h; i++)
	{
		unsigned short* r = (unsigned short*)(base + (size_t)(y + i) * pitch) + x;
		int k = w;
		while (k--) *r++ = c;
	}
}

static void SS_Frame(unsigned char* base, int pitch, int W, int H,
	int x, int y, int w, int h, int t, unsigned short c)
{
	SS_FillPx(base, pitch, W, H, x, y, w, t, c);
	SS_FillPx(base, pitch, W, H, x, y + h - t, w, t, c);
	SS_FillPx(base, pitch, W, H, x, y, t, h, c);
	SS_FillPx(base, pitch, W, H, x + w - t, y, t, h, c);
}

/* ---------------- 图像 ----------------
 * 两个框里放的都是 16 位小图。尺寸一变就重分配, 不做容量复用 (设置界面, 不在乎)。 */

static void SS_PicFree(SS_Pic* p)
{
	if (p->px) VirtualFree(p->px, 0, MEM_RELEASE);
	p->px = NULL;
	p->w = p->h = 0;
}

/* 缩放 (宽高由调用方按比例算好): 缩小时对目标像素覆盖的源矩形取平均, 放大时
 * 落到最近点。src 为 16 位, srcPitch 字节/行; flip=1 表示源是自底向上 (BMP)。 */
static void SS_Shrink(unsigned short* dst, int dw, int dh,
	const unsigned char* src, int sw, int sh, int srcPitch, int flip)
{
	int x, y;

	for (y = 0; y < dh; y++)
	{
		int sy0 = (int)((long long)y * sh / dh);
		int sy1 = (int)((long long)(y + 1) * sh / dh);
		if (sy1 <= sy0) sy1 = sy0 + 1;
		for (x = 0; x < dw; x++)
		{
			int sx0 = (int)((long long)x * sw / dw);
			int sx1 = (int)((long long)(x + 1) * sw / dw);
			unsigned ar = 0, ag = 0, ab = 0, n = 0;
			int xx, yy;

			if (sx1 <= sx0) sx1 = sx0 + 1;
			for (yy = sy0; yy < sy1; yy++)
			{
				const unsigned short* row = (const unsigned short*)
					(src + (size_t)(flip ? (sh - 1 - yy) : yy) * srcPitch);
				for (xx = sx0; xx < sx1; xx++)
				{
					unsigned short c = row[xx];
					ar += SS_R(c); ag += SS_G(c); ab += SS_B(c); n++;
				}
			}
			dst[(size_t)y * dw + x] = n ? SS_Pack(ar / n, ag / n, ab / n) : 0;
		}
	}
}

/* 等比缩进 bw x bh 的框 (只填满, 不拉伸)。 */
static int SS_PicFit(SS_Pic* p, const unsigned char* src, int sw, int sh, int srcPitch,
	int flip, int bw, int bh)
{
	int ow, oh;

	SS_PicFree(p);
	if (!src || sw <= 0 || sh <= 0 || bw < 8 || bh < 8) return 0;

	ow = bw;
	oh = (int)((long long)bw * sh / sw);
	if (oh > bh) { oh = bh; ow = (int)((long long)bh * sw / sh); }
	if (ow < 1) ow = 1;
	if (oh < 1) oh = 1;

	p->px = (unsigned short*)VirtualAlloc(NULL, (size_t)ow * oh * 2u,
		MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!p->px) return 0;
	SS_Shrink(p->px, ow, oh, src, sw, sh, srcPitch, flip);
	p->w = ow;
	p->h = oh;
	return 1;
}

#pragma pack(push, 1)
typedef struct {
	unsigned short magic;
	unsigned int   filesz;
	unsigned short r1, r2;
	unsigned int   off;
	unsigned int   hdrSize;
	int            w;
	int            h;
	unsigned short planes;
	unsigned short bits;
	unsigned int   compress;
	unsigned int   imgSize;
	int            hres, vres;
	unsigned int   nCol, nImp;
} SS_BmpHdr;
#pragma pack(pop)

/* 读 BMP (只认我们自己写的 16 位) 缩放进框。 */
static int SS_PicLoadBmp(SS_Pic* p, const char* path, int bw, int bh)
{
	HANDLE f;
	DWORD got = 0, sz;
	unsigned char* d;
	SS_BmpHdr* hd;
	int sh, ok = 0;

	f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE) return 0;
	sz = GetFileSize(f, NULL);
	d = (sz >= sizeof(SS_BmpHdr) && sz <= (16u << 20))
		? (unsigned char*)VirtualAlloc(NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
		: NULL;
	if (d && ReadFile(f, d, sz, &got, NULL) && got == sz)
	{
		hd = (SS_BmpHdr*)d;
		sh = (hd->h < 0) ? -hd->h : hd->h;
		if (hd->magic == 0x4D42 && hd->hdrSize >= 40 && hd->bits == 16 &&
			hd->w > 0 && hd->w <= 4096 && sh > 0 && sh <= 4096 &&
			hd->off < sz && (size_t)hd->off + (size_t)hd->w * 2 * (size_t)sh <= sz)
			ok = SS_PicFit(p, d + hd->off, hd->w, sh, hd->w * 2, hd->h > 0, bw, bh);
	}
	if (d) VirtualFree(d, 0, MEM_RELEASE);
	CloseHandle(f);
	return ok;
}

/* 丢掉下框缓存, 连同"缓存的是哪一档"的记认。 */
static void SS_ThumbReset()
{
	SS_PicFree(&s_PicB);
	s_ThumbName[0] = 0;
}

/* 写 16 位 BMP（移植自 MOEx Shot.cpp Shot_WriteBmp565）。
 * 掩码跟引擎实际像素布局走: PR_GBits==6 ⇒ 565, 否则 555。 */
static int SS_WriteBmp(const char* path, const unsigned short* pix, int w, int h)
{
#pragma pack(push, 1)
	struct {
		unsigned char magic[2];
		unsigned int filesz;
		unsigned short creator1, creator2;
		unsigned int off;
		unsigned int hdrSize;
		int width, height;
		unsigned short planes;
		unsigned short bits;
		unsigned int compress;   /* BI_BITFIELDS = 3 */
		unsigned int imgSize;
		int hres, vres;
		unsigned int nColors, nImpColors;
		unsigned int maskR, maskG, maskB;
	} hd;
#pragma pack(pop)
	HANDLE f;
	DWORD put = 0;
	unsigned sz = (unsigned)((size_t)w * (size_t)h * 2u);

	f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE) return 0;

	hd.magic[0] = 'B'; hd.magic[1] = 'M';
	hd.creator1 = 0; hd.creator2 = 0;
	hd.hdrSize = 40u;
	hd.width = w;
	hd.height = -h;
	hd.planes = 1;
	hd.bits = 16;
	hd.compress = 3u;
	hd.imgSize = sz;
	hd.hres = 4000;
	hd.vres = 4000;
	hd.nColors = 0; hd.nImpColors = 0;
	if (PR_GBits == 6) { hd.maskR = 0xF800u; hd.maskG = 0x07E0u; hd.maskB = 0x001Fu; }
	else               { hd.maskR = 0x7C00u; hd.maskG = 0x03E0u; hd.maskB = 0x001Fu; }
	hd.off = (unsigned)sizeof(hd);
	hd.filesz = hd.off + sz;

	if (!WriteFile(f, &hd, (DWORD)sizeof(hd), &put, NULL) ||
		put != (DWORD)sizeof(hd) ||
		!WriteFile(f, pix, sz, &put, NULL) || put != sz)
	{
		CloseHandle(f);
		DeleteFileA(path);
		return 0;
	}
	CloseHandle(f);
	return 1;
}

/* ---------------- 初始化与配置 ---------------- */

void SaveShot_LoadConfig(CCINIClass& ini)
{
	int w;

	s_Enable = ini.ReadBool("Phobos.SaveShot", "Enable", true) ? 1 : 0;
	w = ini.ReadInteger("Phobos.SaveShot", "Width", SS_DEF_W);
	s_PrevW = ini.ReadInteger("Phobos.SaveShot", "PreviewWidth", SS_DEF_PREV);
	if (w < SS_MIN_W) w = SS_MIN_W;
	if (w > SS_MAX_W) w = SS_MAX_W;
	if (s_PrevW < SS_MIN_PREV) s_PrevW = SS_MIN_PREV;
	if (s_PrevW > 420) s_PrevW = 420;
	s_W = w;
	s_Init = 1;
	SS_DefaultDir();
	Debug::Log(s_Enable ? "saveshot: on\n" : "saveshot: off\n");
}

/* ---------------- 取图 ---------------- */

/* 视口原图 = Composite 上的 ViewBounds（Phobos 无缩放档，等价 MOEx SB_RawView
 * 的非缩放分支）。返回 0 = 暂不可用。 */
static int SS_RawView(DSurface** surf, int* vx, int* vy, int* vw, int* vh)
{
	auto const r = DSurface::ViewBounds;
	if (r.Width < 16 || r.Height < 16) return 0;
	auto s = DSurface::Composite;
	if (!s || !PR_IsPlausiblePtr(s)) return 0;
	if (s->GetBytesPerPixel() != 2 || s->GetWidth() < 16 || s->GetHeight() < 16) return 0;
	int x = r.X, y = r.Y, w = r.Width, h = r.Height;
	/* 视口恒在面内, 只夹右下（保留 MOEx Sb_ClipR 的右下夹取语义）。 */
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x + w > s->GetWidth()) w = s->GetWidth() - x;
	if (y + h > s->GetHeight()) h = s->GetHeight() - y;
	if (w < 16 || h < 16) return 0;
	*surf = s;
	*vx = x; *vy = y; *vw = w; *vh = h;
	return 1;
}

/* 盒式降采样到 s_W 宽。 */
static void SS_Capture()
{
	DSurface* surf = NULL;
	int vx, vy, vw, vh, dw, dh, pitch;
	unsigned char* base;

	if (!SS_RawView(&surf, &vx, &vy, &vw, &vh)) return;
	pitch = surf->GetPitch();
	if (pitch < surf->GetWidth() * 2) return;

	dw = s_W; if (dw > vw) dw = vw;
	dh = (int)((long long)dw * vh / vw);
	if (dh < 1) dh = 1;
	if (dh > dw * 2) dh = dw * 2;          /* 极端宽高比钳一下 */

	s_Aspect = (int)(((long long)dh * 1000) / dw);
	if (s_Aspect < 250) s_Aspect = 250;
	if (s_Aspect > 2000) s_Aspect = 2000;

	if (!s_Pix || s_PixW != dw || s_PixH != dh)
	{
		if (s_Pix) VirtualFree(s_Pix, 0, MEM_RELEASE);
		s_PixW = s_PixH = 0;
		s_Pix = (unsigned short*)VirtualAlloc(NULL, (size_t)dw * dh * 2u,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!s_Pix) return;
		s_PixW = dw;
		s_PixH = dh;
	}

	base = (unsigned char*)surf->Lock(0, 0);
	if (!base) return;
	SS_Shrink(s_Pix, dw, dh, base + (size_t)vy * pitch + (size_t)vx * 2, vw, vh, pitch, 0);
	surf->Unlock();
	s_LastTick = GetTickCount();
}

/* 对话框建立那一刻的战场。调用点是 0x4F1140（引擎建好对话框、ShowWindow 之前）
 * —— 此时视口里还是刚渲染完的画面, 菜单一个像素都没画上去。之后再抓就晚了:
 * 菜单/对话框是画在 Composite 上的, 会正好盖住视口那块。 */
void SaveShot_CaptureNow()
{
	if (!s_Init || !s_Enable) return;
	if (!*(volatile unsigned*)SS_SCEN_STARTED) return;   /* 未开局 */
	/* 已经开着对话框时又建了子对话框: 画面里已经有菜单像素, 再抓就是拿菜单当
	 * "当前画面"。只在"从无到有"那次抓。 */
	if (s_Dlg && IsWindow((HWND)s_Dlg)) return;
	if (!s_Pix)
		SS_Capture();
}

/* 只在本局还没抓过图时补一张兜底（Weather_AfterMap 每帧调）。 */
void SaveShot_Tick()
{
	if (!s_Init || !s_Enable) return;
	if (!*(volatile unsigned*)SS_SCEN_STARTED) return;
	if (s_Dlg && IsWindow((HWND)s_Dlg)) return;             /* 存/读档界面开着 */
	/* 正常路径由 SaveShot_CaptureNow 抓; 这里只补最开始那帧兜底
	 * (没有它, 在"还没弹过任何对话框"之前存档就无图可写)。 */
	if (s_Pix) return;
	if (GetTickCount() - s_LastTick < SS_TICK_MS) return;
	SS_Capture();
}

/* ---------------- 落盘 ---------------- */

/* 0x67CF42 = 存储创建调用点, 参数是引擎拼好的完整路径。该处先做
 * MultiByteToWideChar 再交给存储, 所以正常是宽字符; 但 API 用的是 StgCreateDocfile
 * (非 W), 保险起见两种都认: 第 2 字节是 0 就当宽字符。 */
void SaveShot_OnSaveBegin(void* pathPtr)
{
	const unsigned char* p0;
	char path[MAX_PATH], dir[MAX_PATH], base[SS_NAME_MAX];
	int wide;

	if (!s_Init || !s_Enable) return;

	s_BaseName[0] = 0;
	p0 = (const unsigned char*)pathPtr;
	if (!p0 || !PR_IsPlausiblePtr(p0) || !p0[0]) { SS_Log("bad path ptr", NULL); return; }

	wide = (p0[1] == 0 && p0[0] >= 0x20 && p0[0] < 0x7F);
	if (wide)
	{
		if (!WideCharToMultiByte(CP_ACP, 0, (const wchar_t*)p0, -1, path, MAX_PATH, NULL, NULL))
		{
			SS_Log("wide->ansi failed", NULL);
			return;
		}
	}
	else
	{
		lstrcpynA(path, (const char*)p0, MAX_PATH);
	}

	SS_SplitPath(path, dir, base);
	lstrcpynA(s_BaseName, base, SS_NAME_MAX);

	/* 引擎给的是相对路径 ("Saved Games\SAVE0001.SAV"): 补成绝对, 免得进程 CWD
	 * 变了以后读 Thumbs 读不到。 */
	if (dir[0])
	{
		if (dir[0] == '\\' || dir[1] == ':')
			lstrcpynA(s_SaveDir, dir, MAX_PATH);
		else
			_snprintf_s(s_SaveDir, sizeof(s_SaveDir), _TRUNCATE, "%s\\%s",
				Phobos::GetProgramDirectory().string().c_str(), dir);
	}
	SS_DefaultDir();

	SS_Log(wide ? "pathW " : "pathA ", path);
}

/* 存档尾部（存储已 Commit+Release, 成功标志低字节=1）: 写旁挂 BMP。 */
void SaveShot_OnSaveEnd(int ok)
{
	char dir[MAX_PATH], path[MAX_PATH];

	if (!s_Init || !s_Enable) return;
	if (!ok) { SS_Log("end bl=0", NULL); return; }
	if (!s_BaseName[0]) { SS_Log("end no path", NULL); return; }
	if (!s_Pix || s_PixW <= 0 || s_PixH <= 0) { SS_Log("no frame cached, skip", NULL); return; }

	_snprintf_s(dir, sizeof(dir), _TRUNCATE, "%s\\Thumbs", s_SaveDir);
	CreateDirectoryA(dir, NULL);   /* 已存在返回 ERROR_ALREADY_EXISTS, 忽略 */

	SS_ThumbPath(path, MAX_PATH, s_SaveDir, s_BaseName);
	ok = SS_WriteBmp(path, s_Pix, s_PixW, s_PixH);
	if (ok) SS_ThumbReset();       /* 新写的这张要能被下次读档读到 */
	SS_Log(ok ? "write " : "write FAILED ", path);
}

/* ---------------- 找对话框 ---------------- */

/* 真对话框的 DLGPROC 恒非零; 用 CreateWindowEx 建的原始 #32770 窗口附加字节为
 * 零初始化, 必为 0。MapDialogRect 对后者会直接解引用未初始化的 DialogInfo 指针
 * (崩溃实证: USER32 读 [pwnd+0x128]->+8, 值是分配残留), 所以碰窗口几何前先过这道。 */
static int SS_IsRealDialog(HWND h)
{
	return h && GetWindowLongPtrA(h, DWLP_DLGPROC) != 0;
}

/* 对话框是 #32770, 但窗口被撑成整屏, 只能靠"有没有我们的列表控件"认。 */
typedef struct { HWND hwnd; int listId; } SS_Find;

static BOOL CALLBACK SS_EnumCb(HWND h, LPARAM lp)
{
	SS_Find* f = (SS_Find*)lp;
	char cls[24];
	DWORD pid = 0;

	if (GetClassNameA(h, cls, sizeof(cls)) < 2) return TRUE;
	if (lstrcmpA(cls, "#32770") != 0) return TRUE;
	/* EnumWindows 会枚举到其它进程的 #32770（输入法/其它程序的对话框），
	 * 必须限定本进程，否则后续 MapDialogRect / SendMessage 会跨进程乱来。 */
	GetWindowThreadProcessId(h, &pid);
	if (pid != GetCurrentProcessId()) return TRUE;
	if (!SS_IsRealDialog(h)) return TRUE;
	if (GetDlgItem(h, SS_ID_LIST_LOAD)) { f->hwnd = h; f->listId = SS_ID_LIST_LOAD; return FALSE; }
	if (GetDlgItem(h, SS_ID_LIST_SAVE)) { f->hwnd = h; f->listId = SS_ID_LIST_SAVE; return FALSE; }
	return TRUE;
}

static HWND SS_FindDialog(int* listId)
{
	SS_Find f;

	f.hwnd = NULL;
	f.listId = 0;
	EnumWindows(SS_EnumCb, (LPARAM)&f);
	if (!f.hwnd) EnumChildWindows(GetDesktopWindow(), SS_EnumCb, (LPARAM)&f);
	if (listId) *listId = f.listId;
	return f.hwnd;
}

/* 面板尺寸: 引擎按模板换算成像素并居中, 固定 800x600。 */
static void SS_PanelRect(HWND hDlg)
{
	RECT rc, client;

	GetClientRect(hDlg, &client);
	SetRect(&rc, 0, 0, SS_TPL_CX, SS_TPL_CY);
	if (!SS_IsRealDialog(hDlg) ||
		!MapDialogRect(hDlg, &rc) || rc.right < 240 || rc.bottom < 180)
		SetRect(&rc, 0, 0, SS_TPL_PX_W, SS_TPL_PX_H);

	s_PanelW = rc.right;
	s_PanelH = rc.bottom;
	s_Px = (client.right - s_PanelW) / 2;
	s_Py = (client.bottom - s_PanelH) / 2;
	if (s_Px < 0) s_Px = 0;
	if (s_Py < 0) s_Py = 0;
}

/* ---------------- 预览绘制 ---------------- */

static void SS_Blit(DSurface* surf, int dx, int dy,
	const unsigned short* src, int sw, int sh)
{
	unsigned char* base;
	int pitch, W, H, i, j;

	W = surf->GetWidth();
	H = surf->GetHeight();
	pitch = surf->GetPitch();
	if (W <= 0 || H <= 0 || pitch < W * 2) return;
	if (dx >= W || dy >= H || dx + sw <= 0 || dy + sh <= 0) return;
	base = (unsigned char*)surf->Lock(0, 0);
	if (!base) return;
	for (j = 0; j < sh; j++)
	{
		int sy = dy + j, x0, x1;
		unsigned short* row;
		if (sy < 0 || sy >= H) continue;
		row = (unsigned short*)(base + (size_t)sy * pitch);
		x0 = dx < 0 ? -dx : 0;
		x1 = sw; if (dx + x1 > W) x1 = W - dx;
		for (i = x0; i < x1; i++) row[dx + i] = src[(size_t)j * sw + i];
	}
	surf->Unlock();
}

/* 预览框: 深底 + 一圈亮边, 空的时候看得见框在哪。一次 Lock 画完。
 * 标题/占位文字用引擎 DrawText（对话框阶段字体已初始化）。 */
static void SS_DrawBox(DSurface* surf, const RECT* b, const SS_Pic* im,
	const wchar_t* cap, const wchar_t* noneText)
{
	int bw = b->right - b->left, bh = b->bottom - b->top;
	unsigned char* base;
	int pitch, W, H;
	unsigned short fill, edge;

	if (bw < 12 || bh < 12) return;

	W = surf->GetWidth();
	H = surf->GetHeight();
	pitch = surf->GetPitch();
	if (W < 1 || H < 1 || pitch < W * 2) return;

	fill = PR_Blend16(0, 10, 10, 14, 255);
	edge = PR_Blend16(0, 96, 98, 108, 255);
	base = (unsigned char*)surf->Lock(0, 0);
	if (!base) return;
	SS_FillPx(base, pitch, W, H, b->left, b->top, bw, bh, fill);
	SS_Frame(base, pitch, W, H, b->left, b->top, bw, bh, 1, edge);
	surf->Unlock();

	if (im->px && im->w > 0 && im->h > 0)
	{
		SS_Blit(surf, b->left + (bw - im->w) / 2, b->top + (bh - im->h) / 2,
			im->px, im->w, im->h);
	}
	else if (noneText && *noneText && BitFont::Instance)
	{
		int tw = 0, th = 0;
		RectangleStruct bound = surf->GetRect();
		Point2D pt { b->left + 6, b->top + (bh - 14) / 2 };
		if (BitFont::Instance->GetTextDimension(noneText, &tw, &th, 120))
			pt.X = b->left + (bw - tw) / 2;
		if (pt.X < b->left + 2) pt.X = b->left + 2;
		surf->DrawText(noneText, &bound, &pt, Drawing::RGB_To_Int(118, 118, 128), 0, TextPrintType::NoShadow);
	}

	/* 框上缘外侧的标题行。 */
	if (cap && *cap && b->top >= 14 && BitFont::Instance)
	{
		RectangleStruct bound = surf->GetRect();
		Point2D pt { b->left + 1, b->top - 14 };
		surf->DrawText(cap, &bound, &pt, Drawing::RGB_To_Int(206, 210, 220), 0, TextPrintType::NoShadow);
	}
}

static int SS_DlgAlive()
{
	if (!s_Dlg || !s_DlgList) return 0;
	if (!IsWindow((HWND)s_Dlg)) return 0;
	return GetDlgItem((HWND)s_Dlg, s_DlgList) != NULL;
}

/* 忘掉当前对话框（核心版没有自管控件, 只清缓存与状态）。 */
static void SS_Forget()
{
	s_Dlg = NULL;
	s_DlgList = 0;
	SS_PicFree(&s_PicA);
	SS_ThumbReset();
}

/* 0x622396: 对话框内容画进临时面（ESI）之后。无对话框时该点每帧都进,
 * 枚举窗口要节流。预览固定叠加在面板右缘内侧, 不重排原版控件。 */
void SaveShot_PaintSurface(void* surf)
{
	HWND hwnd, list;
	int listId = 0, aw, ah, bw, bh, boxW, boxH, x, y;
	char name[SS_NAME_MAX];
	char nm[40];
	RECT boxA, boxB;

	if (!s_Init || !s_Enable || !surf || !PR_IsPlausiblePtr(surf)) return;
	if (!PR_IsPlausiblePtr(*(void**)surf)) return;   /* vtable 粗判 */

	auto s = static_cast<DSurface*>(surf);

	if (SS_DlgAlive())
	{
		hwnd = (HWND)s_Dlg;
		listId = s_DlgList;
	}
	else
	{
		/* 没对话框时这个钩点仍然每帧进, 枚举窗口要节流 */
		if (GetTickCount() - s_LastScan < 200) return;
		s_LastScan = GetTickCount();
		hwnd = SS_FindDialog(&listId);
		if (!hwnd) { SS_Forget(); return; }
		s_Dlg = (void*)hwnd;
		s_DlgList = listId;
		SS_PicFree(&s_PicA);
		SS_ThumbReset();
	}

	list = GetDlgItem(hwnd, listId);
	if (!list) { SS_Forget(); return; }

	SS_PanelRect(hwnd);

	/* 固定叠加: 两框竖排在面板右缘内侧。尺寸按当前视口宽高比。 */
	boxW = s_PrevW;
	boxH = (int)((long long)boxW * s_Aspect / 1000);
	if (boxH < 60) boxH = 60;
	x = s_Px + s_PanelW - boxW - 12;
	y = s_Py + 20;
	if (x < s_Px + 4) x = s_Px + 4;
	if (y + boxH * 2 + 26 > s_Py + s_PanelH - 8)
		y = s_Py + s_PanelH - 8 - (boxH * 2 + 26);
	if (y < s_Py + 4) y = s_Py + 4;
	SetRect(&boxA, x, y, x + boxW, y + boxH);
	SetRect(&boxB, x, y + boxH + 26, x + boxW, y + boxH * 2 + 26);

	/* 上框: 当前画面 (滚动缓存, 对话框开着时不刷新) */
	aw = boxA.right - boxA.left - 2;   /* 框内留 1px 边框 */
	ah = boxA.bottom - boxA.top - 2;
	if (!s_PicA.px && s_Pix && s_PixW > 0 && s_PixH > 0)
		SS_PicFit(&s_PicA, (const unsigned char*)s_Pix, s_PixW, s_PixH,
			s_PixW * 2, 0, aw, ah);

	/* 下框: 列表选中那一档已存的截图 (主名, 去掉目录与扩展名) */
	bw = boxB.right - boxB.left - 2;
	bh = boxB.bottom - boxB.top - 2;
	SS_SplitPath(SS_SelectedFile(list, nm) ? nm : "", NULL, name);
	if (lstrcmpA(name, s_ThumbName) != 0)
	{
		SS_PicFree(&s_PicB);
		lstrcpynA(s_ThumbName, name, SS_NAME_MAX);   /* 加载失败也记下, 免得每帧重读磁盘 */
		if (name[0])
		{
			char path[MAX_PATH];
			SS_ThumbPath(path, MAX_PATH, s_SaveDir, name);
			if (!SS_PicLoadBmp(&s_PicB, path, bw, bh))
				SS_Log("no thumb ", path);
		}
	}
	else if (s_PicB.px && (s_PicB.w > bw + 2 || s_PicB.h > bh + 2))
	{
		SS_ThumbReset();        /* 换排版后框变小了 */
	}

	SS_DrawBox(s, &boxA, &s_PicA, L"当前画面", L"无截图");
	SS_DrawBox(s, &boxB, &s_PicB, L"存档截图", L"无截图");
}

/* ---------------- Syringe 钩子 ---------------- */

#include <Helpers/Macro.h>

/* 0x67CF42: StgCreateDocfileW 调用点 (EDX = 完整路径指针)。
 * 原指令 FF 15 C8 15 7E 00, 返回 0 照原样重放。 */
DEFINE_HOOK(0x67CF42, SaveShot_SaveBegin, 0x6)
{
	GET(void*, pathPtr, EDX);
	SaveShot_OnSaveBegin(pathPtr);
	return 0;
}

/* 0x67D2F1: 存档尾部 (BL = 1 成功)。
 * 原指令 5F 5E 8A C3 5D (pop edi / pop esi / mov al,bl / pop ebp)。 */
DEFINE_HOOK(0x67D2F1, SaveShot_SaveEnd, 0x5)
{
	GET(DWORD, result, EBX);
	SaveShot_OnSaveEnd((int)(result & 0xFFu));
	return 0;
}

/* 0x4F1140: 引擎建好对话框、还没 ShowWindow —— 抓当前视口纯战场画面。 */
DEFINE_HOOK(0x4F1140, SaveShot_CaptureOnDialog, 0x5)
{
	SaveShot_CaptureNow();
	return 0;
}

/* 0x622396: 对话框内容画进临时面 (ESI) 之后, 叠加预览框。 */
DEFINE_HOOK(0x622396, SaveShot_PaintSurfaceHook, 0x6)
{
	GET(void*, surf, ESI);
	SaveShot_PaintSurface(surf);
	return 0;
}
