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

    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器
    
    create_sys_list_btn(list, "1. ", "", "部门设置", design_event_cb, "DEPT");
    create_sys_list_btn(list, "2. ", "", "班次设置", design_event_cb, "SHIFT");
    create_sys_list_btn(list, "3. ", "", "考勤规则", design_event_cb, "RULE");
    create_sys_list_btn(list, "4. ", "", "人员排班", design_event_cb, "SCH");
    
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for(uint32_t i = 0; i < child_cnt; i++) {
        UiManager::getInstance()->addObjToGroup(lv_obj_get_child(list, i));
    }
    
    // 默认聚焦第一个
    if (child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
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