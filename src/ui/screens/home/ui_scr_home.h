#ifndef UI_SCR_HOME_H
#define UI_SCR_HOME_H

#include <lvgl.h>
#include <string> // [Fix] 需要 string 支持

namespace ui {
namespace home {

/**
 * @brief 加载主页
 */
void load_screen();

/**
 * @brief 更新时间显示的接口 (供 EventBus 回调使用)
 */
void update_time(const std::string& time_str, const std::string& date_str);

/**
 * @brief 更新磁盘状态显示的接口
 */
void update_disk_status(bool is_full);

} // namespace home
} // namespace ui

#endif // UI_SCR_HOME_H