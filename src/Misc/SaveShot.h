#pragma once
/* SaveShot.h - 存档截图管理（核心版）。
 * 移植自 MagicShiba/MO_ExperienceEnhancements src/gui/SaveShot.cpp（核心裁剪：
 * 保留"存档时自动截图落盘 + 存/读档对话框预览"，裁掉对话框重做/按钮/输入法框）。
 *
 * 落盘走旁挂文件 <存档目录>\Thumbs\<存档名>.bmp，不碰 .SAV 本体；
 * 预览走 0x622396 的整屏对话框临时面：在面板右缘内侧竖排两个截图框，
 * 上=当前画面，下=列表选中那档已存的图。固定叠加，不重排原版控件。
 * 纯读渲染面 + 写本地文件，不参与模拟，联机无操作。
 */

class CCINIClass;

/* 读 [Phobos.SaveShot]（Phobos.ini 装载时由 OptionsClass_LoadSettings 调一次）。 */
void SaveShot_LoadConfig(CCINIClass& ini);

/* 兜底取图：Weather_AfterMap 每帧调，只在还没抓过图时每秒补一张。 */
void SaveShot_Tick();

/* 对话框建立那一刻抓当前视口（0x4F1140，战场还没被菜单盖住）。 */
void SaveShot_CaptureNow();

/* 0x67CF42（StgCreateDocfileW 调用点）：记录存档路径，拆出目录与主名。 */
void SaveShot_OnSaveBegin(void* pathPtr);

/* 0x67D2F1（存档尾部）：成功则把当前缓存写成旁挂 BMP。 */
void SaveShot_OnSaveEnd(int ok);

/* 0x622396（对话框内容画进临时面之后）：叠加预览框，自判是不是存/读档对话框。 */
void SaveShot_PaintSurface(void* surf);
