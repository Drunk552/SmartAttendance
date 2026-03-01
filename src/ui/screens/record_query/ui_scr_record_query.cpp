#include "ui_scr_record_query.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../../ui_controller.h"
#include "../menu/ui_scr_menu.h"
#include <cstdio>
#include <string>
#include <vector>
#include <ctime>
#include <cctype> // 需要用到 isdigit

namespace ui {
namespace record_query {

// ================= [内部状态: 屏幕指针] =================
static lv_obj_t *scr_query = nullptr;//记录查询主菜单界面
static lv_obj_t *scr_job_query = nullptr;//工号查询界面
static lv_obj_t *scr_browse_job_query =nullptr;//浏览工号查询界面
static lv_obj_t *scr_browse_job_query_result =nullptr;//浏览工号查询结果界面
static lv_obj_t *scr_download_job_query = nullptr;//下载工号查询界面
static lv_obj_t* scr_time_query = nullptr;//时间查询界面
static lv_obj_t *scr_browse_time_query = nullptr;//浏览时间查询界面
static lv_obj_t *scr_browse_time_query_result = nullptr;//浏览时间查询结果界面
static lv_obj_t *scr_download_time_query = nullptr;//下载时间查询界面

// ================= [内部状态: 输入框指针] =================
static lv_obj_t *g_ta_dl_browse_job_uid = nullptr;//浏览工号查询工号输入框
static lv_obj_t *g_ta_dl_browse_job_start = nullptr;//浏览工号查询开始输入框
static lv_obj_t *g_ta_dl_browse_job_end = nullptr;//浏览工号查询结束输入框
static lv_obj_t *g_ta_dl_download_job_uid = nullptr;//下载工号查询工号输入框
static lv_obj_t *g_ta_dl_download_job_start = nullptr;//下载工号查询开始输入框
static lv_obj_t *g_ta_dl_download_job_end = nullptr;//下载工号查询结束输入框
static lv_obj_t *g_ta_dl_browse_time_start = nullptr;//浏览时间查询开始输入框
static lv_obj_t *g_ta_dl_browse_time_end = nullptr;//浏览时间查询结束输入框
static lv_obj_t *g_ta_dl_download_time_start = nullptr;//下载时间查询开始输入框
static lv_obj_t *g_ta_dl_download_time_end = nullptr;//下载时间查询结束输入框

// ================= [内部状态: 控件与数据] =================
static lv_obj_t *g_btn_dl_browse_job_confirm =nullptr;//浏览工号查询确认按钮
static lv_obj_t *obj_browse_view = nullptr;//浏览工号查询结果界面容器
static lv_obj_t *time_browse_view = nullptr;//浏览时间查询结果界面容器
static lv_obj_t *g_btn_dl_download_job_confirm =nullptr;//下载工号查询确认按钮
static lv_obj_t *g_btn_dl_browse_time_confirm =nullptr;//浏览时间查询确认按钮
static lv_obj_t *g_btn_dl_download_time_confirm =nullptr;//下载时间查询确认按钮

// ================= [内部状态: 注册临时数据暂存] =================
static int g_job_query_uid = 0;//获取工号查询工号
static time_t g_job_query_start_ts = 0;//获取工号查询开始时间
static time_t g_job_query_end_ts = 0;//获取工号查询结束时间
static int g_time_query_uid = -1;// 告诉底层：我要查所有人
static time_t g_time_query_start_ts = 0;//获取时间查询开始时间
static time_t g_time_query_end_ts = 0;//获取时间查询结束时间

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

// 辅助函数：将 "YYYY-MM-DD" 转换为当天的起/止时间戳
static time_t convert_date_to_timestamp(const std::string& date_str, bool end_of_day) {
    struct tm tm = {0};
    if (sscanf(date_str.c_str(), "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) == 3) {
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        if (end_of_day) {
            tm.tm_hour = 23; tm.tm_min = 59; tm.tm_sec = 59;
        } else {
            tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0;
        }
        return mktime(&tm);
    }
    return 0;
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
        show_popup_msg("下载考勤报表失败", "报表下载成功!\n报表已下载至U盘!", nullptr, "我知道了");
    } else {
        show_popup_msg("下载考勤报表成功", "报表下载失败!\n请检查设备!", nullptr, "我知道了");
    }
    
    // 释放堆内存
    delete ctx;
}


// =========================================================
// 一、 记录查询主菜单 (Record Query) (一级界面)
// =========================================================

//记录查询主菜单事件回调
static void record_query_event_cb(lv_event_t *e) {

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
            load_job_query_screen(); //工号查询
        } 
        else if (index == 1) {
            load_time_query_screen();//时间查询
        }
    }
}

//记录查询主菜单界面
void load_record_query_menu_screen() {
        if (scr_query){
        lv_obj_delete(scr_query);
        scr_query = nullptr;
    }

    BaseScreenParts parts = create_base_screen("记录查询");
    scr_query = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::RECORD_QUERY, &scr_query);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_query, [](lv_event_t * e) {
        scr_query = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器

    // 创建按钮
    create_sys_list_btn(list, "1. ", "", "工号查询", record_query_event_cb, (const char*)(intptr_t)0);
    create_sys_list_btn(list, "2. ", "", "时间查询", record_query_event_cb, (const char*)(intptr_t)1);

    uint32_t child_cnt = lv_obj_get_child_cnt(list);// 遍历容器子对象(按钮)加入组
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个按钮
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_query);
    UiManager::getInstance()->destroyAllScreensExcept(scr_query);// 加载后销毁其他屏幕，保持资源清晰
}


// =========================================================
// 1. 工号查询 (Job Query) (二级界面)
// =========================================================

//工号查询事件回调
static void job_query_event_cb(lv_event_t *e) {

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
            load_record_query_menu_screen(); // 返回上一级记录查询界面
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
            load_browse_job_query_screen(); //浏览工号查询界面
        } 
        else if (index == 1) {
            load_download_job_query_screen ();//下载工号查询界面
        }
    }

}

//工号查询界面
void load_job_query_screen() {
        if (scr_job_query){
        lv_obj_delete(scr_job_query);
        scr_job_query = nullptr;
    }

    BaseScreenParts parts = create_base_screen("工号查询");
    scr_job_query = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::JOB_QUERY, &scr_job_query);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_job_query, [](lv_event_t * e) {
        scr_job_query = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器

    // 创建按钮
    create_sys_list_btn(list, "1. ", "", "浏览", job_query_event_cb, (const char*)(intptr_t)0);
    create_sys_list_btn(list, "2. ", "", "下载", job_query_event_cb, (const char*)(intptr_t)1);

    uint32_t child_cnt = lv_obj_get_child_cnt(list);// 遍历容器子对象(按钮)加入组
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个按钮
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_job_query);
    UiManager::getInstance()->destroyAllScreensExcept(scr_job_query);// 加载后销毁其他屏幕，保持资源清晰
}


// =========================================================
// 1.1 浏览工号查询 (Browse Job Query) (三级界面)
// =========================================================

// 浏览工号查询事件回调
static void browse_job_query_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 

    uint32_t key = 0;
    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 处理 ESC 键退出 (返回工号查询界面)
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act()); 
        load_job_query_screen(); //工号查询界面
        return;
    }
    // ================= 焦点在【工号输入框】 =================
    if (current_target == g_ta_dl_browse_job_uid) {
        if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_ta_dl_browse_job_start);
            lv_indev_wait_release(lv_indev_get_act());
        }
    }
    // ================= 焦点在【开始时间输入框】 =================
    else if (current_target == g_ta_dl_browse_job_start) {
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_browse_job_uid); // ↑跳回工号
            return; 
        } else if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_ta_dl_browse_job_end); // ↓跳到结束时间
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【结束时间输入框】 =================
    else if (current_target == g_ta_dl_browse_job_end) {
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_browse_job_start); // ↑跳回开始时间
            return; 
        } else if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_btn_dl_browse_job_confirm); // ↓跳到确认按钮
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【确认按钮】 =================
    else if (current_target == g_btn_dl_browse_job_confirm) {
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_browse_job_end); // ↑跳回结束时间
            return; 
        }

        // 按下确认按钮
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            lv_indev_wait_release(lv_indev_get_act());
            
            std::string uid_txt = lv_textarea_get_text(g_ta_dl_browse_job_uid);
            std::string s_txt = lv_textarea_get_text(g_ta_dl_browse_job_start);
            std::string e_txt = lv_textarea_get_text(g_ta_dl_browse_job_end);

            // 1. 判空校验
            if (uid_txt.empty() || s_txt.empty() || e_txt.empty()) {
                show_popup_msg("浏览工号查询失败", "工号和时间都不能为空!\n请输入工号和时间!", nullptr, "我知道了");
                return;
            }
            
            // 2. 格式校验 
            if (!is_valid_date_format(s_txt) || !is_valid_date_format(e_txt)) {
                show_popup_msg("浏览工号查询失败", "格式错误！\n日期格式必须为:\nYYYY-MM-DD\n(例如 2026-01-01)", nullptr, "我知道了");
                lv_group_focus_obj(is_valid_date_format(s_txt) ? g_ta_dl_browse_job_end : g_ta_dl_browse_job_start);
                return;
            }

            // 3. 时间穿越与逻辑校验
            std::string current_date = get_current_date_str();

            if (s_txt > current_date || e_txt > current_date) {
                show_popup_msg("浏览工号查询失败", ("时间错误！\n无法导出未来时间的报表!\n当前日期: " + current_date).c_str(), nullptr, "我知道了");
                return;
            }

            if (s_txt > e_txt) {
                show_popup_msg("浏览工号查询失败", "时间错误!\n【开始时间】不能晚于【结束时间】!", nullptr, "我知道了");
                lv_group_focus_obj(g_ta_dl_browse_job_start);
                return;
            }
            
            int uid = std::stoi(uid_txt); // 将工号转为整型

            if (!UiController::getInstance()->checkUserExists(uid)) { 
                show_popup_msg("浏览工号查询失败", "工号错误!\n 该工号不存在,请检查工号! ", nullptr, "我知道了");
                lv_group_focus_obj(g_ta_dl_browse_job_uid); // 焦点移回工号输入框，方便用户重输
                return;
            }

            g_job_query_uid = uid;
            g_job_query_start_ts = convert_date_to_timestamp(s_txt, false); // 当天 00:00:00
            g_job_query_end_ts = convert_date_to_timestamp(e_txt, true);    // 当天 23:59:59

            load_browse_job_query_result_screen ();//跳到四级界面，浏览查询结果界面

        }
    }
}

// 浏览工号查询界面
void load_browse_job_query_screen() {

    if(scr_browse_job_query){
        lv_obj_delete(scr_browse_job_query);
        scr_browse_job_query = nullptr;
    }

    // 1. 创建基础屏幕
    BaseScreenParts parts = create_base_screen("浏览工号查询");
    scr_browse_job_query = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::BROWSE_JOB_QUERY, &scr_browse_job_query);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_browse_job_query, [](lv_event_t * e) {
        scr_browse_job_query = nullptr;
        g_ta_dl_browse_job_uid = nullptr;
        g_ta_dl_browse_job_start = nullptr;
        g_ta_dl_browse_job_end = nullptr;
        g_btn_dl_browse_job_confirm = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t* form_cont = create_form_container(parts.content);

    // 1. 工号输入框
    g_ta_dl_browse_job_uid = create_form_input(form_cont, "员工工号:", "请输入工号", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_browse_job_uid, "0123456789"); // 严格限制只能输入数字
    lv_obj_add_event_cb(g_ta_dl_browse_job_uid, browse_job_query_event_cb, LV_EVENT_ALL, nullptr);
    UiManager::getInstance()->addObjToGroup(g_ta_dl_browse_job_uid);

    // 2. 开始时间输入框
    g_ta_dl_browse_job_start = create_form_input(form_cont, "开始时间:", "如: 2026-01-01", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_browse_job_start, "0123456789-"); // 严格限制数字和横杠
    lv_obj_add_event_cb(g_ta_dl_browse_job_start, browse_job_query_event_cb, LV_EVENT_ALL, nullptr);
    UiManager::getInstance()->addObjToGroup(g_ta_dl_browse_job_start);

    // 3. 结束时间输入框
    g_ta_dl_browse_job_end = create_form_input(form_cont, "结束时间:", "如: 2026-01-31", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_browse_job_end, "0123456789-"); // 严格限制数字和横杠
    lv_obj_add_event_cb(g_ta_dl_browse_job_end, browse_job_query_event_cb, LV_EVENT_ALL, nullptr);
    UiManager::getInstance()->addObjToGroup(g_ta_dl_browse_job_end);

    // 4. 确认浏览按钮
    g_btn_dl_browse_job_confirm = create_form_btn(form_cont, "确认浏览", browse_job_query_event_cb, nullptr);
    lv_obj_add_event_cb(g_btn_dl_browse_job_confirm, browse_job_query_event_cb, LV_EVENT_KEY, nullptr); 
    UiManager::getInstance()->addObjToGroup(g_btn_dl_browse_job_confirm);

    // 默认聚焦在工号输入框
    lv_group_focus_obj(g_ta_dl_browse_job_uid);

    lv_screen_load(scr_browse_job_query);
    UiManager::getInstance()->destroyAllScreensExcept(scr_browse_job_query);
}


// =========================================================
// 1.1.1 浏览工号查询结果 (Browse Job Query Result) (四级界面)
// =========================================================

// 浏览工号查询结果事件回调
static void browse_job_query_result_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    // 获取按键值 (如果不是按键事件，key 会是 0)
    if(code==LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 导航逻辑 (兼容键盘方向键)
    if (code == LV_EVENT_KEY) {
        if(key == LV_KEY_ESC) {
            load_browse_job_query_screen(); // 返回浏览工号查询界面
            lv_indev_wait_release(lv_indev_get_act());// 【防连跳核心】 --- IGNORE ---
            return;// 处理完返回后直接返回，避免继续执行下面的导航逻辑
         } 
        else if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(UiManager::getInstance()->getKeypadGroup());// 向下或向右导航
            return;// 处理完返回后直接返回，避免继续执行下面的导航逻辑
        } 
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_group_focus_prev(UiManager::getInstance()->getKeypadGroup());// 向上或向左导航
            return;
         }
    }
    
}

// 浏览工号查询结果界面实现
void load_browse_job_query_result_screen() {

    if (scr_browse_job_query_result){
        lv_obj_delete(scr_browse_job_query_result);
        scr_browse_job_query_result = nullptr;
    }

    BaseScreenParts parts = create_base_screen("浏览工号查询结果");
    scr_browse_job_query_result = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::BROWSE_JOB_QUERY_RESULT, &scr_browse_job_query_result);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_browse_job_query_result, [](lv_event_t * e) {
        scr_browse_job_query_result = nullptr;
        obj_browse_view = nullptr; // 把这个全局内容区指针也清空！
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    // ==========================================
    // 将内容区改为 Flex 垂直布局，方便表头和列表堆叠
    // ==========================================
    lv_obj_set_flex_flow(parts.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parts.content, 5, 0); // 内容区内边距 
    lv_obj_set_style_pad_gap(parts.content, 5, 0); // 表头和下方列表的间距

    // ==========================================
    // 创建独立表头行 (Header Row)
    // ==========================================
    lv_obj_t * header_row = lv_obj_create(parts.content);
    lv_obj_set_width(header_row, LV_PCT(100));
    lv_obj_set_height(header_row, 30);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0); 
    lv_obj_set_style_border_width(header_row, 0, 0);       
    lv_obj_set_style_pad_all(header_row, 0, 0);

    // 开启横向排列，上下居中
    lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 表头 - 第 1 列：工号 (分配 25% 宽度，内部文字居中)
    lv_obj_t * lbl_h_id = lv_label_create(header_row);
    lv_obj_set_width(lbl_h_id, LV_PCT(25));
    lv_label_set_text(lbl_h_id, "工号");
    lv_obj_set_style_text_color(lbl_h_id, lv_color_white(), 0);// 白色字体
    lv_obj_add_style(lbl_h_id, &style_text_cn, 0);
    lv_obj_set_style_text_align(lbl_h_id, LV_TEXT_ALIGN_CENTER, 0);

    // 表头 - 第 2 列：姓名 (分配 40% 宽度，内部文字居中)
    lv_obj_t * lbl_h_name = lv_label_create(header_row);
    lv_obj_set_width(lbl_h_name, LV_PCT(40));
    lv_label_set_text(lbl_h_name, "日期");
    lv_obj_set_style_text_color(lbl_h_name, lv_color_white(), 0);// 白色字体
    lv_obj_add_style(lbl_h_name, &style_text_cn, 0);
    lv_obj_set_style_text_align(lbl_h_name, LV_TEXT_ALIGN_CENTER, 0);

    // 表头 - 第 3 列：部门 (分配 35% 宽度，内部文字居中)
    lv_obj_t * lbl_h_dept = lv_label_create(header_row);
    lv_obj_set_width(lbl_h_dept, LV_PCT(35));
    lv_label_set_text(lbl_h_dept, "时间");
    lv_obj_set_style_text_color(lbl_h_dept, lv_color_white(), 0);// 白色字体
    lv_obj_add_style(lbl_h_dept, &style_text_cn, 0);
    lv_obj_set_style_text_align(lbl_h_dept, LV_TEXT_ALIGN_CENTER, 0);

    // 创建列表容器 (挂在 parts.content 上)
    obj_browse_view = lv_obj_create(parts.content);
    lv_obj_set_size(obj_browse_view, LV_PCT(100), LV_PCT(100)); // 占满中心空白区
    lv_obj_set_flex_grow(obj_browse_view, 1);// 让它占满剩余空间
    lv_obj_set_style_bg_opa(obj_browse_view, LV_OPA_TRANSP, 0); // 让底层蓝色透过来
    lv_obj_set_style_border_width(obj_browse_view, 0, 0);       // 去掉灰色边框
    lv_obj_set_style_pad_all(obj_browse_view, 0, 0);           // 左右上下留一点呼吸空间
    lv_obj_set_style_pad_gap(obj_browse_view, 5, 0);            // 列表项之间的间距
    lv_obj_set_flex_flow(obj_browse_view, LV_FLEX_FLOW_COLUMN); // 开启垂直滚动的流式布局

    // 通过 Controller 获取特定工号、特定日期的打卡记录
    std::vector<AttendanceRecord> records = UiController::getInstance()->getRecords(g_job_query_uid, g_job_query_start_ts, g_job_query_end_ts);

    if (records.empty()) {
        // 无数据时的占位提示
        lv_obj_t* empty_label = lv_label_create(parts.content);
        lv_label_set_text(empty_label, "该员工当天无打卡记录");
        lv_obj_add_style(empty_label, &style_text_cn, 0);
        lv_obj_center(empty_label);
        
        // 绑定一个隐形的按键事件，确保按 ESC 还能退出去
        lv_obj_add_event_cb(parts.screen, browse_job_query_result_event_cb, LV_EVENT_KEY, nullptr);
    } else {
        // 遍历记录，生成按钮
        for (size_t i = 0; i < records.size(); i++) {
            const auto& rec = records[i];

            // 1. 解析时间戳
            // 先将 long long 转成标准时间类型 time_t
            time_t t_val = static_cast<time_t>(rec.timestamp); 
            // 再传地址给 localtime
            struct tm* tm_info = localtime(&t_val);
            char date_buf[32]; // 存放 年-月-日
            char time_buf[32]; // 存放 时:分:秒
            strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm_info);
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

            // 2. 创建一个没有任何文本的空按钮，并清理掉默认的子控件
            lv_obj_t* btn = create_sys_list_btn(obj_browse_view, "", "", "", browse_job_query_result_event_cb, nullptr);
            lv_obj_clean(btn); // 核心：清空里面原本占位的空 Label

            // 3. 设置按钮内部为横向 Flex 布局，垂直居中对齐
            lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            // 4. 左侧组件：工号
            lv_obj_t* lbl_uid = lv_label_create(btn);
            lv_label_set_text_fmt(lbl_uid, "%d", rec.user_id);
            lv_obj_add_style(lbl_uid, &style_text_cn, 0);

            // 5. 中间组件：日期 (利用 flex_grow 自动拉伸填满中间区域，并设置文字居中)
            lv_obj_t* lbl_date = lv_label_create(btn);
            lv_label_set_text_fmt(lbl_date, "%s", date_buf);
            lv_obj_add_style(lbl_date, &style_text_cn, 0);
            lv_obj_set_flex_grow(lbl_date, 1);  // 霸占所有剩余的横向空间
            lv_obj_set_style_text_align(lbl_date, LV_TEXT_ALIGN_CENTER, 0); // 空间内文字居中

            // 6. 右侧组件：时间 (被中间的 Date 挤到最右侧紧贴边缘)
            lv_obj_t* lbl_time = lv_label_create(btn);
            lv_label_set_text_fmt(lbl_time, "%s", time_buf);
            lv_obj_add_style(lbl_time, &style_text_cn, 0);

            // 7. 加入按键物理组
            UiManager::getInstance()->addObjToGroup(btn);
        }

        // 默认聚焦第一条记录
        if (lv_obj_get_child_cnt(obj_browse_view) > 0) {
            lv_group_focus_obj(lv_obj_get_child(obj_browse_view, 0));
        }
    }

    // 加载这个全新生成的屏幕，并销毁其他老旧屏幕
    lv_screen_load(scr_browse_job_query_result);
    UiManager::getInstance()->destroyAllScreensExcept(scr_browse_job_query_result);
}


// =========================================================
// 1.2 下载工号查询 (Download Job Query) (三级界面)
// =========================================================

// 下载工号查询事件回调 (个人)
static void download_job_query_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 

    uint32_t key = 0;
    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 处理 ESC 键退出 (返回工号查询界面)
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act()); 
        load_job_query_screen ();//工号查询界面
        return;
    }

    // ================= 焦点在【工号输入框】 =================
    if (current_target == g_ta_dl_download_job_uid) {
        if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_ta_dl_download_job_start);
            lv_indev_wait_release(lv_indev_get_act());
        }
    }
    // ================= 焦点在【开始时间输入框】 =================
    else if (current_target == g_ta_dl_download_job_start) {
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_download_job_uid); // ↑跳回工号
            return; 
        } else if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_ta_dl_download_job_end); // ↓跳到结束时间
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【结束时间输入框】 =================
    else if (current_target == g_ta_dl_download_job_end) {
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_download_job_start); // ↑跳回开始时间
            return; 
        } else if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_btn_dl_download_job_confirm); // ↓跳到确认按钮
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【下载按钮】 =================
    else if (current_target == g_btn_dl_download_job_confirm) {
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_download_job_end); // ↑跳回结束时间
            return; 
        }

        // 按下确认下载
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            lv_indev_wait_release(lv_indev_get_act());
            
            std::string uid_txt = lv_textarea_get_text(g_ta_dl_download_job_uid);
            std::string s_txt = lv_textarea_get_text(g_ta_dl_download_job_start);
            std::string e_txt = lv_textarea_get_text(g_ta_dl_download_job_end);

            // 1. 判空校验
            if (uid_txt.empty() || s_txt.empty() || e_txt.empty()) {
                show_popup_msg("下载个人考勤报表失败", "工号和时间都不能为空!\n请输入工号和时间!", nullptr, "我知道了");
                return;
            }
            
            // 2. 格式校验 
            if (!is_valid_date_format(s_txt) || !is_valid_date_format(e_txt)) {
                show_popup_msg("下载个人考勤报表失败", "格式错误！\n日期格式必须为:\nYYYY-MM-DD\n(例如 2026-01-01)", nullptr, "我知道了");
                lv_group_focus_obj(is_valid_date_format(s_txt) ? g_ta_dl_download_job_end : g_ta_dl_download_job_start);
                return;
            }

            // 3. 时间穿越与逻辑校验
            std::string current_date = get_current_date_str();

            if (s_txt > current_date || e_txt > current_date) {
                show_popup_msg("下载个人考勤报表失败", ("时间错误！\n无法导出未来时间的报表!\n当前日期: " + current_date).c_str(), nullptr, "我知道了");
                return;
            }

            if (s_txt > e_txt) {
                show_popup_msg("下载个人考勤报表失败", "时间错误!\n【开始时间】不能晚于【结束时间】!", nullptr, "我知道了");
                lv_group_focus_obj(g_ta_dl_download_job_start);
                return;
            }
            
            int uid = std::stoi(uid_txt); // 将工号转为整型

            if (!UiController::getInstance()->checkUserExists(uid)) { 
                show_popup_msg("下载个人考勤报表失败", "工号错误!\n 该工号不存在,请检查工号! ", nullptr, "我知道了");
                lv_group_focus_obj(g_ta_dl_download_job_uid); // 焦点移回工号输入框，方便用户重输
                return;
            }

            // 4. 创建加载圈并启动后台导出线程
            lv_obj_t* spin = lv_spinner_create(lv_screen_active());
            lv_obj_center(spin);
            
            std::thread([uid, s_txt, e_txt, spin](){
                bool ret = UiController::getInstance()->exportUserReport(uid, s_txt.c_str(), e_txt.c_str());
                AsyncExportCtx* ctx = new AsyncExportCtx{spin, ret};
                lv_async_call(ui_on_export_complete, ctx);
            }).detach();
        }
    }
}

//下载工号查询界面
void load_download_job_query_screen() {

    if(scr_download_job_query){
        lv_obj_delete(scr_download_job_query);
        scr_download_job_query = nullptr;
    }

    // 1. 创建基础屏幕
    BaseScreenParts parts = create_base_screen("下载个人考勤报表");
    scr_download_job_query = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::DOWNLOAD_JOB_QUERY, &scr_download_job_query);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_download_job_query, [](lv_event_t * e) {
        scr_download_job_query = nullptr;
        g_ta_dl_download_job_uid = nullptr;
        g_ta_dl_download_job_start = nullptr;
        g_ta_dl_download_job_end = nullptr;
        g_btn_dl_download_job_confirm = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    // 绑定全局 ESC 返回事件
    lv_obj_add_event_cb(scr_download_job_query, download_job_query_event_cb, LV_EVENT_KEY, nullptr);
    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t* form_cont = create_form_container(parts.content);//

    // 1. 工号输入框
    g_ta_dl_download_job_uid = create_form_input(form_cont, "员工工号:", "请输入工号", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_download_job_uid, "0123456789"); // 严格限制只能输入数字
    lv_obj_add_event_cb(g_ta_dl_download_job_uid, download_job_query_event_cb, LV_EVENT_ALL, nullptr);
    UiManager::getInstance()->addObjToGroup(g_ta_dl_download_job_uid);

    // 2. 开始时间输入框
    g_ta_dl_download_job_start = create_form_input(form_cont, "开始时间:", "如: 2026-01-01", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_download_job_start, "0123456789-"); // 严格限制数字和横杠
    lv_obj_add_event_cb(g_ta_dl_download_job_start, download_job_query_event_cb, LV_EVENT_ALL, nullptr);
    UiManager::getInstance()->addObjToGroup(g_ta_dl_download_job_start);

    // 3. 结束时间输入框
    g_ta_dl_download_job_end = create_form_input(form_cont, "结束时间:", "如: 2026-01-31", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_download_job_end, "0123456789-"); // 严格限制数字和横杠
    lv_obj_add_event_cb(g_ta_dl_download_job_end, download_job_query_event_cb, LV_EVENT_ALL, nullptr);
    UiManager::getInstance()->addObjToGroup(g_ta_dl_download_job_end);

    // 4. 确认下载按钮
    g_btn_dl_download_job_confirm = create_form_btn(form_cont, "确认下载", download_job_query_event_cb, nullptr);
    lv_obj_add_event_cb(g_btn_dl_download_job_confirm, download_job_query_event_cb, LV_EVENT_KEY, nullptr); 
    UiManager::getInstance()->addObjToGroup(g_btn_dl_download_job_confirm);

    // 默认聚焦在工号输入框
    lv_group_focus_obj(g_ta_dl_download_job_uid);

    lv_screen_load(scr_download_job_query);
    UiManager::getInstance()->destroyAllScreensExcept(scr_download_job_query);
}


// =========================================================
// 2. 时间查询 (Time Query) (二级界面)
// =========================================================

//时间查询事件回调
static void time_query_event_cb(lv_event_t *e) {

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
            load_record_query_menu_screen(); // 返回上一级记录查询界面
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
            load_browse_time_query_screen();//浏览时间查询界面
        } 
        else if (index == 1) {
            load_download_time_query_screen ();//下载时间查询界面
        }
    }

}

//时间查询界面
void load_time_query_screen() {
        if (scr_time_query){
        lv_obj_delete(scr_time_query);
        scr_time_query = nullptr;
    }

    BaseScreenParts parts = create_base_screen("时间查询");
    scr_time_query = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::TIME_QUERY, &scr_time_query);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_time_query, [](lv_event_t * e) {
        scr_time_query = nullptr;
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器

    // 创建按钮
    create_sys_list_btn(list, "1. ", "", "浏览", time_query_event_cb, (const char*)(intptr_t)0);
    create_sys_list_btn(list, "2. ", "", "下载", time_query_event_cb, (const char*)(intptr_t)1);

    uint32_t child_cnt = lv_obj_get_child_cnt(list);// 遍历容器子对象(按钮)加入组
    for(uint32_t i=0; i<child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        UiManager::getInstance()->addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个按钮
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_time_query);
    UiManager::getInstance()->destroyAllScreensExcept(scr_time_query);// 加载后销毁其他屏幕，保持资源清晰
}


// =========================================================
// 2.1 浏览时间查询 (Browse Time Query) (三级界面)
// =========================================================

// 浏览时间查询事件回调
static void browse_time_query_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 

    uint32_t key = 0;
    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 处理 ESC 键退出 (返回工号查询界面)
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act()); 
        load_job_query_screen(); //工号查询界面
        return;
    }
    // ================= 焦点在【开始时间输入框】 =================
    if (current_target == g_ta_dl_browse_time_start) {
        // 按下回车或↓键，跳到结束时间输入框
        if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_ta_dl_browse_time_end);
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【结束时间输入框】 =================
    else if (current_target == g_ta_dl_browse_time_end) {
        // 按下↑键，跳回开始时间
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_browse_time_start);
            return; 
        }
        // 按下回车或↓键，跳到确认浏览按钮
        else if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_btn_dl_browse_time_confirm);
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【确认按钮】 =================
    else if (current_target == g_btn_dl_browse_time_confirm) {
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_browse_time_end); // ↑跳回结束时间
            return; 
        }

        // 按下确认按钮
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            lv_indev_wait_release(lv_indev_get_act());
            
            std::string s_txt = lv_textarea_get_text(g_ta_dl_browse_time_start);
            std::string e_txt = lv_textarea_get_text(g_ta_dl_browse_time_end);

            // 1. 判空校验
            if ( s_txt.empty() || e_txt.empty()) {
                show_popup_msg("浏览时间查询失败", "开始时间和结束时间不能为空!\n请输入开始时间和结束时间!", nullptr, "我知道了");
                return;
            }
            
            // 2. 格式校验 
            if (!is_valid_date_format(s_txt) || !is_valid_date_format(e_txt)) {
                show_popup_msg("浏览时间查询失败", "格式错误！\n日期格式必须为:\nYYYY-MM-DD\n(例如 2026-01-01)", nullptr, "我知道了");
                lv_group_focus_obj(is_valid_date_format(s_txt) ? g_ta_dl_browse_time_end : g_ta_dl_browse_time_start);
                return;
            }

            // 3. 时间穿越与逻辑校验
            std::string current_date = get_current_date_str();

            if (s_txt > current_date || e_txt > current_date) {
                show_popup_msg("浏览时间查询失败", ("时间错误！\n无法导出未来时间的报表!\n当前日期: " + current_date).c_str(), nullptr, "我知道了");
                return;
            }

            if (s_txt > e_txt) {
                show_popup_msg("浏览时间查询失败", "时间错误!\n【开始时间】不能晚于【结束时间】!", nullptr, "我知道了");
                lv_group_focus_obj(g_ta_dl_browse_time_start);
                return;
            }

            g_time_query_start_ts = convert_date_to_timestamp(s_txt, false); // 当天 00:00:00
            g_time_query_end_ts = convert_date_to_timestamp(e_txt, true);    // 当天 23:59:59

            load_browse_time_query_result_screen ();//跳到四级界面，浏览时间查询结果界面
        }
    }
}

// 浏览时间查询界面
void load_browse_time_query_screen() {

    if(scr_browse_time_query){
        lv_obj_delete(scr_browse_time_query);
        scr_browse_time_query = nullptr;
    }

    // 1. 创建基础屏幕
    BaseScreenParts parts = create_base_screen("浏览时间查询");
    scr_browse_time_query = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::BROWSE_JOB_QUERY, &scr_browse_time_query);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_browse_time_query, [](lv_event_t * e) {
        scr_browse_time_query = nullptr;
        g_ta_dl_browse_time_start = nullptr;
        g_ta_dl_browse_time_end = nullptr;
        g_btn_dl_browse_time_confirm = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t* form_cont = create_form_container(parts.content);

    // 1. 开始时间输入框
    g_ta_dl_browse_time_start = create_form_input(form_cont, "开始时间:", "如: 2026-01-01", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_browse_time_start, "0123456789-"); // 严格限制数字和横杠
    lv_obj_add_event_cb(g_ta_dl_browse_time_start, browse_time_query_event_cb, LV_EVENT_ALL, nullptr);
    UiManager::getInstance()->addObjToGroup(g_ta_dl_browse_time_start);

    // 2. 结束时间输入框
    g_ta_dl_browse_time_end = create_form_input(form_cont, "结束时间:", "如: 2026-01-31", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_browse_time_end, "0123456789-"); // 严格限制数字和横杠
    lv_obj_add_event_cb(g_ta_dl_browse_time_end, browse_time_query_event_cb, LV_EVENT_ALL, nullptr);
    UiManager::getInstance()->addObjToGroup(g_ta_dl_browse_time_end);

    // 3. 确认浏览按钮
    g_btn_dl_browse_time_confirm = create_form_btn(form_cont, "确认浏览", browse_job_query_event_cb, nullptr);
    lv_obj_add_event_cb(g_btn_dl_browse_time_confirm, browse_time_query_event_cb, LV_EVENT_KEY, nullptr); 
    UiManager::getInstance()->addObjToGroup(g_btn_dl_browse_time_confirm);

    // 默认聚焦在开始时间输入框
    lv_group_focus_obj(g_ta_dl_browse_time_start);

    lv_screen_load(scr_browse_time_query);
    UiManager::getInstance()->destroyAllScreensExcept(scr_browse_time_query);
}


// =========================================================
// 1.1.1 浏览时间查询结果 (Browse Time Query Result) (四级界面)
// =========================================================

// 浏览时间查询结果事件回调
static void browse_time_query_result_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    uint32_t key = 0;

    // 获取按键值 (如果不是按键事件，key 会是 0)
    if(code==LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 导航逻辑 (兼容键盘方向键)
    if (code == LV_EVENT_KEY) {
        if(key == LV_KEY_ESC) {
            load_browse_time_query_screen(); // 返回浏览时间查询界面
            lv_indev_wait_release(lv_indev_get_act());// 【防连跳核心】 --- IGNORE ---
            return;// 处理完返回后直接返回，避免继续执行下面的导航逻辑
         } 
        else if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
            lv_group_focus_next(UiManager::getInstance()->getKeypadGroup());// 向下或向右导航
            return;// 处理完返回后直接返回，避免继续执行下面的导航逻辑
        } 
        else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
            lv_group_focus_prev(UiManager::getInstance()->getKeypadGroup());// 向上或向左导航
            return;
         }
    }
    
}

// 浏览时间查询结果界面实现
void load_browse_time_query_result_screen() {

    if (scr_browse_time_query_result){
        lv_obj_delete(scr_browse_time_query_result);
        scr_browse_time_query_result = nullptr;
    }

    BaseScreenParts parts = create_base_screen("浏览时间查询结果");
    scr_browse_time_query_result = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::BROWSE_TIME_QUERY_RESULT, &scr_browse_time_query_result);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_browse_time_query_result, [](lv_event_t * e) {
        scr_browse_time_query_result = nullptr;
        time_browse_view = nullptr; // 把这个全局内容区指针也清空！
    }, LV_EVENT_DELETE, NULL);

    UiManager::getInstance()->resetKeypadGroup();// 重置输入组，准备添加新控件

    // ==========================================
    // 将内容区改为 Flex 垂直布局，方便表头和列表堆叠
    // ==========================================
    lv_obj_set_flex_flow(parts.content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parts.content, 5, 0); // 内容区内边距 
    lv_obj_set_style_pad_gap(parts.content, 5, 0); // 表头和下方列表的间距

    // ==========================================
    // 创建独立表头行 (Header Row)
    // ==========================================
    lv_obj_t * header_row = lv_obj_create(parts.content);
    lv_obj_set_width(header_row, LV_PCT(100));
    lv_obj_set_height(header_row, 30);
    lv_obj_set_style_bg_opa(header_row, LV_OPA_TRANSP, 0); 
    lv_obj_set_style_border_width(header_row, 0, 0);       
    lv_obj_set_style_pad_all(header_row, 0, 0);

    // 开启横向排列，上下居中
    lv_obj_set_flex_flow(header_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 表头 - 第 1 列：工号 (分配 25% 宽度，内部文字居中)
    lv_obj_t * lbl_h_id = lv_label_create(header_row);
    lv_obj_set_width(lbl_h_id, LV_PCT(25));
    lv_label_set_text(lbl_h_id, "工号");
    lv_obj_set_style_text_color(lbl_h_id, lv_color_white(), 0);// 白色字体
    lv_obj_add_style(lbl_h_id, &style_text_cn, 0);
    lv_obj_set_style_text_align(lbl_h_id, LV_TEXT_ALIGN_CENTER, 0);

    // 表头 - 第 2 列：姓名 (分配 40% 宽度，内部文字居中)
    lv_obj_t * lbl_h_name = lv_label_create(header_row);
    lv_obj_set_width(lbl_h_name, LV_PCT(40));
    lv_label_set_text(lbl_h_name, "日期");
    lv_obj_set_style_text_color(lbl_h_name, lv_color_white(), 0);// 白色字体
    lv_obj_add_style(lbl_h_name, &style_text_cn, 0);
    lv_obj_set_style_text_align(lbl_h_name, LV_TEXT_ALIGN_CENTER, 0);

    // 表头 - 第 3 列：部门 (分配 35% 宽度，内部文字居中)
    lv_obj_t * lbl_h_dept = lv_label_create(header_row);
    lv_obj_set_width(lbl_h_dept, LV_PCT(35));
    lv_label_set_text(lbl_h_dept, "时间");
    lv_obj_set_style_text_color(lbl_h_dept, lv_color_white(), 0);// 白色字体
    lv_obj_add_style(lbl_h_dept, &style_text_cn, 0);
    lv_obj_set_style_text_align(lbl_h_dept, LV_TEXT_ALIGN_CENTER, 0);

    // 创建列表容器 (挂在 parts.content 上)
    time_browse_view = lv_obj_create(parts.content);
    lv_obj_set_size(time_browse_view, LV_PCT(100), LV_PCT(100)); // 占满中心空白区
    lv_obj_set_flex_grow(time_browse_view, 1);// 让它占满剩余空间
    lv_obj_set_style_bg_opa(time_browse_view, LV_OPA_TRANSP, 0); // 让底层蓝色透过来
    lv_obj_set_style_border_width(time_browse_view, 0, 0);       // 去掉灰色边框
    lv_obj_set_style_pad_all(time_browse_view, 0, 0);           // 左右上下留一点呼吸空间
    lv_obj_set_style_pad_gap(time_browse_view, 5, 0);            // 列表项之间的间距
    lv_obj_set_flex_flow(time_browse_view, LV_FLEX_FLOW_COLUMN); // 开启垂直滚动的流式布局

    // 通过 Controller 获取特定工号、特定日期的打卡记录
    std::vector<AttendanceRecord> records = UiController::getInstance()->getRecords(g_time_query_uid, g_time_query_start_ts, g_time_query_end_ts);

    if (records.empty()) {
        // 无数据时的占位提示
        lv_obj_t* empty_label = lv_label_create(parts.content);
        lv_label_set_text(empty_label, "该时间段无打卡记录");
        lv_obj_add_style(empty_label, &style_text_cn, 0);
        lv_obj_center(empty_label);
        
        // 绑定一个隐形的按键事件，确保按 ESC 还能退出去
        lv_obj_add_event_cb(parts.screen, browse_time_query_result_event_cb, LV_EVENT_KEY, nullptr);
    } else {
        // 遍历记录，生成按钮
        for (size_t i = 0; i < records.size(); i++) {
            const auto& rec = records[i];

            // 1. 解析时间戳
            // 先将 long long 转成标准时间类型 time_t
            time_t t_val = static_cast<time_t>(rec.timestamp); 
            // 再传地址给 localtime
            struct tm* tm_info = localtime(&t_val);
            char date_buf[32]; // 存放 年-月-日
            char time_buf[32]; // 存放 时:分:秒
            strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm_info);
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

            // 2. 创建一个没有任何文本的空按钮，并清理掉默认的子控件
            lv_obj_t* btn = create_sys_list_btn(time_browse_view, "", "", "", browse_time_query_result_event_cb, nullptr);
            lv_obj_clean(btn); // 核心：清空里面原本占位的空 Label

            // 3. 设置按钮内部为横向 Flex 布局，垂直居中对齐
            lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

            // 4. 左侧组件：工号
            lv_obj_t* lbl_uid = lv_label_create(btn);
            lv_label_set_text_fmt(lbl_uid, "%d", rec.user_id);
            lv_obj_add_style(lbl_uid, &style_text_cn, 0);

            // 5. 中间组件：日期 (利用 flex_grow 自动拉伸填满中间区域，并设置文字居中)
            lv_obj_t* lbl_date = lv_label_create(btn);
            lv_label_set_text_fmt(lbl_date, "%s", date_buf);
            lv_obj_add_style(lbl_date, &style_text_cn, 0);
            lv_obj_set_flex_grow(lbl_date, 1);  // 霸占所有剩余的横向空间
            lv_obj_set_style_text_align(lbl_date, LV_TEXT_ALIGN_CENTER, 0); // 空间内文字居中

            // 6. 右侧组件：时间 (被中间的 Date 挤到最右侧紧贴边缘)
            lv_obj_t* lbl_time = lv_label_create(btn);
            lv_label_set_text_fmt(lbl_time, "%s", time_buf);
            lv_obj_add_style(lbl_time, &style_text_cn, 0);

            // 7. 加入按键物理组
            UiManager::getInstance()->addObjToGroup(btn);
        }

        // 默认聚焦第一条记录
        if (lv_obj_get_child_cnt(time_browse_view) > 0) {
            lv_group_focus_obj(lv_obj_get_child(time_browse_view, 0));
        }
    }

    // 加载这个全新生成的屏幕，并销毁其他老旧屏幕
    lv_screen_load(scr_browse_time_query_result);
    UiManager::getInstance()->destroyAllScreensExcept(scr_browse_time_query_result);
}


// =========================================================
// 2.2 下载时间查询 (Download Time Query) (三级界面)
// =========================================================

// 下载时间查询事件回调 (全员)
static void download_time_query_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_target = (lv_obj_t *)lv_event_get_current_target(e); 

    uint32_t key = 0;
    if(code == LV_EVENT_KEY) {
        key = lv_event_get_key(e);
    }

    // 1. 处理 ESC 键退出 (返回工号查询界面)
    if (code == LV_EVENT_KEY && key == LV_KEY_ESC) {
        lv_indev_wait_release(lv_indev_get_act()); 
        load_time_query_screen ();//时间查询界面
        return;
    }

    // ================= 焦点在【开始时间输入框】 =================
    if (current_target == g_ta_dl_download_time_start) {
        // 按下回车或↓键，跳到结束时间输入框
        if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_ta_dl_download_time_end);
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【结束时间输入框】 =================
    else if (current_target == g_ta_dl_download_time_end) {
        // 按下↑键，跳回开始时间
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_download_time_start);
            return; 
        }
        // 按下回车或↓键，跳到确认浏览按钮
        else if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_btn_dl_download_time_confirm);
            lv_indev_wait_release(lv_indev_get_act());
        }
    } 
    // ================= 焦点在【下载按钮】 =================
    else if (current_target == g_btn_dl_download_time_confirm) {
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_download_time_end); // ↑跳回结束时间
            return; 
        }

        // 按下确认下载
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            lv_indev_wait_release(lv_indev_get_act());
            
            std::string s_txt = lv_textarea_get_text(g_ta_dl_download_time_start);
            std::string e_txt = lv_textarea_get_text(g_ta_dl_download_time_end);

            // 1. 判空校验
            if (s_txt.empty() || e_txt.empty()) {
                show_popup_msg("下载考勤报表失败", "下载考勤报表失败!\n请输入有效的日期!", nullptr, "我知道了");
                return;
            }

            // 2. 格式校验
            if (!is_valid_date_format(s_txt) || !is_valid_date_format(e_txt)) {
                show_popup_msg("下载考勤报表失败", "格式错误!\n日期格式必须为:\nYYYY-MM-DD\n(例如 2026-01-01)",nullptr, "我知道了");
                // 焦点回到填错的框
                lv_group_focus_obj(is_valid_date_format(s_txt) ? g_ta_dl_download_time_end : g_ta_dl_download_time_start);
                return;
            }
            
            // ================== 业务逻辑时间穿越校验  ==================
            std::string current_date = get_current_date_str();

            // 3. 检查是否包含未来时间
            if (s_txt > current_date || e_txt > current_date) {
                show_popup_msg("下载考勤报表失败", ("时间错误!\n无法导出未来时间的报表!\n当前日期: " + current_date).c_str(),nullptr, "我知道了");
                return;
            }

            // 4. 检查开始时间是否晚于结束时间
            if (s_txt > e_txt) {
                show_popup_msg("下载考勤报表失败", "时间错误!\n【开始时间】不能晚于\n【结束时间】!",nullptr, "我知道了");
                lv_group_focus_obj(g_ta_dl_download_time_start); // 焦点移回开始时间让用户修改
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

//下载时间查询界面
void load_download_time_query_screen() {

    if(scr_download_time_query){
        lv_obj_delete(scr_download_time_query);
        scr_download_time_query = nullptr;
    }

    // 1. 创建基础屏幕
    BaseScreenParts parts = create_base_screen("下载考勤报表");
    scr_download_time_query = parts.screen;
    UiManager::getInstance()->registerScreen(ScreenType::DOWNLOAD_TIME_QUERY, &scr_download_time_query);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_download_time_query, [](lv_event_t * e) {
        scr_download_time_query = nullptr;
        g_ta_dl_download_time_start = nullptr;
        g_ta_dl_download_time_end = nullptr;
        g_btn_dl_download_time_confirm = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    UiManager::getInstance()->resetKeypadGroup();

    lv_obj_t* form_cont = create_form_container(parts.content);

    // 1. 开始时间输入框
    g_ta_dl_download_time_start = create_form_input(form_cont, "开始时间:", "如: 2026-01-01", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_download_time_start, "0123456789-"); // 严格限制数字和横杠
    lv_obj_add_event_cb(g_ta_dl_download_time_start, download_time_query_event_cb, LV_EVENT_ALL, nullptr);
    UiManager::getInstance()->addObjToGroup(g_ta_dl_download_time_start);

    // 2. 结束时间输入框
    g_ta_dl_download_time_end = create_form_input(form_cont, "结束时间:", "如: 2026-01-31", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_download_time_end, "0123456789-"); // 严格限制数字和横杠
    lv_obj_add_event_cb(g_ta_dl_download_time_end, download_time_query_event_cb, LV_EVENT_ALL, nullptr);
    UiManager::getInstance()->addObjToGroup(g_ta_dl_download_time_end);

    // 3. 确认下载按钮
    g_btn_dl_download_time_confirm = create_form_btn(form_cont, "确认下载", download_time_query_event_cb, nullptr);
    lv_obj_add_event_cb(g_btn_dl_download_time_confirm, download_time_query_event_cb, LV_EVENT_KEY, nullptr); 
    UiManager::getInstance()->addObjToGroup(g_btn_dl_download_time_confirm);

    // 默认聚焦在工号输入框
    lv_group_focus_obj(g_ta_dl_download_time_start);

    lv_screen_load(scr_download_time_query);
    UiManager::getInstance()->destroyAllScreensExcept(scr_download_time_query);
}


} // namespace record_query
} // namespace ui