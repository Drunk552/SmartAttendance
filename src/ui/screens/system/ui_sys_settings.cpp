#include "ui_sys_settings.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../../ui_controller.h"
#include "../menu/ui_scr_menu.h"
#include <cstdio>
#include <cstdlib>

namespace ui {
namespace system {

static lv_obj_t *scr_sys = nullptr;
static lv_obj_t *scr_param = nullptr;

// =================系统主界面============

// 菜单按钮事件回调
static void sys_main_event_cb(lv_event_t *e) {
    if (lv_event_get_key(e) == LV_KEY_ENTER) {
        const char* tag = (const char*)lv_event_get_user_data(e);
        
        if (strcmp(tag, "PARAM") == 0) {
            load_system_param_screen();
        }
        else if (strcmp(tag, "RESET") == 0) {
            // Factory Reset
            UiController::getInstance()->factoryReset();
            show_popup("Reset", "System Resetting...\nBye!");
            // 延时退出
            lv_timer_t *t = lv_timer_create([](lv_timer_t*){ exit(0); }, 2000, nullptr);
            lv_timer_set_repeat_count(t, 1);
        }
    } 
    else if (lv_event_get_key(e) == LV_KEY_ESC) {
        ui::menu::load_screen();
    }
}

// 主屏幕实现
void load_sys_settings_menu_screen() {
    if (scr_sys) lv_obj_delete(scr_sys);

    BaseScreenParts parts = create_base_screen("system / 系统设置");
    scr_sys = parts.screen;
    lv_obj_add_style(scr_sys, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::SYS_SETTINGS, &scr_sys);

    // Grid 布局
    static int32_t col_dsc[] = {200, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {50, 50, LV_GRID_TEMPLATE_LAST};
    
    lv_obj_t *grid = lv_obj_create(scr_sys);
    lv_obj_set_size(grid, 220, 200);
    lv_obj_center(grid);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 10, 0);

    // 1. 参数设置
    lv_obj_t *b1 = create_sys_grid_btn(grid, 0, LV_SYMBOL_SETTINGS, "Params", "参数设置", sys_main_event_cb, "PARAM");
    
    // 2. 恢复出厂 (红色样式)
    lv_obj_t *b2 = create_sys_grid_btn(grid, 1, LV_SYMBOL_TRASH, "Reset", "恢复出厂", sys_main_event_cb, "RESET");
    // 手动覆盖背景色为红
    lv_obj_set_style_bg_color(b2, lv_palette_main(LV_PALETTE_RED), 0);

    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(b1);
    UiManager::getInstance()->addObjToGroup(b2);
    lv_group_focus_obj(b1);

    lv_obj_add_event_cb(scr_sys, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) ui::menu::load_screen();
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_sys);

    lv_screen_load(scr_sys);
    UiManager::getInstance()->destroyAllScreensExcept(scr_sys);
}

// ================= 参数设置==========

// 菜单按钮事件回调
static void param_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    const char* tag = (const char*)lv_event_get_user_data(e);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);

        // [Constraint] 保留原手动导航逻辑
        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_obj_t *grid = lv_obj_get_parent(btn);
            uint32_t total = lv_obj_get_child_cnt(grid);
            uint32_t index = lv_obj_get_index(btn);
            uint32_t next_index = (index + 1) % total;
            lv_group_focus_obj(lv_obj_get_child(grid, next_index));
        }
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_obj_t *grid = lv_obj_get_parent(btn);
            uint32_t total = lv_obj_get_child_cnt(grid);
            uint32_t index = lv_obj_get_index(btn);
            uint32_t next_index = (index + total - 1) % total;
            lv_group_focus_obj(lv_obj_get_child(grid, next_index));
        }
        else if (key == LV_KEY_ESC) {
            load_sys_settings_menu_screen(); // 返回上一级
        }
        else if (key == LV_KEY_ENTER) {
            if (strcmp(tag, "THRESHOLD") == 0) {
                // 模拟切换阈值
                show_popup("Threshold", "Value: 0.75 (Fixed)");
            }
            else if (strcmp(tag, "ROI") == 0) {
                // 模拟切换 ROI
                show_popup("ROI", "ROI Crop: ON");
            }
        }
    }
}

// 加载高级设置屏幕实现
void load_system_param_screen() {
    if (scr_param) lv_obj_delete(scr_param);
    scr_param = lv_obj_create(nullptr);
    lv_obj_add_style(scr_param, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::SYS_ADVANCED, &scr_param);

    lv_obj_t *title = lv_label_create(scr_param);
    lv_label_set_text(title, "参数设置 / Params");
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    static int32_t col_dsc[] = {200, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {50, 50, LV_GRID_TEMPLATE_LAST};
    
    lv_obj_t *grid = lv_obj_create(scr_param);
    lv_obj_set_size(grid, 220, 150);
    lv_obj_center(grid);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);

    // 仅使用基础图标
    lv_obj_t *b1 = create_sys_grid_btn(grid, 0, LV_SYMBOL_SETTINGS, "Threshold", "识别阈值", param_event_cb, "THRESHOLD");
    lv_obj_t *b2 = create_sys_grid_btn(grid, 1, LV_SYMBOL_EYE_OPEN, "ROI Crop", "ROI裁剪", param_event_cb, "ROI");

    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(b1);
    UiManager::getInstance()->addObjToGroup(b2);
    lv_group_focus_obj(b1);

    lv_obj_add_event_cb(scr_param, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) load_sys_settings_menu_screen();
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_param);

    lv_screen_load(scr_param);
    UiManager::getInstance()->destroyAllScreensExcept(scr_param);
}

} // namespace system
} // namespace ui