#ifndef UI_SCR_SYS_INFO_H
#define UI_SCR_SYS_INFO_H

#include <lvgl.h>

namespace ui {
namespace sys_info {

/**
 * @brief 系统信息界面
 */
void load_sys_info_menu_screen();

/**
 * @brief 存储信息界面
 */
void load_storage_info_screen();

/**
 * @brief 设备信息界面
 */
void load_facility_info_screen();


}
}

#endif // UI_SCR_SYS_INFO_H