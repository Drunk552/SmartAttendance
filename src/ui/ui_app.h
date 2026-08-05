#ifndef UI_APP_H
#define UI_APP_H

#ifdef __cplusplus
namespace smart_attendance::app {
class UiBackgroundJobQueue;
class UiSystemStatusMailbox;
}

namespace smart_attendance::hal {
class IDisplay;
class IKeypad;
}

class UiController;
void ui_configure_controller(UiController& controller) noexcept;

void ui_configure_background_jobs(
    smart_attendance::app::UiBackgroundJobQueue& backgroundJobs) noexcept;
void ui_configure_system_status(
    smart_attendance::app::UiSystemStatusMailbox& systemStatus) noexcept;
void ui_configure_platform(
    smart_attendance::hal::IDisplay& display,
    smart_attendance::hal::IKeypad& keypad) noexcept;
void ui_process_background_results();
void ui_process_system_status();

extern "C" {
#endif

/**
 * @brief UI 子系统入口初始化
 * @details 负责 HAL (SDL/FB) 初始化、输入设备配置、管理器启动以及加载主页
 */
void ui_init(void);

/**
 * @brief 在 UI 主线程释放 LVGL、SDL 及页面后台任务观察状态。
 * @note 调用前必须先停止所有可能生产 UI 数据的后台 Worker。
 */
void ui_shutdown(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // UI_APP_H
