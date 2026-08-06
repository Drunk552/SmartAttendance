#ifndef UI_SCR_MENU_H
#define UI_SCR_MENU_H

#include <lvgl.h>

namespace smart_attendance::ui { struct MenuPageDependencies; }

namespace ui {
namespace menu {

void configureDependencies(
    smart_attendance::ui::MenuPageDependencies& dependencies) noexcept;

/**
 * @brief 加载系统菜单屏幕
*/
void load_menu_screen();

}
}

#endif // UI_SCR_MENU_H
