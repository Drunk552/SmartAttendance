#include "ui_scr_att_stats.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../../ui_controller.h"
#include "../menu/ui_scr_menu.h"

#include <thread>
#include <string>
#include <cstdio>
#include <ctime>
#include <cctype> // 需要用到 isdigit

namespace ui {
namespace att_stats {

// ================= [内部状态: 屏幕指针] =================
static lv_obj_t *scr_stats = nullptr;
static lv_obj_t *sub_screen_cont = nullptr; // 用于显示子界面
static lv_obj_t *scr_download_all = nullptr;// 下载报表界面
static lv_obj_t *scr_download_personal = nullptr;// 下载个人报表界面

// ================= [内部状态: 输入框指针] =================
static lv_obj_t* g_ta_dl_all_start = nullptr;     // 全员报表下载界面开始时间输入框
static lv_obj_t* g_ta_dl_all_end = nullptr;       // 全员报表下载界面结束时间输入框
static lv_obj_t* g_btn_dl_all_confirm = nullptr;  // 全员报表下载界面确认下载按钮
static lv_obj_t* g_ta_dl_psn_uid = nullptr;       // 个人考勤报表下载界面工号输入框
static lv_obj_t* g_ta_dl_psn_start = nullptr;     // 个人考勤报表下载界面开始时间输入框
static lv_obj_t* g_ta_dl_psn_end = nullptr;       // 个人考勤报表下载界面结束时间输入框
static lv_obj_t* g_btn_dl_psn_confirm = nullptr;  // 个人考勤报表下载界面确认下载按钮

// ================= [内部状态: 控件与数据] =================


// ================= [内部状态: 注册临时数据暂存] =================


// ===================== 辅助函数 =================

// 辅助函数：严格校验日期格式是否为 YYYY-MM-DD
static bool is_valid_date_format(const std::string& date) {
    // 1. 检查长度是否严格为 10位 (例如 2026-01-01)
    if (date.length() != 10) return false;
    
    // 2. 检查横杠的位置
    if (date[4] != '-' || date[7] != '-') return false;
    
    // 3. 检查其他位置是否都是数字
    for (int i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) continue;
        if (!isdigit(date[i])) return false;
    }
    
    // 4. 提取年月日进行简单的逻辑校验
    int year = std::stoi(date.substr(0, 4));
    int month = std::stoi(date.substr(5, 2));
    int day = std::stoi(date.substr(8, 2));
    
    if (year < 2000 || year > 2100) return false; // 限制合理年份
    if (month < 1 || month > 12) return false;    // 月份 1-12
    if (day < 1 || day > 31) return false;        // 天数 1-31 (粗略校验即可，防止崩溃)
    
    return true;
}

// 辅助函数：获取当前系统日期，格式为 YYYY-MM-DD
static std::string get_current_date_str() {
    time_t now = time(nullptr);
    struct tm tstruct;
    char buf[20];
    tstruct = *localtime(&now);
    // 格式化为 YYYY-MM-DD
    strftime(buf, sizeof(buf), "%Y-%m-%d", &tstruct);
    return std::string(buf);
}

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
        show_popup_msg("导出全员考勤报表失败", "全员报表导出成功!\n报表已下载至U盘!", nullptr, "我知道了");
    } else {
        show_popup_msg("导出全员考勤报表成功", "全员报表导出失败!\n请检查设备!", nullptr, "我知道了");
    }
    
    // 释放堆内存
    delete ctx;
}

//下载考勤报表 (全员)事件回调函数
static void download_all_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 

    uint32_t key = 0;
    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 处理 ESC 键退出 (返回考勤统计菜单)
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act()); 
        ui::att_stats::load_att_stats_menu_screen(); 
        return;
    }

    // ================= 焦点在【开始时间输入框】 =================
    if (current_target == g_ta_dl_all_start) {
        // 按下回车或↓键，跳到结束时间输入框
        if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_ta_dl_all_end);
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【结束时间输入框】 =================
    else if (current_target == g_ta_dl_all_end) {
        // 按下↑键，跳回开始时间
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_all_start);
            return; 
        }
        // 按下回车或↓键，跳到下载按钮
        else if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_btn_dl_all_confirm);
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【下载按钮】 =================
    else if (current_target == g_btn_dl_all_confirm) {
        // 按下↑键，跳回结束时间
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_all_end);
            return; 
        }

        // 按下确认下载
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            lv_indev_wait_release(lv_indev_get_act());
            
            std::string s_txt = lv_textarea_get_text(g_ta_dl_all_start);
            std::string e_txt = lv_textarea_get_text(g_ta_dl_all_end);

            // 1. 判空校验
            if (s_txt.empty() || e_txt.empty()) {
                show_popup_msg("导出全员考勤报表失败", "导出考勤报表失败!\n请输入有效的日期!", nullptr, "我知道了");;
                return;
            }

            // 2. 格式校验
            if (!is_valid_date_format(s_txt) || !is_valid_date_format(e_txt)) {
                show_popup_msg("导出全员考勤报表失败", "日期格式必须为:\nYYYY-MM-DD\n(例如 2026-01-01)",nullptr, "我知道了");
                // 焦点回到填错的框
                lv_group_focus_obj(is_valid_date_format(s_txt) ? g_ta_dl_all_end : g_ta_dl_all_start);
                return;
            }
            
            // ================== 业务逻辑时间穿越校验  ==================
            std::string current_date = get_current_date_str();

            // 3. 检查是否包含未来时间
            if (s_txt > current_date || e_txt > current_date) {
                show_popup_msg("导出全员考勤报表失败", ("时间错误,无法导出未来时间的报表！\n当前日期: " + current_date).c_str(),nullptr, "我知道了");
                return;
            }

            // 4. 检查开始时间是否晚于结束时间
            if (s_txt > e_txt) {
                show_popup_msg("导出全员考勤报表失败", "时间错误,【开始时间】不能晚于\n【结束时间】!",nullptr, "我知道了");
                lv_group_focus_obj(g_ta_dl_all_start); // 焦点移回开始时间让用户修改
                return;
            }

            // 创建加载圈并启动后台导出线程
            lv_obj_t* spin = lv_spinner_create(lv_screen_active());
            lv_obj_center(spin);
            
            std::thread([s_txt, e_txt, spin](){
                bool ret = UiController::getInstance()->exportCustomReport(s_txt.c_str(), e_txt.c_str());
                AsyncExportCtx* ctx = new AsyncExportCtx{spin, ret};
                lv_async_call(ui_on_export_complete, ctx);
            }).detach();
        }
    }
}

// 下载考勤报表 (全员)界面
void load_download_all_screen() {
    if(scr_download_all){
        lv_obj_delete(scr_download_all);
        scr_download_all = nullptr;
    }

    // 1. 创建基础屏幕
    BaseScreenParts parts = create_base_screen("下载全员考勤报表");
    scr_download_all = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::ALL_ATT_STATS, &scr_download_all);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_download_all, [](lv_event_t * e) {
        scr_download_all = nullptr;
        g_ta_dl_all_start = nullptr;
        g_ta_dl_all_end = nullptr;
        g_btn_dl_all_confirm = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    // 绑定全局 ESC 返回事件
    lv_obj_add_event_cb(scr_download_all, download_all_event_cb, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->resetKeypadGroup();

    // 创建统一表单容器
    lv_obj_t* form_cont = create_form_container(parts.content);

    // 1. 开始时间输入框
    g_ta_dl_all_start = create_form_input(form_cont, "开始时间:", "如: 2026-01-01", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_all_start, "0123456789-"); // 只允许输入数字和横杠
    lv_obj_add_event_cb(g_ta_dl_all_start, download_all_event_cb, LV_EVENT_ALL, nullptr);
    UiManager::getInstance()->addObjToGroup(g_ta_dl_all_start);

    // 2. 结束时间输入框
    g_ta_dl_all_end = create_form_input(form_cont, "结束时间:", "如: 2026-01-31", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_all_start, "0123456789-"); // 只允许输入数字和横杠
    lv_obj_add_event_cb(g_ta_dl_all_end, download_all_event_cb, LV_EVENT_ALL, nullptr);
    UiManager::getInstance()->addObjToGroup(g_ta_dl_all_end);

    // 3. 确认下载按钮
    // create_form_btn 会自动帮你绑定 LV_EVENT_CLICKED
    g_btn_dl_all_confirm = create_form_btn(form_cont, "确认下载", download_all_event_cb, nullptr);
    // 补充绑定 LV_EVENT_KEY 以处理键盘的 UP/DOWN 焦点跳转
    lv_obj_add_event_cb(g_btn_dl_all_confirm, download_all_event_cb, LV_EVENT_KEY, nullptr); 
    UiManager::getInstance()->addObjToGroup(g_btn_dl_all_confirm);

    // 默认聚焦在开始时间
    lv_group_focus_obj(g_ta_dl_all_start);

    lv_screen_load(scr_download_all);
    UiManager::getInstance()->destroyAllScreensExcept(scr_download_all);
}

// --- 界面 B: 下载个人报表 ---
void load_download_personal_screen() {
    if (scr_download_personal){
        lv_obj_delete(scr_download_personal);
        scr_download_personal = nullptr;
    }

    BaseScreenParts parts = create_base_screen("下载个人考勤报表");
    scr_download_personal = parts.screen;
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

//考勤统计界面按键事件回调
static void stats_menu_btn_cb(lv_event_t *e) {
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
            ui::menu::load_menu_screen(); // 返回上一级
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
            // A：下载考勤报表
            load_download_all_screen(); // 跳转到下载考勤报表界面
        } 
        else if (index == 1) {
            // B：下载个人报表
            load_download_personal_screen();// 跳转到下载个人报表界面
        }
        else {
            show_popup_local("Info", "Coming Soon"); // 其他功能占位
        }
    }
}

// 创建界面(create)-考勤统计界面
void create_att_stats_menu_screen() {
    if (scr_stats){
        lv_obj_delete(scr_stats);
        scr_stats = nullptr;
    }

    BaseScreenParts parts = create_base_screen("考勤统计");
    scr_stats = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::ATT_STATS, &scr_stats);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_stats, [](lv_event_t * e) {
        scr_stats = nullptr;
        sub_screen_cont = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器

    // 创建按钮 
    // 参数: 父对象, 行号, 图标, 英文标题, 中文标题, 回调, 索引(作为user_data)
    
    // 按钮 0: 下载考勤报表
    create_sys_list_btn(list, "1. ", "", "下载考勤报表", stats_menu_btn_cb, (const char*)(intptr_t)0);

    // 按钮 1: 下载个人考勤报表
    create_sys_list_btn(list, "2. ", "", "下载个人考勤报表", stats_menu_btn_cb, (const char*)(intptr_t)1);

    // 按钮 2: 下载员工设置 (占位)
    create_sys_list_btn(list, "3. ", "", "下载员工设置", stats_menu_btn_cb, (const char*)(intptr_t)2);

    // 按钮 3: 上传员工设置 (占位)
    create_sys_list_btn(list, "4. ", "", "上传员工设置", stats_menu_btn_cb, (const char*)(intptr_t)3);
                        
    // 按钮 4: 下载员工数据 (占位)
    create_sys_list_btn(list, "5. ", "", "下载员工数据", stats_menu_btn_cb, (const char*)(intptr_t)4);

    // 遍历容器子对象(按钮)加入组
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for(uint32_t i=0; i < child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_stats);
    UiManager::getInstance()->destroyAllScreensExcept(scr_stats);// 加载后销毁其他屏幕，保持资源清晰

}

// 加载界面 (Load)
void load_att_stats_menu_screen() {
    if (!scr_stats) {
        create_att_stats_menu_screen();
    } 
    else {
        // 如果屏幕已经存在（只是切回来），我们需要重新把按键组接管过来
        // 这部分逻辑仿照 ui_user_mgmt.cpp 的加载逻辑
        UiManager::getInstance()->resetKeypadGroup();
        if (sub_screen_cont) {
             uint32_t child_cnt = lv_obj_get_child_cnt(sub_screen_cont);
             for(uint32_t i=0; i < child_cnt; i++) {
                 UiManager::getInstance()->addObjToGroup(lv_obj_get_child(sub_screen_cont, i));
             }
             if(child_cnt > 0) {
                 lv_group_focus_obj(lv_obj_get_child(sub_screen_cont, 0));
             }
        }
    }

    std::printf("[UI] Enter: Attendance Stats\n");

    // 切换屏幕
    lv_screen_load(scr_stats);

    // 销毁其他屏幕，节省内存
    UiManager::getInstance()->destroyAllScreensExcept(scr_stats);
}

} // namespace att_stats
} // namespace ui