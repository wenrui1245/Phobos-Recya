#pragma once
/* PixelRender.h - 16bpp 像素工具（移植自 MagicShiba/MO_ExperienceEnhancements，
 * 原 GdiText 的像素格式校准/混合部分，YRpp 化改写）。
 *
 * 引擎的窗口/离屏面是 16 位色，但 565 与 555 两种布局都可能出现（取决于
 * 渲染路径与驱动），逐像素混合前必须先在 Temp 面上探测一次真实位深。
 * 校准由 PixelRender_Calibrate()（见 PixelRender.cpp）在首次绘制前完成。
 */

/* 指针粗判（MOEx GdiText.cpp IsPlausiblePtr）：用户态、4 对齐。 */
inline int PR_IsPlausiblePtr(const void* p)
{
	unsigned u = (unsigned)(size_t)p;
	return (u >= 0x00010000u && u < 0x7FF00000u && (u & 3u) == 0);
}

/* 当前位深（Calibrate 后生效；默认 565）。 */
extern int PR_RBits, PR_GBits, PR_BBits;

/* 位深展开表：通道 5/6 位值 -> 8 位值。 */
extern unsigned char PR_ExpR[32], PR_ExpG[64], PR_ExpB[32];
extern int PR_ExpReady;

void PR_BuildExpand();
void PR_Calibrate();

/* 前景 (fr,fg,fb)（8 位/通道）以 alpha a（0..255）混到 16bpp 目标像素上。 */
inline unsigned short PR_Blend16(unsigned short dst, unsigned fr,
	unsigned fg, unsigned fb, unsigned a)
{
	unsigned dr8, dg8, db8, or_, og, ob;
	if (a == 0) return dst;
	if (!PR_ExpReady) PR_BuildExpand();
	/* a=255 直接写颜色。 */
	if (a == 255)
	{
		if (PR_GBits == 6)
			return (unsigned short)((((fr * 31u + 127u) / 255u) << 11) |
				(((fg * 63u + 127u) / 255u) << 5) |
				((fb * 31u + 127u) / 255u));
		return (unsigned short)((((fr * 31u + 127u) / 255u) << 10) |
			(((fg * 31u + 127u) / 255u) << 5) |
			((fb * 31u + 127u) / 255u));
	}
	if (PR_GBits == 6)
	{
		dr8 = PR_ExpR[(dst >> 11) & 0x1Fu];
		dg8 = PR_ExpG[(dst >> 5) & 0x3Fu];
		db8 = PR_ExpB[dst & 0x1Fu];
		or_ = (fr * a + dr8 * (255u - a) + 127u) / 255u;
		og = (fg * a + dg8 * (255u - a) + 127u) / 255u;
		ob = (fb * a + db8 * (255u - a) + 127u) / 255u;
		or_ = (or_ * 31u + 127u) / 255u;
		og = (og * 63u + 127u) / 255u;
		ob = (ob * 31u + 127u) / 255u;
		return (unsigned short)((or_ << 11) | (og << 5) | ob);
	}
	dr8 = PR_ExpR[(dst >> 10) & 0x1Fu];
	dg8 = PR_ExpG[(dst >> 5) & 0x1Fu];
	db8 = PR_ExpB[dst & 0x1Fu];
	or_ = (fr * a + dr8 * (255u - a) + 127u) / 255u;
	og = (fg * a + dg8 * (255u - a) + 127u) / 255u;
	ob = (fb * a + db8 * (255u - a) + 127u) / 255u;
	or_ = (or_ * 31u + 127u) / 255u;
	og = (og * 31u + 127u) / 255u;
	ob = (ob * 31u + 127u) / 255u;
	return (unsigned short)((or_ << 10) | (og << 5) | ob);
}

/* 纯黑叠加（阴影）：与 PR_Blend16(…,0,0,0,a) 结果相同，更省。 */
inline unsigned short PR_BlendDark(unsigned short dst, unsigned a)
{
	unsigned m = 255u - a, or_, og, ob;
	if (a == 0) return dst;
	if (a == 255) return 0;
	if (!PR_ExpReady) PR_BuildExpand();
	if (PR_GBits == 6)
	{
		or_ = (PR_ExpR[(dst >> 11) & 0x1Fu] * m + 127u) / 255u;
		og = (PR_ExpG[(dst >> 5) & 0x3Fu] * m + 127u) / 255u;
		ob = (PR_ExpB[dst & 0x1Fu] * m + 127u) / 255u;
		or_ = (or_ * 31u + 127u) / 255u;
		og = (og * 63u + 127u) / 255u;
		ob = (ob * 31u + 127u) / 255u;
		return (unsigned short)((or_ << 11) | (og << 5) | ob);
	}
	or_ = (PR_ExpR[(dst >> 10) & 0x1Fu] * m + 127u) / 255u;
	og = (PR_ExpG[(dst >> 5) & 0x1Fu] * m + 127u) / 255u;
	ob = (PR_ExpB[dst & 0x1Fu] * m + 127u) / 255u;
	or_ = (or_ * 31u + 127u) / 255u;
	og = (og * 31u + 127u) / 255u;
	ob = (ob * 31u + 127u) / 255u;
	return (unsigned short)((or_ << 10) | (og << 5) | ob);
}
