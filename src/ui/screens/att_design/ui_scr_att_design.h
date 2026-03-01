#ifndef UI_SCR_ATT_DESIGN_H
#define UI_SCR_ATT_DESIGN_H

#include <lvgl.h>

namespace ui {
namespace att_design {

/**
 * @brief 加载考勤设计菜单 (部门、班次、规则)
 */
void load_att_design_menu_screen();

/**
 * @brief 加载部门设置子界面
 */
void load_dept_screen();

/**
 * @brief 加载班次设置子界面
 */
void load_shift_screen();

/**
 * @brief 加载考勤规则子界面
 */
void load_rule_screen();

/**
 * @brief 加载人员排班子界面
 */
void load_schedule_screen();

} // namespace att_design
} // namespace ui

#endif // UI_SCR_ATT_DESIGN_H