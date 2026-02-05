#include "ui_scr_sys_info.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../../ui_controller.h"
#include "../menu/ui_scr_menu.h"

#include <string>
#include <cstdio>
#include <vector>

namespace ui {
namespace sys_info {

static lv_obj_t *scr_sys = nullptr;

// [Constraint] 本地统计逻辑，保留原代码算法
static void get_storage_statistics(int &total_users, int &admin_count, int &pwd_users) {
    total_users = 0;
    admin_count = 0;
    pwd_users = 0;

    // 调用 Controller 获取原始数据
    auto users = UiController::getInstance()->getAllUsers();
    total_users = users.size();

    // 手动遍历计算
    for (const auto& u : users) {
        if (u.role == 1) admin_count++;
        if (!u.password.empty()) pwd_users++;
    }
}

// 菜单点击回调
static void sys_menu_event_cb(lv_event_t *e) {
    if (lv_event_get_key(e) == LV_KEY_ENTER) {
        const char* tag = (const char*)lv_event_get_user_data(e);
        if (strcmp(tag, "STORAGE") == 0) {
            load_storage_info_screen();
        } else {
            show_popup("Info", "Device: SmartAtt-V1.5\nVer: 2024.01");
        }
    } else if (lv_event_get_key(e) == LV_KEY_ESC) {
        ui::menu::load_screen();
    }
}

// 主屏幕实现
void load_sys_info_menu_screen() {
    if (scr_sys) lv_obj_delete(scr_sys);
    scr_sys = lv_obj_create(nullptr);
    lv_obj_add_style(scr_sys, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::SYS_INFO, &scr_sys);

    lv_obj_t *title = lv_label_create(scr_sys);
    lv_label_set_text(title, "系统信息 / Sys Info");
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // 简单列表菜单
    lv_obj_t *list = lv_obj_create(scr_sys);
    lv_obj_set_size(list, 200, 200);
    lv_obj_center(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    
    // 按钮1: 基础信息
    lv_obj_t *b1 = lv_button_create(list);
    lv_obj_set_width(b1, LV_PCT(100));
    lv_label_set_text(lv_label_create(b1), "Basic Info / 基础信息");
    lv_obj_add_style(b1, &style_btn_default, 0);
    lv_obj_add_style(b1, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(b1, sys_menu_event_cb, LV_EVENT_KEY, (void*)"BASIC");

    // 按钮2: 存储详情
    lv_obj_t *b2 = lv_button_create(list);
    lv_obj_set_width(b2, LV_PCT(100));
    lv_label_set_text(lv_label_create(b2), "Storage / 存储详情");
    lv_obj_add_style(b2, &style_btn_default, 0);
    lv_obj_add_style(b2, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(b2, sys_menu_event_cb, LV_EVENT_KEY, (void*)"STORAGE");

    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(b1);
    UiManager::getInstance()->addObjToGroup(b2);
    lv_group_focus_obj(b1);
    
    // ESC
    lv_obj_add_event_cb(scr_sys, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) ui::menu::load_screen();
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_sys);

    lv_screen_load(scr_sys);
    UiManager::getInstance()->destroyAllScreensExcept(scr_sys);
}

// [Constraint] 二级页面实现
void load_storage_info_screen() {
    if (scr_sys) lv_obj_delete(scr_sys); // 复用同一个全局指针，或新建
    scr_sys = lv_obj_create(nullptr);
    lv_obj_add_style(scr_sys, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::STORAGE_INFO, &scr_sys);

    lv_obj_t *title = lv_label_create(scr_sys);
    lv_label_set_text(title, "存储统计 / Storage Stats");
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // 计算数据
    int total = 0, admin = 0, pwd = 0;
    get_storage_statistics(total, admin, pwd); // 调用本地逻辑

    // 显示内容
    lv_obj_t *cont = lv_obj_create(scr_sys);
    lv_obj_set_size(cont, 200, 180);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x222222), 0);

    auto add_line = [&](const char* label, int val, lv_color_t color) {
        lv_obj_t *line = lv_obj_create(cont);
        lv_obj_set_size(line, LV_PCT(100), 30);
        lv_obj_remove_style_all(line);
        lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(line, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *l = lv_label_create(line);
        lv_label_set_text(l, label);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_obj_add_style(l, &style_text_cn, 0);

        lv_obj_t *v = lv_label_create(line);
        lv_label_set_text_fmt(v, "%d", val);
        lv_obj_set_style_text_color(v, color, 0);

        return line;
    };

    add_line("Total Users / 总用户", total, lv_palette_main(LV_PALETTE_BLUE));
    add_line("Admins / 管理员", admin, lv_palette_main(LV_PALETTE_RED));
    add_line("Pwd Users / 密码用户", pwd, lv_palette_main(LV_PALETTE_ORANGE));
    add_line("Free Space / 剩余空间", 120, lv_palette_main(LV_PALETTE_GREEN)); // 模拟值

    // ESC 返回上一级
    lv_obj_add_event_cb(scr_sys, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) load_sys_info_menu_screen(); // 返回 SysInfo 菜单
    }, LV_EVENT_KEY, nullptr);

    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(scr_sys);
    lv_group_focus_obj(scr_sys);

    lv_screen_load(scr_sys);
    UiManager::getInstance()->destroyAllScreensExcept(scr_sys);
}

} // namespace sys_info
} // namespace ui