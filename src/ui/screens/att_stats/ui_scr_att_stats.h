#ifndef UI_SCR_ATT_STATS_H
#define UI_SCR_ATT_STATS_H

#include <lvgl.h>

namespace ui {
namespace att_stats {

/**
 * @brief 考勤统计主菜单界面
 * 包含：下载全员报表、下载个人报表入口等
 */
void load_att_stats_menu_screen();

/**
 * @brief 下载 (全员) 考勤报表界面
 */
void load_download_all_screen();

/**
 * @brief 下载 (个人) 考勤报表界面
 */
void load_download_personal_screen();


} // namespace att_stats
} // namespace ui

#endif // UI_SCR_ATT_STATS_H