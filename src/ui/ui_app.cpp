#include "infrastructure/logging/logger.h"
/**
 * @file ui_app.cpp
 * @brief UI 入口 (WSL2/PC 仿真版)
 * @details 使用 SDL2 驱动显示窗口和接收输入，而非读写底层设备节点
 */

#include "ui_app.h"
#include "app/ui_background_job_queue.h"
#include "app/ui_system_status_mailbox.h"
#include "hal/display.h"
#include "hal/keypad.h"
#include <lvgl.h>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <stdexcept>

// 模块接口
#include "managers/ui_manager.h"
#include "common/ui_style.h"
#include "common/ui_widgets.h"
#include "ui_page_dependencies.h"
#include "ui_runtime.h"

// 引入主页头文件
#include "screens/home/ui_scr_home.h"
#include "screens/menu/ui_scr_menu.h"
#include "screens/user_mgmt/ui_user_mgmt.h"
#include "screens/record_query/ui_scr_record_query.h"
#include "screens/att_stats/ui_scr_att_stats.h"
#include "screens/att_design/ui_scr_att_design.h"
#include "screens/system/ui_sys_settings.h"
#include "screens/sys_info/ui_scr_sys_info.h"

// 全局退出标志 (定义在 main.cpp)
extern "C" {
    extern volatile sig_atomic_t g_program_should_exit;
}

namespace {

smart_attendance::app::UiBackgroundJobQueue* g_backgroundJobs = nullptr;
smart_attendance::app::UiSystemStatusMailbox* g_systemStatus = nullptr;
smart_attendance::hal::IDisplay* g_display = nullptr;
smart_attendance::hal::IKeypad* g_keypad = nullptr;
UiManager* g_uiManager = nullptr;
} // namespace

void ui_configure_page_dependencies(
    smart_attendance::ui::UiPageDependencies& dependencies) noexcept {
    ui::home::configureDependencies(dependencies.home);
    ui::user_mgmt::configureDependencies(dependencies.userManagement);
    ui::record_query::configureDependencies(dependencies.recordQuery);
    ui::att_stats::configureDependencies(dependencies.attendanceStatistics);
    ui::att_design::configureDependencies(dependencies.attendanceDesign);
    ui::system::configureDependencies(dependencies.systemSettings);
    ui::sys_info::configureDependencies(dependencies.systemInfo);
    ui::menu::configureDependencies(dependencies.menu);
}

void ui_configure_background_jobs(
    smart_attendance::app::UiBackgroundJobQueue& backgroundJobs) noexcept {
    g_backgroundJobs = &backgroundJobs;
    ui::record_query::configureBackgroundJobs(backgroundJobs);
    ui::att_stats::configureBackgroundJobs(backgroundJobs);
}

void ui_configure_system_status(
    smart_attendance::app::UiSystemStatusMailbox& systemStatus) noexcept {
    g_systemStatus = &systemStatus;
}

void ui_configure_platform(
    smart_attendance::hal::IDisplay& display,
    smart_attendance::hal::IKeypad& keypad,
    UiManager& uiManager) noexcept {
    g_display = &display;
    g_keypad = &keypad;
    g_uiManager = &uiManager;
}

void ui_process_background_results() {
    if (g_backgroundJobs == nullptr) {
        return;
    }

    smart_attendance::app::UiBackgroundJobResult result{};
    while (g_backgroundJobs->tryPopResult(result)) {
        switch (result.owner) {
        case smart_attendance::app::UiBackgroundJobOwner::RecordQuery:
            ui::record_query::handleBackgroundJobResult(result);
            break;
        case smart_attendance::app::UiBackgroundJobOwner::AttendanceStatistics:
            ui::att_stats::handleBackgroundJobResult(result);
            break;
        }
    }
}

void ui_process_system_status() {
    if (g_systemStatus == nullptr) {
        return;
    }

    smart_attendance::app::UiSystemStatusSnapshot snapshot{};
    if (!g_systemStatus->tryConsume(snapshot)) {
        return;
    }

    update_base_screen_time(snapshot.timeText, snapshot.weekdayText);
    ui::home::update_time(snapshot.timeText, {});
    if (snapshot.diskStatusKnown) {
        ui::home::update_disk_status(snapshot.diskFull);
    }
}

void ui_init(void) {
    SA_LOG_INFO(">>> [UI] 初始化 (WSL2 SDL仿真版)...\n");

    // 1. 防止 SDL 窗口黑屏休眠
    setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "0", 1);

    // ============================================================
    // 2. LVGL & HAL 初始化 (使用 SDL 驱动)
    // ============================================================
    lv_init();

    if (g_display == nullptr || !g_display->initialize()) {
        lv_deinit();
        throw std::runtime_error("platform display initialization failed");
    }
    SA_LOG_INFO("[UI] Display initialized (%dx%d).\n",
           g_display->width(), g_display->height());

    if (g_keypad == nullptr || !g_keypad->initialize()) {
        g_display->shutdown();
        lv_deinit();
        throw std::runtime_error("platform keypad initialization failed");
    }

    // ============================================================
    // 3. 管理器初始化 (创建 Group)
    // ============================================================
    ui_style_init();
    if (g_uiManager == nullptr) {
        g_keypad->shutdown();
        g_display->shutdown();
        lv_deinit();
        throw std::runtime_error("UI manager is not configured");
    }
    g_uiManager->init(); // 内部创建 g_keypad_group

    // ============================================================
    // 4. 绑定键盘到 UI (解决无法操作菜单的问题)
    // ============================================================
    lv_group_t* group = g_uiManager->getKeypadGroup();
    if (group != nullptr) {
        lv_indev_t* input = lv_indev_get_next(nullptr);
        while (input != nullptr) {
            if (lv_indev_get_type(input) == LV_INDEV_TYPE_KEYPAD) {
                lv_indev_set_group(input, group);
            }
            input = lv_indev_get_next(input);
        }
        lv_group_set_wrap(group, true);
        SA_LOG_INFO("[UI] Platform keypad bound to Manager Group.\n");
    }

    // ============================================================
    // 5. 启动业务与加载主页
    // ============================================================
    SA_LOG_INFO("[UI] Loading Home Screen...\n");
    // 直接调用模块加载函数
    ui::home::load_screen();

    SA_LOG_INFO("[UI] Initialization Completed.\n");
}

void ui_shutdown(void) {
    SA_LOG_INFO(">>> [UI] 正在释放 LVGL/SDL 资源...\n");
    g_backgroundJobs = nullptr;
    g_systemStatus = nullptr;
    ui::record_query::resetBackgroundJobs();
    ui::att_stats::resetBackgroundJobs();
    if (g_keypad != nullptr) {
        g_keypad->shutdown();
    }
    if (g_display != nullptr) {
        g_display->shutdown();
    }
    lv_deinit();
    uiResetDeviceServices();
    g_keypad = nullptr;
    g_display = nullptr;
    g_uiManager = nullptr;
    SA_LOG_INFO(">>> [UI] LVGL/SDL 资源已释放。\n");
}
