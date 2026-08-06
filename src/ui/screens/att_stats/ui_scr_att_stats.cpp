#include "ui_scr_att_stats.h"
#include "../../managers/ui_manager.h"
#include "../../common/ui_style.h"
#include "../../common/ui_widgets.h"
#include "../../ui_page_dependencies.h"
#include "../../../app/ui_background_job_queue.h"
#include "../menu/ui_scr_menu.h"

#include <string>
#include <cstdio>
#include <ctime>
#include <cctype> // 需要用到 isdigit
#include <unordered_map>

namespace ui {
namespace att_stats {

static smart_attendance::ui::AttendanceStatisticsPageDependencies* dependencies_ = nullptr;

void configureDependencies(
    smart_attendance::ui::AttendanceStatisticsPageDependencies& dependencies) noexcept {
    dependencies_ = &dependencies;
}

static smart_attendance::ui::AttendanceStatisticsPageDependencies& dependencies() {
    return *dependencies_;
}

// ================= [内部状态: 屏幕指针] =================
static lv_obj_t *scr_stats = nullptr;//考勤统计界面
static lv_obj_t *sub_screen_cont = nullptr; // 用于显示考勤统计子界面
static lv_obj_t *scr_download_all = nullptr;// 下载报表界面
static lv_obj_t *scr_download_personal = nullptr;// 下载个人报表界面

// ================= [内部状态: 输入框指针] =================
static lv_obj_t* g_ta_dl_all_start = nullptr;     // 下载全员报表界面开始时间输入框
static lv_obj_t* g_ta_dl_all_end = nullptr;       // 下载全员报表界面结束时间输入框
static lv_obj_t* g_btn_dl_all_confirm = nullptr;  // 下载全员报表界面确认下载按钮
static lv_obj_t* g_ta_dl_psn_uid = nullptr;       // 下载个人考勤报表界面工号输入框
static lv_obj_t* g_ta_dl_psn_start = nullptr;     // 下载个人考勤报表界面开始时间输入框
static lv_obj_t* g_ta_dl_psn_end = nullptr;       // 下载个人考勤报表界面结束时间输入框
static lv_obj_t* g_btn_dl_psn_confirm = nullptr;  // 下载个人考勤报表界面确认下载按钮

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
    const auto nowResult = dependencies().rtc.now();
    const time_t now = nowResult
        ? static_cast<time_t>(nowResult.value().unixSeconds)
        : 0;
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

static smart_attendance::app::UiBackgroundJobQueue* g_background_jobs = nullptr;
static std::unordered_map<std::uint64_t, lv_obj_t*> g_background_spinners;

static void show_export_result(lv_obj_t* spinner, bool success) {
    if (spinner && lv_obj_is_valid(spinner)) {
        lv_obj_delete(spinner);
    }

    if (success) {
        show_popup_msg("导出全员考勤报表成功", "全员报表导出成功!\n报表已下载至U盘!", nullptr, "我知道了");
    } else {
        show_popup_msg("导出全员考勤报表失败", "全员报表导出失败!\n请检查设备!", nullptr, "我知道了");
    }
}

static void show_settings_export_result(lv_obj_t* spinner, bool success) {
    if (spinner && lv_obj_is_valid(spinner)) {
        lv_obj_delete(spinner);
    }

    if (success) {
        show_popup_msg("导出成功", "员工设置表导出成功!\n文件已保存至U盘!", nullptr, "确认");
    } else {
        show_popup_msg("导出失败", "员工设置表导出失败!\n请检查U盘是否插入或存储已满!", nullptr, "确认");
    }
}

static void show_settings_import_result(lv_obj_t* spinner,
                                        bool success,
                                        int invalidTimeCount) {
    if (spinner && lv_obj_is_valid(spinner)) {
        lv_obj_delete(spinner);
    }

    if (success && invalidTimeCount > 0) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "员工设置表上传成功!\n员工信息已同步到设备!\n\n"
                 "警告: %d 个时间字段格式非法\n"
                 "(已自动跳过，请检查表格中的班次时间)",
                 invalidTimeCount);
        show_popup_msg("上传成功(含异常)", msg, nullptr, "确认");
    } else if (success) {
        show_popup_msg("上传成功", "员工设置表上传成功!\n员工信息已同步到设备!", nullptr, "确认");
    } else {
        show_popup_msg("上传失败",
                       "员工设置表上传失败!\n请确认:\n1. U盘已插入\n2. 文件名为\"员工设置表.xlsx\"\n3. 文件格式正确",
                       nullptr, "确认");
    }
}

void configureBackgroundJobs(
    smart_attendance::app::UiBackgroundJobQueue& backgroundJobs) noexcept {
    g_background_jobs = &backgroundJobs;
}

void resetBackgroundJobs() noexcept {
    g_background_jobs = nullptr;
    g_background_spinners.clear();
}

void handleBackgroundJobResult(
    const smart_attendance::app::UiBackgroundJobResult& result) {
    const auto spinner = g_background_spinners.find(result.requestId);
    if (spinner == g_background_spinners.end()) {
        return;
    }
    if (result.type ==
        smart_attendance::app::UiBackgroundJobType::EmployeeSettingsExport) {
        show_settings_export_result(spinner->second, result.success);
    } else if (result.type ==
               smart_attendance::app::UiBackgroundJobType::EmployeeSettingsImport) {
        show_settings_import_result(
            spinner->second, result.success, result.invalidTimeCount);
    } else {
        show_export_result(spinner->second, result.success);
    }
    g_background_spinners.erase(spinner);
}

// =========================================================
// 一、 考勤统计主菜单界面 (Att Stats) (一级界面)
// =========================================================

//考勤统计主菜单界面按键事件回调
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
        lv_group_t* group = dependencies().uiManager.getKeypadGroup();
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
        else if (index == 2) {
            // C: 下载员工设置表

            // 1. 创建并显示居中的加载圈 (Spinner) 阻塞屏幕交互
            lv_obj_t* spin = lv_spinner_create(lv_screen_active());
            lv_obj_center(spin);

            std::uint64_t requestId = 0;
            const auto submitResult = g_background_jobs == nullptr
                ? smart_attendance::app::UiBackgroundJobSubmitError::Stopped
                : g_background_jobs->submitEmployeeSettingsExport(
                      smart_attendance::app::UiBackgroundJobOwner::AttendanceStatistics,
                      requestId);
            if (submitResult != smart_attendance::app::UiBackgroundJobSubmitError::None) {
                lv_obj_delete(spin);
                show_popup_msg("导出失败",
                               "后台任务繁忙或正在退出，请稍后重试!",
                               nullptr,
                               "确认");
                return;
            }
            g_background_spinners.emplace(requestId, spin);
        }
        else if (index == 3) {
            // D: 上传员工设置表
            // 流程：从当前模拟/真实存储的 usb_settings/员工设置表.xlsx 导入数据库

            // 1. 显示加载圈，告知用户正在处理
            lv_obj_t* spin = lv_spinner_create(lv_screen_active());
            lv_obj_center(spin);

            std::uint64_t requestId = 0;
            const auto submitResult = g_background_jobs == nullptr
                ? smart_attendance::app::UiBackgroundJobSubmitError::Stopped
                : g_background_jobs->submitEmployeeSettingsImport(
                      smart_attendance::app::UiBackgroundJobOwner::AttendanceStatistics,
                      requestId);
            if (submitResult != smart_attendance::app::UiBackgroundJobSubmitError::None) {
                lv_obj_delete(spin);
                show_popup_msg("上传失败",
                               "后台任务繁忙或正在退出，请稍后重试!",
                               nullptr,
                               "确认");
                return;
            }
            g_background_spinners.emplace(requestId, spin);
        }
        else {
            show_popup_msg ("hello!", "该功能暂未开放!", nullptr, nullptr);//其他功能占位
        }
    }
}

//考勤统计主菜单界面
void load_att_stats_menu_screen() {

    if (scr_stats){
        lv_obj_delete(scr_stats);
        scr_stats = nullptr;
    }

    BaseScreenParts parts = create_base_screen("考勤统计");
    scr_stats = parts.screen;
    dependencies().uiManager.registerScreen(ScreenType::ATT_STATS, &scr_stats);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_stats, [](lv_event_t * e) {
        scr_stats = nullptr;
        sub_screen_cont = nullptr;
    }, LV_EVENT_DELETE, NULL);

    dependencies().uiManager.resetKeypadGroup();// 重置输入组，准备添加新控件

    lv_obj_t* list = create_list_container(parts.content);// 创建统一列表容器

    // 创建按钮
    // 参数: 父对象, 行号, 图标, 英文标题, 中文标题, 回调, 索引(作为user_data)

    // 按钮 0: 下载考勤报表
    create_sys_list_btn(list, "1. ", "", "下载考勤报表", stats_menu_btn_cb, (const char*)(intptr_t)0);

    // 按钮 1: 下载个人考勤报表
    create_sys_list_btn(list, "2. ", "", "下载个人考勤报表", stats_menu_btn_cb, (const char*)(intptr_t)1);

    // 按钮 2: 下载员工设置
    create_sys_list_btn(list, "3. ", "", "下载员工设置", stats_menu_btn_cb, (const char*)(intptr_t)2);

    // 按钮 3: 上传员工设置 (占位)
    create_sys_list_btn(list, "4. ", "", "上传员工设置", stats_menu_btn_cb, (const char*)(intptr_t)3);

    // 按钮 4: 下载员工数据 (占位)
    create_sys_list_btn(list, "5. ", "", "下载员工数据", stats_menu_btn_cb, (const char*)(intptr_t)4);

    // 遍历容器子对象(按钮)加入组
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    for(uint32_t i=0; i < child_cnt; i++) {
        lv_obj_t* btn = lv_obj_get_child(list, i);
        dependencies().uiManager.addObjToGroup(btn);// 加入按键组
    }
    // 聚焦第一个
    if(child_cnt > 0) {
        lv_group_focus_obj(lv_obj_get_child(list, 0));
    }

    lv_screen_load(scr_stats);
    dependencies().uiManager.destroyAllScreensExcept(scr_stats);// 加载后销毁其他屏幕，保持资源清晰

}


// =========================================================
// 1.  下载 (全员) 考勤报表 (Download All) (二级界面)
// =========================================================

//下载考勤报表 (全员)事件回调
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
                show_popup_msg("导出全员考勤报表失败", "导出考勤报表失败!\n请输入有效的日期!", nullptr, "我知道了");
                return;
            }

            // 2. 格式校验
            if (!is_valid_date_format(s_txt) || !is_valid_date_format(e_txt)) {
                show_popup_msg("导出全员考勤报表失败", "格式错误!\n日期格式必须为:\nYYYY-MM-DD\n(例如 2026-01-01)",nullptr, "我知道了");
                // 焦点回到填错的框
                lv_group_focus_obj(is_valid_date_format(s_txt) ? g_ta_dl_all_end : g_ta_dl_all_start);
                return;
            }

            // ================== 业务逻辑时间穿越校验  ==================
            std::string current_date = get_current_date_str();

            // 3. 检查是否包含未来时间
            if (s_txt > current_date || e_txt > current_date) {
                show_popup_msg("导出全员考勤报表失败", ("时间错误!\n无法导出未来时间的报表!\n当前日期: " + current_date).c_str(),nullptr, "我知道了");
                return;
            }

            // 4. 检查开始时间是否晚于结束时间
            if (s_txt > e_txt) {
                show_popup_msg("导出全员考勤报表失败", "时间错误!\n【开始时间】不能晚于\n【结束时间】!",nullptr, "我知道了");
                lv_group_focus_obj(g_ta_dl_all_start); // 焦点移回开始时间让用户修改
                return;
            }

            // 创建加载圈并提交给应用级有界报表队列
            lv_obj_t* spin = lv_spinner_create(lv_screen_active());
            lv_obj_center(spin);

            std::uint64_t requestId = 0;
            const auto submitResult = g_background_jobs == nullptr
                ? smart_attendance::app::UiBackgroundJobSubmitError::Stopped
                : g_background_jobs->submitCustomReport(
                      smart_attendance::app::UiBackgroundJobOwner::AttendanceStatistics,
                      s_txt, e_txt, requestId);
            if (submitResult != smart_attendance::app::UiBackgroundJobSubmitError::None) {
                lv_obj_delete(spin);
                show_popup_msg("导出全员考勤报表失败",
                               "后台任务繁忙或正在退出，请稍后重试!",
                               nullptr,
                               "我知道了");
                return;
            }
            g_background_spinners.emplace(requestId, spin);
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
    dependencies().uiManager.registerScreen(ScreenType::ALL_ATT_STATS, &scr_download_all);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_download_all, [](lv_event_t * e) {
        scr_download_all = nullptr;
        g_ta_dl_all_start = nullptr;
        g_ta_dl_all_end = nullptr;
        g_btn_dl_all_confirm = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    // 绑定全局 ESC 返回事件
    lv_obj_add_event_cb(scr_download_all, download_all_event_cb, LV_EVENT_KEY, nullptr);
    dependencies().uiManager.resetKeypadGroup();

    // 创建统一表单容器
    lv_obj_t* form_cont = create_form_container(parts.content);

    // 1. 开始时间输入框
    g_ta_dl_all_start = create_form_input(form_cont, "开始时间:", "如: 2026-01-01", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_all_start, "0123456789-"); // 只允许输入数字和横杠
    lv_obj_add_event_cb(g_ta_dl_all_start, download_all_event_cb, LV_EVENT_ALL, nullptr);
    dependencies().uiManager.addObjToGroup(g_ta_dl_all_start);

    // 2. 结束时间输入框
    g_ta_dl_all_end = create_form_input(form_cont, "结束时间:", "如: 2026-01-31", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_all_end, "0123456789-"); // 只允许输入数字和横杠
    lv_obj_add_event_cb(g_ta_dl_all_end, download_all_event_cb, LV_EVENT_ALL, nullptr);
    dependencies().uiManager.addObjToGroup(g_ta_dl_all_end);

    // 3. 确认下载按钮
    // create_form_btn 会自动帮你绑定 LV_EVENT_CLICKED
    g_btn_dl_all_confirm = create_form_btn(form_cont, "确认下载", download_all_event_cb, nullptr);
    // 补充绑定 LV_EVENT_KEY 以处理键盘的 UP/DOWN 焦点跳转
    lv_obj_add_event_cb(g_btn_dl_all_confirm, download_all_event_cb, LV_EVENT_KEY, nullptr);
    dependencies().uiManager.addObjToGroup(g_btn_dl_all_confirm);

    // 默认聚焦在开始时间
    lv_group_focus_obj(g_ta_dl_all_start);

    lv_screen_load(scr_download_all);
    dependencies().uiManager.destroyAllScreensExcept(scr_download_all);
}

// =========================================================
// 2.  下载 (个人) 考勤报表 (Download Personal) (二级界面)
// =========================================================

// 下载个人考勤报表事件回调 (个人)
static void download_personal_event_cb(lv_event_t *e) {
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

    // ================= 焦点在【工号输入框】 =================
    if (current_target == g_ta_dl_psn_uid) {
        if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_ta_dl_psn_start);
            lv_indev_wait_release(lv_indev_get_act());
        }
    }
    // ================= 焦点在【开始时间输入框】 =================
    else if (current_target == g_ta_dl_psn_start) {
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_psn_uid); // ↑跳回工号
            return;
        } else if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_ta_dl_psn_end); // ↓跳到结束时间
            lv_indev_wait_release(lv_indev_get_act());
        }
    }
    // ================= 焦点在【结束时间输入框】 =================
    else if (current_target == g_ta_dl_psn_end) {
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_psn_start); // ↑跳回开始时间
            return;
        } else if (code == LV_EVENT_KEY && (key == LV_KEY_ENTER || key == LV_KEY_DOWN)) {
            lv_group_focus_obj(g_btn_dl_psn_confirm); // ↓跳到确认按钮
            lv_indev_wait_release(lv_indev_get_act());
        }
    }
    // ================= 焦点在【下载按钮】 =================
    else if (current_target == g_btn_dl_psn_confirm) {
        if (code == LV_EVENT_KEY && key == LV_KEY_UP) {
            lv_group_focus_obj(g_ta_dl_psn_end); // ↑跳回结束时间
            return;
        }

        // 按下确认下载
        if (code == LV_EVENT_CLICKED || (code == LV_EVENT_KEY && key == LV_KEY_ENTER)) {
            lv_indev_wait_release(lv_indev_get_act());

            std::string uid_txt = lv_textarea_get_text(g_ta_dl_psn_uid);
            std::string s_txt = lv_textarea_get_text(g_ta_dl_psn_start);
            std::string e_txt = lv_textarea_get_text(g_ta_dl_psn_end);

            // 1. 判空校验
            if (uid_txt.empty() || s_txt.empty() || e_txt.empty()) {
                show_popup_msg("导出个人考勤报表失败", "工号和时间都不能为空!\n请输入工号和时间!", nullptr, "我知道了");
                return;
            }

            // 2. 格式校验
            if (!is_valid_date_format(s_txt) || !is_valid_date_format(e_txt)) {
                show_popup_msg("导出个人考勤报表失败", "格式错误！\n日期格式必须为:\nYYYY-MM-DD\n(例如 2026-01-01)", nullptr, "我知道了");
                lv_group_focus_obj(is_valid_date_format(s_txt) ? g_ta_dl_psn_end : g_ta_dl_psn_start);
                return;
            }

            // 3. 时间穿越与逻辑校验
            std::string current_date = get_current_date_str();

            if (s_txt > current_date || e_txt > current_date) {
                show_popup_msg("导出个人考勤报表失败", ("时间错误！\n无法导出未来时间的报表!\n当前日期: " + current_date).c_str(), nullptr, "我知道了");
                return;
            }

            if (s_txt > e_txt) {
                show_popup_msg("导出个人考勤报表失败", "时间错误!\n【开始时间】不能晚于【结束时间】!", nullptr, "我知道了");
                lv_group_focus_obj(g_ta_dl_psn_start);
                return;
            }

            int uid = std::stoi(uid_txt); // 将工号转为整型

            if (!dependencies().employees.existsById(uid)) {
                show_popup_msg("导出个人考勤报表失败", "工号错误!\n 该工号不存在,请检查工号! ", nullptr, "我知道了");
                lv_group_focus_obj(g_ta_dl_psn_uid); // 焦点移回工号输入框，方便用户重输
                return;
            }

            // 4. 创建加载圈并提交给应用级有界报表队列
            lv_obj_t* spin = lv_spinner_create(lv_screen_active());
            lv_obj_center(spin);

            std::uint64_t requestId = 0;
            const auto submitResult = g_background_jobs == nullptr
                ? smart_attendance::app::UiBackgroundJobSubmitError::Stopped
                : g_background_jobs->submitUserReport(
                      smart_attendance::app::UiBackgroundJobOwner::AttendanceStatistics,
                      uid, s_txt, e_txt, requestId);
            if (submitResult != smart_attendance::app::UiBackgroundJobSubmitError::None) {
                lv_obj_delete(spin);
                show_popup_msg("导出个人考勤报表失败",
                               "后台任务繁忙或正在退出，请稍后重试!",
                               nullptr,
                               "我知道了");
                return;
            }
            g_background_spinners.emplace(requestId, spin);
        }
    }
}

//下载个人考勤报表界面
void load_download_personal_screen() {

    if(scr_download_personal){
        lv_obj_delete(scr_download_personal);
        scr_download_personal = nullptr;
    }

    // 1. 创建基础屏幕
    BaseScreenParts parts = create_base_screen("下载个人考勤报表");
    scr_download_personal = parts.screen;
    dependencies().uiManager.registerScreen(ScreenType::PERSONAGE_ATT_STATS, &scr_download_personal);

    // 绑定销毁回调
    lv_obj_add_event_cb(scr_download_personal, [](lv_event_t * e) {
        scr_download_personal = nullptr;
        g_ta_dl_psn_uid = nullptr;
        g_ta_dl_psn_start = nullptr;
        g_ta_dl_psn_end = nullptr;
        g_btn_dl_psn_confirm = nullptr;
    }, LV_EVENT_DELETE, nullptr);

    // 绑定全局 ESC 返回事件
    lv_obj_add_event_cb(scr_download_personal, download_personal_event_cb, LV_EVENT_KEY, nullptr);
    dependencies().uiManager.resetKeypadGroup();

    lv_obj_t* form_cont = create_form_container(parts.content);//

    // 1. 工号输入框
    g_ta_dl_psn_uid = create_form_input(form_cont, "员工工号:", "请输入工号", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_psn_uid, "0123456789"); // 严格限制只能输入数字
    lv_obj_add_event_cb(g_ta_dl_psn_uid, download_personal_event_cb, LV_EVENT_ALL, nullptr);
    dependencies().uiManager.addObjToGroup(g_ta_dl_psn_uid);

    // 2. 开始时间输入框
    g_ta_dl_psn_start = create_form_input(form_cont, "开始时间:", "如: 2026-01-01", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_psn_start, "0123456789-"); // 严格限制数字和横杠
    lv_obj_add_event_cb(g_ta_dl_psn_start, download_personal_event_cb, LV_EVENT_ALL, nullptr);
    dependencies().uiManager.addObjToGroup(g_ta_dl_psn_start);

    // 3. 结束时间输入框
    g_ta_dl_psn_end = create_form_input(form_cont, "结束时间:", "如: 2026-01-31", "", false);
    lv_textarea_set_accepted_chars(g_ta_dl_psn_end, "0123456789-"); // 严格限制数字和横杠
    lv_obj_add_event_cb(g_ta_dl_psn_end, download_personal_event_cb, LV_EVENT_ALL, nullptr);
    dependencies().uiManager.addObjToGroup(g_ta_dl_psn_end);

    // 4. 确认下载按钮
    g_btn_dl_psn_confirm = create_form_btn(form_cont, "确认下载", download_personal_event_cb, nullptr);
    lv_obj_add_event_cb(g_btn_dl_psn_confirm, download_personal_event_cb, LV_EVENT_KEY, nullptr);
    dependencies().uiManager.addObjToGroup(g_btn_dl_psn_confirm);

    // 默认聚焦在工号输入框
    lv_group_focus_obj(g_ta_dl_psn_uid);

    lv_screen_load(scr_download_personal);
    dependencies().uiManager.destroyAllScreensExcept(scr_download_personal);
}


} // namespace att_stats
} // namespace ui
