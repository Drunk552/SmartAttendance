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

static void stats_menu_btn_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);
    intptr_t index = (intptr_t)lv_event_get_user_data(e);
    lv_group_t* group = UiManager::getInstance()->getKeypadGroup();// 获取全局按键组

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // 处理 ESC 返回主菜单
        if (key == LV_KEY_ESC) {
            ui::menu::load_screen();
            return;
        }

        // 处理上下键导航 (在 Grid 中循环切换)
        if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(group);
        }
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_group_focus_prev(group);
        }
        // 处理确认键
        else if (key == LV_KEY_ENTER) {
            // 创建新屏幕进入具体功能
            lv_obj_t* new_scr = lv_obj_create(NULL);
            
            // 为子屏幕绑定 ESC 返回逻辑 (简单的返回上一级)
            // 注意：这里我们简单地重新加载当前菜单来实现“返回”
            lv_obj_add_event_cb(new_scr, [](lv_event_t* e){
                if(lv_event_get_key(e) == LV_KEY_ESC) {
                    // 为避免前向声明问题，我们暂时直接调用 load_menu_screen，或者你需要前向声明 load_att_stats_screen
                    // 这里为了代码编译通过，先返回主菜单，或者你可以自己实现返回上一级
                    ui::menu::load_screen(); 
                }
            }, LV_EVENT_KEY, NULL);
            
            // 将新屏幕加入组，以便接收按键
            UiManager::getInstance()->resetKeypadGroup();
            UiManager::getInstance()->addObjToGroup(new_scr);

            if (index == 0) create_download_all_screen(new_scr);
            else if (index == 1) create_download_personal_screen(new_scr);
            else {
                lv_obj_delete(new_scr); // 不需要新屏幕了
                ui::menu::load_screen();     // 恢复环境
                show_popup("Info", "Coming Soon"); 
                return;
            }

            lv_screen_load_anim(new_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, true);
        }
    }
}

// 创建界面(create)
void create_att_stats_menu_screen() {
    if (scr_stats) return;

    // 1. 创建屏幕 - 统一使用黑色背景
    scr_stats = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_stats, lv_color_black(), 0);
    UiManager::getInstance()->registerScreen(ScreenType::ATT_STATS, &scr_stats);

    // 2. 标题 - 统一顶部标题样式
    lv_obj_t *title = lv_label_create(scr_stats);
    lv_label_set_text(title, "Attendance Stats / 考勤统计");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_style(title, &style_text_cn, 0); // 应用中文字体

    // 3. Grid 布局 - 1列 5行 (列表式布局，清晰易读)
    static int32_t col_dsc[] = {220, LV_GRID_TEMPLATE_LAST}; 
    static int32_t row_dsc[] = {45, 45, 45, 45, 45, LV_GRID_TEMPLATE_LAST}; 

    sub_screen_cont = lv_obj_create(scr_stats);
    lv_obj_set_size(sub_screen_cont, 230, 260); // 调整容器大小适配内容
    lv_obj_align(sub_screen_cont, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_layout(sub_screen_cont, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(sub_screen_cont, col_dsc, row_dsc);
    
    // 样式改为透明
    lv_obj_set_style_bg_opa(sub_screen_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sub_screen_cont, 0, 0);
    lv_obj_set_style_pad_row(sub_screen_cont, 5, 0); // 按钮间距

    // 4. 创建按钮 
    // 参数: 父对象, 行号, 图标, 英文标题, 中文标题, 回调, 索引(作为user_data)
    
    // 按钮 0: 下载考勤报表
    create_sys_grid_btn(sub_screen_cont, 0, LV_SYMBOL_DOWNLOAD, "", "考勤报表", 
                        stats_menu_btn_cb, (const char*)(intptr_t)0);

    // 按钮 1: 下载个人报表
    create_sys_grid_btn(sub_screen_cont, 1, LV_SYMBOL_EDIT, "", "个人报表", 
                        stats_menu_btn_cb, (const char*)(intptr_t)1);

    // 按钮 2: 下载设置 (占位)
    create_sys_grid_btn(sub_screen_cont, 2, LV_SYMBOL_SETTINGS, "", "下载设置", 
                        stats_menu_btn_cb, (const char*)(intptr_t)2);

    // 按钮 3: 上传设置 (占位)
    create_sys_grid_btn(sub_screen_cont, 3, LV_SYMBOL_UPLOAD, "", "上传设置", 
                        stats_menu_btn_cb, (const char*)(intptr_t)3);
                        
    // 按钮 4: 下载数据 (占位)
    create_sys_grid_btn(sub_screen_cont, 4, LV_SYMBOL_SD_CARD, "", "下载数据", 
                        stats_menu_btn_cb, (const char*)(intptr_t)4);
}

// 加载界面 (Load)
void load_att_stats_menu_screen() {
    if (!scr_stats) create_att_stats_menu_screen();

    std::printf("[UI] Enter: Attendance Stats\n");

    lv_group_t* group = UiManager::getInstance()->getKeypadGroup();
    // 关键修复：正确设置输入组
    lv_group_remove_all_objs(group);
    
    if (sub_screen_cont) {
        uint32_t cnt = lv_obj_get_child_cnt(sub_screen_cont);
        for(uint32_t i=0; i<cnt; i++) {
            lv_group_add_obj(group, lv_obj_get_child(sub_screen_cont, i));
        }
        // 默认聚焦第一个按钮
        if(cnt > 0) lv_group_focus_obj(lv_obj_get_child(sub_screen_cont, 0));
    }

    // 还要把背景加入组，以防焦点丢失时按键失效
    lv_group_add_obj(group, scr_stats);

    lv_screen_load(scr_stats);
    UiManager::getInstance()->destroyAllScreensExcept(scr_stats);
}

} // namespace att_stats
} // namespace ui