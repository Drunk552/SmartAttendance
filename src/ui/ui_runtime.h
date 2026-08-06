#ifndef SMART_ATTENDANCE_UI_UI_RUNTIME_H
#define SMART_ATTENDANCE_UI_UI_RUNTIME_H

#include <atomic>

namespace smart_attendance::app {
class UiSystemStatusMailbox;
}

namespace smart_attendance::hal {
class IRtc;
class IStorageDevice;
}

namespace smart_attendance::storage {
class IEmployeeSettingsImportRepository;
}

class UiManager;

/** @brief 注入监控和导入任务使用的时钟与受控存储目录。 */
void uiConfigureDeviceServices(
    smart_attendance::hal::IRtc& rtc,
    smart_attendance::hal::IStorageDevice& storage) noexcept;
void uiResetDeviceServices() noexcept;
void uiConfigureManager(UiManager& uiManager) noexcept;

/** @brief 运行由 TaskManager 持有的时间与磁盘监控任务。 */
void uiRunMonitorTask(
    const std::atomic<bool>& stopRequested,
    smart_attendance::app::UiSystemStatusMailbox& statusMailbox);
void uiWakeMonitorTask();

/** @brief 将业务层最新帧投递到 UI 管理器的固定缓冲区。 */
void uiRunFrameDeliveryTask(const std::atomic<bool>& stopRequested);

/** @brief 兼容现有员工设置 XLSX 格式的导入实现。 */
bool uiImportEmployeeSettings(
    smart_attendance::storage::IEmployeeSettingsImportRepository& repository,
    int* invalidTimeCount = nullptr);

#endif // SMART_ATTENDANCE_UI_UI_RUNTIME_H
