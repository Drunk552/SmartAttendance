/**
 * @file application.h
 * @brief 声明整机应用生命周期、任务编排和数据库资源所有权。
 */

#ifndef SMART_ATTENDANCE_APP_APPLICATION_H
#define SMART_ATTENDANCE_APP_APPLICATION_H

#include "task_manager.h"

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
    RuntimeDirectoryUnavailable,
    DatabaseInitializationFailed
};

/**
 * @brief 旧数据层接入 Application 的过渡适配接口。
 *
 * 两个回调均由 Application 调用；调用方必须保证回调在 Application 生命周期内有效。
 */
struct DatabaseLifecycle {
    bool (*initialize)();
    void (*close)();
};

class Application final {
public:
    /**
     * @brief 创建应用组合根，但不立即访问文件系统或数据库。
     * @param databaseLifecycle 数据库初始化和关闭回调，Application 不拥有回调目标。
     * @param taskInitializer Worker 启动前的业务资源初始化入口。
     * @param captureWorkerLifecycle 摄像头采集 Worker 入口。
     * @param databaseWriterWorkerLifecycle 数据库写 Worker 入口和唤醒函数。
     * @param runtimeDirectory 运行时数据目录；初始化成功后成为进程工作目录。
     */
    Application(DatabaseLifecycle databaseLifecycle,
                TaskInitializer taskInitializer,
                WorkerLifecycle captureWorkerLifecycle,
                WorkerLifecycle databaseWriterWorkerLifecycle,
                std::filesystem::path runtimeDirectory);
    ~Application() noexcept;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    /**
     * @brief 创建运行目录并初始化唯一的数据库连接。
     * @return 成功返回 None；失败返回具体原因且状态保持 Created。
     *
     * @note 该函数修改进程工作目录，因此必须在 UI、业务线程和文件访问启动前调用。
     */
    ApplicationInitError initialize() noexcept;

    /** @brief 启动后台任务并将应用从 Initialized 转换为 Running。 */
    bool markRunning() noexcept;

    /**
     * @brief 请求停止应用。
     * @note 停止请求先传递给 TaskManager，线程等待由 stop() 统一执行。
     */
    bool requestStop() noexcept;

    /**
     * @brief 等待后台任务退出，再关闭数据库并进入 Stopped 状态。
     * @return 全部资源成功停止返回 true；状态不合法或回调异常时返回 false。
     */
    bool stop() noexcept;

    ApplicationState state() const noexcept;
    const std::filesystem::path& runtimeDirectory() const noexcept;

private:
    void closeDatabaseNoexcept() noexcept;

    DatabaseLifecycle databaseLifecycle_;
    TaskManager taskManager_;
    std::filesystem::path runtimeDirectory_;
    ApplicationState state_{ApplicationState::Created};
    bool databaseInitialized_{false};
};

} // namespace smart_attendance::app

#endif // SMART_ATTENDANCE_APP_APPLICATION_H
