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
 * @brief 加载部门详情子界面 [新增]
 * @param dept_id 部门 ID
 */
void load_dept_detail_screen(int dept_id);

/**
 * @brief 加载班次设置子界面
 */
void load_shift_set_screen();

/**
 * @brief 班次信息界面
 */
void load_shift_info_screen(int shift_id);

/**
 * @brief 加载考勤规则子界面
 */
void load_rule_screen();

/**
 * @brief 加载排班设置子界面
 */
void load_schedule_screen();

/**
 * @brief 加载公司设置子界面
 */
void load_company_screen();

/**
 * @brief 加载定时响铃子界面 [新增]
 */
void load_bell_screen();
} // namespace att_design
} // namespace ui

#endif // UI_SCR_ATT_DESIGN_H