#ifndef UI_SYS_SETTINGS_H
#define UI_SYS_SETTINGS_H

#include <lvgl.h>

namespace ui {
namespace system {

/**
 * @brief 加载系统设置主屏幕
 */
void load_screen();

/**
 * @brief 加载高级设置屏幕
 */
void load_param_screen();

} // namespace system
} // namespace ui

#endif // UI_SYS_SETTINGS_H