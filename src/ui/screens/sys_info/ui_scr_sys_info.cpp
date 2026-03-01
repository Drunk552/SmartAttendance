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


// ================= [内部状态: 屏幕指针] =================
static lv_obj_t *scr_sys = nullptr;//系统信息主菜单界面
static lv_obj_t *scr_storage_info = nullptr;//存储信息界面
static lv_obj_t *scr_facility_info = nullptr;//设备信息界面

// ================= [内部状态: 输入框指针] =================


// ================= [内部状态: 控件与数据] =================


// ================= [内部状态: 注册临时数据暂存] =================


// ===================== 辅助函数 =================


// =========================================================
// 一、 系统设置主菜单 (Sys Info) (一级界面)
// =========================================================


// 系统设置主菜单事件回调
static void sys_info_menu_event_cb(lv_event_t *e) {

    lv_event_code_t code = lv_event_get_code(e);

    // 提前获取 key，方便后面判断
    uint32_t key = 0;
    if (code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 导航与返回逻辑 (仅处理按键)
    if (code == LV_EVENT_KEY) {
        
        // --- ESC 返回 ---
        if (key == LV_KEY_ESC) {
            ui::menu::load_menu_screen(); // 返回上一级系统主菜单
            return; // 防止继续执行下面代码
        }

        // --- 方向键导航 ---
        lv_group_t* group = UiManager::getInstance()->getKeypadGroup();
        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(group);// 向下/向右导航
        }
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_group_focus_prev(group);// 向上/向左导航
        }
    }

    // 2. 触发逻辑 (兼容 触摸点击 和 键盘回车)
    if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
        
        lv_indev_wait_release(lv_indev_get_act());// 【防连跳核心】 --- IGNORE ---

        // 获取 index (放在这里获取更安全)
        const char* user_data = (const char*)lv_event_get_user_data(e);
        intptr_t index = (intptr_t)user_data;

        if (index == 0) {
            load_storage_info_screen();//存储信息界面
        } 
        else if (index == 1) {
            load_facility_info_screen();//设备信息界面
        }
    }
}

// 系统信息主菜单界面
void load_sys_info_menu_screen() {
        if (scr_sys){
        lv_obj_delete(scr_sys);
        scr_sys = nullptr;
    }

    BaseScreenParts parts = create_base_screen("系统信息");
    scr_sys = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::SYS_INFO, &scr_sys);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_sys, [](lv_event_t * e) {
        scr_sys = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器

    // 创建按钮
    create_sys_list_btn(list, "1. ", "", "存储信息", sys_info_menu_event_cb, (const char*)(intptr_t)0);
    create_sys_list_btn(list, "2. ", "", "设备信息", sys_info_menu_event_cb, (const char*)(intptr_t)1);

    uint32_t child_cnt = lv_obj_get_child_cnt(list);// 遍历容器子对象(按钮)加入组
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个按钮
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_sys);
    UiManager::getInstance()->destroyAllScreensExcept(scr_sys);// 加载后销毁其他屏幕，保持资源清晰
}


// =========================================================
// 1. 存储信息 (Storage Info) (二级界面)
// =========================================================

//存储信息事件回调
static void storage_info_event_cb(lv_event_t *e) {

    lv_event_code_t code = lv_event_get_code(e);

    // 提前获取 key，方便后面判断
    uint32_t key = 0;
    if (code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 导航与返回逻辑 (仅处理按键)
    if (code == LV_EVENT_KEY) {
        
        // --- ESC 返回 ---
        if (key == LV_KEY_ESC) {
            load_sys_info_menu_screen(); // 返回上一级系统信息主菜单界面
            return; // 防止继续执行下面代码
        }

        // --- 方向键导航 ---
        lv_group_t* group = UiManager::getInstance()->getKeypadGroup();
        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(group);// 向下/向右导航
        }
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_group_focus_prev(group);// 向上/向左导航
        }
    }
}

//存储信息界面STORAGE_INFO
void load_storage_info_screen() {

    if (scr_storage_info){
        lv_obj_delete(scr_storage_info);
        scr_storage_info = nullptr;
    }

    BaseScreenParts parts = create_base_screen("存储信息");
    scr_storage_info = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::STORAGE_INFO, &scr_storage_info);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_storage_info, [](lv_event_t * e) {
        scr_storage_info = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器

    //获取存储信息数
    SystemStats stats = UiController::getInstance()->getSystemStatistics();

    char buf[64];
    // 1. 员工注册数
    snprintf(buf, sizeof(buf), "%d", stats.total_employees);//员工注册数
    create_sys_list_btn(list, "1. ", "员工注册数：", buf, storage_info_event_cb, (const char*)(intptr_t)0);

    // 2. 管理员注册数
    snprintf(buf, sizeof(buf), "%d", stats.total_admins);//管理员注册数
    create_sys_list_btn(list, "2. ", "管理员注册数：", buf, storage_info_event_cb, (const char*)(intptr_t)1);

    // 3. 人脸注册数
    snprintf(buf, sizeof(buf), "%d", stats.total_faces);//人脸注册数
    create_sys_list_btn(list, "3. ", "人脸注册数：", buf, storage_info_event_cb, (const char*)(intptr_t)2);

    // 4. 指纹注册数
    snprintf(buf, sizeof(buf), "%d", stats.total_fingerprints);//指纹注册数
    create_sys_list_btn(list, "4. ", "指纹注册数：", buf, storage_info_event_cb, (const char*)(intptr_t)3);

    // 5. 卡号注册数
    snprintf(buf, sizeof(buf), "%d", stats.total_cards);//卡号注册数
    create_sys_list_btn(list, "5. ", "卡号注册数：", buf, storage_info_event_cb, (const char*)(intptr_t)4);

    uint32_t child_cnt = lv_obj_get_child_cnt(list);// 遍历容器子对象(按钮)加入组
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个按钮
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_storage_info);
    UiManager::getInstance()->destroyAllScreensExcept(scr_storage_info);// 加载后销毁其他屏幕，保持资源清晰

}


// =========================================================
// 2. 设备信息 (Facility Info) (二级界面)
// =========================================================

//设备信息事件回调
static void facility_info_event_cb(lv_event_t *e) {

    lv_event_code_t code = lv_event_get_code(e);

    // 提前获取 key，方便后面判断
    uint32_t key = 0;
    if (code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 导航与返回逻辑 (仅处理按键)
    if (code == LV_EVENT_KEY) {
        
        // --- ESC 返回 ---
        if (key == LV_KEY_ESC) {
            load_sys_info_menu_screen(); // 返回上一级系统信息主菜单界面
            return; // 防止继续执行下面代码
        }

        // --- 方向键导航 ---
        lv_group_t* group = UiManager::getInstance()->getKeypadGroup();
        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(group);// 向下/向右导航
        }
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_group_focus_prev(group);// 向上/向左导航
        }
    }

}

//设备信息界面
void load_facility_info_screen() {

    if (scr_facility_info){
        lv_obj_delete(scr_facility_info);
        scr_facility_info = nullptr;
    }

    BaseScreenParts parts = create_base_screen("设备信息");
    scr_facility_info = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::FACILITY_INFO, &scr_facility_info);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_facility_info, [](lv_event_t * e) {
        scr_facility_info = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器

    // 创建按钮
    create_sys_list_btn(list, "1. ", "", "设备名称", facility_info_event_cb, (const char*)(intptr_t)0);
    create_sys_list_btn(list, "2. ", "", "机器号", facility_info_event_cb, (const char*)(intptr_t)1);
    create_sys_list_btn(list, "3. ", "", "固定版本", facility_info_event_cb, (const char*)(intptr_t)2);

    uint32_t child_cnt = lv_obj_get_child_cnt(list);// 遍历容器子对象(按钮)加入组
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个按钮
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_facility_info);
    UiManager::getInstance()->destroyAllScreensExcept(scr_facility_info);// 加载后销毁其他屏幕，保持资源清晰

}


} // namespace sys_info
} // namespace ui