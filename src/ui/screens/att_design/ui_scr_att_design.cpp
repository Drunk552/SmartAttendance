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
        ui::menu::load_menu_screen();
    }
}

// 加载考勤设计菜单实现
void load_att_design_menu_screen() {
    // 先删除旧屏幕（如果存在），避免重复创建
    if (scr_design){
        lv_obj_delete(scr_design);
        scr_design = nullptr;
    }

    // 创建新屏幕并注册
    BaseScreenParts parts = create_base_screen("考勤设计");
    scr_design = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::ATT_DESIGN, &scr_design);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_design, [](lv_event_t * e) {
        scr_design = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t *grid = create_menu_grid_container(parts.content);// 创建统一样式的菜单 Grid 容器

    // [Constraint] 1列 Grid 菜单
    static int32_t col_dsc[] = {200, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {50, 50, 50, 50, LV_GRID_TEMPLATE_LAST};

    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    lv_obj_set_style_pad_row(grid, 10, 0);
    
    lv_obj_t *b1 = create_sys_grid_btn(grid, 0, "1. ", "Depts", "部门设置", design_event_cb, "DEPT");
    lv_obj_t *b2 = create_sys_grid_btn(grid, 1, "2. ", "Shifts", "班次设置", design_event_cb, "SHIFT");
    lv_obj_t *b3 = create_sys_grid_btn(grid, 2, "3. ", "Rules", "考勤规则", design_event_cb, "RULE");
    lv_obj_t *b4 = create_sys_grid_btn(grid, 3, "4. ", "Schedule", "人员排班", design_event_cb, "SCH");

    // 按键组管理
    UiManager::getInstance()->resetKeypadGroup();
    
    uint32_t child_cnt = lv_obj_get_child_cnt(grid);
    for(uint32_t i = 0; i < child_cnt; i++) {
        UiManager::getInstance()->addObjToGroup(lv_obj_get_child(grid, i));
    }
    
    // 默认聚焦第一个
    if (child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(grid, 0));
    }

    // 处理 ESC 返回
    lv_obj_add_event_cb(scr_design, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) {
             // 返回主菜单的函数
             ui::menu::load_menu_screen(); 
        }
    }, LV_EVENT_KEY, nullptr);
    
    // 把屏幕本身加入组以响应 ESC
    UiManager::getInstance()->addObjToGroup(scr_design);

    // 加载与清理
    lv_screen_load(scr_design);
    UiManager::getInstance()->destroyAllScreensExcept(scr_design);
}

} // namespace att_design
} // namespace ui