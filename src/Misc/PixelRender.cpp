/* PixelRender.cpp - 16bpp 位深校准与展开表（移植自 MagicShiba/MO_ExperienceEnhancements
 * GdiText.cpp 的 Calibrate/GtBuildExpand，改用 YRpp DSurface 接口）。
 *
 * 在 Temp 面上 FillRect 纯红再读回，判定 565 还是 555。只做一次。
 */
#include "PixelRender.h"

#include <Surface.h>
#include <GeneralDefinitions.h>
#include <Utilities/Debug.h>

int PR_RBits = 5, PR_GBits = 6, PR_BBits = 5;  /* 默认 565 */
int PR_Calibrated = 0;

unsigned char PR_ExpR[32], PR_ExpG[64], PR_ExpB[32];
int PR_ExpReady = 0;

void PR_BuildExpand()
{
	int i, rN = (1 << PR_RBits) - 1, gN = (1 << PR_GBits) - 1, bN = (1 << PR_BBits) - 1;
	if (rN < 1) rN = 31;
	if (gN < 1) gN = 63;
	if (bN < 1) bN = 31;
	for (i = 0; i < 32; i++) PR_ExpR[i] = (unsigned char)((i * 255 + rN / 2) / rN);
	for (i = 0; i < 32; i++) PR_ExpB[i] = (unsigned char)((i * 255 + bN / 2) / bN);
	for (i = 0; i < 64; i++)
	{
		unsigned v = (unsigned)((i * 255 + gN / 2) / gN);
		PR_ExpG[i] = (unsigned char)(v > 255u ? 255u : v);
	}
	PR_ExpReady = 1;
}

void PR_Calibrate()
{
	if (PR_Calibrated)
		return;

	do
	{
		auto tmp = DSurface::Temp;
		if (!tmp || !PR_IsPlausiblePtr(tmp))
		{
			Debug::Log("PixelRender: no temp surface, assume 565\n");
			break;
		}

		RectangleStruct rc { 0, 0, 4, 4 };
		/* COLORREF 纯红（COLORREF 布局 = 0x00BBGGRR）。 */
		tmp->DrawRect(&rc, 0x000000FFu);

		auto px = (unsigned short*)tmp->Lock(0, 0);
		if (!px)
		{
			Debug::Log("PixelRender: temp lock failed, assume 565\n");
			break;
		}

		unsigned raw = *px;
		tmp->Unlock();

		if ((raw & 0xF800u) == 0xF800u)
		{
			PR_RBits = 5; PR_GBits = 6; PR_BBits = 5;
		}
		else if ((raw & 0x7C00u) == 0x7C00u)
		{
			PR_RBits = 5; PR_GBits = 5; PR_BBits = 5;
		}
		else
		{
			Debug::Log("PixelRender: unexpected probe pixel %04X, assume 565\n", raw);
		}
	} while (false);

	PR_ExpReady = 0;
	PR_BuildExpand();
	PR_Calibrated = 1;

	Debug::Log("PixelRender: %d%d%d\n", PR_RBits, PR_GBits, PR_BBits);
}
