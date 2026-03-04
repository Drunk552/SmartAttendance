#include "ui_scr_att_design.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../menu/ui_scr_menu.h"
#include <cstring> 

namespace ui {
namespace att_design {

// ==================== 全局变量 ====================
static lv_obj_t *scr_design = nullptr;   // 考勤设计主菜单屏幕
static lv_obj_t *scr_dept = nullptr;     // 部门设置屏幕
static lv_obj_t *scr_shift = nullptr;    // 班次设置屏幕
static lv_obj_t *scr_rule = nullptr;     // 考勤规则屏幕
static lv_obj_t *scr_schedule = nullptr; // 人员排班屏幕

// ==================== 主菜单事件回调 ====================
// 菜单按钮事件回调
static void design_event_cb(lv_event_t *e) {
    const char* tag = (const char*)lv_event_get_user_data(e); // 获取按钮的 tag
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *list = lv_obj_get_parent(btn);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(list);

        // 上下导航（1列列表，循环）
        if (key == LV_KEY_DOWN) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + 1) % total));
        }
        else if (key == LV_KEY_UP) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + total - 1) % total));
        }
        // 左右导航（如果是多列布局，也可以支持）
        else if (key == LV_KEY_RIGHT) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + 1) % total));
        }
        else if (key == LV_KEY_LEFT) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + total - 1) % total));
        }
        // ESC 返回主菜单
        else if (key == LV_KEY_ESC) {
            ui::menu::load_menu_screen();
        }
        // ENTER 触发功能
        else if (key == LV_KEY_ENTER) {
            if (strcmp(tag, "DEPT") == 0) {
                load_dept_screen(); // 跳转到部门设置界面
            } else if (strcmp(tag, "SHIFT") == 0) {
                load_shift_screen(); // 跳转到班次设置界面
            } else if (strcmp(tag, "RULE") == 0) {
                load_rule_screen(); // 跳转到考勤规则界面
            } else if (strcmp(tag, "SCH") == 0) {
                load_schedule_screen(); // 跳转到人员排班界面
            }
        }
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

    UiManager::getInstance()->resetKeypadGroup(); // 重置输入组，准备添加新控件

    lv_obj_t* list = create_list_container(parts.content); // 创建统一列表容器

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
            ui::menu::load_menu_screen(); 
        }
    }, LV_EVENT_KEY, nullptr);

    // 把屏幕本身加入组以响应 ESC
    UiManager::getInstance()->addObjToGroup(scr_design);

    // 加载与清理
    lv_screen_load(scr_design);
    UiManager::getInstance()->destroyAllScreensExcept(scr_design);
}

// ==================== 部门设置子界面 ====================

// 部门设置子界面按钮事件回调
static void dept_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *list = lv_obj_get_parent(btn);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(list);

        // 上下导航（1列列表，循环）
        if (key == LV_KEY_DOWN) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + 1) % total));
            lv_timer_handler();// 处理按键
        } else if (key == LV_KEY_UP) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + total - 1) % total));
            lv_timer_handler();// 处理按键
        }
        // ESC 返回上级菜单
        else if (key == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
        // ENTER 触发功能
        else if (key == LV_KEY_ENTER) {
            const char* tag = (const char*)lv_event_get_user_data(e);
            if (strcmp(tag, "ADD") == 0) {
                // TODO: 新增部门逻辑
                show_popup("Hint", "Add Department Feature Under Construction");
            } else if (strcmp(tag, "EDIT") == 0) {
                // TODO: 修改部门逻辑
                show_popup("Hint", "Edit Department Feature Under Construction");
            } else if (strcmp(tag, "DELETE") == 0) {
                // TODO: 删除部门逻辑
                show_popup("Hint", "Delete Department Feature Under Construction");
            }
        }
    }
}

void load_dept_screen() {
    // 删除旧屏幕（如果存在）
    if (scr_dept) {
        lv_obj_delete(scr_dept);
        scr_dept = nullptr;
    }

    // 创建新屏幕并注册
    BaseScreenParts parts = create_base_screen("部门设置");
    scr_dept = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::DEPT_SETTING, &scr_dept);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_dept, [](lv_event_t *e) {
        scr_dept = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 创建列表容器
    lv_obj_t* list = create_list_container(parts.content);

    // 添加功能按钮
    create_sys_list_btn(list, "1. ", "", "新增部门", dept_btn_event_cb, "ADD");
    create_sys_list_btn(list, "2. ", "", "修改部门", dept_btn_event_cb, "EDIT");
    create_sys_list_btn(list, "3. ", "", "删除部门", dept_btn_event_cb, "DELETE");

    // 将按钮加入输入组
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for (uint32_t i = 0; i < child_cnt; i++) {
        UiManager::getInstance()->addObjToGroup(lv_obj_get_child(list, i));
    }

    // 默认聚焦第一个按钮
    if (child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    // 处理 ESC 返回
    lv_obj_add_event_cb(scr_dept, [](lv_event_t* e) {
        if (lv_event_get_key(e) == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
    }, LV_EVENT_KEY, nullptr);

    // 把屏幕本身加入组以响应 ESC
    UiManager::getInstance()->addObjToGroup(scr_dept);

    // 加载与清理
    lv_screen_load(scr_dept);
    UiManager::getInstance()->destroyAllScreensExcept(scr_dept);
}

// ==================== 班次设置子界面 ====================

// 班次设置子界面按钮事件回调
static void shift_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *list = lv_obj_get_parent(btn);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(list);

        // 上下导航（1列列表，循环）
        if (key == LV_KEY_DOWN) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + 1) % total));
            lv_timer_handler(); // 处理按键
        } else if (key == LV_KEY_UP) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + total - 1) % total));
            lv_timer_handler(); // 处理按键
        }
        // ESC 返回上级菜单
        else if (key == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
        // ENTER 触发功能
        else if (key == LV_KEY_ENTER) {
            const char* tag = (const char*)lv_event_get_user_data(e);
            if (strcmp(tag, "ADD") == 0) {
                // TODO: 新增班次逻辑
                show_popup("Hint", "Add Shift Feature Under Construction");
            } else if (strcmp(tag, "EDIT") == 0) {
                // TODO: 修改班次逻辑
                show_popup("Hint", "Edit Shift Feature Under Construction");
            } else if (strcmp(tag, "DELETE") == 0) {
                // TODO: 删除班次逻辑
                show_popup("Hint", "Delete Shift Feature Under Construction");
            }
        }
    }
}

void load_shift_screen() {
    // 删除旧屏幕（如果存在）
    if (scr_shift) {
        lv_obj_delete(scr_shift);
        scr_shift = nullptr;
    }

    // 创建新屏幕并注册
    BaseScreenParts parts = create_base_screen("班次设置");
    scr_shift = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SHIFT_SETTING, &scr_shift);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_shift, [](lv_event_t *e) {
        scr_shift = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 创建列表容器
    lv_obj_t* list = create_list_container(parts.content);

    // 添加功能按钮
    create_sys_list_btn(list, "1. ", "", "新增班次", shift_btn_event_cb, "ADD");
    create_sys_list_btn(list, "2. ", "", "修改班次", shift_btn_event_cb, "EDIT");
    create_sys_list_btn(list, "3. ", "", "删除班次", shift_btn_event_cb, "DELETE");

    // 将按钮加入输入组
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for (uint32_t i = 0; i < child_cnt; i++) {
        UiManager::getInstance()->addObjToGroup(lv_obj_get_child(list, i));
    }

    // 默认聚焦第一个按钮
    if (child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    // 处理 ESC 返回
    lv_obj_add_event_cb(scr_shift, [](lv_event_t* e) {
        if (lv_event_get_key(e) == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
    }, LV_EVENT_KEY, nullptr);

    // 把屏幕本身加入组以响应 ESC
    UiManager::getInstance()->addObjToGroup(scr_shift);

    // 加载与清理
    lv_screen_load(scr_shift);
    UiManager::getInstance()->destroyAllScreensExcept(scr_shift);
}

// ==================== 考勤规则子界面 ====================

// 考勤规则子界面按钮事件回调
static void rule_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *list = lv_obj_get_parent(btn);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(list);

        // 上下导航（1列列表，循环）
        if (key == LV_KEY_DOWN) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + 1) % total));
            lv_timer_handler(); // 处理按键
        } else if (key == LV_KEY_UP) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + total - 1) % total));
            lv_timer_handler(); // 处理按键
        }
        // ESC 返回上级菜单
        else if (key == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
        // ENTER 触发功能
        else if (key == LV_KEY_ENTER) {
            const char* tag = (const char*)lv_event_get_user_data(e);
            if (strcmp(tag, "ADD") == 0) {
                // TODO: 新增班次逻辑
                show_popup("Hint", "Add Rule Feature Under Construction");
            } else if (strcmp(tag, "EDIT") == 0) {
                // TODO: 修改班次逻辑
                show_popup("Hint", "Edit Rule Feature Under Construction");
            } else if (strcmp(tag, "DELETE") == 0) {
                // TODO: 删除班次逻辑
                show_popup("Hint", "Delete Rule Feature Under Construction");
            }
        }
    }
}

void load_rule_screen() {
    // 删除旧屏幕（如果存在）
    if (scr_rule) {
        lv_obj_delete(scr_rule);
        scr_rule = nullptr;
    }

    // 创建新屏幕并注册
    BaseScreenParts parts = create_base_screen("考勤规则");
    scr_rule = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::RULE_SETTING, &scr_rule);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_rule, [](lv_event_t *e) {
        scr_rule = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 创建列表容器
    lv_obj_t* list = create_list_container(parts.content);

    // 添加功能按钮
    create_sys_list_btn(list, "1. ", "", "新增规则", rule_btn_event_cb, "ADD");
    create_sys_list_btn(list, "2. ", "", "修改规则", rule_btn_event_cb, "EDIT");
    create_sys_list_btn(list, "3. ", "", "删除规则", rule_btn_event_cb, "DELETE");

    // 将按钮加入输入组
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for (uint32_t i = 0; i < child_cnt; i++) {
        UiManager::getInstance()->addObjToGroup(lv_obj_get_child(list, i));
    }

    // 默认聚焦第一个按钮
    if (child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    // 处理 ESC 返回
    lv_obj_add_event_cb(scr_rule, [](lv_event_t* e) {
        if (lv_event_get_key(e) == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
    }, LV_EVENT_KEY, nullptr);

    // 把屏幕本身加入组以响应 ESC
    UiManager::getInstance()->addObjToGroup(scr_rule);

    // 加载与清理
    lv_screen_load(scr_rule);
    UiManager::getInstance()->destroyAllScreensExcept(scr_rule);
}

// ==================== 人员排班子界面 ====================

// 人员排班子界面按钮事件回调
static void schedule_btn_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t *list = lv_obj_get_parent(btn);

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        uint32_t index = lv_obj_get_index(btn);
        uint32_t total = lv_obj_get_child_cnt(list);

        // 上下导航（1列列表，循环）
        if (key == LV_KEY_DOWN) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + 1) % total));
            lv_timer_handler(); // 处理按键
        } else if (key == LV_KEY_UP) {
            lv_group_focus_obj(lv_obj_get_child(list, (index + total - 1) % total));
            lv_timer_handler(); // 处理按键
        }
        // ESC 返回上级菜单
        else if (key == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
        // ENTER 触发功能
        else if (key == LV_KEY_ENTER) {
            const char* tag = (const char*)lv_event_get_user_data(e);
            if (strcmp(tag, "ADD") == 0) {
                // TODO: 新增排班逻辑
                show_popup("Hint", "Add Schedule Feature Under Construction");
            } else if (strcmp(tag, "EDIT") == 0) {
                // TODO: 修改排班逻辑
                show_popup("Hint", "Edit Schedule Feature Under Construction");
            } else if (strcmp(tag, "DELETE") == 0) {
                // TODO: 删除排班逻辑
                show_popup("Hint", "Delete Schedule Feature Under Construction");
            }
        }
    }
}

void load_schedule_screen() {
    // 删除旧屏幕（如果存在）
    if (scr_schedule) {
        lv_obj_delete(scr_schedule);
        scr_schedule = nullptr;
    }

    // 创建新屏幕并注册
    BaseScreenParts parts = create_base_screen("人员排班");
    scr_schedule = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SCHEDULE_SETTING, &scr_schedule);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_schedule, [](lv_event_t *e) {
        scr_schedule = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();

    // 创建列表容器
    lv_obj_t* list = create_list_container(parts.content);

    // 添加功能按钮
    create_sys_list_btn(list, "1. ", "", "新增排班", schedule_btn_event_cb, "ADD");
    create_sys_list_btn(list, "2. ", "", "修改排班", schedule_btn_event_cb, "EDIT");
    create_sys_list_btn(list, "3. ", "", "删除排班", schedule_btn_event_cb, "DELETE");

    // 将按钮加入输入组
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for (uint32_t i = 0; i < child_cnt; i++) {
        UiManager::getInstance()->addObjToGroup(lv_obj_get_child(list, i));
    }

    // 默认聚焦第一个按钮
    if (child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    // 处理 ESC 返回
    lv_obj_add_event_cb(scr_schedule, [](lv_event_t* e) {
        if (lv_event_get_key(e) == LV_KEY_ESC) {
            load_att_design_menu_screen();
        }
    }, LV_EVENT_KEY, nullptr);

    // 把屏幕本身加入组以响应 ESC
    UiManager::getInstance()->addObjToGroup(scr_schedule);

    // 加载与清理
    lv_screen_load(scr_schedule);
    UiManager::getInstance()->destroyAllScreensExcept(scr_schedule);
}

} // namespace att_design
} // namespace ui