#include "ui_scr_att_design.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../menu/ui_scr_menu.h"

namespace ui {
namespace att_design {

static lv_obj_t *scr_design = nullptr;

// 菜单按钮事件回调
static void design_event_cb(lv_event_t *e) {
    if (lv_event_get_key(e) == LV_KEY_ENTER) {
        // 暂无具体业务实现，仅弹窗
        show_popup("Hint", "Feature under construction");
    } else if (lv_event_get_key(e) == LV_KEY_ESC) {
        ui::menu::load_screen();
    }
}

// 加载考勤设计菜单实现
void load_menu() {
    if (scr_design) lv_obj_delete(scr_design);
    scr_design = lv_obj_create(nullptr);
    lv_obj_add_style(scr_design, &style_base, 0); // [Constraint] 黑底
    UiManager::getInstance()->registerScreen(ScreenType::ATT_DESIGN, &scr_design);

    lv_obj_t *title = lv_label_create(scr_design);
    lv_label_set_text(title, "考勤设计 / Att Design");
    lv_obj_add_style(title, &style_text_cn, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // [Constraint] 1列 Grid 菜单
    static int32_t col_dsc[] = {200, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {50, 50, 50, 50, LV_GRID_TEMPLATE_LAST};
    
    lv_obj_t *grid = lv_obj_create(scr_design);
    lv_obj_set_size(grid, 220, 240);
    lv_obj_align(grid, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_layout(grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, 10, 0);

    lv_obj_t *b1 = create_sys_grid_btn(grid, 0, LV_SYMBOL_DIRECTORY, "Depts", "部门设置", design_event_cb, "DEPT");
    lv_obj_t *b2 = create_sys_grid_btn(grid, 1, LV_SYMBOL_SETTINGS, "Shifts", "班次设置", design_event_cb, "SHIFT");
    lv_obj_t *b3 = create_sys_grid_btn(grid, 2, LV_SYMBOL_EDIT, "Rules", "考勤规则", design_event_cb, "RULE");
    lv_obj_t *b4 = create_sys_grid_btn(grid, 3, LV_SYMBOL_LIST, "Schedule", "人员排班", design_event_cb, "SCH");

    UiManager::getInstance()->resetKeypadGroup();
    UiManager::getInstance()->addObjToGroup(b1);
    UiManager::getInstance()->addObjToGroup(b2);
    UiManager::getInstance()->addObjToGroup(b3);
    UiManager::getInstance()->addObjToGroup(b4);
    lv_group_focus_obj(b1);

    // ESC
    lv_obj_add_event_cb(scr_design, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) ui::menu::load_screen();
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_design);

    lv_screen_load(scr_design);
    UiManager::getInstance()->destroyAllScreensExcept(scr_design);
}

} // namespace att_design
} // namespace ui