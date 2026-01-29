#ifndef UI_SCR_RECORD_QUERY_H
#define UI_SCR_RECORD_QUERY_H

#include <lvgl.h>

namespace ui {
namespace record_query {

/**
 * @brief 加载查询输入页面 (输入工号)
 */
void load_screen();

/**
 * @brief 加载查询结果页面 (显示打卡记录列表)
 * @param user_id 要查询的员工ID
 */
void load_result_screen(int user_id);

} // namespace record_query
} // namespace ui

#endif // UI_SCR_RECORD_QUERY_H