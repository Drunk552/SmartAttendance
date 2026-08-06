/**
 * @file application.h
 * @brief 声明整机应用组合根及主循环和资源关闭顺序。
 */

#ifndef SMART_ATTENDANCE_APP_APPLICATION_H
#define SMART_ATTENDANCE_APP_APPLICATION_H

#include "application_services.h"
#include "task_manager.h"
#include "ui/presenters/employee_lookup_presenter.h"
#include "ui/presenters/settings_presenter.h"
#include "ui/presenters/department_presenter.h"
#include "ui/presenters/shift_presenter.h"
#include "ui/presenters/attendance_query_presenter.h"
#include "ui/presenters/maintenance_presenter.h"
#include "ui/presenters/system_info_presenter.h"
#include "ui/ui_page_dependencies.h"
#include "ui/managers/ui_manager.h"

#include <filesystem>

namespace smart_attendance::app {

enum class ApplicationState {
    Created,
    Initialized,
    Running,
    StopRequested,
    Stopped
};

enum class ApplicationInitError {
    None,
    InvalidState,
    InvalidDatabaseLifecycle,
    InvalidUiLifecycle,
    InvalidApplicationLoop,
    InvalidBusinessLifecycle,
    InvalidPlatformDevices,
    RuntimeDirectoryUnavailable,
    DatabaseInitializationFailed,
    UiInitializationFailed
};

enum class ApplicationRunError {
    None,
    InvalidState,
    LoopFailed
};

/**
 * @brief 旧 UI 子系统接入 Application 的过渡生命周期接口。
 *
 * 两个回调均由主线程调用且必须同步完成。initialize 若抛出异常，必须先自行
 * 清理其部分初始化状态；Application 会停止后续启动并关闭已经打开的数据库。
 */
struct UiLifecycle {
    void (*initialize)();
    void (*shutdown)();
};

/**
 * @brief 将平台停止条件和单次 UI 处理注入 Application 主循环。
 *
 * 两个回调均在调用 run() 的主线程执行，不得创建分离线程。runOnce 必须自行
 * 限制单次阻塞时间，以便退出请求可以被及时观察。
 */
struct ApplicationLoop {
    bool (*shouldStop)();
    void (*runOnce)();
};

class Application final {
public:
    /**
     * @brief 创建应用组合根，但不立即访问文件系统或数据库。
     * @param databaseLifecycle 数据库初始化和关闭回调，Application 不拥有回调目标。
     * @param uiLifecycle UI 主线程初始化和关闭回调，Application 不拥有回调目标。
     * @param applicationLoop 主线程停止条件和单次循环处理回调。
     * @param businessLifecycle Worker 启动前初始化及 UI 关闭后资源释放入口。
     * @param userReportExporter 个人报表导出函数。
     * @param customReportExporter 按时间范围导出全员报表的函数。
     * @param employeeSettingsExporter 员工设置表导出函数。
     * @param employeeSettingsImporter 员工设置表导入函数。
     * @param monitorWorkerLifecycle 向有界 UI 状态邮箱写入的监控 Worker 入口及唤醒函数。
     * @param frameDeliveryWorkerLifecycle UI 帧投递 Worker 入口。
     * @param captureWorkerLifecycle 摄像头采集 Worker 入口。
     * @param databaseWriterWorkerLifecycle 数据库写 Worker 入口和唤醒函数。
     * @param platformDevices 当前构建平台的完整 HAL 对象集合，所有权转入 Application。
     * @param runtimeDirectory 运行时数据目录；初始化成功后成为进程工作目录。
     */
    Application(DatabaseLifecycle databaseLifecycle,
                UiLifecycle uiLifecycle,
                ApplicationLoop applicationLoop,
                BusinessLifecycle businessLifecycle,
                UserReportExporter userReportExporter,
                CustomReportExporter customReportExporter,
                EmployeeSettingsExporter employeeSettingsExporter,
                EmployeeSettingsImporter employeeSettingsImporter,
                MonitorWorkerLifecycle monitorWorkerLifecycle,
                WorkerLifecycle frameDeliveryWorkerLifecycle,
                WorkerLifecycle captureWorkerLifecycle,
                WorkerLifecycle databaseWriterWorkerLifecycle,
                PlatformDevices platformDevices,
                std::filesystem::path runtimeDirectory);
    ~Application() noexcept;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    /**
     * @brief 创建运行目录，依次初始化唯一数据库连接和 UI 子系统。
     * @return 成功返回 None；失败返回具体原因且状态保持 Created。
     *
     * @note 该函数修改进程工作目录，因此必须在 UI、业务线程和文件访问启动前调用。
     */
    ApplicationInitError initialize() noexcept;

    /** @brief 启动后台任务并将应用从 Initialized 转换为 Running。 */
    bool markRunning() noexcept;

    /**
     * @brief 在调用线程执行应用主循环，直到 shouldStop() 返回 true。
     * @return 正常结束返回 None；状态错误或回调异常返回具体错误。
     * @note 返回后状态仍为 Running；调用方必须继续执行 requestStop() 和 stop()。
     */
    ApplicationRunError run() noexcept;

    /**
     * @brief 请求停止应用。
     * @note 停止请求先传递给 TaskManager，线程等待由 stop() 统一执行。
     */
    bool requestStop() noexcept;

    /**
     * @brief 等待任务退出，依次关闭 UI、业务资源和数据库并进入 Stopped 状态。
     * @return 全部资源成功停止返回 true；状态不合法或回调异常时返回 false。
     */
    bool stop() noexcept;

    ApplicationState state() const noexcept;
    const std::filesystem::path& runtimeDirectory() const noexcept;
    UiBackgroundJobQueue& uiBackgroundJobs() noexcept;
    UiSystemStatusMailbox& uiSystemStatus() noexcept;

    /** @brief 显式访问由 ApplicationServices 持有的统一打卡服务。 */
    services::PunchService& punchService() noexcept;

    /** @brief 显式访问由 ApplicationServices 持有的人脸算法引擎。 */
    biometric::face::IFaceRecognitionEngine& faceRecognitionEngine() noexcept;

    hal::ICamera& camera() noexcept;
    hal::IDisplay& display() noexcept;
    hal::IKeypad& keypad() noexcept;
    hal::IRtc& rtc() noexcept;
    hal::IStorageDevice& storage() noexcept;

    /** @brief 返回由组合根持有的员工角色 Presenter。 */
    ui::EmployeeLookupPresenter& employeeLookupPresenter() noexcept;

    /** @brief 返回由组合根持有的系统设置 Presenter。 */
    ui::SettingsPresenter& settingsPresenter() noexcept;

    /** @brief 返回由组合根持有的部门设置 Presenter。 */
    ui::DepartmentPresenter& departmentPresenter() noexcept;

    ui::ShiftPresenter& shiftPresenter() noexcept;
    ui::AttendanceQueryPresenter& attendanceQueryPresenter() noexcept;
    ui::MaintenancePresenter& maintenancePresenter() noexcept;

    /** @brief 将组合根持有的对象绑定到旧 C/UI 运行入口。 */
    bool configureRuntimeBindings() noexcept;

private:
    bool shutdownUiNoexcept() noexcept;

    UiLifecycle uiLifecycle_;
    ApplicationLoop applicationLoop_;
    ApplicationServices services_;
    UiManager uiManager_;
    ui::EmployeeLookupPresenter employeeLookupPresenter_;
    ui::SettingsPresenter settingsPresenter_;
    ui::DepartmentPresenter departmentPresenter_;
    ui::ShiftPresenter shiftPresenter_;
    ui::AttendanceQueryPresenter attendanceQueryPresenter_;
    ui::MaintenancePresenter maintenancePresenter_;
    ui::SystemInfoPresenter systemInfoPresenter_;
    ui::UiPageDependencies uiPageDependencies_;
    TaskManager taskManager_;
    std::filesystem::path runtimeDirectory_;
    ApplicationState state_{ApplicationState::Created};
    bool uiInitialized_{false};
};

} // namespace smart_attendance::app

#endif // SMART_ATTENDANCE_APP_APPLICATION_H
