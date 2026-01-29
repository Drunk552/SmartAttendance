#include "ui_scr_att_stats.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../../ui_controller.h"
#include "../menu/ui_scr_menu.h"

#include <thread>
#include <string>
#include <cstdio>

namespace ui {
namespace att_stats {

static lv_obj_t *scr_stats = nullptr;
static lv_obj_t *sub_screen_cont = nullptr; // 用于显示子界面

// ===================== 考勤统计 START =================

// --- 辅助：通用弹窗 ---
static void show_popup_local(const char* title, const char* msg) {
    lv_obj_t* mbox = lv_msgbox_create(lv_screen_active());
    lv_msgbox_add_title(mbox, title);
    lv_msgbox_add_text(mbox, msg);
    lv_msgbox_add_close_button(mbox);
}

// --- 聚焦高亮事件 ---
static void ta_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        lv_textarea_set_cursor_pos(ta, 0);
    } else if (code == LV_EVENT_DEFOCUSED) {
        lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
    }
}

// ====================== 异步导出相关 =================

// 1. 定义上下文结构体
struct AsyncExportCtx {
    lv_obj_t* spinner; // 加载圈指针
    bool success;      // 导出结果
};

// 2. UI线程回调函数 (由 lv_async_call 触发)
static void ui_on_export_complete(void* data) {
    AsyncExportCtx* ctx = (AsyncExportCtx*)data;
    
    // 移除加载圈
    if (ctx->spinner && lv_obj_is_valid(ctx->spinner)) {
        lv_obj_delete(ctx->spinner);
    }
    
    // 显示结果弹窗
    if (ctx->success) {
        show_popup_local("Success", "Report Downloaded to USB!");
    } else {
        show_popup_local("Failed", "Check USB or Dates.");
    }
    
    // 释放堆内存
    delete ctx;
}

// --- 界面 A: 下载考勤报表 (全员) ---
static void create_download_all_screen(lv_obj_t* parent) {
    lv_obj_clean(parent); // 清理容器

    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);

    lv_label_set_text(lv_label_create(cont), "Download Report (YYYY-MM-DD)");
    lv_obj_add_style(lv_obj_get_child(cont, 0), &style_text_cn, 0);

    // 开始时间
    lv_obj_t* ta_s = lv_textarea_create(cont);
    lv_textarea_set_placeholder_text(ta_s, "Start: 2024-01-01");
    lv_textarea_set_one_line(ta_s, true);
    lv_obj_set_width(ta_s, 200);
    lv_obj_add_event_cb(ta_s, ta_event_cb, LV_EVENT_ALL, NULL);
    UiManager::getInstance()->addObjToGroup(ta_s);

    // 结束时间
    lv_obj_t* ta_e = lv_textarea_create(cont);
    lv_textarea_set_placeholder_text(ta_e, "End: 2024-01-31");
    lv_textarea_set_one_line(ta_e, true);
    lv_obj_set_width(ta_e, 200);
    lv_obj_add_event_cb(ta_e, ta_event_cb, LV_EVENT_ALL, NULL);
    UiManager::getInstance()->addObjToGroup(ta_e);

    // 下载按钮
    lv_obj_t* btn = lv_button_create(cont);
    lv_obj_set_width(btn, 120);
    lv_label_set_text(lv_label_create(btn), "Download");
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
    UiManager::getInstance()->addObjToGroup(btn);
    
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        lv_obj_t* btn_obj = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* cont_obj = lv_obj_get_parent(btn_obj);
        
        // 获取输入框对象 (根据添加顺序: label=0, ta_s=1, ta_e=2, btn=3)
        //  依赖子对象索引
        lv_obj_t* t_s = lv_obj_get_child(cont_obj, 1);
        lv_obj_t* t_e = lv_obj_get_child(cont_obj, 2);
        
        std::string s_txt = lv_textarea_get_text(t_s);
        std::string e_txt = lv_textarea_get_text(t_e);

        // 1. 基础校验
        if (s_txt.empty() || e_txt.empty()) {
            show_popup_local("Error", "Please enter valid dates!");
            return;
        }
        
        // 2. 创建加载圈
        lv_obj_t* spin = lv_spinner_create(lv_screen_active());
        lv_obj_center(spin);
        
        // 3. 启动分离线程
        std::thread([s_txt, e_txt, spin](){
            // 调用业务层
            bool ret = UiController::getInstance()->exportCustomReport(s_txt.c_str(), e_txt.c_str());
            
            AsyncExportCtx* ctx = new AsyncExportCtx{spin, ret};
            lv_async_call(ui_on_export_complete, ctx);
            
        }).detach();

    }, LV_EVENT_CLICKED, NULL);

    // 默认聚焦
    lv_group_focus_obj(ta_s);
}

// --- 界面 B: 下载个人报表 ---
static void create_download_personal_screen(lv_obj_t* parent) {
    lv_obj_clean(parent);

    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);

    lv_label_set_text(lv_label_create(cont), "Personal Report Download");
    lv_obj_add_style(lv_obj_get_child(cont, 0), &style_text_cn, 0);

    // 工号
    lv_obj_t* ta_id = lv_textarea_create(cont);
    lv_textarea_set_placeholder_text(ta_id, "User ID");
    lv_textarea_set_one_line(ta_id, true);
    lv_obj_set_width(ta_id, 200);
    lv_obj_add_event_cb(ta_id, ta_event_cb, LV_EVENT_ALL, NULL);
    UiManager::getInstance()->addObjToGroup(ta_id);

    // 开始时间
    lv_obj_t* ta_s = lv_textarea_create(cont);
    lv_textarea_set_placeholder_text(ta_s, "Start Date");
    lv_textarea_set_one_line(ta_s, true);
    lv_obj_set_width(ta_s, 200);
    lv_obj_add_event_cb(ta_s, ta_event_cb, LV_EVENT_ALL, NULL);
    UiManager::getInstance()->addObjToGroup(ta_s);
    
    // 结束时间
    lv_obj_t* ta_e = lv_textarea_create(cont);
    lv_textarea_set_placeholder_text(ta_e, "End Date");
    lv_textarea_set_one_line(ta_e, true);
    lv_obj_set_width(ta_e, 200);
    lv_obj_add_event_cb(ta_e, ta_event_cb, LV_EVENT_ALL, NULL);
    UiManager::getInstance()->addObjToGroup(ta_e);

    // 按钮
    lv_obj_t* btn = lv_button_create(cont);
    lv_obj_set_width(btn, 120);
    lv_label_set_text(lv_label_create(btn), "Download");
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
    UiManager::getInstance()->addObjToGroup(btn);

    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        lv_obj_t* btn_obj = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* cont_obj = lv_obj_get_parent(btn_obj);
        
        // 依赖子对象索引: label=0, ta_id=1, ta_s=2, ta_e=3, btn=4
        lv_obj_t* t_id = lv_obj_get_child(cont_obj, 1);
        lv_obj_t* t_s = lv_obj_get_child(cont_obj, 2); 
        lv_obj_t* t_e = lv_obj_get_child(cont_obj, 3);

        const char* id_txt = lv_textarea_get_text(t_id);
        if (strlen(id_txt) == 0) { show_popup_local("Error", "Enter User ID"); return; }
        
        // 校验 ID
        int uid = atoi(id_txt);
        UserData u = UiController::getInstance()->getUserInfo(uid);
        if (u.id == 0) {
            show_popup_local("Error", "User ID not found!");
            return;
        }

        std::string s_txt = lv_textarea_get_text(t_s);
        std::string e_txt = lv_textarea_get_text(t_e);

        lv_obj_t* spin = lv_spinner_create(lv_screen_active());
        lv_obj_center(spin);

        std::thread([uid, s_txt, e_txt, spin](){
            bool ret = UiController::getInstance()->exportUserReport(uid, s_txt.c_str(), e_txt.c_str());
            
            AsyncExportCtx* ctx = new AsyncExportCtx{spin, ret};
            lv_async_call(ui_on_export_complete, ctx);

        }).detach();

    }, LV_EVENT_CLICKED, NULL);
    
    // 默认聚焦
    lv_group_focus_obj(ta_id);
}

// ===================== 主入口逻辑 =================

// 菜单点击回调
static void stats_menu_btn_cb(lv_event_t *e) {
    const char* tag = (const char*)lv_event_get_user_data(e);
    if(sub_screen_cont) {
        UiManager::getInstance()->resetKeypadGroup();
        if(strcmp(tag, "ALL") == 0) create_download_all_screen(sub_screen_cont);
        else if(strcmp(tag, "USER") == 0) create_download_personal_screen(sub_screen_cont);
    }
}

void load_screen() {
    if (scr_stats) lv_obj_delete(scr_stats);
    scr_stats = lv_obj_create(nullptr);
    lv_obj_add_style(scr_stats, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::ATT_STATS, &scr_stats);

    // 左侧：菜单栏
    lv_obj_t *menu_col = lv_obj_create(scr_stats);
    lv_obj_set_size(menu_col, 80, LV_PCT(100));
    lv_obj_align(menu_col, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_flex_flow(menu_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(menu_col, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(menu_col, 0, 0);
    lv_obj_set_style_pad_all(menu_col, 5, 0);

    // 右侧：内容区
    sub_screen_cont = lv_obj_create(scr_stats);
    lv_obj_set_size(sub_screen_cont, 160, LV_PCT(100)); // 240 - 80 = 160
    lv_obj_align(sub_screen_cont, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(sub_screen_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sub_screen_cont, 0, 0);

    // 创建左侧菜单按钮
    UiManager::getInstance()->resetKeypadGroup();

    auto add_btn = [&](const char* txt, const char* tag) {
        lv_obj_t* btn = lv_button_create(menu_col);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_label_set_text(lv_label_create(btn), txt);
        lv_obj_add_style(btn, &style_btn_default, 0);
        lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
        lv_obj_add_event_cb(btn, stats_menu_btn_cb, LV_EVENT_CLICKED, (void*)tag);
        // 支持按 Enter 触发
        lv_obj_add_event_cb(btn, [](lv_event_t* e){
            if(lv_event_get_key(e) == LV_KEY_ENTER) {
                lv_obj_send_event((lv_obj_t*)lv_event_get_target(e), LV_EVENT_CLICKED, NULL);
            }
        }, LV_EVENT_KEY, nullptr);
        UiManager::getInstance()->addObjToGroup(btn);
    };

    add_btn("All\nReport", "ALL");
    add_btn("User\nReport", "USER");

    // ESC 返回
    lv_obj_add_event_cb(scr_stats, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) ui::menu::load_screen();
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_stats); // 兜底

    // 默认显示第一个子界面
    create_download_all_screen(sub_screen_cont);

    lv_screen_load(scr_stats);
    UiManager::getInstance()->destroyAllScreensExcept(scr_stats);
}

} // namespace att_stats
} // namespace ui