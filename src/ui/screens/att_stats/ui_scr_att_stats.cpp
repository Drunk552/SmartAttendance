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
static lv_obj_t *scr_download_all = nullptr;// 下载报表界面
static lv_obj_t *scr_download_personal = nullptr;// 下载个人报表界面

// ===================== 考勤统计 START =================

// --- 辅助：通用弹窗 ---
static void show_popup_local(const char* title, const char* msg) {
    lv_obj_t* mbox = lv_msgbox_create(lv_screen_active());
    lv_msgbox_add_title(mbox, title);
    lv_msgbox_add_text(mbox, msg);
    lv_msgbox_add_close_button(mbox);
}

// --- 辅助：ESC 键返回考勤统计菜单 ---
static void report_esc_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_KEY) {
        if (lv_event_get_key(e) == LV_KEY_ESC) {
            // 异步安全返回考勤统计菜单
            lv_async_call([](void*) { ui::att_stats::load_att_stats_menu_screen(); }, nullptr);
        }
    }
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
void load_download_all_screen() {
    if(scr_download_all) lv_obj_delete(scr_download_all);

    BaseScreenParts parts = create_base_screen("下载考勤报表");
    scr_download_all = parts.screen;
    lv_obj_add_style(scr_download_all, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::ALL_ATT_STATS, &scr_download_all);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_download_all, [](lv_event_t * e) {
        scr_download_all = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    // ==========================================
    // 3. 创建内容容器 (挂在 parts.content 上)
    // 这样做是为了保持你原有的 flex 布局和 get_child 索引逻辑完全不变
    // ==========================================
    lv_obj_t* cont = lv_obj_create(parts.content);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_set_style_pad_row(cont, 25, 0);// 设置 Flex 布局的子元素间距，25 代表每个控件之间隔开 25 像素
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0); // 透明背景
    lv_obj_set_style_border_width(cont, 0, 0);       // 去掉边框
    lv_obj_set_style_pad_all(cont, 10, 0);// 内边距让内容不贴边

    // 第 0 个子对象：标题 Label
    lv_obj_t* title_label = lv_label_create(cont); // 先拿到指针
    lv_label_set_text(title_label, "下载考勤报表 (YYYY-MM-DD)");
    lv_obj_add_style(title_label, &style_text_cn, 0);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0); // 设为白色

    // 第 1 个子对象：开始时间
    lv_obj_t* ta_s = lv_textarea_create(cont);
    lv_textarea_set_placeholder_text(ta_s, "Start/开始: 2026-01-01");
    lv_textarea_set_one_line(ta_s, true);
    lv_obj_set_width(ta_s, 200);
    lv_obj_add_event_cb(ta_s, ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ta_s, report_esc_event_cb, LV_EVENT_KEY, NULL);
    UiManager::getInstance()->addObjToGroup(ta_s);

    // 第 2 个子对象：结束时间
    lv_obj_t* ta_e = lv_textarea_create(cont);
    lv_textarea_set_placeholder_text(ta_e, "End/结束: 2026-01-31");
    lv_textarea_set_one_line(ta_e, true);
    lv_obj_set_width(ta_e, 200);
    lv_obj_add_event_cb(ta_e, ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ta_e, report_esc_event_cb, LV_EVENT_KEY, NULL);
    UiManager::getInstance()->addObjToGroup(ta_e);

    // 第 3 个子对象：下载按钮
    lv_obj_t* btn = lv_button_create(cont); 
    lv_obj_set_width(btn, 150);
    lv_label_set_text(lv_label_create(btn), "Download/下载");
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn, report_esc_event_cb, LV_EVENT_KEY, NULL);
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

    // 兜底 ESC 返回逻辑
    lv_obj_add_event_cb(scr_download_all, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) {
            ui::att_stats::load_att_stats_menu_screen(); // 按ESC退回考勤统计菜单
        }
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_download_all);

    // 加载这个新屏幕，并销毁旧屏幕
    lv_screen_load(scr_download_all);
    UiManager::getInstance()->destroyAllScreensExcept(scr_download_all);

}

// --- 界面 B: 下载个人报表 ---
void load_download_personal_screen() {
    if (scr_download_personal) lv_obj_delete(scr_download_personal);

    BaseScreenParts parts = create_base_screen("下载个人考勤报表");
    scr_download_personal = parts.screen;
    lv_obj_add_style(scr_download_personal, &style_base, 0);

    UiManager::getInstance()->registerScreen(ScreenType::PERSONAGE_ATT_STATS, &scr_download_personal);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_download_personal, [](lv_event_t * e) {
        scr_download_personal = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    // 创建内容容器 (直接挂在 parts.content 上)
    lv_obj_t* cont = lv_obj_create(parts.content);
    lv_obj_set_size(cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 15, 0);// 设置 Flex 布局的子元素间距，15 代表每个控件之间隔开 15 像素
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 10, 0);// 内边距让内容不贴边

    // [子对象0] 标题
    lv_obj_t* title_label = lv_label_create(cont);
    lv_label_set_text(title_label, "下载个人考勤报表 (YYYY-MM-DD)");
    lv_obj_add_style(title_label, &style_text_cn, 0);
    lv_obj_set_style_text_color(title_label, THEME_COLOR_TEXT_MAIN, 0);

    // [子对象1] 用户 ID 输入框
    lv_obj_t* ta_id = lv_textarea_create(cont);
    lv_textarea_set_placeholder_text(ta_id, "User ID/工号: 0");
    lv_textarea_set_one_line(ta_id, true);
    lv_obj_set_width(ta_id,200);
    lv_obj_add_event_cb(ta_id, ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ta_id, report_esc_event_cb, LV_EVENT_KEY, NULL);
    UiManager::getInstance()->addObjToGroup(ta_id);

    // [子对象2] 开始时间 输入框
    lv_obj_t* ta_s = lv_textarea_create(cont);
    lv_textarea_set_placeholder_text(ta_s, "Start/开始: 2026-01-01");
    lv_textarea_set_one_line(ta_s, true);
    lv_obj_set_width(ta_s,200);
    lv_obj_add_event_cb(ta_s, ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ta_s, report_esc_event_cb, LV_EVENT_KEY, NULL);
    UiManager::getInstance()->addObjToGroup(ta_s);

    // [子对象3] 结束时间 输入框
    lv_obj_t* ta_e = lv_textarea_create(cont);
    lv_textarea_set_placeholder_text(ta_e, "End/结束: 2026-01-01");
    lv_textarea_set_one_line(ta_e, true);
    lv_obj_set_width(ta_e,200);
    lv_obj_add_event_cb(ta_e, ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ta_e, report_esc_event_cb, LV_EVENT_KEY, NULL);
    UiManager::getInstance()->addObjToGroup(ta_e);

    // [子对象4] 下载按钮
    lv_obj_t* btn = lv_button_create(cont);
    lv_label_set_text(lv_label_create(btn), "Download/下载");
    lv_obj_set_width(btn,150);
    lv_obj_add_style(btn, &style_btn_focused, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(btn, report_esc_event_cb, LV_EVENT_KEY, NULL);
    UiManager::getInstance()->addObjToGroup(btn);

    // === 按钮点击事件 (注意获取子对象索引变为1, 2, 3) ===
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        lv_obj_t* btn_obj = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* cont_obj = lv_obj_get_parent(btn_obj);
        
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

    // 默认聚焦 ID 输入框
    lv_group_focus_obj(ta_id);

    // 返回逻辑与屏幕加载
    lv_obj_add_event_cb(scr_download_personal, [](lv_event_t* e){
        if(lv_event_get_key(e) == LV_KEY_ESC) {
            ui::att_stats::load_att_stats_menu_screen(); // 按 ESC 返回考勤统计菜单
        }
    }, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->addObjToGroup(scr_download_personal);

    // 加载并清理
    lv_screen_load(scr_download_personal);
    UiManager::getInstance()->destroyAllScreensExcept(scr_download_personal);
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
            if (index == 0) {
                // A：下载考勤报表
                load_download_all_screen(); 
            } 
            else if (index == 1) {
                // B：下载个人报表
                load_download_personal_screen();
            }
            else {
                show_popup_local("Info", "Coming Soon"); 
            }
        }
    }
}

// 创建界面(create)
void create_att_stats_menu_screen() {
    if (scr_stats) return;

    BaseScreenParts parts = create_base_screen("att_stats / 考勤统计");
    scr_stats = parts.screen;
    // 1. 创建屏幕 - 统一使用黑色背景
    lv_obj_add_style(scr_stats, &style_base, 0);
    UiManager::getInstance()->registerScreen(ScreenType::ATT_STATS, &scr_stats);

    // 3. Grid 布局 - 1列 5行 (列表式布局，清晰易读)
    static int32_t col_dsc[] = {220, LV_GRID_TEMPLATE_LAST}; 
    static int32_t row_dsc[] = {45, 45, 45, 45, 45, LV_GRID_TEMPLATE_LAST}; 

    sub_screen_cont = lv_obj_create(parts.content);
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
    create_sys_grid_btn(sub_screen_cont, 0, LV_SYMBOL_DOWNLOAD, "", "下载考勤报表", 
                        stats_menu_btn_cb, (const char*)(intptr_t)0);

    // 按钮 1: 下载个人考勤报表
    create_sys_grid_btn(sub_screen_cont, 1, LV_SYMBOL_EDIT, "", "下载个人考勤报表", 
                        stats_menu_btn_cb, (const char*)(intptr_t)1);

    // 按钮 2: 下载员工设置 (占位)
    create_sys_grid_btn(sub_screen_cont, 2, LV_SYMBOL_SETTINGS, "", "下载员工设置", 
                        stats_menu_btn_cb, (const char*)(intptr_t)2);

    // 按钮 3: 上传员工设置 (占位)
    create_sys_grid_btn(sub_screen_cont, 3, LV_SYMBOL_UPLOAD, "", "上传员工设置", 
                        stats_menu_btn_cb, (const char*)(intptr_t)3);
                        
    // 按钮 4: 下载员工数据 (占位)
    create_sys_grid_btn(sub_screen_cont, 4, LV_SYMBOL_SD_CARD, "", "下载员工数据", 
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