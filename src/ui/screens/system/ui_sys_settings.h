#ifndef UI_SYS_SETTINGS_H
#define UI_SYS_SETTINGS_H

#include <lvgl.h>

namespace ui {
namespace system {

/**
 * @brief 加载系统设置主屏幕
 */
void load_sys_settings_menu_screen();



/**
 * @brief 加载基础设置屏幕
 */
void load_sys_settings_basic_screen();

/**
 * @brief 加载基础设置-时间设置屏幕
 */
void load_sys_basic_time_settings_screen();

/**
 * @brief 加载基础设置-日期屏幕
 * @brief 加载基础设置-日期格式设置屏幕
 * @brief 加载基础设置-日期设置屏幕
 */
void load_sys_basic_date_screen();
void load_sys_basic_date_format_screen();
void load_sys_basic_date_settings_screen();

/**
 * @brief 加载基础设置-音量屏幕
 */
void load_sys_basic_volume_settings_screen();

/**
 * @brief 加载基础设置-语言屏幕
 */
void load_sys_basic_language_settings_screen();

/**
 * @brief 加载基础设置-屏保时间屏幕
 */
void load_sys_basic_screensafe_settings_screen();

/**
 * @brief 加载基础设置-机器号设置屏幕
 */
void load_sys_basic_machine_id_screen();

/**
 * @brief 加载基础设置-返回主界面时间屏幕
 */
void load_sys_basic_return_time_screen();

/**
 * @brief 加载基础设置-管理员总数屏幕
 */
void load_sys_basic_admin_count_screen();

/**
 * @brief 加载基础设置-记录警告数屏幕
 */
void load_sys_basic_warn_count_screen();  



/**
 * @brief 加载高级设置界面
 */
void load_sys_settings_advanced_screen();

void load_sys_advanced_clean_records_sreen();
void load_sys_advanced_clean_employee_sreen();
void load_sys_advanced_clean_data_sreen();
void load_sys_advanced_factory_reset_sreen();
void load_sys_advanced_system_update_sreen();

/**
 * @brief 加载参数设置屏幕
 */
void load_sys_settings_param_screen();

/**
 * @brief 加载自检功能屏幕
 */
void load_sys_settings_selfcheck_screen();




} // namespace system
} // namespace ui

#endif // UI_SYS_SETTINGS_H